#include "ProductionSystem.h"
#include "ECS/Components.h"

void ProductionSystem::Update(entt::registry& registry, float realDt) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    float gameDt = timeView.get<TimeManager>(*timeView.begin()).GameDt(realDt);
    if (gameDt <= 0.0f) return;

    // Convert gameDt (scaled real seconds) to game-hours.
    // At 1x: 1 real second = 1 game minute = 1/60 game hour.
    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.0f;

    auto facView = registry.view<ProductionFacility>();
    for (auto entity : facView) {
        const auto& fac = facView.get<ProductionFacility>(entity);
        if (fac.settlement == entt::null || !registry.valid(fac.settlement)) continue;

        auto* stockpile = registry.try_get<Stockpile>(fac.settlement);
        if (!stockpile) continue;

        stockpile->quantities[fac.output] += fac.baseRate * gameHoursDt;
    }
}
