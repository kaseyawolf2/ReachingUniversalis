#include "ConsumptionSystem.h"
#include "ECS/Components.h"
#include <algorithm>
#include <map>
#include <string>

// Stockpile draw-down rates per NPC per game-hour.
static constexpr float FOOD_CONSUME_RATE  = 0.5f;
static constexpr float WATER_CONSUME_RATE = 0.8f;
// Wood consumed per NPC per game-hour as fuel (Winter rate; scaled by SeasonHeatDrainMult).
static constexpr float WOOD_HEAT_RATE     = 0.03f;

// Base wage paid to working NPCs (gold per game-hour, from settlement treasury).
// Actual wage scales with the NPC's relevant skill:
//   wage = WAGE_RATE * (0.5 + skill)  → range [0.15, 0.45] g/hr
// This means a master craftsperson earns 3× a complete beginner, rewarding specialisation.
static constexpr float WAGE_RATE = 0.3f;

// Threshold below which stockpile is considered "empty" for migration purposes.
static constexpr float STOCK_LOW = 0.01f;

// How often an NPC can make an emergency market purchase when stockpile is empty (game-hours).
// They pay market price, money goes to settlement treasury.
static constexpr float PURCHASE_INTERVAL = 2.f;

static std::map<entt::entity, float> s_desperateCooldown;

