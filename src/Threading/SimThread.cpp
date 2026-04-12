#include "SimThread.h"
#include "ECS/Components.h"
#include "World/WorldGenerator.h"
#include <cmath>
#include <chrono>
#include <algorithm>
#include <random>

// Fixed simulation timestep — every sim step advances game time by this much
// regardless of render frame rate or tick speed multiplier.
static constexpr float SIM_STEP_DT   = 1.f / 60.f;
// Max virtual frames processed per real frame (prevents spiral-of-death).
static constexpr int   MAX_CATCHUP   = 8;

SimThread::SimThread(InputSnapshot& input, RenderSnapshot& snapshot)
    : m_input(input), m_snapshot(snapshot)
{
    WorldGenerator::Populate(m_registry);
}

SimThread::~SimThread() {
    Stop();
}

void SimThread::PushTradeRecord(const std::string& desc, float profit) {
    m_tradeLedger.insert(m_tradeLedger.begin(), { desc, profit });
    if ((int)m_tradeLedger.size() > 6) m_tradeLedger.resize(6);
}

void SimThread::Start() {
    m_running = true;
    m_thread  = std::thread(&SimThread::Run, this);
}

void SimThread::Stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}

void SimThread::NotifyWorldClick(float worldX, float worldY) {
    std::lock_guard<std::mutex> lock(m_clickMutex);
    m_pendingClick = true;
    m_clickX = worldX;
    m_clickY = worldY;
}

// ---- Main loop --------------------------------------------------------

void SimThread::Run() {
    using clock    = std::chrono::steady_clock;
    using duration = std::chrono::duration<float>;

    auto  prevTime    = clock::now();
    float accumulator = 0.f;

    while (m_running) {
        auto  now    = clock::now();
        float realDt = duration(now - prevTime).count();
        prevTime     = now;

        // Clamp dt to avoid a huge catchup burst after a hitch
        if (realDt > 0.1f) realDt = 0.1f;
        accumulator += realDt;

        // --- Input (once per real frame) ---
        ProcessInput();

        // --- How many virtual 60 Hz frames have elapsed? ---
        int virtualFrames = 0;
        while (accumulator >= SIM_STEP_DT && virtualFrames < MAX_CATCHUP) {
            accumulator  -= SIM_STEP_DT;
            virtualFrames++;
        }

        // --- Run tickSpeed sim steps per virtual frame ---
        int tickSpeed = 1;
        bool paused   = false;
        {
            auto tmv = m_registry.view<TimeManager>();
            if (tmv.begin() != tmv.end()) {
                const auto& tm = tmv.get<TimeManager>(*tmv.begin());
                tickSpeed = tm.tickSpeed;
                paused    = tm.paused;
            }
        }

        if (!paused) {
            int totalSteps = virtualFrames * tickSpeed;
            for (int i = 0; i < totalSteps; ++i)
                RunSimStep(SIM_STEP_DT);
            m_stepCounter += totalSteps;

            // Auto-respawn if player has died
            auto pv = m_registry.view<PlayerTag>();
            if (pv.empty()) RespawnPlayer();
        }

        // Track steps per second using wall clock
        m_statAccum += realDt;
        if (m_statAccum >= 1.f) {
            m_stepsLastSec = m_stepCounter;
            m_stepCounter  = 0;
            m_statAccum   -= 1.f;
        }

        // --- Write drawable state for the render thread ---
        WriteSnapshot();

        // Yield so we don't pin the core when running faster than real time
        std::this_thread::yield();
    }
}

// ---- Respawn player if dead -------------------------------------------

void SimThread::RespawnPlayer() {
    // Find the settlement with the most combined food + water (healthiest destination)
    entt::entity bestSettl = entt::null;
    float        bestStock = -1.f;
    m_registry.view<Position, Settlement, Stockpile>().each(
        [&](auto e, const Position&, const Settlement&, const Stockpile& sp) {
        float food  = sp.quantities.count(ResourceType::Food)
                      ? sp.quantities.at(ResourceType::Food)  : 0.f;
        float water = sp.quantities.count(ResourceType::Water)
                      ? sp.quantities.at(ResourceType::Water) : 0.f;
        if (food + water > bestStock) { bestStock = food + water; bestSettl = e; }
    });
    if (bestSettl == entt::null) return;

    const auto& sp = m_registry.get<Position>(bestSettl);

    // Get game time for log entry
    auto tmv = m_registry.view<TimeManager>();
    if (tmv.begin() != tmv.end()) {
        const auto& tm = tmv.get<TimeManager>(*tmv.begin());
        auto lv = m_registry.view<EventLog>();
        if (lv.begin() != lv.end()) {
            const auto& sett = m_registry.get<Settlement>(bestSettl);
            lv.get<EventLog>(*lv.begin()).Push(tm.day, (int)tm.hourOfDay,
                "Player respawned at " + sett.name);
        }
    }

    auto player = m_registry.create();
    m_registry.emplace<Position>(player, sp.x, sp.y + 50.f);
    m_registry.emplace<Velocity>(player, 0.f, 0.f);
    m_registry.emplace<MoveSpeed>(player, 100.f);
    // Full needs at spawn
    m_registry.emplace<Needs>(player, Needs{{
        Need{ NeedType::Hunger, 1.f, 0.00083f, 0.3f, 0.004f },
        Need{ NeedType::Thirst, 1.f, 0.00125f, 0.3f, 0.006f },
        Need{ NeedType::Energy, 1.f, 0.00050f, 0.3f, 0.002f },
        Need{ NeedType::Heat,   1.f, 0.00200f, 0.3f, 0.010f }
    }});
    m_registry.emplace<AgentState>(player);
    m_registry.emplace<HomeSettlement>(player, HomeSettlement{ bestSettl });
    m_registry.emplace<DeprivationTimer>(player);
    m_registry.emplace<Inventory>(player, Inventory{ {}, 15 });   // 15-unit carry capacity
    m_registry.emplace<Money>(player, Money{ 10.f });   // small purse on respawn
    m_registry.emplace<Renderable>(player, YELLOW, 10.f);
    m_registry.emplace<PlayerTag>(player);
    // New character starts at age 0 with fresh life expectancy
    static std::mt19937 rng{std::random_device{}()};
    static std::uniform_real_distribution<float> lifespan(60.f, 100.f);
    Age age;
    age.days    = 0.f;
    age.maxDays = lifespan(rng);
    m_registry.emplace<Age>(player, age);
    m_registry.emplace<Name>(player, Name{ "You" });
    // New character starts with average skills (same as initial spawn in WorldGenerator)
    m_registry.emplace<Skills>(player, Skills{ 0.4f, 0.4f, 0.4f });
}

// ---- One simulation step ----------------------------------------------

void SimThread::RunSimStep(float dt) {
    m_timeSystem.Advance(m_registry, dt);
    m_needDrainSystem.Update(m_registry, dt);
    m_consumptionSystem.Update(m_registry, dt);
    m_scheduleSystem.Update(m_registry, dt);
    m_agentDecisionSystem.Update(m_registry, dt);
    m_movementSystem.Update(m_registry, dt);
    m_productionSystem.Update(m_registry, dt);
    m_transportSystem.Update(m_registry, dt);
    m_priceSystem.Update(m_registry, dt);         // adjust prices after stockpile changes
    m_randomEventSystem.Update(m_registry, dt);       // fire events and tick active timers
    m_economicMobilitySystem.Update(m_registry, dt);  // hauler graduation / bankruptcy
    m_constructionSystem.Update(m_registry, dt);       // settlement facility expansion
    m_deathSystem.Update(m_registry, dt);
    m_birthSystem.Update(m_registry, dt);

    // ---- Player work: skill advancement + auto-cancel if player moves ----
    // The player's Working state is set via E key (ProcessInput).
    // While Working, advance the relevant skill for their target facility.
    // If the player provides movement input, cancel Working (they walked away).
    {
        auto pvw = m_registry.view<PlayerTag, AgentState, Velocity, Position>();
        if (pvw.begin() != pvw.end()) {
            auto  pe  = *pvw.begin();
            auto& pst = pvw.get<AgentState>(pe);
            auto& pv2 = pvw.get<Velocity>(pe);

            if (pst.behavior == AgentBehavior::Working) {
                // Cancel Working if the player is moving
                if (std::abs(pv2.vx) > 0.5f || std::abs(pv2.vy) > 0.5f) {
                    pst.behavior = AgentBehavior::Idle;
                    pst.target   = entt::null;
                } else if (pst.target != entt::null && m_registry.valid(pst.target)) {
                    // Advance skill for the facility type
                    if (const auto* fac = m_registry.try_get<ProductionFacility>(pst.target)) {
                        if (auto* skills = m_registry.try_get<Skills>(pe)) {
                            auto tmv2 = m_registry.view<TimeManager>();
                            if (tmv2.begin() != tmv2.end()) {
                                float gDt = tmv2.get<TimeManager>(*tmv2.begin()).GameDt(dt);
                                float gameHoursDt = gDt * GAME_MINS_PER_REAL_SEC / 60.f;
                                static constexpr float SKILL_GAIN = 0.1f / 24.f;
                                skills->Advance(fac->output, SKILL_GAIN * gameHoursDt);
                            }
                        }
                    }
                }
            }
        }
    }
}

// ---- Consume pending input --------------------------------------------

