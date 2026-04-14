#include "ConsumptionSystem.h"
#include "ECS/Components.h"
#include "World/WorldSchema.h"
#include <algorithm>
#include <map>
#include <string>

// Stockpile draw-down rates per NPC per game-hour.
static constexpr float FOOD_CONSUME_RATE  = 0.5f;
static constexpr float WATER_CONSUME_RATE = 0.8f;
// Wood consumed per NPC per game-hour as fuel (scaled by season heatDrainMod).
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

void ConsumptionSystem::Update(entt::registry& registry, float realDt, const WorldSchema& schema) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    const auto& tm = timeView.get<TimeManager>(*timeView.begin());

    float gameDt      = tm.GameDt(realDt);
    if (gameDt <= 0.f) return;

    // 1 game-hour = 60 game-minutes; GAME_MINS_PER_REAL_SEC scales gameDt to minutes.
    float gameHoursDt  = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;
    SeasonID csId = tm.CurrentSeason(schema.seasons);
    float heatDrainMult = (csId >= 0 && csId < (int)schema.seasons.size())
                          ? schema.seasons[csId].heatDrainMod : 0.f;

    // Per-settlement starvation tracking for food crisis warning
    std::map<entt::entity, int> starvingPerSettlement;

    auto view = registry.view<Needs, HomeSettlement, DeprivationTimer>();
    for (auto entity : view) {
        auto& needs  = view.get<Needs>(entity);
        auto& home   = view.get<HomeSettlement>(entity);
        auto& timer  = view.get<DeprivationTimer>(entity);

        if (home.settlement == entt::null || !registry.valid(home.settlement)) continue;
        if (needs.list.size() < 4) continue;   // guard: medieval config requires 4 needs
        auto* stockpile = registry.try_get<Stockpile>(home.settlement);
        if (!stockpile) continue;

        auto& foodStock  = stockpile->quantities[RES_FOOD];
        auto& waterStock = stockpile->quantities[RES_WATER];

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

        // Snapshot worst need before consumption for mood log
        float worstNeedBefore = 1.f;
        int   worstNeedIdx    = -1;
        for (int i = 0; i < (int)needs.list.size(); ++i) {
            if (needs.list[i].value < worstNeedBefore) {
                worstNeedBefore = needs.list[i].value;
                worstNeedIdx    = i;
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
            // Remember where this meal came from
            if (settl) timer.lastMealSource = settl->name;
        }

        // ---- Gratitude for last meal ----
        // When hunger drops below 0.2, NPC recalls the settlement that fed them.
        if (needs.list[0].value < 0.2f && !timer.lastMealSource.empty()) {
            auto lv2 = registry.view<EventLog>();
            auto tv2 = registry.view<TimeManager>();
            if (!lv2.empty() && !tv2.empty()) {
                const auto& tm2 = tv2.get<TimeManager>(*tv2.begin());
                std::string who = "An NPC";
                if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                char buf[160];
                std::snprintf(buf, sizeof(buf), "%s is grateful to %s for food.",
                              who.c_str(), timer.lastMealSource.c_str());
                lv2.get<EventLog>(*lv2.begin()).Push(tm2.day, (int)tm2.hourOfDay, buf);
            }
            timer.lastMealSource.clear();
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

        // ---- Mood log on need satisfaction ----
        // When the worst need rises from < 0.3 to > 0.5, log relief at 1-in-5 frequency.
        if (worstNeedIdx >= 0 && worstNeedBefore < 0.3f) {
            float afterVal = needs.list[worstNeedIdx].value;
            if (afterVal > 0.5f) {
                static int s_moodLogCounter = 0;
                if (++s_moodLogCounter % 5 == 1) {
                    auto logV = registry.view<EventLog>();
                    auto tmV  = registry.view<TimeManager>();
                    if (!logV.empty() && !tmV.empty()) {
                        const auto& tm2 = tmV.get<TimeManager>(*tmV.begin());
                        std::string who = "An NPC";
                        if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                        std::string stlName = "a settlement";
                        if (settl) stlName = settl->name;
                        static const char* verbs[] = { "eating", "drinking", "resting", "warming up" };
                        const char* verb = (worstNeedIdx >= 0 && worstNeedIdx < (int)(sizeof(verbs)/sizeof(verbs[0])))
                                           ? verbs[worstNeedIdx] : "recovering";
                        char buf[160];
                        std::snprintf(buf, sizeof(buf), "%s feels relieved after %s at %s.",
                                      who.c_str(), verb, stlName.c_str());
                        logV.get<EventLog>(*logV.begin()).Push(tm2.day, (int)tm2.hourOfDay, buf);
                    }
                }
            }
        }

        // ---- Update satisfaction memory ----
        {
            float avg = 0.f;
            for (int i = 0; i < (int)needs.list.size(); ++i) avg += needs.list[i].value;
            timer.lastSatisfaction = needs.list.empty() ? 0.5f : avg / (float)needs.list.size();
        }

        // ---- Starvation desperation log ----
        // Fires when hunger < 0.1, no money, and no food in stockpile (purchase impossible).
        if (needs.list[0].value < 0.1f && (!money || money->balance < 1.f) && !hadFood) {
            static int s_starvationCounter = 0;
            if (++s_starvationCounter % 10 == 1) {
                auto logV = registry.view<EventLog>();
                auto tmV  = registry.view<TimeManager>();
                if (!logV.empty() && !tmV.empty()) {
                    const auto& tm2 = tmV.get<TimeManager>(*tmV.begin());
                    std::string who = "An NPC";
                    if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                    std::string stlName = "a settlement";
                    if (settl) stlName = settl->name;
                    char buf[160];
                    std::snprintf(buf, sizeof(buf), "%s is starving and desperate at %s.",
                                  who.c_str(), stlName.c_str());
                    logV.get<EventLog>(*logV.begin()).Push(tm2.day, (int)tm2.hourOfDay, buf);
                }
            }
        }

        // ---- Starvation begging from friends ----
        // When starving (hunger < 0.1) and broke, beg from a friend at the same settlement.
        if (timer.begTimer > 0.f) timer.begTimer -= gameHoursDt;
        if (needs.list[0].value < 0.1f && (!money || money->balance < 1.f)
            && timer.begTimer <= 0.f) {
            const auto* rel = registry.try_get<Relations>(entity);
            if (rel) {
                entt::entity bestHelper = entt::null;
                float bestAff = 0.4f - 0.01f;
                for (const auto& [fe, aff] : rel->affinity) {
                    if (aff < 0.4f) continue;
                    if (!registry.valid(fe)) continue;
                    const auto* fh = registry.try_get<HomeSettlement>(fe);
                    if (!fh || fh->settlement != home.settlement) continue;
                    const auto* fm = registry.try_get<Money>(fe);
                    if (!fm || fm->balance <= 10.f) continue;
                    if (aff > bestAff) { bestAff = aff; bestHelper = fe; }
                }
                if (bestHelper != entt::null) {
                    auto* helperMoney = registry.try_get<Money>(bestHelper);
                    if (helperMoney) {
                        helperMoney->balance -= 3.f;
                        if (money) money->balance += 3.f;
                        else {
                            auto& m = registry.get_or_emplace<Money>(entity);
                            m.balance += 3.f;
                        }
                        timer.begTimer = 24.f;  // cooldown: once per 24 game-hours
                        auto logV = registry.view<EventLog>();
                        auto tmV  = registry.view<TimeManager>();
                        if (!logV.empty() && !tmV.empty()) {
                            const auto& tm2 = tmV.get<TimeManager>(*tmV.begin());
                            std::string helperName = "A friend";
                            if (const auto* n = registry.try_get<Name>(bestHelper))
                                helperName = n->value;
                            std::string npcName = "an NPC";
                            if (const auto* n = registry.try_get<Name>(entity))
                                npcName = n->value;
                            logV.get<EventLog>(*logV.begin()).Push(tm2.day, (int)tm2.hourOfDay,
                                helperName + " helps starving " + npcName + " with gold.");
                        }
                    }
                }
            }
        }

        // Track starving NPCs per settlement for food crisis warning
        if (needs.list[0].value < 0.15f)
            starvingPerSettlement[home.settlement]++;

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
                    float price = mkt->GetPrice(RES_FOOD);
                    if (money->balance >= price) {
                        money->balance -= price;
                        settl->treasury += price;
                        stockpile->quantities[RES_FOOD] += 1.f;
                        timer.purchaseTimer = 0.f;
                        bought = true; whatBought = "food"; pricePaid = price;
                    }
                }
                // Buy 1 unit of water if empty (separate check)
                if (!hadWater && timer.purchaseTimer >= effectivePurchaseInterval) {
                    float price = mkt->GetPrice(RES_WATER);
                    if (money->balance >= price) {
                        money->balance -= price;
                        settl->treasury += price;
                        stockpile->quantities[RES_WATER] += 1.f;
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
        bool justStole = false;
        if (canSteal) {
            // Steal food if close to dying of hunger
            if (timer.needsAtZero[0] >= STEAL_DESPERATION && foodStock >= STEAL_AMOUNT) {
                foodStock -= STEAL_AMOUNT;
                // Don't refill need — they'll pick it up as consumption next tick
                timer.stealCooldown = STEAL_COOLDOWN;
                timer.fleeTimer     = 4.f;   // sprint away for ~4 real seconds
                justStole = true;

                // Theft costs reputation (-0.5 per act; creates component if missing)
                registry.get_or_emplace<Reputation>(entity).score -= 0.5f;
                if (settl) settl->theftCount++;
                // Social ostracism: theft erodes skills slightly
                std::string skillSuffix;
                if (auto* sk = registry.try_get<Skills>(entity)) {
                    sk->farming       = std::max(0.f, sk->farming       - 0.02f);
                    sk->water_drawing = std::max(0.f, sk->water_drawing - 0.02f);
                    sk->woodcutting   = std::max(0.f, sk->woodcutting   - 0.02f);
                    char sb[32];
                    std::snprintf(sb, sizeof(sb), " (farming %d%%)", (int)(sk->farming * 100));
                    skillSuffix = sb;
                }
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
                        who + " stole food at " + where + skillSuffix);
                }
            }
            // Steal water if close to dying of thirst
            else if (timer.needsAtZero[1] >= STEAL_DESPERATION && waterStock >= STEAL_AMOUNT) {
                waterStock -= STEAL_AMOUNT;
                timer.stealCooldown = STEAL_COOLDOWN;
                timer.fleeTimer     = 4.f;
                justStole = true;
                registry.get_or_emplace<Reputation>(entity).score -= 0.5f;
                if (settl) settl->theftCount++;
                std::string skillSuffix2;
                if (auto* sk = registry.try_get<Skills>(entity)) {
                    sk->farming       = std::max(0.f, sk->farming       - 0.02f);
                    sk->water_drawing = std::max(0.f, sk->water_drawing - 0.02f);
                    sk->woodcutting   = std::max(0.f, sk->woodcutting   - 0.02f);
                    char sb[32];
                    std::snprintf(sb, sizeof(sb), " (water %d%%)", (int)(sk->water_drawing * 100));
                    skillSuffix2 = sb;
                }
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
                        who + " stole water at " + where + skillSuffix2);
                }
            }
        }

        // ---- Grudge: nearby grateful NPCs see through the thief ----
        if (justStole) {
            const auto* thiefPos = registry.try_get<Position>(entity);
            std::string thiefName = "Someone";
            if (auto* n = registry.try_get<Name>(entity)) thiefName = n->value;
            if (thiefPos) {
                auto dtView = registry.view<DeprivationTimer, Position>();
                for (auto other : dtView) {
                    if (other == entity) continue;
                    auto& otherTimer = dtView.get<DeprivationTimer>(other);
                    if (otherTimer.gratitudeTarget != entity || otherTimer.gratitudeTimer <= 0.f)
                        continue;
                    const auto& otherPos = dtView.get<Position>(other);
                    float dx = thiefPos->x - otherPos.x;
                    float dy = thiefPos->y - otherPos.y;
                    if (dx*dx + dy*dy > 80.f * 80.f) continue;
                    // Clear gratitude — betrayed trust
                    otherTimer.gratitudeTimer  = 0.f;
                    otherTimer.gratitudeTarget = entt::null;
                    // Log
                    auto lv4 = registry.view<EventLog>();
                    auto tv4 = registry.view<TimeManager>();
                    if (lv4.begin() != lv4.end() && tv4.begin() != tv4.end()) {
                        const auto& tm4 = tv4.get<TimeManager>(*tv4.begin());
                        std::string witness = "An NPC";
                        if (auto* wn = registry.try_get<Name>(other)) witness = wn->value;
                        lv4.get<EventLog>(*lv4.begin()).Push(
                            tm4.day, (int)tm4.hourOfDay,
                            witness + " saw through " + thiefName + "'s gratitude.");
                    }
                }
            }
        }

        // ---- Wood / Heat ----
        // If wood is available and the season demands heating, burn wood and keep NPCs warm.
        // If no wood (or summer — no demand), NeedDrainSystem naturally drains Heat.
        bool hadWood = false;
        if (heatDrainMult > 0.f) {
            auto& woodStock = stockpile->quantities[RES_WOOD];
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

    // ---- Settlement food crisis warning ----
    // Log once per game-day when ≥ 3 NPCs are starving at one settlement.
    {
        static std::map<entt::entity, int> s_crisisLogDay;
        for (const auto& [settlEnt, count] : starvingPerSettlement) {
            if (count < 3) continue;
            if (s_crisisLogDay[settlEnt] == tm.day) continue;  // already logged today
            s_crisisLogDay[settlEnt] = tm.day;
            auto logV = registry.view<EventLog>();
            if (!logV.empty()) {
                std::string sName = "A settlement";
                if (const auto* s = registry.try_get<Settlement>(settlEnt))
                    sName = s->name;
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                    "%s faces a food crisis \xe2\x80\x94 %d residents starving.",
                    sName.c_str(), count);
                logV.get<EventLog>(*logV.begin()).Push(tm.day, (int)tm.hourOfDay, buf);
            }
        }
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
        auto qty = [&](int t) -> float {
            auto it = sp.quantities.find(t);
            return it != sp.quantities.end() ? it->second : 0.f;
        };
        float food  = qty(RES_FOOD);
        float water = qty(RES_WATER);

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

        float wood  = qty(RES_WOOD);
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
