#include "ConsumptionSystem.h"
#include "ECS/Components.h"
#include <algorithm>
#include <string>

// Stockpile draw-down rates per NPC per game-hour.
static constexpr float FOOD_CONSUME_RATE  = 0.5f;
static constexpr float WATER_CONSUME_RATE = 0.8f;

// Wages paid to working NPCs (gold per game-hour, from settlement treasury).
static constexpr float WAGE_RATE = 0.3f;

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

        // ---- Wages: pay working NPCs from settlement treasury ----
        auto* settl = registry.try_get<Settlement>(home.settlement);
        auto* money = registry.try_get<Money>(entity);
        if (settl && money) {
            const auto* astate = registry.try_get<AgentState>(entity);
            if (astate && astate->behavior == AgentBehavior::Working) {
                float wage = WAGE_RATE * gameHoursDt;
                if (settl->treasury >= wage) {
                    settl->treasury -= wage;
                    money->balance  += wage;
                }
            }
        }

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

    // ---- Per-settlement stockpile alerts (log once on crossing thresholds) ----
    static constexpr float LOW_THRESHOLD   = 20.f;
    static constexpr float EMPTY_THRESHOLD =  1.f;

    auto logv = registry.view<EventLog>();
    EventLog* log = (logv.begin() == logv.end()) ? nullptr
                  : &logv.get<EventLog>(*logv.begin());

    auto timeView2 = registry.view<TimeManager>();
    int  alertDay  = 1; int alertHour = 0;
    if (timeView2.begin() != timeView2.end()) {
        const auto& tm2 = timeView2.get<TimeManager>(*timeView2.begin());
        alertDay = tm2.day; alertHour = (int)tm2.hourOfDay;
    }

    registry.view<Settlement, Stockpile, StockpileAlert>().each(
        [&](const Settlement& s, const Stockpile& sp, StockpileAlert& alert) {
        auto qty = [&](ResourceType t) -> float {
            auto it = sp.quantities.find(t);
            return it != sp.quantities.end() ? it->second : 0.f;
        };
        float food  = qty(ResourceType::Food);
        float water = qty(ResourceType::Water);

        auto checkAlert = [&](float val, bool& emptyFlag, bool& lowFlag,
                               const std::string& res) {
            if (val < EMPTY_THRESHOLD && !emptyFlag) {
                emptyFlag = true; lowFlag = true;
                if (log) log->Push(alertDay, alertHour,
                    res + " EMPTY at " + s.name);
            } else if (val >= EMPTY_THRESHOLD * 2.f) {
                emptyFlag = false;
            }
            if (val < LOW_THRESHOLD && val >= EMPTY_THRESHOLD && !lowFlag) {
                lowFlag = true;
                if (log) log->Push(alertDay, alertHour,
                    res + " low at " + s.name + " (" + std::to_string((int)val) + ")");
            } else if (val >= LOW_THRESHOLD * 1.5f) {
                lowFlag = false;
            }
        };

        checkAlert(food,  alert.foodEmpty,  alert.foodLow,  "Food");
        checkAlert(water, alert.waterEmpty, alert.waterLow, "Water");
    });
}
