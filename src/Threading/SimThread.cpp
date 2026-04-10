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
    m_registry.emplace<Inventory>(player);
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
    m_deathSystem.Update(m_registry, dt);
    m_birthSystem.Update(m_registry, dt);
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

                // If carrying goods → sell them here (same 20% trade tax as haulers)
                if (inv.TotalItems() > 0 && sp2 && mkt2) {
                    static constexpr float TRADE_TAX = 0.20f;
                    float earned = 0.f;
                    float taxTotal = 0.f;
                    for (auto& [type, qty] : inv.contents) {
                        if (qty <= 0) continue;
                        float price = mkt2->GetPrice(type);
                        sp2->quantities[type] += qty;
                        float gross = price * qty;
                        float tax   = gross * TRADE_TAX;
                        earned   += gross - tax;
                        taxTotal += tax;
                    }
                    mon.balance += earned;
                    if (auto* destSettl2 = m_registry.try_get<Settlement>(nearSettl))
                        destSettl2->treasury += taxTotal;
                    inv.contents.clear();
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
                        if (log2) log2->Push(day2, hr2,
                            "Bought " + std::to_string(bestQty) + " goods at "
                            + settl.name + " for " + std::to_string((int)cost) + "g");
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
        RenderSnapshot::AgentRole role = isPlayer ? RenderSnapshot::AgentRole::Player
                                       : isHauler ? RenderSnapshot::AgentRole::Hauler
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
        std::string npcName;
        if (const auto* n = m_registry.try_get<Name>(e))
            npcName = n->value;

        // Trade route destination for haulers en route (makes trade flow visible)
        bool  hasRouteDest = false;
        float routeDestX = 0.f, routeDestY = 0.f;
        if (isHauler) {
            if (const auto* h = m_registry.try_get<Hauler>(e)) {
                if (h->state == HaulerState::GoingToDeposit &&
                    h->targetSettlement != entt::null &&
                    m_registry.valid(h->targetSettlement)) {
                    const auto& dp = m_registry.get<Position>(h->targetSettlement);
                    hasRouteDest = true;
                    routeDestX   = dp.x;
                    routeDestY   = dp.y;
                }
            }
        }

        agents.push_back({ pos.x, pos.y, rend.size,
                           drawColor, ring, hasCargo, cargoColor,
                           role, hp, tp, ep, htp, astate.behavior,
                           balance, ageDays, maxDays, npcName,
                           hasRouteDest, routeDestX, routeDestY });
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
        settlements.push_back({
            pos.x, pos.y, s.radius, s.name,
            (e == m_selectedSettlement),
            static_cast<uint32_t>(e),
            food, water, wood, spop, snapSeason
        });
    });

    // ---- Roads ----
    m_registry.view<Road>().each([&](const Road& road) {
        if (road.from == entt::null || road.to == entt::null) return;
        if (!m_registry.valid(road.from) || !m_registry.valid(road.to)) return;
        const auto& fp = m_registry.get<Position>(road.from);
        const auto& tp = m_registry.get<Position>(road.to);
        roads.push_back({ fp.x, fp.y, tp.x, tp.y, road.blocked });
    });

    // ---- Facilities ----
    m_registry.view<Position, ProductionFacility>().each(
        [&](const Position& pos, const ProductionFacility& fac) {
        facilities.push_back({ pos.x, pos.y, fac.output });
    });

    // ---- World status + stockpile panel ----
    bool anyRoadBlocked = false;
    m_registry.view<Road>().each([&](const Road& r) {
        if (r.blocked) anyRoadBlocked = true;
    });

    // Population trend sampling: every 3 game-days, snapshot per-settlement pop.
    // Compare current pop to the previous snapshot to derive ↑/=/↓ trend.
    {
        auto tmv2 = m_registry.view<TimeManager>();
        if (tmv2.begin() != tmv2.end()) {
            int curDay = tmv2.get<TimeManager>(*tmv2.begin()).day;
            if (curDay >= m_popSampleDay + 3) {
                // Time to refresh population snapshots
                m_popSampleDay = curDay;
                m_registry.view<Position, Settlement>().each(
                    [&](auto e, const Position&, const Settlement&) {
                    int curPop = 0;
                    m_registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
                        [&](const HomeSettlement& hs) { if (hs.settlement == e) ++curPop; });
                    m_popPrev[e] = curPop;
                });
            }
        }
    }

    // Current per-settlement worker counts (needed for net rate estimate)
    std::map<entt::entity, int> workerCount;
    m_registry.view<AgentState, HomeSettlement>(entt::exclude<Hauler, PlayerTag>).each(
        [&](auto, const AgentState& as, const HomeSettlement& hs) {
        if (as.behavior == AgentBehavior::Working) ++workerCount[hs.settlement];
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

        int pop = 0;
        m_registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
            [&](const HomeSettlement& hs) { if (hs.settlement == e) ++pop; });

        // Population trend ('+', '=', '-')
        char trend = '=';
        auto prevIt = m_popPrev.find(e);
        if (prevIt != m_popPrev.end()) {
            if (pop > prevIt->second)      trend = '+';
            else if (pop < prevIt->second) trend = '-';
        }

        worldStatus.push_back({ s.name, food, water, wood,
                                 foodPrice, waterPrice, woodPrice, pop, s.treasury,
                                 s.modifierDuration > 0.f, s.modifierName, trend });

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

            panel.open            = true;
            panel.name            = s.name;
            panel.quantities      = sp.quantities;
            panel.netRatePerHour  = netRate;
            panel.treasury        = s.treasury;
            panel.pop             = pop;
            if (const auto* mkt = m_registry.try_get<Market>(e))
                panel.prices = mkt->price;
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
    std::map<ResourceType, int> playerInventory;
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
            if (const auto* inv = m_registry.try_get<Inventory>(pe))
                playerInventory = inv->contents;
        }
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
        m_snapshot.playerGold     = playerGold;
        m_snapshot.playerInventory = std::move(playerInventory);
        m_snapshot.logEntries    = std::move(logEntries);
        m_snapshot.simStepsPerSec = m_stepsLastSec;
        m_snapshot.totalEntities  = (int)m_registry.storage<entt::entity>().size();
    }
}
