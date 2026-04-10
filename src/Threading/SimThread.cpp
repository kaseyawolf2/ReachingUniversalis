#include "SimThread.h"
#include "ECS/Components.h"
#include "World/WorldGenerator.h"
#include <cmath>
#include <chrono>
#include <algorithm>

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

    // Continuous: player movement (velocity is set, MovementSystem applies it)
    {
        float mx = m_input.playerMoveX.load();
        float my = m_input.playerMoveY.load();
        auto pv = m_registry.view<PlayerTag, Velocity, MoveSpeed>();
        if (pv.begin() != pv.end()) {
            auto pe   = *pv.begin();
            auto& vel = pv.get<Velocity>(pe);
            float spd = pv.get<MoveSpeed>(pe).value;
            vel.vx = mx * spd;
            vel.vy = my * spd;
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
        float hp = 1.f, tp = 1.f, ep = 1.f;

        if (const auto* needs = m_registry.try_get<Needs>(e)) {
            hp = needs->list[0].value;
            tp = needs->list[1].value;
            ep = needs->list[2].value;
            if (!isPlayer) {
                float worst = std::min({hp, tp, ep});
                if      (worst < 0.15f) drawColor = RED;
                else if (worst < 0.30f) drawColor = ORANGE;
                else if (worst < 0.55f) drawColor = YELLOW;
                else                    drawColor = isHauler ? SKYBLUE : WHITE;
            }
        }

        Color ring = isPlayer ? WHITE : (isHauler ? DARKBLUE : DARKGRAY);

        bool  hasCargo   = false;
        Color cargoColor = WHITE;
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

        agents.push_back({ pos.x, pos.y, rend.size,
                           drawColor, ring, hasCargo, cargoColor,
                           role, hp, tp, ep, astate.behavior });
    });

    // ---- Settlements ----
    m_registry.view<Position, Settlement>().each(
        [&](auto e, const Position& pos, const Settlement& s) {
        settlements.push_back({
            pos.x, pos.y, s.radius, s.name,
            (e == m_selectedSettlement),
            static_cast<uint32_t>(e)
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

    m_registry.view<Position, Settlement, Stockpile>().each(
        [&](auto e, const Position&, const Settlement& s, const Stockpile& sp) {
        float food  = sp.quantities.count(ResourceType::Food)
                      ? sp.quantities.at(ResourceType::Food)  : 0.f;
        float water = sp.quantities.count(ResourceType::Water)
                      ? sp.quantities.at(ResourceType::Water) : 0.f;
        int pop = 0;
        m_registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
            [&](const HomeSettlement& hs) { if (hs.settlement == e) ++pop; });
        worldStatus.push_back({ s.name, food, water, pop });

        // Stockpile panel for selected settlement
        if (e == m_selectedSettlement) {
            panel.open       = true;
            panel.name       = s.name;
            panel.quantities = sp.quantities;
        }
    });

    // ---- HUD data ----
    int  day = 1, hour = 6, minute = 0, tickSpeed = 1, deaths = 0;
    bool paused = false;
    float hourOfDay = 6.f;

    {
        auto tmv = m_registry.view<TimeManager>();
        if (tmv.begin() != tmv.end()) {
            const auto& tm = tmv.get<TimeManager>(*tmv.begin());
            day       = tm.day;
            hour      = (int)tm.hourOfDay;
            minute    = (int)((tm.hourOfDay - hour) * 60.f);
            hourOfDay = tm.hourOfDay;
            tickSpeed = tm.tickSpeed;
            paused    = tm.paused;
        }
    }

    int pop = 0;
    m_registry.view<Needs>().each([&](auto e, const Needs&) {
        if (!m_registry.all_of<PlayerTag>(e)) ++pop;
    });
    deaths = m_deathSystem.totalDeaths;

    // ---- Player HUD ----
    bool          playerAlive    = false;
    float         hungerPct = 1.f, thirstPct = 1.f, energyPct = 1.f;
    float         hungerCrit = 0.3f, thirstCrit = 0.3f, energyCrit = 0.3f;
    AgentBehavior playerBehavior = AgentBehavior::Idle;
    float         playerWX = 640.f, playerWY = 360.f;

    {
        auto pv = m_registry.view<PlayerTag, Position, Needs, AgentState>();
        if (pv.begin() != pv.end()) {
            auto pe = *pv.begin();
            playerAlive    = true;
            const auto& needs = pv.get<Needs>(pe);
            hungerPct  = needs.list[0].value;  hungerCrit  = needs.list[0].criticalThreshold;
            thirstPct  = needs.list[1].value;  thirstCrit  = needs.list[1].criticalThreshold;
            energyPct  = needs.list[2].value;  energyCrit  = needs.list[2].criticalThreshold;
            playerBehavior = pv.get<AgentState>(pe).behavior;
            const auto& ppos = pv.get<Position>(pe);
            playerWX = ppos.x; playerWY = ppos.y;
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
        m_snapshot.tickSpeed    = tickSpeed;
        m_snapshot.paused       = paused;
        m_snapshot.population   = pop;
        m_snapshot.totalDeaths  = deaths;
        m_snapshot.roadBlocked  = anyRoadBlocked;
        m_snapshot.playerAlive  = playerAlive;
        m_snapshot.hungerPct    = hungerPct;
        m_snapshot.thirstPct    = thirstPct;
        m_snapshot.energyPct    = energyPct;
        m_snapshot.hungerCrit   = hungerCrit;
        m_snapshot.thirstCrit   = thirstCrit;
        m_snapshot.energyCrit   = energyCrit;
        m_snapshot.playerBehavior = playerBehavior;
        m_snapshot.playerWorldX  = playerWX;
        m_snapshot.playerWorldY  = playerWY;
        m_snapshot.logEntries    = std::move(logEntries);
        m_snapshot.simStepsPerSec = m_stepsLastSec;
        m_snapshot.totalEntities  = (int)m_registry.storage<entt::entity>().size();
    }
}