void SimThread::ProcessInput() {
    auto tmv = m_registry.view<TimeManager>();
    if (tmv.begin() == tmv.end()) return;
    auto& tm = tmv.get<TimeManager>(*tmv.begin());

    // One-shot: pause
    if (m_input.pauseToggle.exchange(false))
        tm.paused = !tm.paused;

    // One-shot: speed
    static constexpr int SPEEDS[]    = {1, 2, 4, 8, 16, 32, 64, 128};
    static constexpr int NUM_SPEEDS  = sizeof(SPEEDS) / sizeof(SPEEDS[0]);
    if (m_input.speedUp.exchange(false)) {
        for (int i = 0; i < NUM_SPEEDS - 1; ++i)
            if (tm.tickSpeed == SPEEDS[i]) { tm.tickSpeed = SPEEDS[i+1]; break; }
    }
    if (m_input.speedDown.exchange(false)) {
        for (int i = NUM_SPEEDS - 1; i > 0; --i)
            if (tm.tickSpeed == SPEEDS[i]) { tm.tickSpeed = SPEEDS[i-1]; break; }
    }

    // One-shot: player trade (T key) — buy most profitable good or sell loaded goods
    if (m_input.playerTrade.exchange(false)) {
        static constexpr float TRADE_RADIUS = 140.f;
        auto tmv2 = m_registry.view<TimeManager>();
        auto pv2  = m_registry.view<PlayerTag, Position, Inventory, Money>();
        auto lv2  = m_registry.view<EventLog>();
        EventLog* log2 = (lv2.begin() == lv2.end()) ? nullptr
                       : &lv2.get<EventLog>(*lv2.begin());

        if (pv2.begin() != pv2.end()) {
            auto pe2   = *pv2.begin();
            auto& ppos = pv2.get<Position>(pe2);
            auto& inv  = pv2.get<Inventory>(pe2);
            auto& mon  = pv2.get<Money>(pe2);

            int day2 = 1, hr2 = 0;
            if (tmv2.begin() != tmv2.end()) {
                day2 = tmv2.get<TimeManager>(*tmv2.begin()).day;
                hr2  = (int)tmv2.get<TimeManager>(*tmv2.begin()).hourOfDay;
            }

            // Find nearest settlement within range
            entt::entity nearSettl = entt::null;
            float        nearDist  = TRADE_RADIUS * TRADE_RADIUS;
            m_registry.view<Position, Settlement>().each(
                [&](auto e, const Position& sp, const Settlement&) {
                float dx = sp.x - ppos.x, dy = sp.y - ppos.y;
                float d  = dx*dx + dy*dy;
                if (d < nearDist) { nearDist = d; nearSettl = e; }
            });

            if (nearSettl == entt::null) {
                if (log2) log2->Push(day2, hr2, "No settlement in range to trade");
            } else {
                auto& settl = m_registry.get<Settlement>(nearSettl);
                auto* sp2   = m_registry.try_get<Stockpile>(nearSettl);
                auto* mkt2  = m_registry.try_get<Market>(nearSettl);

                // If carrying goods → sell them here (same 20% trade tax as haulers).
                // Reputation reduces the tax: at Legend (200+) up to 10% less tax (= 10% effective).
                if (inv.TotalItems() > 0 && sp2 && mkt2) {
                    static constexpr float TRADE_TAX_BASE = 0.20f;
                    float repDiscount = std::min(0.10f, m_playerReputation * 0.0005f);  // 0.05%/rep up to 10%
                    float effectiveTax = TRADE_TAX_BASE - repDiscount;
                    float earned = 0.f;
                    float taxTotal = 0.f;
                    for (auto& [type, qty] : inv.contents) {
                        if (qty <= 0) continue;
                        float price = mkt2->GetPrice(type);
                        sp2->quantities[type] += qty;
                        float gross = price * qty;
                        float tax   = gross * effectiveTax;
                        earned   += gross - tax;
                        taxTotal += tax;
                    }
                    mon.balance += earned;
                    if (auto* destSettl2 = m_registry.try_get<Settlement>(nearSettl))
                        destSettl2->treasury += taxTotal;
                    inv.contents.clear();
                    char ledgBuf[80];
                    std::snprintf(ledgBuf, sizeof(ledgBuf),
                        "Sold at %s +%.0fg", settl.name.c_str(), earned);
                    PushTradeRecord(ledgBuf, earned);
                    m_playerReputation += 1;  // +1 rep per completed trade
                    if (log2) log2->Push(day2, hr2,
                        "Sold goods at " + settl.name
                        + " for " + std::to_string((int)earned) + "g (tax "
                        + std::to_string((int)taxTotal) + "g)");
                }
                // Inventory empty → buy the highest-profit tradeable good
                else if (inv.TotalItems() == 0 && sp2 && mkt2) {
                    // Find the resource with the best sell price at a different settlement
                    ResourceType bestRes = ResourceType::Food;
                    float bestMargin = 0.f;
                    int   bestQty    = 0;
                    float bestBuy    = 0.f;

                    m_registry.view<Position, Settlement, Stockpile, Market>().each(
                        [&](auto destE, const Position&, const Settlement&,
                            const Stockpile&, const Market& destMkt) {
                        if (destE == nearSettl) return;
                        for (const auto& [res, buyPrice] : mkt2->price) {
                            float sellPrice = destMkt.GetPrice(res);
                            if (sellPrice <= buyPrice) continue;
                            float stock = sp2->quantities.count(res)
                                          ? sp2->quantities.at(res) : 0.f;
                            int qty = std::min(inv.maxCapacity, (int)(stock * 0.5f));
                            if (qty <= 0) continue;
                            float margin = (sellPrice - buyPrice) * qty;
                            if (margin > bestMargin) {
                                bestMargin = margin;
                                bestRes    = res;
                                bestQty    = qty;
                                bestBuy    = buyPrice;
                            }
                        }
                    });

                    if (bestQty > 0 && mon.balance >= bestBuy * bestQty) {
                        sp2->quantities[bestRes] -= bestQty;
                        inv.contents[bestRes]     = bestQty;
                        float cost = bestBuy * bestQty;
                        mon.balance -= cost;
                        // Purchase price goes to the selling settlement's treasury
                        settl.treasury += cost;
                        const char* rn2 = (bestRes == ResourceType::Food)  ? "food"  :
                                          (bestRes == ResourceType::Water) ? "water" : "wood";
                        if (log2) log2->Push(day2, hr2,
                            "Bought " + std::to_string(bestQty) + " goods at "
                            + settl.name + " for " + std::to_string((int)cost) + "g");
                        char ledg2[80];
                        std::snprintf(ledg2, sizeof(ledg2), "Bought %dx%s @%s -%.0fg",
                                      bestQty, rn2, settl.name.c_str(), cost);
                        PushTradeRecord(ledg2, -cost);
                    } else {
                        if (log2) log2->Push(day2, hr2,
                            "Nothing profitable to buy at " + settl.name);
                    }
                }
            }
        }
    }

    // One-shot: road toggle
    if (m_input.roadToggle.exchange(false)) {
        auto logv = m_registry.view<EventLog>();
        EventLog* log = (logv.begin() == logv.end()) ? nullptr
                      : &logv.get<EventLog>(*logv.begin());
        m_registry.view<Road>().each([&](Road& road) {
            road.blocked = !road.blocked;
            if (log) {
                log->Push(tm.day, (int)tm.hourOfDay,
                    road.blocked ? "Road BLOCKED — haulers rerouting"
                                 : "Road CLEARED — trade resumes");
            }
        });
    }

    // One-shot: player settle (H) — adopt nearest settlement as home
    if (m_input.playerSettle.exchange(false)) {
        static constexpr float SETTLE_RADIUS = 140.f;
        auto pv  = m_registry.view<PlayerTag, Position, HomeSettlement>();
        auto lv  = m_registry.view<EventLog>();
        if (pv.begin() != pv.end()) {
            auto  pe   = *pv.begin();
            auto& ppos = pv.get<Position>(pe);
            auto& home = pv.get<HomeSettlement>(pe);

            entt::entity nearest  = entt::null;
            float        nearDist = SETTLE_RADIUS * SETTLE_RADIUS;
            m_registry.view<Position, Settlement>().each(
                [&](auto e, const Position& sp, const Settlement&) {
                float dx = sp.x - ppos.x, dy = sp.y - ppos.y;
                float d  = dx*dx + dy*dy;
                if (d < nearDist) { nearDist = d; nearest = e; }
            });

            if (nearest != entt::null && nearest != home.settlement) {
                home.settlement = nearest;
                if (lv.begin() != lv.end()) {
                    const auto& sett = m_registry.get<Settlement>(nearest);
                    lv.get<EventLog>(*lv.begin()).Push(
                        tm.day, (int)tm.hourOfDay,
                        "You settle at " + sett.name);
                }
            } else if (nearest == entt::null && lv.begin() != lv.end()) {
                lv.get<EventLog>(*lv.begin()).Push(
                    tm.day, (int)tm.hourOfDay, "No settlement nearby to settle in");
            }
        }
    }

    // One-shot: player sleep toggle (Z) — toggle between Sleeping and Idle
    if (m_input.playerSleep.exchange(false)) {
        auto pv = m_registry.view<PlayerTag, AgentState, Velocity>();
        auto lv = m_registry.view<EventLog>();
        if (pv.begin() != pv.end()) {
            auto pe     = *pv.begin();
            auto& state = pv.get<AgentState>(pe);
            auto& vel   = pv.get<Velocity>(pe);
            if (state.behavior == AgentBehavior::Sleeping) {
                state.behavior = AgentBehavior::Idle;
                if (lv.begin() != lv.end())
                    lv.get<EventLog>(*lv.begin()).Push(
                        tm.day, (int)tm.hourOfDay, "You wake up");
            } else {
                state.behavior = AgentBehavior::Sleeping;
                vel.vx = vel.vy = 0.f;
                if (lv.begin() != lv.end())
                    lv.get<EventLog>(*lv.begin()).Push(
                        tm.day, (int)tm.hourOfDay, "You go to sleep");
            }
        }
    }

    // One-shot: player work (E) — confront nearby bandit OR work at nearest facility
    if (m_input.playerWork.exchange(false)) {
        static constexpr float WORK_RADIUS    = 80.f;
        static constexpr float CONFRONT_RANGE = 80.f;
        auto pv3 = m_registry.view<PlayerTag, Position, AgentState>();
        auto lv3 = m_registry.view<EventLog>();
        EventLog* plog = (lv3.begin() == lv3.end()) ? nullptr
                        : &lv3.get<EventLog>(*lv3.begin());

        if (pv3.begin() != pv3.end()) {
            auto  pe3   = *pv3.begin();
            auto& ppos3 = pv3.get<Position>(pe3);
            auto& pst3  = pv3.get<AgentState>(pe3);

            // Check for nearby bandits first — confront takes priority over working
            entt::entity nearBandit  = entt::null;
            float        nearBanditD = CONFRONT_RANGE * CONFRONT_RANGE;
            m_registry.view<BanditTag, Position, Money>().each(
                [&](auto be, const Position& bp, const Money&) {
                float dx = bp.x - ppos3.x, dy = bp.y - ppos3.y;
                float d  = dx*dx + dy*dy;
                if (d < nearBanditD) { nearBanditD = d; nearBandit = be; }
            });
            if (nearBandit != entt::null) {
                auto* bMoney = m_registry.try_get<Money>(nearBandit);
                if (bMoney) {
                    float recover = bMoney->balance * 0.5f;
                    if (auto* pMoney = m_registry.try_get<Money>(pe3))
                        pMoney->balance += recover;
                    bMoney->balance -= recover;
                    m_registry.remove<BanditTag>(nearBandit);
                    m_playerReputation += 10;
                    if (plog) {
                        std::string bandName = "a bandit";
                        if (const auto* n = m_registry.try_get<Name>(nearBandit))
                            bandName = n->value;
                        // Find nearest road to bandit for log detail
                        const auto* bpos = m_registry.try_get<Position>(nearBandit);
                        std::string roadLabel;
                        if (bpos) {
                            float bestD2 = std::numeric_limits<float>::max();
                            m_registry.view<Road>().each([&](const Road& road) {
                                if (road.blocked) return;
                                const auto* pa = m_registry.try_get<Position>(road.from);
                                const auto* pb = m_registry.try_get<Position>(road.to);
                                if (!pa || !pb) return;
                                float mx = (pa->x + pb->x) * 0.5f;
                                float my = (pa->y + pb->y) * 0.5f;
                                float dx2 = mx - bpos->x, dy2 = my - bpos->y;
                                float d2 = dx2*dx2 + dy2*dy2;
                                if (d2 < bestD2) {
                                    bestD2 = d2;
                                    std::string nA, nB;
                                    if (const auto* sa = m_registry.try_get<Settlement>(road.from))
                                        nA = sa->name;
                                    if (const auto* sb = m_registry.try_get<Settlement>(road.to))
                                        nB = sb->name;
                                    if (!nA.empty() && !nB.empty())
                                        roadLabel = " on the " + nA + "-" + nB + " road";
                                    else
                                        roadLabel.clear();
                                }
                            });
                        }
                        char buf[256];
                        std::snprintf(buf, sizeof(buf),
                            "Player confronted %s%s, recovered %.1fg (+10 rep)",
                            bandName.c_str(), roadLabel.c_str(), recover);
                        plog->Push(tm.day, (int)tm.hourOfDay, buf);
                    }
                }
            } else if (pst3.behavior == AgentBehavior::Working) {
                // Toggle off
                pst3.behavior = AgentBehavior::Idle;
                if (plog) plog->Push(tm.day, (int)tm.hourOfDay, "You stop working");
            } else {
                // Find nearest production facility in range
                entt::entity nearFac = entt::null;
                float nearDSq = WORK_RADIUS * WORK_RADIUS;
                ResourceType nearOut = ResourceType::Food;

                m_registry.view<Position, ProductionFacility>().each(
                    [&](auto fe, const Position& fp, const ProductionFacility& fac) {
                    if (fac.baseRate <= 0.f) return;
                    float dx = fp.x - ppos3.x, dy = fp.y - ppos3.y;
                    float d = dx*dx + dy*dy;
                    if (d < nearDSq) { nearDSq = d; nearFac = fe; nearOut = fac.output; }
                });

                if (nearFac != entt::null) {
                    pst3.behavior = AgentBehavior::Working;
                    pst3.target   = nearFac;
                    const char* facName = (nearOut == ResourceType::Food)  ? "farm"       :
                                         (nearOut == ResourceType::Water) ? "well"       :
                                         (nearOut == ResourceType::Wood)  ? "lumber mill" : "facility";
                    if (plog) plog->Push(tm.day, (int)tm.hourOfDay,
                        std::string("You begin working at the ") + facName);
                } else {
                    if (plog) plog->Push(tm.day, (int)tm.hourOfDay, "No facility nearby to work at");
                }
            }
        }
    }

    // One-shot: player buy (Q) — buy 1 unit of a resource from nearest settlement at market price.
    // Requires: player has enough gold, settlement has stock, player is within range.
    // This enables the merchant playstyle: earn gold via E-key work, buy cheap, sell elsewhere via T.
    if (m_input.playerBuy.exchange(false)) {
        static constexpr float BUY_RADIUS = 150.f;
        auto pv4 = m_registry.view<PlayerTag, Position, Inventory, Money>();
        auto lv4 = m_registry.view<EventLog>();
        EventLog* blog = (lv4.begin() == lv4.end()) ? nullptr
                        : &lv4.get<EventLog>(*lv4.begin());

        if (pv4.begin() != pv4.end()) {
            auto  pe4  = *pv4.begin();
            auto& pp4  = pv4.get<Position>(pe4);
            auto& inv4 = pv4.get<Inventory>(pe4);
            auto& mon4 = pv4.get<Money>(pe4);

            // Find nearest settlement
            entt::entity nearS = entt::null;
            float        nearD = BUY_RADIUS * BUY_RADIUS;
            m_registry.view<Position, Settlement>().each(
                [&](auto se, const Position& sp, const Settlement&) {
                float dx = sp.x - pp4.x, dy = sp.y - pp4.y;
                float d  = dx*dx + dy*dy;
                if (d < nearD) { nearD = d; nearS = se; }
            });

            if (nearS == entt::null) {
                if (blog) blog->Push(tm.day, (int)tm.hourOfDay, "No settlement in range to buy from");
            } else {
                auto* sp4  = m_registry.try_get<Stockpile>(nearS);
                auto* mkt4 = m_registry.try_get<Market>(nearS);
                const auto* sn4 = m_registry.try_get<Settlement>(nearS);
                std::string sname = sn4 ? sn4->name : "?";

                if (inv4.TotalItems() >= inv4.maxCapacity) {
                    if (blog) blog->Push(tm.day, (int)tm.hourOfDay, "Inventory full — can't buy more");
                } else if (sp4 && mkt4) {
                    // Find cheapest available resource (best value for the player to carry).
                    // Buy as many units as the player can carry and afford, capped at half
                    // the stockpile so the settlement isn't stripped bare.
                    ResourceType cheapRes  = ResourceType::Food;
                    float        cheapPrice = std::numeric_limits<float>::max();
                    int          buyQty    = 0;

                    int freeSlots = inv4.maxCapacity - inv4.TotalItems();
                    for (auto res : { ResourceType::Food, ResourceType::Water, ResourceType::Wood }) {
                        float stock = sp4->quantities.count(res) ? sp4->quantities.at(res) : 0.f;
                        if (stock < 1.f) continue;
                        float price = mkt4->GetPrice(res);
                        if (price >= cheapPrice) continue;
                        // How many can the player afford and carry (max half the stockpile)
                        int canAfford = static_cast<int>(mon4.balance / price);
                        int maxHalf   = static_cast<int>(stock * 0.5f);
                        int qty = std::min({freeSlots, canAfford, maxHalf});
                        if (qty <= 0) continue;
                        cheapPrice = price;
                        cheapRes   = res;
                        buyQty     = qty;
                    }

                    if (buyQty > 0) {
                        sp4->quantities[cheapRes]   -= (float)buyQty;
                        inv4.contents[cheapRes]     += buyQty;
                        float totalCost = cheapPrice * buyQty;
                        mon4.balance                -= totalCost;
                        // Purchase price goes to the selling settlement's treasury
                        if (auto* settl4 = m_registry.try_get<Settlement>(nearS))
                            settl4->treasury += totalCost;
                        const char* rname = (cheapRes == ResourceType::Food)  ? "food"  :
                                            (cheapRes == ResourceType::Water) ? "water" : "wood";
                        char buf[120];
                        std::snprintf(buf, sizeof(buf), "Bought %d %s at %s for %.2fg (%.2fg/unit)",
                                      buyQty, rname, sname.c_str(), totalCost, cheapPrice);
                        if (blog) blog->Push(tm.day, (int)tm.hourOfDay, buf);
                        char ledg[80];
                        std::snprintf(ledg, sizeof(ledg), "Bought %dx%s @%s -%.0fg",
                                      buyQty, rname, sname.c_str(), totalCost);
                        PushTradeRecord(ledg, -totalCost);
                    } else {
                        if (blog) blog->Push(tm.day, (int)tm.hourOfDay,
                            "Nothing affordable to buy at " + sname + " (stockpile empty or no gold)");
                    }
                }
            }
        }
    }

    // One-shot: player buy cart (V key) — spend 300g at a nearby settlement to
    // increase carry capacity by 10. Can be done multiple times (up to a cap of 45).
    if (m_input.playerBuyCart.exchange(false)) {
        static constexpr float CART_COST    = 300.f;
        static constexpr float CART_RADIUS  = 150.f;
        static constexpr int   CART_GAIN    = 10;
        static constexpr int   CART_MAX_CAP = 45;
        auto pvc = m_registry.view<PlayerTag, Position, Money, Inventory>();
        auto lvc = m_registry.view<EventLog>();
        EventLog* blogc = (lvc.begin() == lvc.end()) ? nullptr
                        : &lvc.get<EventLog>(*lvc.begin());
        if (pvc.begin() != pvc.end()) {
            auto  pec  = *pvc.begin();
            auto& ppc  = pvc.get<Position>(pec);
            auto& monc = pvc.get<Money>(pec);
            auto& invc = pvc.get<Inventory>(pec);

            // Must be near a settlement
            entt::entity nearSettlC = entt::null;
            float nearDistC = CART_RADIUS * CART_RADIUS;
            m_registry.view<Position, Settlement>().each(
                [&](auto se, const Position& sp, const Settlement&) {
                float dx = sp.x - ppc.x, dy = sp.y - ppc.y;
                if (dx*dx + dy*dy < nearDistC) { nearDistC = dx*dx+dy*dy; nearSettlC = se; }
            });

            if (nearSettlC == entt::null) {
                if (blogc) blogc->Push(tm.day, (int)tm.hourOfDay,
                    "Cart: must be near a settlement to purchase");
            } else if (invc.maxCapacity >= CART_MAX_CAP) {
                if (blogc) blogc->Push(tm.day, (int)tm.hourOfDay,
                    "Cart: already at maximum carry capacity");
            } else if (monc.balance < CART_COST) {
                char buf[80];
                std::snprintf(buf, sizeof(buf), "Cart: need %.0fg (have %.0fg)",
                    CART_COST, monc.balance);
                if (blogc) blogc->Push(tm.day, (int)tm.hourOfDay, buf);
            } else {
                monc.balance      -= CART_COST;
                invc.maxCapacity  += CART_GAIN;
                // Cart is purchased from the settlement — credit its treasury
                if (auto* cartSettl = m_registry.try_get<Settlement>(nearSettlC))
                    cartSettl->treasury += CART_COST;
                char buf[80];
                std::snprintf(buf, sizeof(buf),
                    "Bought a cart — carry capacity now %d (paid %.0fg)",
                    invc.maxCapacity, CART_COST);
                if (blogc) blogc->Push(tm.day, (int)tm.hourOfDay, buf);
            }
        }
    }

    // One-shot: player build (C key) — spend 200g to fund a new production facility
    // at the nearest settlement, building whichever resource has the highest market price.
    if (m_input.playerBuild.exchange(false)) {
        static constexpr float BUILD_RADIUS = 150.f;
        static constexpr float BUILD_COST   = 200.f;
        static constexpr float BUILD_RATE   = 3.f;
        auto pv5 = m_registry.view<PlayerTag, Position, Money>();
        auto lv5 = m_registry.view<EventLog>();
        EventLog* blog5 = (lv5.begin() == lv5.end()) ? nullptr
                        : &lv5.get<EventLog>(*lv5.begin());
        if (pv5.begin() != pv5.end()) {
            auto  pe5  = *pv5.begin();
            auto& pp5  = pv5.get<Position>(pe5);
            auto& mon5 = pv5.get<Money>(pe5);

            // Find nearest settlement within BUILD_RADIUS
            entt::entity nearSettl5 = entt::null;
            float nearDist5 = BUILD_RADIUS * BUILD_RADIUS;
            m_registry.view<Position, Settlement>().each(
                [&](auto se, const Position& sp, const Settlement&) {
                float dx = sp.x - pp5.x, dy = sp.y - pp5.y;
                float d2 = dx*dx + dy*dy;
                if (d2 < nearDist5) { nearDist5 = d2; nearSettl5 = se; }
            });

            if (nearSettl5 == entt::null) {
                if (blog5) blog5->Push(tm.day, (int)tm.hourOfDay,
                    "Build: no settlement nearby — move closer first");
            } else if (mon5.balance < BUILD_COST) {
                char buf[80];
                std::snprintf(buf, sizeof(buf),
                    "Build: need %.0fg (have %.0fg)", BUILD_COST, mon5.balance);
                if (blog5) blog5->Push(tm.day, (int)tm.hourOfDay, buf);
            } else {
                // Pick the most expensive resource at this settlement
                const auto* mkt5 = m_registry.try_get<Market>(nearSettl5);
                const auto* sPos5 = m_registry.try_get<Position>(nearSettl5);
                const auto* st5   = m_registry.try_get<Settlement>(nearSettl5);
                if (mkt5 && sPos5) {
                    ResourceType buildType5 = ResourceType::Food;
                    float bestPrice5 = -1.f;
                    for (auto r : { ResourceType::Food, ResourceType::Water, ResourceType::Wood }) {
                        float p = mkt5->GetPrice(r);
                        if (p > bestPrice5) { bestPrice5 = p; buildType5 = r; }
                    }
                    // Deduct gold and create facility at player's current position
                    mon5.balance -= BUILD_COST;
                    auto newFac5 = m_registry.create();
                    m_registry.emplace<Position>(newFac5, pp5.x, pp5.y);
                    m_registry.emplace<ProductionFacility>(newFac5,
                        ProductionFacility{ buildType5, BUILD_RATE, nearSettl5, {} });
                    const char* rname5 = (buildType5 == ResourceType::Food)  ? "farm"      :
                                         (buildType5 == ResourceType::Water) ? "well"      : "lumber mill";
                    m_playerReputation += 5;  // +5 rep for funding a facility
                    char buf[140];
                    std::snprintf(buf, sizeof(buf),
                        "You built a %s for %s (%.0fg)",
                        rname5, st5 ? st5->name.c_str() : "?", BUILD_COST);
                    if (blog5) blog5->Push(tm.day, (int)tm.hourOfDay, buf);
                }
            }
        }
    }

    // One-shot: player found settlement (P key) — spend 1,500g to establish a new settlement
    // at the player's current location. Minimum distance from any existing settlement: 400px.
    // Seeds with shelter + one production facility + 4 starter NPCs + 1 hauler.
    if (m_input.playerFoundSettlement.exchange(false)) {
        static constexpr float FOUND_COST       = 1500.f;
        static constexpr float FOUND_MIN_DIST   = 400.f;
        static constexpr float MAP_W_F          = 2400.f;
        static constexpr float MAP_H_F          =  720.f;

        auto pvf = m_registry.view<PlayerTag, Position, Money, HomeSettlement>();
        auto lvf = m_registry.view<EventLog>();
        EventLog* blogf = (lvf.begin() == lvf.end()) ? nullptr
                        : &lvf.get<EventLog>(*lvf.begin());

        if (pvf.begin() != pvf.end()) {
            auto  pef  = *pvf.begin();
            auto& ppf  = pvf.get<Position>(pef);
            auto& monf = pvf.get<Money>(pef);
            auto& hmf  = pvf.get<HomeSettlement>(pef);

            // Check gold
            if (monf.balance < FOUND_COST) {
                char buf[80];
                std::snprintf(buf, sizeof(buf),
                    "Found settlement: need %.0fg (have %.0fg)", FOUND_COST, monf.balance);
                if (blogf) blogf->Push(tm.day, (int)tm.hourOfDay, buf);
            } else {
                // Check distance from existing settlements
                float nearestSettlDist = FOUND_MIN_DIST * FOUND_MIN_DIST;
                bool tooClose = false;
                m_registry.view<Position, Settlement>().each(
                    [&](auto se, const Position& sp, const Settlement&) {
                    float dx = sp.x - ppf.x, dy = sp.y - ppf.y;
                    if (dx*dx + dy*dy < nearestSettlDist) tooClose = true;
                });

                if (tooClose) {
                    if (blogf) blogf->Push(tm.day, (int)tm.hourOfDay,
                        "Found settlement: too close to an existing settlement (need 400+ distance)");
                } else if (ppf.x < 60.f || ppf.x > MAP_W_F - 60.f ||
                           ppf.y < 60.f || ppf.y > MAP_H_F - 60.f) {
                    if (blogf) blogf->Push(tm.day, (int)tm.hourOfDay,
                        "Found settlement: too close to the map edge");
                } else {
                    // Pick a settlement name
                    static const char* FOUND_NAMES[] = {
                        "Ironvale","Copperhold","Stoneford","Ashgate","Saltmere",
                        "Thornwick","Coalport","Silversea","Flintridge","Emberhaven",
                        "Crowpeak","Dustmoor","Glassford","Shadowfen","Pinegrave"
                    };
                    static int nameIdx = 0;
                    const char* newName = FOUND_NAMES[nameIdx % 15];
                    ++nameIdx;

                    // Deduct gold — it becomes the new settlement's seed treasury
                    monf.balance -= FOUND_COST;

                    // Create settlement entity
                    auto newSettl = m_registry.create();
                    m_registry.emplace<Position>(newSettl, ppf.x, ppf.y);
                    m_registry.emplace<Settlement>(newSettl,
                        Settlement{ newName, 120.f, 1.f, 0.f, "", FOUND_COST, 15 });
                    m_registry.emplace<BirthTracker>(newSettl);
                    m_registry.emplace<StockpileAlert>(newSettl);
                    m_registry.emplace<Stockpile>(newSettl, Stockpile{{
                        { ResourceType::Food,  60.f },
                        { ResourceType::Water, 60.f },
                        { ResourceType::Wood,  40.f }
                    }});
                    // Starting market: mid-range prices
                    m_registry.emplace<Market>(newSettl, Market{{
                        { ResourceType::Food,  4.f },
                        { ResourceType::Water, 4.f },
                        { ResourceType::Wood,  4.f }
                    }});

                    // Shelter node
                    {
                        auto rest = m_registry.create();
                        m_registry.emplace<Position>(rest, ppf.x, ppf.y + 50.f);
                        m_registry.emplace<ProductionFacility>(rest,
                            ProductionFacility{ ResourceType::Shelter, 0.f, newSettl, {} });
                    }

                    // One production facility — pick resource type that's most scarce
                    // (highest average price across existing settlements)
                    {
                        float bestPrice = 0.f;
                        ResourceType buildRes = ResourceType::Food;
                        for (auto res : { ResourceType::Food, ResourceType::Water, ResourceType::Wood }) {
                            float totalP = 0.f; int cnt = 0;
                            m_registry.view<Market>().each([&](auto me, const Market& mkt) {
                                if (me == newSettl) return;
                                totalP += mkt.GetPrice(res); ++cnt;
                            });
                            float avgP = (cnt > 0) ? totalP / cnt : 0.f;
                            if (avgP > bestPrice) { bestPrice = avgP; buildRes = res; }
                        }
                        auto fac = m_registry.create();
                        m_registry.emplace<Position>(fac, ppf.x - 50.f, ppf.y - 50.f);
                        m_registry.emplace<ProductionFacility>(fac,
                            ProductionFacility{ buildRes, 3.f, newSettl, {} });
                    }

                    // Spawn 4 starter NPCs
                    static constexpr float DRAIN_H = 0.00083f, DRAIN_T = 0.00125f,
                                           DRAIN_E = 0.00050f, DRAIN_X = 0.00200f;
                    static constexpr float REFILL_H = 0.004f, REFILL_T = 0.006f,
                                           REFILL_E = 0.002f, REFILL_X = 0.010f;
                    std::mt19937 frng{std::random_device{}()};
                    std::uniform_real_distribution<float> ad(5.f, 35.f), ld(60.f, 100.f);
                    std::uniform_real_distribution<float> tr(0.80f, 1.20f), sk(0.35f, 0.65f);
                    std::uniform_real_distribution<float> ang(0.f, 6.28f);
                    static const char* FN[] = {"Aldric","Bryn","Clara","Daven","Elara","Finn","Gareth","Holt"};
                    static const char* LN[] = {"Smith","Miller","Cooper","Reed","Stone","Vale"};
                    std::uniform_int_distribution<int> fn(0,7), ln(0,5);

                    for (int i = 0; i < 4; ++i) {
                        float a = ang(frng);
                        auto npc = m_registry.create();
                        m_registry.emplace<Position>(npc, ppf.x + std::cos(a)*50.f, ppf.y + std::sin(a)*50.f);
                        m_registry.emplace<Velocity>(npc, 0.f, 0.f);
                        m_registry.emplace<MoveSpeed>(npc, 60.f);
                        Needs nn{{ Need{NeedType::Hunger,1.f,DRAIN_H*tr(frng),0.3f,REFILL_H},
                                   Need{NeedType::Thirst,1.f,DRAIN_T*tr(frng),0.3f,REFILL_T},
                                   Need{NeedType::Energy,1.f,DRAIN_E*tr(frng),0.3f,REFILL_E},
                                   Need{NeedType::Heat,  1.f,DRAIN_X*tr(frng),0.3f,REFILL_X} }};
                        m_registry.emplace<Needs>(npc, nn);
                        m_registry.emplace<AgentState>(npc);
                        m_registry.emplace<HomeSettlement>(npc, HomeSettlement{newSettl});
                        DeprivationTimer dtt; dtt.migrateThreshold = 5.f * 60.f;
                        m_registry.emplace<DeprivationTimer>(npc, dtt);
                        m_registry.emplace<Schedule>(npc);
                        m_registry.emplace<Renderable>(npc, WHITE, 6.f);
                        m_registry.emplace<Money>(npc, Money{10.f});
                        Age age; age.days = ad(frng); age.maxDays = ld(frng);
                        m_registry.emplace<Age>(npc, age);
                        m_registry.emplace<Name>(npc, Name{std::string(FN[fn(frng)])+" "+LN[ln(frng)]});
                        m_registry.emplace<Skills>(npc, Skills{sk(frng),sk(frng),sk(frng)});
                        m_registry.emplace<Reputation>(npc);
                    }

                    // Spawn 1 hauler
                    {
                        float a = ang(frng);
                        auto h = m_registry.create();
                        m_registry.emplace<Position>(h, ppf.x + std::cos(a)*80.f, ppf.y + std::sin(a)*80.f);
                        m_registry.emplace<Velocity>(h, 0.f, 0.f);
                        m_registry.emplace<MoveSpeed>(h, 70.f);
                        Needs hn{{ Need{NeedType::Hunger,1.f,DRAIN_H,0.3f,REFILL_H},
                                   Need{NeedType::Thirst,1.f,DRAIN_T,0.3f,REFILL_T},
                                   Need{NeedType::Energy,1.f,0.f,    0.3f,REFILL_E},
                                   Need{NeedType::Heat,  1.f,DRAIN_X,0.3f,REFILL_X} }};
                        m_registry.emplace<Needs>(h, hn);
                        m_registry.emplace<AgentState>(h);
                        m_registry.emplace<HomeSettlement>(h, HomeSettlement{newSettl});
                        DeprivationTimer hdtt; hdtt.migrateThreshold = 5.f * 60.f;
                        m_registry.emplace<DeprivationTimer>(h, hdtt);
                        m_registry.emplace<Inventory>(h, Inventory{{}, 15});
                        m_registry.emplace<Hauler>(h, Hauler{});
                        m_registry.emplace<Reputation>(h);
                        m_registry.emplace<Money>(h, Money{50.f});
                        m_registry.emplace<Renderable>(h, SKYBLUE, 7.f);
                        Age ha; ha.days = ad(frng); ha.maxDays = ld(frng);
                        m_registry.emplace<Age>(h, ha);
                        m_registry.emplace<Name>(h, Name{std::string(FN[fn(frng)])+" "+LN[ln(frng)]});
                    }

                    // Automatically connect to the nearest existing settlement via road
                    {
                        entt::entity linkTarget = entt::null;
                        float        linkDist2  = std::numeric_limits<float>::max();
                        m_registry.view<Position, Settlement>().each(
                            [&](auto se, const Position& sp, const Settlement&) {
                            if (se == newSettl) return;
                            float dx = sp.x - ppf.x, dy = sp.y - ppf.y;
                            float d2 = dx*dx + dy*dy;
                            if (d2 < linkDist2) { linkDist2 = d2; linkTarget = se; }
                        });
                        if (linkTarget != entt::null) {
                            auto nr = m_registry.create();
                            m_registry.emplace<Road>(nr, Road{ newSettl, linkTarget, false, 0.f });
                            if (blogf) {
                                std::string linkedName = "?";
                                if (const auto* ls = m_registry.try_get<Settlement>(linkTarget))
                                    linkedName = ls->name;
                                blogf->Push(tm.day, (int)tm.hourOfDay,
                                    "Road opened: " + std::string(newName) + " ↔ " + linkedName);
                            }
                        }
                    }

                    // Relocate the player to this settlement as their new home
                    hmf.settlement = newSettl;
                    m_playerReputation += 20;  // +20 rep for founding a settlement

                    if (blogf) {
                        char buf[160];
                        std::snprintf(buf, sizeof(buf),
                            "Player founded %s at (%.0f, %.0f) — %.0fg, 4 settlers + 1 hauler",
                            newName, ppf.x, ppf.y, FOUND_COST);
                        blogf->Push(tm.day, (int)tm.hourOfDay, buf);
                    }
                }
            }
        }
    }

    // One-shot: road repair (R key) — pay 50g to unblock the nearest blocked road within 80px.
    if (m_input.roadRepair.exchange(false)) {
        static constexpr float REPAIR_RADIUS = 80.f;
        static constexpr float REPAIR_COST   = 50.f;
        auto pv6 = m_registry.view<PlayerTag, Position, Money>();
        auto lv6 = m_registry.view<EventLog>();
        EventLog* blog6 = (lv6.begin() == lv6.end()) ? nullptr
                        : &lv6.get<EventLog>(*lv6.begin());
        if (pv6.begin() != pv6.end()) {
            auto  pe6  = *pv6.begin();
            auto& pp6  = pv6.get<Position>(pe6);
            auto& mon6 = pv6.get<Money>(pe6);

            entt::entity nearRoad6 = entt::null;
            float        nearDist6 = REPAIR_RADIUS * REPAIR_RADIUS;
            m_registry.view<Road>().each([&](auto re, const Road& road) {
                if (!road.blocked) return;
                const auto* posA = m_registry.try_get<Position>(road.from);
                const auto* posB = m_registry.try_get<Position>(road.to);
                if (!posA || !posB) return;
                float mx6 = (posA->x + posB->x) * 0.5f;
                float my6 = (posA->y + posB->y) * 0.5f;
                float dx = mx6 - pp6.x, dy = my6 - pp6.y;
                float d2 = dx*dx + dy*dy;
                if (d2 < nearDist6) { nearDist6 = d2; nearRoad6 = re; }
            });

            if (nearRoad6 == entt::null) {
                if (blog6) blog6->Push(tm.day, (int)tm.hourOfDay,
                    "Road repair: no blocked road nearby (walk closer to a blocked road)");
            } else if (mon6.balance < REPAIR_COST) {
                char buf[80];
                std::snprintf(buf, sizeof(buf),
                    "Road repair: need %.0fg (have %.0fg)", REPAIR_COST, mon6.balance);
                if (blog6) blog6->Push(tm.day, (int)tm.hourOfDay, buf);
            } else {
                auto& road6 = m_registry.get<Road>(nearRoad6);
                road6.blocked     = false;
                road6.banditTimer = 0.f;
                road6.condition   = 1.f;   // full restoration on repair
                mon6.balance     -= REPAIR_COST;
                m_playerReputation += 2;  // +2 rep for road repair
                if (blog6) blog6->Push(tm.day, (int)tm.hourOfDay,
                    "You repaired the road (paid " + std::to_string((int)REPAIR_COST) + "g)");
            }
        }
    }

    // One-shot: road build (N key, two-press) — pay 400g to connect two settlements.
    if (m_input.roadBuild.exchange(false)) {
        static constexpr float ROAD_BUILD_COST   = 400.f;
        static constexpr float SETTLE_SNAP_RADIUS = 200.f;
        float fromX = m_input.roadBuildFromX.load();
        float fromY = m_input.roadBuildFromY.load();
        float toX   = m_input.roadBuildToX.load();
        float toY   = m_input.roadBuildToY.load();

        auto lv7 = m_registry.view<EventLog>();
        EventLog* blog7 = (lv7.begin() == lv7.end()) ? nullptr
                        : &lv7.get<EventLog>(*lv7.begin());
        auto pv7 = m_registry.view<PlayerTag, Money>();

        if (pv7.begin() != pv7.end()) {
            auto  pe7  = *pv7.begin();
            auto& mon7 = pv7.get<Money>(pe7);

            // Find nearest settlement to each endpoint
            auto findNearest = [&](float wx, float wy) -> entt::entity {
                entt::entity best = entt::null;
                float bestD2 = SETTLE_SNAP_RADIUS * SETTLE_SNAP_RADIUS;
                m_registry.view<Position, Settlement>().each(
                    [&](auto se, const Position& sp, const Settlement&) {
                    float dx = sp.x - wx, dy = sp.y - wy;
                    float d2 = dx*dx + dy*dy;
                    if (d2 < bestD2) { bestD2 = d2; best = se; }
                });
                return best;
            };

            entt::entity sA = findNearest(fromX, fromY);
            entt::entity sB = findNearest(toX, toY);

            if (sA == entt::null || sB == entt::null) {
                if (blog7) blog7->Push(tm.day, (int)tm.hourOfDay,
                    "Road build: must press N near a settlement at both start and end");
            } else if (sA == sB) {
                if (blog7) blog7->Push(tm.day, (int)tm.hourOfDay,
                    "Road build: start and end must be different settlements");
            } else if (mon7.balance < ROAD_BUILD_COST) {
                char buf[80];
                std::snprintf(buf, sizeof(buf),
                    "Road build: need %.0fg (have %.0fg)", ROAD_BUILD_COST, mon7.balance);
                if (blog7) blog7->Push(tm.day, (int)tm.hourOfDay, buf);
            } else {
                // Check if road already exists between these two settlements
                bool exists = false;
                m_registry.view<Road>().each([&](const Road& r) {
                    if ((r.from == sA && r.to == sB) || (r.from == sB && r.to == sA))
                        exists = true;
                });
                if (exists) {
                    if (blog7) blog7->Push(tm.day, (int)tm.hourOfDay,
                        "Road build: a road already connects those settlements");
                } else {
                    auto newRoad = m_registry.create();
                    m_registry.emplace<Road>(newRoad, Road{ sA, sB, false, 0.f });
                    mon7.balance -= ROAD_BUILD_COST;
                    std::string nameA = "?", nameB = "?";
                    if (const auto* st = m_registry.try_get<Settlement>(sA)) nameA = st->name;
                    if (const auto* st = m_registry.try_get<Settlement>(sB)) nameB = st->name;
                    char buf[120];
                    std::snprintf(buf, sizeof(buf),
                        "You built a road: %s ↔ %s (%.0fg)",
                        nameA.c_str(), nameB.c_str(), ROAD_BUILD_COST);
                    if (blog7) blog7->Push(tm.day, (int)tm.hourOfDay, buf);
                }
            }
        }
    }

    // Continuous: player movement (velocity is set, MovementSystem applies it)
    // Movement is blocked while sleeping — pressing WASD wakes them up.
    {
        float mx = m_input.playerMoveX.load();
        float my = m_input.playerMoveY.load();
        auto pv = m_registry.view<PlayerTag, Velocity, MoveSpeed, AgentState>();
        if (pv.begin() != pv.end()) {
            auto  pe    = *pv.begin();
            auto& vel   = pv.get<Velocity>(pe);
            float spd   = pv.get<MoveSpeed>(pe).value;
            auto& state = pv.get<AgentState>(pe);
            if (state.behavior == AgentBehavior::Sleeping) {
                vel.vx = vel.vy = 0.f;
                // Any movement input wakes the player
                if (mx != 0.f || my != 0.f) {
                    state.behavior = AgentBehavior::Idle;
                    auto lv = m_registry.view<EventLog>();
                    if (lv.begin() != lv.end())
                        lv.get<EventLog>(*lv.begin()).Push(
                            tm.day, (int)tm.hourOfDay, "You wake up");
                }
            } else {
                vel.vx = mx * spd;
                vel.vy = my * spd;
            }
        }
    }

    // One-shot: settlement click (world-space, resolved here on sim thread)
    {
        std::lock_guard<std::mutex> lock(m_clickMutex);
        if (m_pendingClick) {
            m_pendingClick = false;
            float wx = m_clickX, wy = m_clickY;
            entt::entity clicked = entt::null;
            m_registry.view<Position, Settlement>().each(
                [&](auto e, const Position& sp, const Settlement& s) {
                    float dx = wx - sp.x, dy = wy - sp.y;
                    if (dx*dx + dy*dy <= s.radius * s.radius) clicked = e;
                });
            m_selectedSettlement = (clicked == m_selectedSettlement)
                                   ? entt::null : clicked;
        }
    }
}

