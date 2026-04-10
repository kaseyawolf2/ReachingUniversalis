#include "ConsumptionSystem.h"
#include "ECS/Components.h"
#include <algorithm>

// Stockpile draw-down rates per NPC per game-hour.
static constexpr float FOOD_CONSUME_RATE  = 0.5f;
static constexpr float WATER_CONSUME_RATE = 0.8f;

// Threshold below which stockpile is considered "empty" for migration purposes.
static constexpr float STOCK_LOW = 0.01f;

void ConsumptionSystem::Update(entt::registry& registry, float realDt) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    const auto& tm = timeView.get<TimeManager>(*timeView.begin());

    float gameDt      = tm.GameDt(realDt);
    if (gameDt <= 0.f) return;

    // 1 game-hour = 60 game-minutes; GAME_MINS_PER_REAL_SEC scales gameDt to minutes.
    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;

    auto view = registry.view<Needs, HomeSettlement, DeprivationTimer>();
    for (auto entity : view) {
        auto& needs  = view.get<Needs>(entity);
        auto& home   = view.get<HomeSettlement>(entity);
        auto& timer  = view.get<DeprivationTimer>(entity);

        if (home.settlement == entt::null || !registry.valid(home.settlement)) continue;
        auto* stockpile = registry.try_get<Stockpile>(home.settlement);
        if (!stockpile) continue;

        auto& foodStock  = stockpile->quantities[ResourceType::Food];
        auto& waterStock = stockpile->quantities[ResourceType::Water];

        // ---- Food / Hunger ----
        bool hadFood = (foodStock > STOCK_LOW);
        if (hadFood) {
            float draw = FOOD_CONSUME_RATE * gameHoursDt;
            draw = std::min(draw, foodStock);
            foodStock -= draw;
            // Refill hunger exactly enough to cancel NeedDrainSystem's drain.
            needs.list[0].value += needs.list[0].drainRate * gameDt;
            needs.list[0].value  = std::min(needs.list[0].value, 1.f);
        }

        // ---- Water / Thirst ----
        bool hadWater = (waterStock > STOCK_LOW);
        if (hadWater) {
            float draw = WATER_CONSUME_RATE * gameHoursDt;
            draw = std::min(draw, waterStock);
            waterStock -= draw;
            needs.list[1].value += needs.list[1].drainRate * gameDt;
            needs.list[1].value  = std::min(needs.list[1].value, 1.f);
        }

        // ---- Stockpile empty timer (drives migration in AgentDecisionSystem) ----
        bool deprived = (!hadFood || !hadWater);
        if (deprived)
            timer.stockpileEmpty += gameDt;
        else
            timer.stockpileEmpty = std::max(0.f, timer.stockpileEmpty - gameDt * 0.5f);
    }
}
