#include "ProductionSystem.h"
#include "ECS/Components.h"
#include <algorithm>
#include <map>

// At full workforce (BASE_WORKERS) production runs at baseline rate.
// Fewer workers → proportional reduction; more workers → bonus up to 2×.
static constexpr int   BASE_WORKERS   = 5;
static constexpr float MAX_SCALE      = 2.0f;
static constexpr float STOCKPILE_CAP  = 500.f;  // max units per resource type

void ProductionSystem::Update(entt::registry& registry, float realDt) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    float gameDt = timeView.get<TimeManager>(*timeView.begin()).GameDt(realDt);
    if (gameDt <= 0.0f) return;

    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.0f;

    // Count Working-state NPCs per settlement (excludes haulers and player).
    std::map<entt::entity, int> workers;
    registry.view<AgentState, HomeSettlement>(entt::exclude<Hauler, PlayerTag>)
        .each([&](auto e, const AgentState& as, const HomeSettlement& hs) {
            if (as.behavior == AgentBehavior::Working)
                workers[hs.settlement]++;
        });

    auto facView = registry.view<ProductionFacility>();
    for (auto entity : facView) {
        const auto& fac = facView.get<ProductionFacility>(entity);
        if (fac.settlement == entt::null || !registry.valid(fac.settlement)) continue;
        if (fac.baseRate <= 0.f) continue;   // shelter nodes produce nothing

        auto* stockpile = registry.try_get<Stockpile>(fac.settlement);
        if (!stockpile) continue;

        int   w     = workers.count(fac.settlement) ? workers.at(fac.settlement) : 0;
        float scale = std::min(MAX_SCALE, static_cast<float>(w) / BASE_WORKERS);
        scale       = std::max(0.1f, scale);   // never fully zero — keep a trickle

        // Apply any active settlement modifier (e.g. drought reduces production)
        const auto* settl = registry.try_get<Settlement>(fac.settlement);
        float modifier = settl ? settl->productionModifier : 1.f;

        float grossOutput = fac.baseRate * scale * gameHoursDt * modifier;

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