// ---- Build render snapshot --------------------------------------------

void SimThread::WriteSnapshot() {
    // Build into a local struct, then swap under the lock.
    // This minimises the time the mutex is held.

    // Temporary vectors built without the lock:
    std::vector<RenderSnapshot::AgentEntry>       agents;
    std::vector<RenderSnapshot::SettlementEntry>  settlements;
    std::vector<RenderSnapshot::RoadEntry>        roads;
    std::vector<RenderSnapshot::FacilityEntry>    facilities;
    std::vector<RenderSnapshot::SettlementStatus> worldStatus;
    RenderSnapshot::StockpilePanel                panel;

    // ---- Agents ----
    m_registry.view<Position, AgentState, Renderable>().each(
        [&](auto e, const Position& pos, const AgentState& astate, const Renderable& rend) {

        bool isPlayer = m_registry.all_of<PlayerTag>(e);
        bool isHauler = m_registry.all_of<Hauler>(e);
        bool isChild  = m_registry.all_of<ChildTag>(e);
        RenderSnapshot::AgentRole role = isPlayer ? RenderSnapshot::AgentRole::Player
                                       : isHauler ? RenderSnapshot::AgentRole::Hauler
                                       : isChild  ? RenderSnapshot::AgentRole::Child
                                                  : RenderSnapshot::AgentRole::NPC;

        Color drawColor = rend.color;
        float hp = 1.f, tp = 1.f, ep = 1.f, htp = 1.f;

        if (const auto* needs = m_registry.try_get<Needs>(e)) {
            hp  = needs->list[0].value;
            tp  = needs->list[1].value;
            ep  = needs->list[2].value;
            htp = needs->list[3].value;
            if (!isPlayer) {
                float worst = std::min({hp, tp, ep, htp});
                if      (worst < 0.15f) drawColor = RED;
                else if (worst < 0.30f) drawColor = ORANGE;
                else if (worst < 0.55f) drawColor = YELLOW;
                else                    drawColor = isHauler ? SKYBLUE : WHITE;
            }
        }

        Color ring = isPlayer ? WHITE : (isHauler ? DARKBLUE : DARKGRAY);

        bool  hasCargo   = false;
        Color cargoColor = WHITE;
        float balance    = 0.f;
        if (const auto* money = m_registry.try_get<Money>(e))
            balance = money->balance;
        if (isHauler) {
            if (const auto* inv = m_registry.try_get<Inventory>(e)) {
                for (const auto& [type, qty] : inv->contents) {
                    if (qty <= 0) continue;
                    hasCargo  = true;
                    cargoColor = (type == ResourceType::Food)  ? GREEN :
                                 (type == ResourceType::Water) ? BLUE  : BROWN;
                    break;
                }
            }
        }

        float ageDays = 0.f, maxDays = 80.f;
        if (const auto* age = m_registry.try_get<Age>(e)) {
            ageDays = age->days;
            maxDays = age->maxDays;
        }

        // Life-stage color tint (only when not overridden by need-distress color).
        // Distress colors are RED/ORANGE/YELLOW; healthy adults are WHITE/SKYBLUE.
        // We tint healthy children warm-yellow and healthy elders cool-silver.
        bool isHealthyColor = !isPlayer && !isHauler
                           && drawColor.r == 255 && drawColor.g == 255 && drawColor.b == 255;
        if (isHealthyColor) {
            if      (ageDays < 15.f)  drawColor = Color{ 255, 240, 160, 255 };  // child: warm yellow
            else if (ageDays > 65.f)  drawColor = Color{ 180, 200, 220, 255 };  // elder: cool silver
        }

        std::string npcName;
        if (const auto* n = m_registry.try_get<Name>(e))
            npcName = n->value;

        // Populate profession string from the persistent Profession component.
        // Haulers are always "Merchant". Falls back to skill inference if no component.
        std::string profession;
        if (isHauler) {
            profession = "Merchant";
        } else if (!isPlayer) {
            if (const auto* prof = m_registry.try_get<Profession>(e)) {
                profession = ProfessionLabel(prof->type);
            } else {
                // Fallback: infer from strongest skill (for NPCs without a component)
                const auto* sk = m_registry.try_get<Skills>(e);
                if (sk) {
                    float mx = std::max({sk->farming, sk->water_drawing, sk->woodcutting});
                    float spread = mx - std::min({sk->farming, sk->water_drawing, sk->woodcutting});
                    if (spread > 0.05f) {
                        profession = (sk->farming       == mx) ? "Farmer"       :
                                     (sk->water_drawing == mx) ? "Water Carrier" : "Woodcutter";
                    }
                }
            }
        }

        // Trade route destination for haulers en route (makes trade flow visible)
        bool  hasRouteDest = false;
        float routeDestX = 0.f, routeDestY = 0.f;
        std::map<ResourceType, int> haulerCargo;
        std::string haulerDestName;
        if (isHauler) {
            if (const auto* h = m_registry.try_get<Hauler>(e)) {
                if (h->state == HaulerState::GoingToDeposit &&
                    h->targetSettlement != entt::null &&
                    m_registry.valid(h->targetSettlement)) {
                    const auto& dp = m_registry.get<Position>(h->targetSettlement);
                    hasRouteDest = true;
                    routeDestX   = dp.x;
                    routeDestY   = dp.y;
                    if (const auto* ds = m_registry.try_get<Settlement>(h->targetSettlement))
                        haulerDestName = ds->name;
                }
            }
            if (const auto* inv = m_registry.try_get<Inventory>(e))
                haulerCargo = inv->contents;
        }

        // Hauler profit estimation fields
        float haulerBuyPrice = 0.f;
        int   haulerCargoQty = 0;
        bool  nearBankrupt   = false;
        float bankruptProgress = 0.f;
        int   haulerState    = 0;
        if (isHauler) {
            if (const auto* h = m_registry.try_get<Hauler>(e)) {
                haulerBuyPrice    = h->buyPrice;
                nearBankrupt      = h->nearBankrupt;
                bankruptProgress  = h->bankruptProgress;
                haulerState       = static_cast<int>(h->state);
            }
            if (const auto* inv = m_registry.try_get<Inventory>(e)) {
                for (const auto& [res, qty] : inv->contents)
                    haulerCargoQty += qty;
            }
        }

        // Home settlement name and position for tooltip / return-trip line
        std::string homeSettlName;
        float homeX = 0.f, homeY = 0.f;
        bool  hasHome = false;
        float homeMorale = -1.f;
        if (!isPlayer) {
            if (const auto* hs = m_registry.try_get<HomeSettlement>(e)) {
                if (hs->settlement != entt::null && m_registry.valid(hs->settlement)) {
                    if (const auto* sts = m_registry.try_get<Settlement>(hs->settlement)) {
                        homeSettlName = sts->name;
                        homeMorale    = sts->morale;
                    }
                    if (const auto* hp = m_registry.try_get<Position>(hs->settlement)) {
                        homeX = hp->x; homeY = hp->y; hasHome = true;
                    }
                }
            }
        }

        // Skills snapshot
        float farmSkill = -1.f, waterSkill = -1.f, woodSkill = -1.f;
        float wagePerHour = 0.f;
        if (const auto* sk = m_registry.try_get<Skills>(e)) {
            farmSkill  = sk->farming;
            waterSkill = sk->water_drawing;
            woodSkill  = sk->woodcutting;
            // Compute wage estimate for working NPCs (same formula as ConsumptionSystem)
            if (!isHauler && !isPlayer && astate.behavior == AgentBehavior::Working) {
                float bestSkill = std::max({sk->farming, sk->water_drawing, sk->woodcutting});
                wagePerHour = 0.3f * (0.5f + bestSkill);  // WAGE_RATE * skillMult
            }
        }

        // Reputation snapshot
        float reputationScore = 0.f;
        if (const auto* rep = m_registry.try_get<Reputation>(e))
            reputationScore = rep->score;

        // Fatigue snapshot
        bool isFatigued = false;
        if (const auto* sched = m_registry.try_get<Schedule>(e))
            isFatigued = sched->fatigued;

        // Exile snapshot: homeless + theftCount >= 3
        bool isExiled = false;
        {
            const auto* hs = m_registry.try_get<HomeSettlement>(e);
            const auto* dt2 = m_registry.try_get<DeprivationTimer>(e);
            if (hs && dt2 && (hs->settlement == entt::null || !m_registry.valid(hs->settlement))
                && dt2->theftCount >= 3)
                isExiled = true;
        }

        // Scale visual size by life stage so children are visibly smaller.
        // Children (<15 days): 60% size; youth (15-25): 80%; adult: 100%; elderly (>65): 105%
        float drawSize = rend.size;
        if (!isHauler && !isPlayer) {
            if      (ageDays < 15.f) drawSize *= 0.60f;
            else if (ageDays < 25.f) drawSize *= 0.80f;
            else if (ageDays > 65.f) drawSize *= 1.05f;
        }

        // Contentment: weighted average of needs
        float contentment = 0.30f * hp + 0.30f * tp + 0.20f * ep + 0.20f * htp;

        // For children: name of the adult they are following (AgentState::target)
        std::string followingName;
        if (isChild && astate.target != entt::null && m_registry.valid(astate.target)) {
            if (const auto* fn = m_registry.try_get<Name>(astate.target))
                followingName = fn->value;
        }

        std::string familyName;
        if (const auto* ft = m_registry.try_get<FamilyTag>(e))
            familyName = ft->name;

        bool recentlyHelped  = false;
        bool recentlyStole   = false;
        bool isGrateful      = false;
        bool recentWarmthGlow = false;
        bool charityReady    = false;
        float charityTimerLeft = 0.f;
        bool onStrike        = false;
        float strikeHoursLeft = 0.f;
        bool ill             = false;
        int  illNeedIdx      = 0;
        bool harvestBonus    = false;
        if (const auto* dt = m_registry.try_get<DeprivationTimer>(e)) {
            recentlyHelped   = (dt->helpedTimer > 0.f);
            recentlyStole    = (dt->stealCooldown > 46.f);
            isGrateful       = (dt->gratitudeTimer > 0.f);
            recentWarmthGlow = (htp > 0.9f && dt->charityTimer > 0.f);
            charityReady     = (dt->charityTimer <= 0.f);
            charityTimerLeft = dt->charityTimer;
            onStrike         = (dt->strikeDuration > 0.f);
            strikeHoursLeft  = dt->strikeDuration;
            ill              = (dt->illnessTimer > 0.f);
            illNeedIdx       = dt->illnessNeedIdx;
            harvestBonus     = (dt->harvestBonusTimer > 0.f);
        }

        bool isBandit = m_registry.all_of<BanditTag>(e);
        std::string gangName;
        if (isBandit) {
            if (const auto* dt2 = m_registry.try_get<DeprivationTimer>(e))
                gangName = dt2->gangName;
        }
        // Bandits render as dark maroon regardless of need state
        if (isBandit && !isPlayer)
            drawColor = Color{ 140, 30, 30, 255 };

        // Vocation check: profession matches highest skill
        bool inVocation = false;
        if (const auto* prof2 = m_registry.try_get<Profession>(e)) {
            if (const auto* sk2 = m_registry.try_get<Skills>(e)) {
                ResourceType bestRes = ResourceType::Food;
                float bestVal = sk2->farming;
                if (sk2->water_drawing > bestVal) { bestVal = sk2->water_drawing; bestRes = ResourceType::Water; }
                if (sk2->woodcutting   > bestVal) { bestRes = ResourceType::Wood; }
                inVocation = (prof2->type == ProfessionForResource(bestRes)
                              && prof2->type != ProfessionType::Idle);
            }
        }

        // Rumour carrier info
        bool hasRumour = false;
        std::string rumourLabel;
        if (const auto* rum = m_registry.try_get<Rumour>(e)) {
            hasRumour = true;
            switch (rum->type) {
                case RumourType::PlagueNearby:  rumourLabel = "plague";       break;
                case RumourType::DroughtNearby: rumourLabel = "drought";      break;
                case RumourType::BanditRoads:   rumourLabel = "bandits";      break;
                case RumourType::GoodHarvest:   rumourLabel = "good harvest"; break;
            }
        }

        agents.push_back({ pos.x, pos.y, drawSize,
                           drawColor, ring, hasCargo, cargoColor,
                           role, hp, tp, ep, htp, astate.behavior,
                           balance, ageDays, maxDays, npcName,
                           hasRouteDest, routeDestX, routeDestY,
                           std::move(haulerCargo), haulerDestName,
                           profession, homeSettlName,
                           homeX, homeY, hasHome,
                           farmSkill, waterSkill, woodSkill,
                           contentment, std::move(followingName),
                           std::move(familyName), recentlyHelped, recentlyStole,
                           isGrateful, recentWarmthGlow, charityReady, charityTimerLeft,
                           isBandit, std::move(gangName), onStrike, strikeHoursLeft, ill, illNeedIdx,
                           harvestBonus, inVocation,
                           hasRumour, std::move(rumourLabel),
                           haulerBuyPrice, haulerCargoQty,
                           nearBankrupt, bankruptProgress, haulerState,
                           homeMorale, wagePerHour, reputationScore, isFatigued,
                           isExiled });
    });

    // ---- Settlements ----
    // Read current season for settlement ring logic (available after HUD section below;
    // compute it here using a local view since WriteSnapshot builds local data first).
    Season snapSeason = Season::Spring;
    {
        auto stmv = m_registry.view<TimeManager>();
        if (stmv.begin() != stmv.end())
            snapSeason = stmv.get<TimeManager>(*stmv.begin()).CurrentSeason();
    }

    m_registry.view<Position, Settlement>().each(
        [&](auto e, const Position& pos, const Settlement& s) {
        float food = 0.f, water = 0.f, wood = 0.f;
        if (const auto* sp = m_registry.try_get<Stockpile>(e)) {
            food  = sp->quantities.count(ResourceType::Food)
                    ? sp->quantities.at(ResourceType::Food)  : 0.f;
            water = sp->quantities.count(ResourceType::Water)
                    ? sp->quantities.at(ResourceType::Water) : 0.f;
            wood  = sp->quantities.count(ResourceType::Wood)
                    ? sp->quantities.at(ResourceType::Wood)  : 0.f;
        }
        int spop = 0;
        m_registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
            [&](const HomeSettlement& hs) { if (hs.settlement == e) ++spop; });

        // Infer specialty from primary production facility
        std::string specialty;
        {
            ResourceType primary = ResourceType::Food;
            float maxRate = 0.f;
            m_registry.view<ProductionFacility>().each(
                [&](const ProductionFacility& fac) {
                if (fac.settlement == e && fac.baseRate > maxRate) {
                    maxRate = fac.baseRate; primary = fac.output;
                }
            });
            if (maxRate > 0.f)
                specialty = (primary == ResourceType::Food)  ? "Farming" :
                            (primary == ResourceType::Water) ? "Water"   : "Lumber";
        }

        settlements.push_back({
            pos.x, pos.y, s.radius, s.name,
            (e == m_selectedSettlement),
            static_cast<uint32_t>(e),
            food, water, wood, spop, s.popCap, snapSeason, specialty,
            s.modifierName, s.ruinTimer, s.morale, s.tradeVolume,
            s.importCount, s.exportCount, s.desperatePurchases
        });
    });

    // ---- Roads ----
    // Pre-count bandits per road entity (nearest midpoint)
    std::map<entt::entity, int> roadBanditCount;
    {
        auto roadView = m_registry.view<Road>();
        m_registry.view<Position, BanditTag>(entt::exclude<Hauler, PlayerTag>).each(
            [&](auto /*be*/, const Position& bpos) {
                entt::entity nearest = entt::null;
                float bestD2 = std::numeric_limits<float>::max();
                roadView.each([&](auto re, const Road& road) {
                    if (road.blocked) return;
                    const auto* pa = m_registry.try_get<Position>(road.from);
                    const auto* pb = m_registry.try_get<Position>(road.to);
                    if (!pa || !pb) return;
                    float mx = (pa->x + pb->x) * 0.5f;
                    float my = (pa->y + pb->y) * 0.5f;
                    float dx = mx - bpos.x, dy = my - bpos.y;
                    float d2 = dx*dx + dy*dy;
                    if (d2 < bestD2) { bestD2 = d2; nearest = re; }
                });
                if (nearest != entt::null) roadBanditCount[nearest]++;
            });
    }
    m_registry.view<Road>().each([&](auto roadEntity, const Road& road) {
        if (road.from == entt::null || road.to == entt::null) return;
        if (!m_registry.valid(road.from) || !m_registry.valid(road.to)) return;
        const auto& fp = m_registry.get<Position>(road.from);
        const auto& tp = m_registry.get<Position>(road.to);

        // Gather settlement names and market prices for hover tooltip
        std::string nA, nB;
        float fA = 0.f, wA = 0.f, dA = 0.f;   // food/water/wood at A
        float fB = 0.f, wB = 0.f, dB = 0.f;
        auto fillRoadEnd = [&](entt::entity e, std::string& nm,
                               float& food, float& water, float& wood) {
            if (const auto* s = m_registry.try_get<Settlement>(e)) nm = s->name;
            if (const auto* m = m_registry.try_get<Market>(e)) {
                food  = m->GetPrice(ResourceType::Food);
                water = m->GetPrice(ResourceType::Water);
                wood  = m->GetPrice(ResourceType::Wood);
            }
        };
        fillRoadEnd(road.from, nA, fA, wA, dA);
        fillRoadEnd(road.to,   nB, fB, wB, dB);
        // Look up inter-settlement relations for road tooltip
        float relAB = 0.f, relBA = 0.f;
        {
            const auto* sA = m_registry.try_get<Settlement>(road.from);
            const auto* sB = m_registry.try_get<Settlement>(road.to);
            if (sA) { auto it = sA->relations.find(road.to);   if (it != sA->relations.end()) relAB = it->second; }
            if (sB) { auto it = sB->relations.find(road.from); if (it != sB->relations.end()) relBA = it->second; }
        }
        int bCount = 0;
        { auto it = roadBanditCount.find(roadEntity); if (it != roadBanditCount.end()) bCount = it->second; }
        roads.push_back({ fp.x, fp.y, tp.x, tp.y, road.blocked, road.condition,
                          nA, nB, fA, wA, dA, fB, wB, dB, relAB, relBA, bCount });
    });

    // ---- Facilities ----
    // Count workers and average skill per facility for hover tooltip.
    // Workers are matched by home settlement + Working state.
    // skill is pre-aggregated into a (sum, count) pair per settlement + resource type.
    {
        // Build skill accum: [settlement][resIndex] = {sum, count}
        struct SA { float sum = 0.f; int count = 0; };
        std::map<entt::entity, std::array<SA, 3>> facSkillAccum;  // 0=Food,1=Water,2=Wood
        std::map<entt::entity, int> facWorkers;   // settlement → working count
        auto resIdx2 = [](ResourceType rt) -> int {
            switch (rt) {
                case ResourceType::Food:  return 0;
                case ResourceType::Water: return 1;
                case ResourceType::Wood:  return 2;
                default:                 return -1;
            }
        };
        m_registry.view<AgentState, HomeSettlement>().each(
            [&](auto e, const AgentState& as, const HomeSettlement& hs) {
            if (as.behavior != AgentBehavior::Working) return;
            facWorkers[hs.settlement]++;
            if (const auto* sk = m_registry.try_get<Skills>(e)) {
                auto& arr = facSkillAccum[hs.settlement];
                arr[0].sum += sk->farming;       arr[0].count++;
                arr[1].sum += sk->water_drawing; arr[1].count++;
                arr[2].sum += sk->woodcutting;   arr[2].count++;
            }
        });

        m_registry.view<Position, ProductionFacility>().each(
            [&](auto fe, const Position& pos, const ProductionFacility& fac) {
            if (fac.baseRate <= 0.f) return;   // skip shelter nodes

            int  workers  = facWorkers.count(fac.settlement) ? facWorkers.at(fac.settlement) : 0;
            float avgSkill = 0.5f;
            int ri = resIdx2(fac.output);
            if (ri >= 0 && facSkillAccum.count(fac.settlement)) {
                const auto& sa = facSkillAccum.at(fac.settlement)[ri];
                if (sa.count > 0) avgSkill = sa.sum / sa.count;
            }
            std::string sname;
            float facMorale = 0.5f;
            if (fac.settlement != entt::null && m_registry.valid(fac.settlement)) {
                if (const auto* s = m_registry.try_get<Settlement>(fac.settlement)) {
                    sname = s->name;
                    facMorale = s->morale;
                }
            }

            facilities.push_back({ pos.x, pos.y, fac.output, fac.baseRate, workers, avgSkill, sname, facMorale });
        });
    }

    // ---- World status + stockpile panel ----
    bool anyRoadBlocked = false;
    m_registry.view<Road>().each([&](const Road& r) {
        if (r.blocked) anyRoadBlocked = true;
    });

    // Population and price trend sampling — check every 3 and 5 game-days respectively.
    {
        auto tmv2 = m_registry.view<TimeManager>();
        if (tmv2.begin() != tmv2.end()) {
            int curDay  = tmv2.get<TimeManager>(*tmv2.begin()).day;
            int curHour = (int)tmv2.get<TimeManager>(*tmv2.begin()).hourOfDay;

            // Population snapshot (every 3 days) + milestone logging
            if (curDay >= m_popSampleDay + 3) {
                m_popSampleDay = curDay;
                auto lvm = m_registry.view<EventLog>();
                EventLog* popLog = (lvm.begin() == lvm.end()) ? nullptr
                                 : &lvm.get<EventLog>(*lvm.begin());
                m_registry.view<Settlement>().each(
                    [&](auto e, const Settlement& settl) {
                    int curPop = 0;
                    m_registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
                        [&](const HomeSettlement& hs) { if (hs.settlement == e) ++curPop; });
                    m_popPrev[e] = curPop;

                    // Append to population history ring buffer
                    auto& hist = m_popHistory[e];
                    hist.push_back(curPop);
                    if ((int)hist.size() > POPHISTORY_MAX)
                        hist.erase(hist.begin());

                    // Milestone notifications at 10, 20, 30, 40, 50, 60 pop
                    int lastMs = m_popMilestone.count(e) ? m_popMilestone[e] : 0;
                    for (int ms : {10, 20, 30, 40, 50, 60}) {
                        if (curPop >= ms && lastMs < ms) {
                            m_popMilestone[e] = ms;
                            if (popLog) {
                                char buf[80];
                                std::snprintf(buf, sizeof(buf),
                                    "%s reaches %d citizens!", settl.name.c_str(), ms);
                                popLog->Push(curDay, curHour, buf);
                            }
                        }
                    }
                });
            }

            // Price snapshot (every 5 days)
            if (curDay >= m_priceSampleDay + 5) {
                m_priceSampleDay = curDay;
                m_registry.view<Market>().each(
                    [&](auto e, const Market& mkt) {
                    for (const auto& [res, price] : mkt.price)
                        m_pricePrev[e][res] = price;
                });
            }
        }
    }

    // Hauler count per settlement (for status bar and panel)
    std::map<entt::entity, int> haulerCount2;
    m_registry.view<Hauler, HomeSettlement>().each(
        [&](const Hauler&, const HomeSettlement& hs) {
        ++haulerCount2[hs.settlement];
    });

    // Current per-settlement worker and idle counts
    std::map<entt::entity, int> workerCount, idleCount;
    m_registry.view<AgentState, HomeSettlement>(entt::exclude<Hauler, PlayerTag>).each(
        [&](auto, const AgentState& as, const HomeSettlement& hs) {
        if (as.behavior == AgentBehavior::Working) ++workerCount[hs.settlement];
        else if (as.behavior == AgentBehavior::Idle) ++idleCount[hs.settlement];
    });

    // Season modifier for production rate estimate
    Season curSeason = Season::Spring;
    float  curSeasonMod = 1.f;
    float  curHeatMult  = 0.f;
    {
        auto tmv3 = m_registry.view<TimeManager>();
        if (tmv3.begin() != tmv3.end()) {
            curSeason    = tmv3.get<TimeManager>(*tmv3.begin()).CurrentSeason();
            curSeasonMod = SeasonProductionModifier(curSeason);
            curHeatMult  = SeasonHeatDrainMult(curSeason);
        }
    }

    static constexpr float BASE_WORKERS_EST = 5.f;
    static constexpr float FOOD_RATE_EST    = 0.5f;  // food per NPC per game-hour
    static constexpr float WATER_RATE_EST   = 0.8f;  // water per NPC per game-hour
    static constexpr float WOOD_HEAT_EST    = 0.03f; // wood per NPC per game-hour at full heat demand

    m_registry.view<Position, Settlement, Stockpile>().each(
        [&](auto e, const Position&, const Settlement& s, const Stockpile& sp) {
        float food  = sp.quantities.count(ResourceType::Food)
                      ? sp.quantities.at(ResourceType::Food)  : 0.f;
        float water = sp.quantities.count(ResourceType::Water)
                      ? sp.quantities.at(ResourceType::Water) : 0.f;
        float wood  = sp.quantities.count(ResourceType::Wood)
                      ? sp.quantities.at(ResourceType::Wood)  : 0.f;

        // Market prices (default 1.0 if no market component)
        float foodPrice = 1.f, waterPrice = 1.f, woodPrice = 1.f;
        if (const auto* mkt = m_registry.try_get<Market>(e)) {
            foodPrice  = mkt->GetPrice(ResourceType::Food);
            waterPrice = mkt->GetPrice(ResourceType::Water);
            woodPrice  = mkt->GetPrice(ResourceType::Wood);
        }

        int pop = 0, childPop = 0, elderPop = 0;
        m_registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
            [&](auto ne, const HomeSettlement& hs) {
            if (hs.settlement != e) return;
            ++pop;
            if (m_registry.all_of<ChildTag>(ne)) ++childPop;
            if (const auto* ag = m_registry.try_get<Age>(ne))
                if (ag->days > 60.f) ++elderPop;
        });
        int hCount = haulerCount2.count(e) ? haulerCount2.at(e) : 0;

        // Population trend ('+', '=', '-')
        char popTrend = '=';
        auto prevIt = m_popPrev.find(e);
        if (prevIt != m_popPrev.end()) {
            if (pop > prevIt->second)      popTrend = '+';
            else if (pop < prevIt->second) popTrend = '-';
        }

        // Price trends
        char foodPriceTrend = '=', waterPriceTrend = '=', woodPriceTrend = '=';
        auto pIt = m_pricePrev.find(e);
        if (pIt != m_pricePrev.end()) {
            auto getTrend = [&](ResourceType rt, float cur) -> char {
                auto it2 = pIt->second.find(rt);
                if (it2 == pIt->second.end()) return '=';
                return (cur > it2->second * 1.05f) ? '+' :
                       (cur < it2->second * 0.95f) ? '-' : '=';
            };
            foodPriceTrend  = getTrend(ResourceType::Food,  foodPrice);
            waterPriceTrend = getTrend(ResourceType::Water, waterPrice);
            woodPriceTrend  = getTrend(ResourceType::Wood,  woodPrice);
        }

        bool hungerCrisis = false;
        m_registry.view<HomeSettlement, Needs>(entt::exclude<PlayerTag, Hauler>).each(
            [&](const HomeSettlement& hs, const Needs& nd) {
            if (hs.settlement == e && nd.list[0].value < 0.15f) hungerCrisis = true;
        });

        float elderBonus = std::min(0.05f, elderPop * 0.005f);

        // Average contentment of homed NPCs (same exclusions as hunger crisis)
        float contentSum = 0.f;
        int   contentN   = 0;
        m_registry.view<HomeSettlement, Needs>(entt::exclude<PlayerTag, Hauler>).each(
            [&](const HomeSettlement& hs, const Needs& nd) {
            if (hs.settlement != e) return;
            float avg = 0.f;
            for (int i = 0; i < 4; ++i) avg += nd.list[i].value;
            avg *= 0.25f;
            contentSum += avg;
            ++contentN;
        });
        float avgContentment = (contentN > 0) ? contentSum / contentN : 1.f;

        // Sum pending estates: elder (age > 60) balances * 0.8 inheritance fraction
        float pendingEstates = 0.f;
        m_registry.view<HomeSettlement, Age, Money>(entt::exclude<PlayerTag, Hauler>).each(
            [&](const HomeSettlement& hs, const Age& age, const Money& m) {
                if (hs.settlement == e && age.days > 60.f)
                    pendingEstates += m.balance * 0.8f;
            });

        // Estimate gross production rates per resource for this settlement
        float foodRate2 = 0.f, waterRate2 = 0.f, woodRate2 = 0.f;
        {
            int wk = workerCount.count(e) ? workerCount.at(e) : 0;
            float ws = std::min(2.f, std::max(0.1f, wk / BASE_WORKERS_EST));
            float sm = s.productionModifier * curSeasonMod;
            m_registry.view<ProductionFacility>().each(
                [&](const ProductionFacility& fac) {
                if (fac.settlement != e || fac.baseRate <= 0.f) return;
                float r = fac.baseRate * ws * sm;
                switch (fac.output) {
                    case ResourceType::Food:  foodRate2  += r; break;
                    case ResourceType::Water: waterRate2 += r; break;
                    case ResourceType::Wood:  woodRate2  += r; break;
                }
            });
        }

        // Count fatigued workers
        int fatiguedWorkers = 0;
        int recentGivers = 0;
        m_registry.view<HomeSettlement, Schedule, AgentState>(entt::exclude<PlayerTag, Hauler>).each(
            [&](const HomeSettlement& hs, const Schedule& sc, const AgentState& as) {
                if (hs.settlement == e && sc.fatigued && as.behavior == AgentBehavior::Working)
                    ++fatiguedWorkers;
            });
        m_registry.view<HomeSettlement, DeprivationTimer>(entt::exclude<PlayerTag, Hauler>).each(
            [&](const HomeSettlement& hs, const DeprivationTimer& dt) {
                if (hs.settlement == e && dt.charityTimer > 0.f)
                    ++recentGivers;
            });

        worldStatus.push_back({ s.name, food, water, wood,
                                 foodPrice, waterPrice, woodPrice,
                                 pop, hCount, childPop, s.treasury,
                                 s.modifierDuration > 0.f, s.modifierName,
                                 popTrend, foodPriceTrend, waterPriceTrend, woodPriceTrend,
                                 hungerCrisis, elderPop, elderBonus, s.morale,
                                 avgContentment, pendingEstates,
                                 foodRate2, waterRate2, woodRate2, fatiguedWorkers,
                                 recentGivers });

        // Stockpile panel for selected settlement
        if (e == m_selectedSettlement) {
            // Estimate per-resource net flow rate (game-hours)
            // Production: sum up facility output rates (adjusted for workers and season)
            std::map<ResourceType, float> prodRate, consRate;
            int workers = workerCount.count(e) ? workerCount.at(e) : 0;
            float wScale = std::min(2.f, std::max(0.1f, workers / BASE_WORKERS_EST));
            float smod   = s.productionModifier * curSeasonMod;

            m_registry.view<ProductionFacility>().each(
                [&](const ProductionFacility& fac) {
                if (fac.settlement != e || fac.baseRate <= 0.f) return;
                float rate = fac.baseRate * wScale * smod;
                prodRate[fac.output] += rate;
                // Account for input consumption
                for (const auto& [inRes, inPerOut] : fac.inputsPerOutput)
                    consRate[inRes] += inPerOut * rate;
            });

            // NPC food/water consumption
            consRate[ResourceType::Food]  += pop * FOOD_RATE_EST;
            consRate[ResourceType::Water] += pop * WATER_RATE_EST;
            // Wood heat consumption (seasonal)
            if (curHeatMult > 0.f)
                consRate[ResourceType::Wood] += pop * WOOD_HEAT_EST * curHeatMult;

            std::map<ResourceType, float> netRate;
            for (const auto& [res, pr] : prodRate)
                netRate[res] += pr;
            for (const auto& [res, cr] : consRate)
                netRate[res] -= cr;

            // Settlement stability score (0-1): composite of NPC health, stocks, treasury, trend
            // -- NPC need satisfaction (average across residents)
            float needSum = 0.f; int needCount = 0;
            m_registry.view<Needs, HomeSettlement>(entt::exclude<Hauler, PlayerTag>).each(
                [&](const Needs& n, const HomeSettlement& hs) {
                if (hs.settlement != e) return;
                for (const auto& need : n.list) needSum += need.value;
                needCount += 4;
            });
            float needStability = (needCount > 0) ? (needSum / needCount) : 0.f;

            // -- Stockpile level (fraction of "safe" level = 30 units per NPC)
            float safeLevel = std::max(1.f, (float)pop * 30.f);
            float foodFrac  = std::min(1.f, food  / safeLevel);
            float waterFrac = std::min(1.f, water / safeLevel);
            float woodFrac  = (curHeatMult > 0.f) ? std::min(1.f, wood / (safeLevel * 0.5f)) : 1.f;
            float stockStability = (foodFrac + waterFrac + woodFrac) / 3.f;

            // -- Treasury health (0=empty, 1=200g+)
            float tresStability = std::min(1.f, s.treasury / 200.f);

            // -- Population trend (growing=+0.15, stable=0, declining=-0.15)
            float trendBonus = (popTrend == '+') ? 0.15f : (popTrend == '-') ? -0.15f : 0.f;

            float stability = std::max(0.f, std::min(1.f,
                0.35f * needStability + 0.35f * stockStability + 0.15f * tresStability
                + 0.15f + trendBonus));  // base 0.15 so a healthy empty settlement starts positive

            // Filter event log for events mentioning this settlement (last 5)
            std::vector<EventLog::Entry> filteredEvents;
            {
                auto lv2 = m_registry.view<EventLog>();
                if (lv2.begin() != lv2.end()) {
                    const auto& elog = lv2.get<EventLog>(*lv2.begin());
                    for (const auto& entry : elog.entries) {
                        if (entry.message.find(s.name) != std::string::npos)
                            filteredEvents.push_back(entry);
                        if ((int)filteredEvents.size() >= 5) break;
                    }
                }
            }

            panel.open            = true;
            panel.name            = s.name;
            panel.quantities      = sp.quantities;
            panel.netRatePerHour  = netRate;
            panel.prodRatePerHour = prodRate;
            panel.consRatePerHour = consRate;
            panel.treasury         = s.treasury;
            panel.pop              = pop;
            panel.childCount       = childPop;
            panel.popCap           = s.popCap;
            panel.stability        = stability;
            panel.morale           = s.morale;
            panel.workers          = workers;
            panel.idle             = idleCount.count(e) ? idleCount.at(e) : 0;
            panel.theftCount       = s.theftCount;
            panel.modifierName     = s.modifierName;
            panel.modifierHoursLeft = s.modifierDuration;
            // Infer specialty from primary facility (same logic as SettlementEntry)
            {
                ResourceType primary = ResourceType::Food;
                float maxRate = 0.f;
                m_registry.view<ProductionFacility>().each(
                    [&](const ProductionFacility& pf) {
                    if (pf.settlement == e && pf.baseRate > maxRate) {
                        maxRate = pf.baseRate;
                        primary = pf.output;
                    }
                });
                if (maxRate > 0.f)
                    panel.specialty = (primary == ResourceType::Food)  ? "Farming" :
                                     (primary == ResourceType::Water) ? "Water"   : "Lumber";
            }
            panel.recentEvents    = std::move(filteredEvents);
            if (const auto* mkt = m_registry.try_get<Market>(e))
                panel.prices = mkt->price;
            // Population history sparkline
            if (m_popHistory.count(e))
                panel.popHistory = m_popHistory.at(e);

            // Residents list — homed NPCs sorted by balance descending, max 12
            panel.residents.clear();
            float eldestAge = -1.f;
            std::string eldestName;
            m_registry.view<Name, HomeSettlement, Money>(entt::exclude<PlayerTag, Hauler>).each(
                [&](auto npc, const Name& nm, const HomeSettlement& hs, const Money& mn) {
                    if (hs.settlement != e) return;
                    RenderSnapshot::StockpilePanel::AgentInfo ai;
                    ai.name    = nm.value;
                    ai.balance = mn.balance;
                    if (const auto* pr = m_registry.try_get<Profession>(npc))
                        ai.profession = ProfessionLabel(pr->type);
                    if (const auto* ft = m_registry.try_get<FamilyTag>(npc))
                        ai.familyName = ft->name;
                    if (const auto* needs = m_registry.try_get<Needs>(npc)) {
                        float h = needs->list[0].value, t = needs->list[1].value;
                        float e2 = needs->list[2].value, ht = needs->list[3].value;
                        ai.contentment = h * 0.3f + t * 0.3f + e2 * 0.2f + ht * 0.2f;
                    }
                    if (const auto* age = m_registry.try_get<Age>(npc)) {
                        if (age->days > eldestAge) {
                            eldestAge  = age->days;
                            eldestName = nm.value;
                        }
                    }
                    panel.residents.push_back(std::move(ai));
                });
            std::sort(panel.residents.begin(), panel.residents.end(),
                [](const auto& a, const auto& b) { return a.balance > b.balance; });
            if (panel.residents.size() > 12)
                panel.residents.resize(12);
            // Mark the eldest resident (if still in the truncated list)
            if (!eldestName.empty()) {
                for (auto& r : panel.residents) {
                    if (r.name == eldestName) { r.isEldest = true; break; }
                }
            }
        }
    });

    // ---- HUD data ----
    int    day = 1, hour = 6, minute = 0, tickSpeed = 1, deaths = 0;
    bool   paused = false;
    float  hourOfDay = 6.f;
    Season season    = Season::Spring;
    float  temperature = 10.f;

    {
        auto tmv = m_registry.view<TimeManager>();
        if (tmv.begin() != tmv.end()) {
            const auto& tm = tmv.get<TimeManager>(*tmv.begin());
            day         = tm.day;
            hour        = (int)tm.hourOfDay;
            minute      = (int)((tm.hourOfDay - hour) * 60.f);
            hourOfDay   = tm.hourOfDay;
            tickSpeed   = tm.tickSpeed;
            paused      = tm.paused;
            season      = tm.CurrentSeason();
            temperature = AmbientTemperature(season, hourOfDay);
        }
    }

    int pop = 0;
    m_registry.view<Needs>().each([&](auto e, const Needs&) {
        if (!m_registry.all_of<PlayerTag>(e)) ++pop;
    });
    deaths = m_deathSystem.totalDeaths;

    // ---- Player HUD ----
    bool          playerAlive    = false;
    float         hungerPct = 1.f, thirstPct = 1.f, energyPct = 1.f, heatPct = 1.f;
    float         hungerCrit = 0.3f, thirstCrit = 0.3f, energyCrit = 0.3f, heatCrit = 0.3f;
    AgentBehavior playerBehavior = AgentBehavior::Idle;
    float         playerWX = 640.f, playerWY = 360.f;

    float playerAgeDays = 0.f, playerMaxDays = 80.f;
    float playerGold = 0.f;
    float playerFarmSkill = -1.f, playerWaterSkill = -1.f, playerWoodSkill = -1.f;
    std::map<ResourceType, int> playerInventory;
    int   playerInventoryCapacity = 15;
    {
        auto pv = m_registry.view<PlayerTag, Position, Needs, AgentState>();
        if (pv.begin() != pv.end()) {
            auto pe = *pv.begin();
            playerAlive    = true;
            const auto& needs = pv.get<Needs>(pe);
            hungerPct  = needs.list[0].value;  hungerCrit  = needs.list[0].criticalThreshold;
            thirstPct  = needs.list[1].value;  thirstCrit  = needs.list[1].criticalThreshold;
            energyPct  = needs.list[2].value;  energyCrit  = needs.list[2].criticalThreshold;
            heatPct    = needs.list[3].value;  heatCrit    = needs.list[3].criticalThreshold;
            playerBehavior = pv.get<AgentState>(pe).behavior;
            const auto& ppos = pv.get<Position>(pe);
            playerWX = ppos.x; playerWY = ppos.y;
            if (const auto* age = m_registry.try_get<Age>(pe)) {
                playerAgeDays = age->days;
                playerMaxDays = age->maxDays;
            }
            if (const auto* mon = m_registry.try_get<Money>(pe))
                playerGold = mon->balance;
            if (const auto* inv = m_registry.try_get<Inventory>(pe)) {
                playerInventory         = inv->contents;
                playerInventoryCapacity = inv->maxCapacity;
            }
            if (const auto* sk = m_registry.try_get<Skills>(pe)) {
                playerFarmSkill  = sk->farming;
                playerWaterSkill = sk->water_drawing;
                playerWoodSkill  = sk->woodcutting;
            }
        }
    }

    // ---- Plague zone detection ----
    // Player is considered "in a plague zone" if they're within the radius of any
    // settlement currently afflicted by Plague. This flag causes extra need drain
    // in NeedDrainSystem and shows a warning in the HUD.
    bool playerInPlagueZone = false;
    m_registry.view<Position, Settlement>().each(
        [&](const Position& sp, const Settlement& s) {
        if (s.modifierName != "Plague") return;
        float dx = playerWX - sp.x, dy = playerWY - sp.y;
        if (dx*dx + dy*dy <= s.radius * s.radius)
            playerInPlagueZone = true;
    });

    // ---- Trade opportunity hint ----
    // If the player is near a settlement (within BUY_RADIUS), shows the best trade
    // route *from that settlement* — what to pick up here and where to sell it.
    // If the player is not near any settlement, falls back to the global best margin.
    std::string tradeHint;
    {
        static const ResourceType kTradeRes[] = {
            ResourceType::Food, ResourceType::Water, ResourceType::Wood };
        static const char* kTradeNames[] = { "Food", "Water", "Wood" };
        static constexpr float HINT_RADIUS    = 200.f; // wider than BUY_RADIUS for early warning
        static constexpr float MIN_MARGIN     = 2.0f;

        // Effective tax rate after reputation discount (matches player sell code)
        static constexpr float TRADE_TAX_BASE_H = 0.20f;
        float repDiscount_h = std::min(0.10f, m_playerReputation * 0.0005f);
        float effectiveTax_h = TRADE_TAX_BASE_H - repDiscount_h;

        struct HintResult {
            int   resIdx   = -1;
            float buyPrice = 0.f, sellPrice = 0.f, netPerUnit = 0.f;
            std::string buyName, sellName;
        };

        auto computeHint = [&](entt::entity fromSettl) -> HintResult {
            HintResult best;
            const auto* fromMkt = m_registry.try_get<Market>(fromSettl);
            if (!fromMkt) return best;
            const auto* fromName = m_registry.try_get<Settlement>(fromSettl);
            for (int ri = 0; ri < 3; ++ri) {
                ResourceType res = kTradeRes[ri];
                float buyP = fromMkt->GetPrice(res);
                // Find settlement that pays the most for this resource
                float    bestSell = 0.f;
                std::string bestSellName;
                m_registry.view<Settlement, Market>().each(
                    [&](auto se, const Settlement& s, const Market& mkt) {
                        if (se == fromSettl) return;
                        float p = mkt.GetPrice(res);
                        if (p > bestSell) { bestSell = p; bestSellName = s.name; }
                    });
                // Real net per unit: sell revenue after tax, minus buy cost
                float net = bestSell * (1.f - effectiveTax_h) - buyP;
                if (net > best.netPerUnit && net > MIN_MARGIN * (1.f - effectiveTax_h)) {
                    best.resIdx    = ri;
                    best.buyPrice  = buyP;
                    best.sellPrice = bestSell;
                    best.netPerUnit= net;
                    best.buyName   = fromName ? fromName->name : "?";
                    best.sellName  = bestSellName;
                }
            }
            return best;
        };

        // Find nearest settlement to player
        entt::entity nearSettl = entt::null;
        float nearDist = HINT_RADIUS * HINT_RADIUS;
        m_registry.view<Position, Settlement>().each(
            [&](auto e, const Position& sp, const Settlement&) {
                float dx = sp.x - playerWX, dy = sp.y - playerWY;
                float d  = dx*dx + dy*dy;
                if (d < nearDist) { nearDist = d; nearSettl = e; }
            });

        HintResult result;
        if (nearSettl != entt::null) {
            result = computeHint(nearSettl);
        }

        // Fallback: global best if no nearby settlement or net is too low
        if (result.resIdx < 0) {
            struct PriceRecord { float price; std::string name; };
            for (int ri = 0; ri < 3; ++ri) {
                ResourceType res = kTradeRes[ri];
                PriceRecord lo{ 9999.f, "" }, hi{ 0.f, "" };
                m_registry.view<Settlement, Market>().each(
                    [&](auto, const Settlement& s, const Market& mkt) {
                        float p = mkt.GetPrice(res);
                        if (p < lo.price) lo = { p, s.name };
                        if (p > hi.price) hi = { p, s.name };
                    });
                if (lo.name.empty() || hi.name.empty()) continue;
                float net = hi.price * (1.f - effectiveTax_h) - lo.price;
                if (net > result.netPerUnit && net > MIN_MARGIN) {
                    result = { ri, lo.price, hi.price, net, lo.name, hi.name };
                }
            }
        }

        if (result.resIdx >= 0) {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "%s: buy %s %.1fg → sell %s %.1fg (net +%.1fg/unit)",
                kTradeNames[result.resIdx],
                result.buyName.c_str(),  result.buyPrice,
                result.sellName.c_str(), result.sellPrice,
                result.netPerUnit);
            tradeHint = buf;
        }
    }

    // ---- Economy-wide statistics ----
    float econTotalGold     = 0.f;
    float econAvgNpcWealth  = 0.f;
    float econRichestWealth = 0.f;
    std::string econRichestName;
    int   econHaulerCount   = 0;
    {
        float npcGoldSum = 0.f;
        int   npcCount   = 0;

        // NPC + hauler balances
        m_registry.view<Money>(entt::exclude<PlayerTag>).each(
            [&](auto e, const Money& mon) {
                econTotalGold += mon.balance;
                bool isHauler = m_registry.all_of<Hauler>(e);
                if (isHauler) {
                    ++econHaulerCount;
                } else {
                    npcGoldSum += mon.balance;
                    ++npcCount;
                    if (mon.balance > econRichestWealth) {
                        econRichestWealth = mon.balance;
                        if (const auto* n = m_registry.try_get<Name>(e))
                            econRichestName = n->value;
                        else
                            econRichestName = "?";
                    }
                }
            });

        // Player balance
        m_registry.view<PlayerTag, Money>().each([&](auto, const Money& mon) {
            econTotalGold += mon.balance;
        });

        // Settlement treasuries
        m_registry.view<Settlement>().each([&](const Settlement& s) {
            econTotalGold += s.treasury;
        });

        econAvgNpcWealth = (npcCount > 0) ? npcGoldSum / npcCount : 0.f;
    }

    // ---- Event log ----
    std::vector<EventLog::Entry> logEntries;
    {
        auto lv = m_registry.view<EventLog>();
        if (lv.begin() != lv.end())
            logEntries = std::vector<EventLog::Entry>(
                m_registry.get<EventLog>(*lv.begin()).entries.begin(),
                m_registry.get<EventLog>(*lv.begin()).entries.end());
    }

    // ---- Swap into shared snapshot under lock ----
    {
        std::lock_guard<std::mutex> lock(m_snapshot.mutex);
        m_snapshot.agents       = std::move(agents);
        m_snapshot.settlements  = std::move(settlements);
        m_snapshot.roads        = std::move(roads);
        m_snapshot.facilities   = std::move(facilities);
        m_snapshot.worldStatus  = std::move(worldStatus);
        m_snapshot.stockpilePanel = std::move(panel);
        m_snapshot.day          = day;
        m_snapshot.hour         = hour;
        m_snapshot.minute       = minute;
        m_snapshot.hourOfDay    = hourOfDay;
        m_snapshot.season       = season;
        m_snapshot.temperature  = temperature;
        m_snapshot.tickSpeed    = tickSpeed;
        m_snapshot.paused       = paused;
        m_snapshot.population   = pop;
        m_snapshot.totalDeaths  = deaths;
        m_snapshot.roadBlocked  = anyRoadBlocked;
        m_snapshot.playerAlive  = playerAlive;
        m_snapshot.hungerPct    = hungerPct;
        m_snapshot.thirstPct    = thirstPct;
        m_snapshot.energyPct    = energyPct;
        m_snapshot.heatPct      = heatPct;
        m_snapshot.hungerCrit   = hungerCrit;
        m_snapshot.thirstCrit   = thirstCrit;
        m_snapshot.energyCrit   = energyCrit;
        m_snapshot.heatCrit     = heatCrit;
        m_snapshot.playerBehavior = playerBehavior;
        m_snapshot.playerWorldX  = playerWX;
        m_snapshot.playerWorldY  = playerWY;
        m_snapshot.playerAgeDays  = playerAgeDays;
        m_snapshot.playerMaxDays  = playerMaxDays;
        m_snapshot.playerGold       = playerGold;
        m_snapshot.playerFarmSkill  = playerFarmSkill;
        m_snapshot.playerWaterSkill = playerWaterSkill;
        m_snapshot.playerWoodSkill  = playerWoodSkill;
        m_snapshot.playerInventory         = std::move(playerInventory);
        m_snapshot.playerInventoryCapacity = playerInventoryCapacity;
        m_snapshot.tradeHint               = std::move(tradeHint);
        m_snapshot.playerInPlagueZone      = playerInPlagueZone;
        m_snapshot.playerReputation        = m_playerReputation;
        // Reputation rank titles
        m_snapshot.playerRank =
            (m_playerReputation >= 200) ? "Legend" :
            (m_playerReputation >= 100) ? "Magnate" :
            (m_playerReputation >=  50) ? "Notable" :
            (m_playerReputation >=  20) ? "Merchant" :
            (m_playerReputation >=   5) ? "Traveler" : "Stranger";
        m_snapshot.tradeLedger             = m_tradeLedger;
        m_snapshot.logEntries    = std::move(logEntries);
        m_snapshot.econTotalGold     = econTotalGold;
        m_snapshot.econAvgNpcWealth  = econAvgNpcWealth;
        m_snapshot.econRichestWealth = econRichestWealth;
        m_snapshot.econRichestName   = std::move(econRichestName);
        m_snapshot.econHaulerCount   = econHaulerCount;
        m_snapshot.simStepsPerSec = m_stepsLastSec;
        m_snapshot.totalEntities  = (int)m_registry.storage<entt::entity>().size();
    }
}
