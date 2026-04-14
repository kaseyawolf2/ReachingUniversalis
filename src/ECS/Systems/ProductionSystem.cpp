#include "ProductionSystem.h"
#include "ECS/Components.h"
#include "World/WorldSchema.h"
#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <unordered_map>

static std::mt19937 s_prodRng(42);

// At full workforce (BASE_WORKERS) production runs at baseline rate.
// Fewer workers → proportional reduction; more workers → bonus up to 2×.
static constexpr int   BASE_WORKERS   = 5;
static constexpr float MAX_SCALE      = 2.0f;
static constexpr float STOCKPILE_CAP  = 500.f;  // max units per resource type

// Food spoils at this fraction per game-hour (base rate; summer doubles it).
// 0.5% per hour = ~12% loss per game-day; keeps stockpiles from bloating indefinitely.
static constexpr float FOOD_SPOILAGE_RATE = 0.005f;

void ProductionSystem::Update(entt::registry& registry, float realDt, const WorldSchema& schema) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    const auto& tm = timeView.get<TimeManager>(*timeView.begin());
    float gameDt = tm.GameDt(realDt);
    if (gameDt <= 0.0f) return;

    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.0f;
    SeasonID seasonId = tm.CurrentSeason(schema.seasons);
    float seasonMod   = 1.f;
    float baseTemp    = 20.f;
    if (seasonId >= 0 && seasonId < (int)schema.seasons.size()) {
        seasonMod = schema.seasons[seasonId].productionMod;
        baseTemp  = schema.seasons[seasonId].baseTemperature;
    }

    // ---- Food spoilage ----
    // Hot seasons (base temp >= 25) double spoilage; cold seasons (base temp <= 0) halve it.
    float spoilageMult = (baseTemp >= 25.f) ? 2.0f :
                         (baseTemp <= 0.f)  ? 0.5f : 1.0f;
    float spoilFraction = FOOD_SPOILAGE_RATE * spoilageMult * gameHoursDt;
    registry.view<Stockpile>().each([&](Stockpile& sp) {
        auto it = sp.quantities.find(RES_FOOD);
        if (it != sp.quantities.end() && it->second > 0.f) {
            float loss = it->second * spoilFraction;
            it->second = std::max(0.f, it->second - loss);
        }
    });

    // Count Working-state NPCs per settlement and accumulate skill per resource type.
    // Apprentice children (ChildTag, age 12–14) count as 0.2 workers (produce at 20% rate).
    // Also count elders (age > 60) at their home settlement for the wisdom/experience bonus.
    struct SkillAccum { float sum = 0.f; int count = 0; };
    std::unordered_map<entt::entity, float> workers;  // float to allow fractional apprentice contributions
    std::unordered_map<entt::entity, int>   workerHeadCount; // integer count for crowding detection
    std::unordered_map<entt::entity, int>   elderCount; // elders per settlement for production bonus
    // [settlement][resourceID] — keyed by resource ID for generic skill mapping
    std::unordered_map<entt::entity, std::map<int, SkillAccum>> skillData;
    // Profession diversity: bitmask per settlement (bit N = professionID N that produces a resource)
    std::unordered_map<entt::entity, uint32_t> profDiversity;

    // Include player if Working — they contribute to the facility they're at.
    registry.view<AgentState, HomeSettlement>(entt::exclude<Hauler>)
        .each([&](auto e, const AgentState& as, const HomeSettlement& hs) {
            if (hs.settlement == entt::null || !registry.valid(hs.settlement)) return;
            const auto* ageComp = registry.try_get<Age>(e);
            bool isElder = (ageComp && ageComp->days > 60.f);
            // Elder bonus: count elders present at home settlement (regardless of working).
            if (isElder) {
                elderCount[hs.settlement]++;
                if (as.behavior != AgentBehavior::Working) return;
            }
            if (as.behavior != AgentBehavior::Working) return;
            // Apprentice children contribute at 20% of adult rate.
            bool isApprentice = registry.all_of<ChildTag>(e)
                                && ageComp
                                && ageComp->days >= 12.f
                                && ageComp->days < 15.f;
            // "ambitious" behaviour modifier: motivated workers produce 10% more
            bool hasAmbitiousGoal = false;
            if (const auto* g = registry.try_get<Goal>(e))
                if (g->goalId >= 0 && g->goalId < (int)schema.goals.size())
                    hasAmbitiousGoal = (schema.goals[g->goalId].behaviourModEnum == GoalBehaviourMod::Ambitious);
            float workerContrib = isApprentice ? 0.2f : (hasAmbitiousGoal ? 1.1f : 1.0f);
            // Peak-age bonus: prime working years (25–55) get +10% output
            if (!isApprentice && ageComp && ageComp->days >= 25.f && ageComp->days <= 55.f)
                workerContrib *= 1.1f;
            // Good-harvest personal event: worker is "on fire" — 1.5× contribution
            if (!isApprentice) {
                if (const auto* dt = registry.try_get<DeprivationTimer>(e))
                    if (dt->harvestBonusTimer > 0.f)
                        workerContrib = 1.5f;
            }
            // Elder knowledge bonus: working elders provide tacit knowledge (+0.05)
            if (isElder) {
                workerContrib += 0.05f;
                workerContrib = std::min(2.0f, workerContrib);  // cap per-elder contribution
            }
            // Track fatigue: fatigued workers produce at 80% rate
            // Overworked penalty: 10+ consecutive hours → 85% rate
            if (const auto* sched = registry.try_get<Schedule>(e)) {
                if (sched->fatigued)
                    workerContrib *= 0.8f;
                if (sched->consecutiveWorkHours >= 10)
                    workerContrib *= 0.85f;
            }
            // Contentment factor: unhappy NPCs produce less
            if (const auto* needs = registry.try_get<Needs>(e)) {
                float sum = 0.f;
                for (int ni = 0; ni < 4; ++ni) sum += needs->list[ni].value;
                float contentment = sum / 4.f;
                float contentFactor = (contentment >= 0.7f) ? 1.0f :
                                      (contentment >= 0.4f) ? 0.85f : 0.65f;
                workerContrib *= contentFactor;
            }
            // Grief penalty: grieving workers produce at half rate
            if (const auto* dt = registry.try_get<DeprivationTimer>(e)) {
                if (dt->griefTimer > 0.f)
                    workerContrib *= 0.5f;
            }
            // Reconciliation glow: recently reconciled workers produce +5%
            if (auto* dt2 = registry.try_get<DeprivationTimer>(e)) {
                if (dt2->reconcileGlow > 0.f) {
                    workerContrib *= 1.05f;
                    dt2->reconcileGlow = std::max(0.f, dt2->reconcileGlow - gameHoursDt);
                }
            }
            // Jack-of-all-trades: generalists with all skills >= 0.4 get +5%
            if (const auto* skills = registry.try_get<Skills>(e)) {
                if (skills->AllAbove(0.4f))
                    workerContrib *= 1.05f;
            }
            // Track profession diversity for settlement bonus
            if (const auto* prof = registry.try_get<Profession>(e)) {
                if (prof->type >= 0 && prof->type < (int)schema.professions.size()
                    && schema.professions[prof->type].producesResource != INVALID_ID)
                    profDiversity[hs.settlement] |= (uint32_t(1) << prof->type);
            }
            workers[hs.settlement] += workerContrib;
            workerHeadCount[hs.settlement]++;
            if (const auto* skills = registry.try_get<Skills>(e)) {
                auto& arr = skillData[hs.settlement];
                // Accumulate per-resource skill using schema mapping
                for (const auto& sd : schema.skills) {
                    if (sd.forResource != INVALID_ID) {
                        int ri = sd.forResource;
                        arr[ri].sum += skills->Get(sd.id);
                        arr[ri].count++;
                    }
                }
            }
        });

    // ---- Settlement profession diversity bonus ----
    // Settlements with all producing professions get +3% production.
    uint32_t fullProfMask = 0;
    for (auto& pd : schema.professions)
        if (pd.producesResource != INVALID_ID) fullProfMask |= (uint32_t(1) << pd.id);
    {
        static std::map<entt::entity, int> s_diverseLogged;
        for (auto& [settl, w] : workers) {
            if (fullProfMask != 0 && profDiversity.count(settl)
                && (profDiversity[settl] & fullProfMask) == fullProfMask) {
                w *= 1.03f;
                // Log once per game-day at 1-in-10 frequency
                if (s_diverseLogged.count(settl) == 0 || s_diverseLogged[settl] != tm.day) {
                    if (s_prodRng() % 10 == 0) {
                        auto logView2 = registry.view<EventLog>();
                        if (logView2.begin() != logView2.end()) {
                            std::string sname = "A settlement";
                            if (const auto* s = registry.try_get<Settlement>(settl)) sname = s->name;
                            logView2.get<EventLog>(*logView2.begin()).Push(tm.day, (int)tm.hourOfDay,
                                sname + " benefits from a diverse workforce.");
                        }
                    }
                    s_diverseLogged[settl] = tm.day;
                }
            }
        }
    }

    // ---- Facility crowding log ----
    // Log once per settlement per game-day when 4+ workers are competing.
    {
        static std::map<entt::entity, int> s_lastCrowdLog;
        auto logView = registry.view<EventLog>();
        EventLog* crowdLog = (logView.begin() != logView.end())
                             ? &logView.get<EventLog>(*logView.begin()) : nullptr;
        for (const auto& [settl, count] : workerHeadCount) {
            if (count < 4) continue;
            if (s_lastCrowdLog.count(settl) && s_lastCrowdLog[settl] == tm.day) continue;
            s_lastCrowdLog[settl] = tm.day;
            if (crowdLog) {
                std::string settName = "A settlement";
                if (const auto* s = registry.try_get<Settlement>(settl))
                    settName = s->name;
                char buf[120];
                std::snprintf(buf, sizeof(buf), "%s is crowded — %d workers competing.",
                              settName.c_str(), count);
                crowdLog->Push(tm.day, (int)tm.hourOfDay, buf);
            }
        }
        // Prune dead entities
        for (auto it = s_lastCrowdLog.begin(); it != s_lastCrowdLog.end(); )
            if (!registry.valid(it->first)) it = s_lastCrowdLog.erase(it); else ++it;
    }

    // ---- Settlement specialisation: count masters per skill per settlement ----
    // masterCount[settlement][skillId] = number of NPCs with that skill >= 0.9
    std::unordered_map<entt::entity, std::map<int, int>> masterCount;
    registry.view<Skills, HomeSettlement>(entt::exclude<Hauler>).each(
        [&](auto, const Skills& sk, const HomeSettlement& hs) {
            if (hs.settlement == entt::null || !registry.valid(hs.settlement)) return;
            auto& mc = masterCount[hs.settlement];
            for (int si = 0; si < sk.Size(); ++si)
                if (sk.levels[si] >= 0.9f) mc[si]++;
        });
    // Log specialisation once per settlement+type combination
    static std::set<uint64_t> s_specLogged;

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
        if (skillData.count(fac.settlement)) {
            auto it = skillData.at(fac.settlement).find(fac.output);
            if (it != skillData.at(fac.settlement).end() && it->second.count > 0) {
                float avgSkill = it->second.sum / it->second.count;
                skillMult = 0.5f + avgSkill;   // range [0.5, 1.5]
            }
        }

        // Apply settlement modifier (drought etc.), season modifier, elder wisdom, and morale.
        // Elder bonus: each elder at home = +0.5% production, capped at +5%.
        // Continuous morale factor: 1.0 + 0.3*(morale - 0.5)
        // At morale 0.8 → +9%, at 0.5 → 0%, at 0.2 → -9%
        const auto* settl = registry.try_get<Settlement>(fac.settlement);
        int   elders     = elderCount.count(fac.settlement) ? elderCount.at(fac.settlement) : 0;
        float elderBonus = 1.f + std::min(0.05f, elders * 0.005f);
        float moraleBonus = 1.0f;
        if (settl)
            moraleBonus = 1.0f + 0.3f * (settl->morale - 0.5f);
        // Specialisation bonus: +15% when >=3 masters in the facility's resource skill
        float specBonus = 1.0f;
        int facSkillId = Skills::SkillIdForResource(fac.output, schema);
        if (facSkillId != INVALID_ID && masterCount.count(fac.settlement)) {
            const auto& mc = masterCount.at(fac.settlement);
            auto mcIt = mc.find(facSkillId);
            if (mcIt != mc.end() && mcIt->second >= 3) {
                specBonus = 1.15f;
                // Log once per settlement+type
                uint64_t specKey = ((uint64_t)entt::to_integral(fac.settlement) << 32) | (uint32_t)facSkillId;
                if (s_specLogged.insert(specKey).second) {
                    std::string typeName = (facSkillId < (int)schema.skills.size())
                        ? schema.skills[facSkillId].displayName : "Skill";
                    auto lv = registry.view<EventLog>();
                    if (lv.begin() != lv.end()) {
                        std::string sName = settl ? settl->name : "Settlement";
                        char buf[128];
                        std::snprintf(buf, sizeof(buf),
                            "%s has a thriving %s tradition (+15%% output).",
                            sName.c_str(), typeName.c_str());
                        auto& tm2 = registry.view<TimeManager>().get<TimeManager>(
                            *registry.view<TimeManager>().begin());
                        lv.get<EventLog>(*lv.begin()).Push(tm2.day, (int)tm2.hourOfDay, buf);
                    }
                }
            }
        }
        // Social cohesion bonus: +1% per friendship pair at the settlement, capped at +10%.
        // Count mutual pairs where both NPCs have Relations::affinity >= 0.5 toward each other.
        float cohesionBonus = 1.0f;
        if (fac.settlement != entt::null) {
            // Cache per settlement per frame to avoid recounting for every facility
            static std::map<entt::entity, int> s_cachedPairs;
            static int s_cachedDay = -1;
            static int s_cachedHour = -1;
            auto tmv = registry.view<TimeManager>();
            int curDay = 0, curHour = 0;
            if (!tmv.empty()) {
                const auto& tmRef = tmv.get<TimeManager>(*tmv.begin());
                curDay = tmRef.day; curHour = (int)tmRef.hourOfDay;
            }
            if (curDay != s_cachedDay || curHour != s_cachedHour) {
                s_cachedPairs.clear();
                s_cachedDay = curDay;
                s_cachedHour = curHour;
                // Build per-settlement friendship pair counts
                registry.view<Relations, HomeSettlement>(
                    entt::exclude<Hauler, PlayerTag, ChildTag>).each(
                    [&](auto e, const Relations& rel, const HomeSettlement& hs) {
                    for (const auto& [other, aff] : rel.affinity) {
                        if (aff < 0.5f) continue;
                        if (e >= other) continue;  // count each pair once
                        if (!registry.valid(other)) continue;
                        const auto* oHome = registry.try_get<HomeSettlement>(other);
                        if (!oHome || oHome->settlement != hs.settlement) continue;
                        const auto* oRel = registry.try_get<Relations>(other);
                        if (!oRel) continue;
                        auto it = oRel->affinity.find(e);
                        if (it == oRel->affinity.end() || it->second < 0.5f) continue;
                        s_cachedPairs[hs.settlement]++;
                    }
                });
            }
            auto it = s_cachedPairs.find(fac.settlement);
            if (it != s_cachedPairs.end() && it->second > 0) {
                float bonus = std::min(0.10f, it->second * 0.01f);
                cohesionBonus = 1.0f + bonus;
            }
        }

        float modifier   = (settl ? settl->productionModifier : 1.f) * seasonMod * elderBonus * moraleBonus * specBonus * cohesionBonus;

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