void ConsumptionSystem::Update(entt::registry& registry, float realDt) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    const auto& tm = timeView.get<TimeManager>(*timeView.begin());

    float gameDt      = tm.GameDt(realDt);
    if (gameDt <= 0.f) return;

    // 1 game-hour = 60 game-minutes; GAME_MINS_PER_REAL_SEC scales gameDt to minutes.
    float gameHoursDt  = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;
    float heatDrainMult = SeasonHeatDrainMult(tm.CurrentSeason());

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
        // settl and money are used again below for market purchases, so declared here.
        auto* settl = registry.try_get<Settlement>(home.settlement);
        auto* money = registry.try_get<Money>(entity);
        {
            const auto* astate  = registry.try_get<AgentState>(entity);
            const auto* ageComp = registry.try_get<Age>(entity);
            bool isChild = ageComp && ageComp->days < 15.f;
            bool isElder = ageComp && ageComp->days > 60.f;
            if (settl && money && astate && !isChild && !isElder && astate->behavior == AgentBehavior::Working) {
                // Scale wage by the NPC's best skill — specialised workers earn more.
                // Determine which facility type the NPC is working at to pick the right skill.
                float skillMult = 1.0f;
                if (const auto* sk = registry.try_get<Skills>(entity)) {
                    // Use the highest skill as the wage modifier regardless of exact facility.
                    // This rewards NPCs that have developed their aptitude.
                    float bestSkill = std::max({sk->farming, sk->water_drawing, sk->woodcutting});
                    skillMult = 0.5f + bestSkill;  // range [0.5, 1.5]
                }
                float wage = WAGE_RATE * skillMult * gameHoursDt;
                if (settl->treasury >= wage) {
                    settl->treasury -= wage;
                    money->balance  += wage;
                }
            }
            // Elders draw subsistence from their own savings (no treasury cost).
            // Rate: 0.1 g/game-hour — covers basic needs out of accumulated wealth.
            static constexpr float ELDER_SUBSISTENCE_RATE = 0.1f;
            if (money && isElder) {
                float cost = ELDER_SUBSISTENCE_RATE * gameHoursDt;
                money->balance = std::max(0.f, money->balance - cost);
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

        // Drain desperation log cooldown
        if (auto cdIt = s_desperateCooldown.find(entity); cdIt != s_desperateCooldown.end()) {
            cdIt->second -= gameHoursDt;
            if (cdIt->second <= 0.f) s_desperateCooldown.erase(cdIt);
        }

        // ---- Emergency market purchase ----
        // When stockpile is empty, an NPC with money can buy goods at market price.
        // Gold flows to the settlement treasury; need is refilled.
        // SaveGold goal: NPCs hoarding gold buy less frequently (4h interval instead of 2h).
        timer.purchaseTimer += gameHoursDt;
        float effectivePurchaseInterval = PURCHASE_INTERVAL;
        if (const auto* g = registry.try_get<Goal>(entity))
            if (g->type == GoalType::SaveGold)
                effectivePurchaseInterval *= 2.f;   // hoarders delay emergency purchases
        if (timer.purchaseTimer >= effectivePurchaseInterval && settl && money && money->balance > 0.f) {
            auto* mkt = registry.try_get<Market>(home.settlement);
            if (mkt) {
                bool bought = false;
                const char* whatBought = nullptr;
                float pricePaid = 0.f;
                // Buy 1 unit of food if empty
                if (!hadFood) {
                    float price = mkt->GetPrice(ResourceType::Food);
                    if (money->balance >= price) {
                        money->balance -= price;
                        settl->treasury += price;
                        stockpile->quantities[ResourceType::Food] += 1.f;
                        timer.purchaseTimer = 0.f;
                        bought = true; whatBought = "food"; pricePaid = price;
                    }
                }
                // Buy 1 unit of water if empty (separate check)
                if (!hadWater && timer.purchaseTimer >= effectivePurchaseInterval) {
                    float price = mkt->GetPrice(ResourceType::Water);
                    if (money->balance >= price) {
                        money->balance -= price;
                        settl->treasury += price;
                        stockpile->quantities[ResourceType::Water] += 1.f;
                        timer.purchaseTimer = 0.f;
                        bought = true; whatBought = "water"; pricePaid = price;
                    }
                }
                // Track desperation purchase count on settlement
                if (bought) {
                    settl->desperatePurchases++;
                }
                // Log desperation purchase (rate-limited per NPC to once per 12 game-hours)
                if (bought) {
                    float& cd = s_desperateCooldown[entity];
                    if (cd <= 0.f) {
                        auto logV = registry.view<EventLog>();
                        if (logV.begin() != logV.end()) {
                            auto& evLog = logV.get<EventLog>(*logV.begin());
                            std::string who = "NPC";
                            if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                            char buf[128];
                            std::snprintf(buf, sizeof(buf), "%s desperate — bought %s at %s market for %.1fg",
                                          who.c_str(), whatBought, settl->name.c_str(), pricePaid);
                            auto tmV = registry.view<TimeManager>();
                            if (tmV.begin() != tmV.end()) {
                                const auto& tmRef = tmV.get<TimeManager>(*tmV.begin());
                                evLog.Push(tmRef.day, (int)tmRef.hourOfDay, buf);
                            }
                        }
                        cd = 12.f;
                    }
                }
            }
        }

        // ---- Desperate theft: steal from stockpile when close to death ----
        // An NPC within 6 hours of dying of hunger or thirst, with no money and
        // no goods in the stockpile, will steal a small amount directly.
        // Theft is rare (cooldown 4 game-hours) and only when truly desperate.
        static constexpr float STEAL_DESPERATION = 6.f * 60.f;   // 6 game-hours of needsAtZero
        static constexpr float STEAL_AMOUNT      = 2.f;           // units stolen
        static constexpr float STEAL_COOLDOWN    = 4.f;           // game-hours between thefts
        timer.stealCooldown = std::max(0.f, timer.stealCooldown - gameHoursDt);

        bool canSteal = (timer.stealCooldown <= 0.f)
                     && (!money || money->balance < 1.f);   // only if broke
        if (canSteal) {
            // Steal food if close to dying of hunger
            if (timer.needsAtZero[0] >= STEAL_DESPERATION && foodStock >= STEAL_AMOUNT) {
                foodStock -= STEAL_AMOUNT;
                // Don't refill need — they'll pick it up as consumption next tick
                timer.stealCooldown = STEAL_COOLDOWN;
                timer.fleeTimer     = 4.f;   // sprint away for ~4 real seconds

                // Log it
                auto lv3 = registry.view<EventLog>();
                auto tv3 = registry.view<TimeManager>();
                if (lv3.begin() != lv3.end() && tv3.begin() != tv3.end()) {
                    const auto& tm3 = tv3.get<TimeManager>(*tv3.begin());
                    std::string who = "An NPC";
                    if (auto* n = registry.try_get<Name>(entity)) who = n->value;
                    std::string where = "?";
                    if (settl) where = settl->name;
                    lv3.get<EventLog>(*lv3.begin()).Push(
                        tm3.day, (int)tm3.hourOfDay,
                        who + " stole food at " + where + " (desperate)");
                }
                // Theft costs reputation
                if (auto* rep = registry.try_get<Reputation>(entity))
                    rep->score -= 0.2f;
                // Social ostracism: theft erodes skills slightly
                if (auto* sk = registry.try_get<Skills>(entity)) {
                    sk->farming       = std::max(0.f, sk->farming       - 0.02f);
                    sk->water_drawing = std::max(0.f, sk->water_drawing - 0.02f);
                    sk->woodcutting   = std::max(0.f, sk->woodcutting   - 0.02f);
                }
            }
            // Steal water if close to dying of thirst
            else if (timer.needsAtZero[1] >= STEAL_DESPERATION && waterStock >= STEAL_AMOUNT) {
                waterStock -= STEAL_AMOUNT;
                timer.stealCooldown = STEAL_COOLDOWN;
                timer.fleeTimer     = 4.f;
                auto lv3 = registry.view<EventLog>();
                auto tv3 = registry.view<TimeManager>();
                if (lv3.begin() != lv3.end() && tv3.begin() != tv3.end()) {
                    const auto& tm3 = tv3.get<TimeManager>(*tv3.begin());
                    std::string who = "An NPC";
                    if (auto* n = registry.try_get<Name>(entity)) who = n->value;
                    std::string where = "?";
                    if (settl) where = settl->name;
                    lv3.get<EventLog>(*lv3.begin()).Push(
                        tm3.day, (int)tm3.hourOfDay,
                        who + " stole water at " + where + " (desperate)");
                }
                if (auto* rep = registry.try_get<Reputation>(entity))
                    rep->score -= 0.2f;
                if (auto* sk = registry.try_get<Skills>(entity)) {
                    sk->farming       = std::max(0.f, sk->farming       - 0.02f);
                    sk->water_drawing = std::max(0.f, sk->water_drawing - 0.02f);
                    sk->woodcutting   = std::max(0.f, sk->woodcutting   - 0.02f);
                }
            }
        }

        // ---- Wood / Heat ----
        // If wood is available and the season demands heating, burn wood and keep NPCs warm.
        // If no wood (or summer — no demand), NeedDrainSystem naturally drains Heat.
        bool hadWood = false;
        if (heatDrainMult > 0.f) {
            auto& woodStock = stockpile->quantities[ResourceType::Wood];
            hadWood = (woodStock > STOCK_LOW);
            if (hadWood) {
                float draw = WOOD_HEAT_RATE * heatDrainMult * gameHoursDt;
                draw = std::min(draw, woodStock);
                woodStock -= draw;
                // Cancel out the Heat drain for this tick
                needs.list[3].value += needs.list[3].drainRate * heatDrainMult * gameDt;
                needs.list[3].value  = std::min(needs.list[3].value, 1.f);
            }
        } else {
            // Summer — no cold, gradually restore heat to full
            needs.list[3].value = std::min(1.f, needs.list[3].value + needs.list[3].refillRate * gameDt);
        }

        // ---- Stockpile empty timer (drives migration in AgentDecisionSystem) ----
        // Also include winter heat deprivation in the "deprived" check.
        bool heatDeprived = (heatDrainMult > 0.f) && !hadWood;
        bool deprived = (!hadFood || !hadWater || heatDeprived);
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

        float wood  = qty(ResourceType::Wood);
        checkAlert(food,  alert.foodEmpty,  alert.foodLow,  "Food");
        checkAlert(water, alert.waterEmpty, alert.waterLow, "Water");
        checkAlert(wood,  alert.woodEmpty,  alert.woodLow,  "Wood");

        // Treasury alerts (wages stop when depleted)
        static constexpr float LOW_TREASURY = 50.f;
        float treasury = s.treasury;
        if (treasury < 1.f && !alert.treasuryEmpty) {
            alert.treasuryEmpty = true;
            alert.treasuryLow   = true;
            if (log) log->Push(alertDay, alertHour,
                "Treasury EMPTY at " + s.name + " — wages halted");
        } else if (treasury >= 10.f) {
            alert.treasuryEmpty = false;
        }
        if (treasury < LOW_TREASURY && treasury >= 1.f && !alert.treasuryLow) {
            alert.treasuryLow = true;
            if (log) log->Push(alertDay, alertHour,
                "Treasury low at " + s.name + " (" + std::to_string((int)treasury) + "g)");
        } else if (treasury >= LOW_TREASURY * 1.5f) {
            alert.treasuryLow = false;
        }
    });
}
