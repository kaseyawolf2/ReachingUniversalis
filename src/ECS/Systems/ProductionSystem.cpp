#include "ProductionSystem.h"
#include "ECS/Components.h"
#include <algorithm>
#include <map>

// At full workforce (BASE_WORKERS) production runs at baseline rate.
// Fewer workers → proportional reduction; more workers → bonus up to 2×.
static constexpr int   BASE_WORKERS   = 5;
static constexpr float MAX_SCALE      = 2.0f;
static constexpr float STOCKPILE_CAP  = 500.f;  // max units per resource type

// Food spoils at this fraction per game-hour (base rate; summer doubles it).
// 0.5% per hour = ~12% loss per game-day; keeps stockpiles from bloating indefinitely.
static constexpr float FOOD_SPOILAGE_RATE = 0.005f;

void ProductionSystem::Update(entt::registry& registry, float realDt) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    const auto& tm = timeView.get<TimeManager>(*timeView.begin());
    float gameDt = tm.GameDt(realDt);
    if (gameDt <= 0.0f) return;

    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.0f;
    Season season     = tm.CurrentSeason();
    float seasonMod   = SeasonProductionModifier(season);

    // ---- Food spoilage ----
    // Summer doubles the spoilage rate (heat); winter halves it (cold preserves food).
    float spoilageMult = (season == Season::Summer) ? 2.0f :
                         (season == Season::Winter) ? 0.5f : 1.0f;
    float spoilFraction = FOOD_SPOILAGE_RATE * spoilageMult * gameHoursDt;
    registry.view<Stockpile>().each([&](Stockpile& sp) {
        auto it = sp.quantities.find(ResourceType::Food);
        if (it != sp.quantities.end() && it->second > 0.f) {
            float loss = it->second * spoilFraction;
            it->second = std::max(0.f, it->second - loss);
        }
    });

    // Count Working-state NPCs per settlement and accumulate skill per resource type.
    // Apprentice children (ChildTag, age 12–14) count as 0.2 workers (produce at 20% rate).
    struct SkillAccum { float sum = 0.f; int count = 0; };
    std::map<entt::entity, float> workers;  // float to allow fractional apprentice contributions
    // [settlement][resourceIndex 0=Food,1=Water,2=Wood]
    std::map<entt::entity, std::array<SkillAccum, 3>> skillData;

    auto resIdx = [](ResourceType rt) -> int {
        switch (rt) {
            case ResourceType::Food:  return 0;
            case ResourceType::Water: return 1;
            case ResourceType::Wood:  return 2;
            default:                  return -1;
        }
    };

    // Include player if Working — they contribute to the facility they're at.
    registry.view<AgentState, HomeSettlement>(entt::exclude<Hauler>)
        .each([&](auto e, const AgentState& as, const HomeSettlement& hs) {
            if (as.behavior != AgentBehavior::Working) return;
            // Apprentice children contribute at 20% of adult rate.
            const auto* ageComp = registry.try_get<Age>(e);
            bool isApprentice = registry.all_of<ChildTag>(e)
                                && ageComp
                                && ageComp->days >= 12.f
                                && ageComp->days < 15.f;
            workers[hs.settlement] += isApprentice ? 0.2f : 1.0f;
            if (const auto* skills = registry.try_get<Skills>(e)) {
                auto& arr = skillData[hs.settlement];
                arr[0].sum += skills->farming;       arr[0].count++;
                arr[1].sum += skills->water_drawing; arr[1].count++;
                arr[2].sum += skills->woodcutting;   arr[2].count++;
            }
        });

    auto facView = registry.view<ProductionFacility>();
    for (auto entity : facView) {
        const auto& fac = facView.get<ProductionFacility>(entity);
        if (fac.settlement == entt::null || !registry.valid(fac.settlement)) continue;
        if (fac.baseRate <= 0.f) continue;   // shelter nodes produce nothing

        auto* stockpile = registry.try_get<Stockpile>(fac.settlement);
        if (!stockpile) continue;

        float w     = workers.count(fac.settlement) ? workers.at(fac.settlement) : 0.f;
        float scale = std::min(MAX_SCALE, w / static_cast<float>(BASE_WORKERS));
        scale       = std::max(0.1f, scale);   // never fully zero — keep a trickle

        // Skill multiplier: average relevant skill of working NPCs (0→1, default 0.5).
        // A fully skilled workforce (1.0) produces 2× the baseline; unskilled (0) produces 0×.
        // Blended: output × (0.5 + skill), so skill=0.5 → ×1.0, skill=1.0 → ×1.5
        float skillMult = 1.0f;
        int ri = resIdx(fac.output);
        if (ri >= 0 && skillData.count(fac.settlement)) {
            const auto& sa = skillData.at(fac.settlement)[ri];
            if (sa.count > 0) {
                float avgSkill = sa.sum / sa.count;
                skillMult = 0.5f + avgSkill;   // range [0.5, 1.5]
            }
        }

        // Apply settlement modifier (drought etc.) and season modifier
        const auto* settl = registry.try_get<Settlement>(fac.settlement);
        float modifier = (settl ? settl->productionModifier : 1.f) * seasonMod;

        float grossOutput = fac.baseRate * scale * skillMult * gameHoursDt * modifier;

        // Yield cycle: each facility has a slow sinusoidal ±20% variation
        // (period 15–25 game-days, phase staggered by entity ID) to simulate
        // natural variance like soil depletion, fish stock cycles, wood growth.
        {
            float gameHours  = tm.gameSeconds * GAME_MINS_PER_REAL_SEC / 60.f;
            uint32_t eid     = entt::to_integral(entity);
            float period     = 15.f * 24.f + (float)(eid % 240);  // 15–25 day period in hours
            float phase      = (float)(eid % 628) * 0.01f;        // 0–6.28 offset per facility
            float cycleMult  = 1.f + 0.20f * std::sin(6.28318f * gameHours / period + phase);
            grossOutput *= cycleMult;
        }

        // Consume inputs — if insufficient, scale down production proportionally
        if (!fac.inputsPerOutput.empty()) {
            float inputScale = 1.f;
            for (const auto& [inRes, inPerOut] : fac.inputsPerOutput) {
                float required = inPerOut * grossOutput;
                if (required <= 0.f) continue;
                float available = stockpile->quantities.count(inRes)
                                  ? stockpile->quantities.at(inRes) : 0.f;
                if (available <= 0.f) { inputScale = 0.f; break; }
                inputScale = std::min(inputScale, available / required);
            }
            grossOutput *= std::min(1.f, inputScale);
            // Consume the actual inputs used
            for (const auto& [inRes, inPerOut] : fac.inputsPerOutput) {
                float used = inPerOut * grossOutput;
                stockpile->quantities[inRes] = std::max(0.f,
                    (stockpile->quantities.count(inRes)
                     ? stockpile->quantities.at(inRes) : 0.f) - used);
            }
        }

        float& qty = stockpile->quantities[fac.output];
        qty = std::min(STOCKPILE_CAP, qty + grossOutput);
    }
}
