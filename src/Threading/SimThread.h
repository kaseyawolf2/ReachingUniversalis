#pragma once
#include <thread>
#include <atomic>
#include <map>
#include <entt/entt.hpp>

#include "Threading/InputSnapshot.h"
#include "Threading/RenderSnapshot.h"
#include "World/WorldSchema.h"

#include "ECS/Systems/TimeSystem.h"
#include "ECS/Systems/NeedDrainSystem.h"
#include "ECS/Systems/ConsumptionSystem.h"
#include "ECS/Systems/ScheduleSystem.h"
#include "ECS/Systems/AgentDecisionSystem.h"
#include "ECS/Systems/MovementSystem.h"
#include "ECS/Systems/ProductionSystem.h"
#include "ECS/Systems/TransportSystem.h"
#include "ECS/Systems/DeathSystem.h"
#include "ECS/Systems/BirthSystem.h"
#include "ECS/Systems/PriceSystem.h"
#include "ECS/Systems/RandomEventSystem.h"
#include "ECS/Systems/EconomicMobilitySystem.h"
#include "ECS/Systems/ConstructionSystem.h"

// SimThread owns the ECS registry and all simulation systems.
// It runs on a dedicated background thread so the render thread never stalls
// waiting for simulation work (and vice versa).
//
// Communication:
//   InputSnapshot  (main thread → sim thread) — keyboard/mouse intent
//   RenderSnapshot (sim thread → main thread) — drawable state each frame

class SimThread {
public:
    // input, snapshot, and schema must outlive this object.
    SimThread(InputSnapshot& input, RenderSnapshot& snapshot, const WorldSchema& schema);
    ~SimThread();

    void Start();
    void Stop();

    // Called by main thread to pass a clicked world position for settlement
    // selection hit-testing. SimThread resolves the entity.
    void NotifyWorldClick(float worldX, float worldY);

private:
    void        Run();
    void        ProcessInput();
    void        RunSimStep(float dt);
    void        WriteSnapshot();
    void        RespawnPlayer();   // called when PlayerTag entity no longer exists

    InputSnapshot&      m_input;
    RenderSnapshot&     m_snapshot;
    const WorldSchema&  m_schema;

    entt::registry  m_registry;

    // Simulation systems — all owned here, only touched on the sim thread
    TimeSystem          m_timeSystem;
    NeedDrainSystem     m_needDrainSystem;
    ConsumptionSystem   m_consumptionSystem;
    ScheduleSystem      m_scheduleSystem;
    AgentDecisionSystem m_agentDecisionSystem;
    MovementSystem      m_movementSystem;
    ProductionSystem    m_productionSystem;
    TransportSystem     m_transportSystem;
    DeathSystem         m_deathSystem;
    BirthSystem         m_birthSystem;
    PriceSystem             m_priceSystem;
    RandomEventSystem       m_randomEventSystem;
    EconomicMobilitySystem  m_economicMobilitySystem;
    ConstructionSystem      m_constructionSystem;

    std::thread       m_thread;
    std::atomic<bool> m_running{false};

    // Sim throughput tracking
    int   m_stepCounter   = 0;
    int   m_stepsLastSec  = 0;
    float m_statAccum     = 0.f;

    // World-space click pending from main thread (protected by m_clickMutex)
    std::mutex        m_clickMutex;
    bool              m_pendingClick{false};
    float             m_clickX{0.f}, m_clickY{0.f};

    // Selected settlement entity (sim-thread-only state)
    entt::entity      m_selectedSettlement{entt::null};

    // Population trend tracking: sample pop every N days, compare to previous sample
    std::map<entt::entity, int> m_popPrev;   // population at last sample point
    int                         m_popSampleDay{0};  // day of last sample

    // Price trend tracking: sample prices every N days
    std::map<entt::entity, std::map<int, float>> m_pricePrev;
    int                                                    m_priceSampleDay{0};

    // Player trade ledger — last 6 trades, newest first
    std::vector<RenderSnapshot::TradeRecord> m_tradeLedger;
    void PushTradeRecord(const std::string& desc, float profit);

    // Population milestone tracker — fires log events at pop milestones per settlement
    std::map<entt::entity, int> m_popMilestone;  // last milestone logged per settlement

    // Player reputation (persists through respawn via registry PlayerTag component)
    int m_playerReputation = 0;

    // Schema-derived skill display names, built once at construction.
    // Used by WriteSnapshot to avoid rebuilding per frame.
    std::vector<std::string> m_cachedSkillNames;

    // Population history for sparkline: maps settlement entity → ring buffer of daily pop samples
    // Sampled once per game-day (at the same interval as m_popPrev).
    // Maximum POPHISTORY_MAX entries (oldest overwritten).
    static constexpr int POPHISTORY_MAX = 30;
    std::map<entt::entity, std::vector<int>> m_popHistory;  // newest at back

    // Per-system profiling: accumulate microseconds per 1-second window
    static constexpr int PROFILE_COUNT = 16;  // systems + WriteSnapshot + ProcessInput
    struct ProfileAccum {
        const char* name;
        float       accumUs = 0.f;  // total microseconds this window
        float       avgUs   = 0.f;  // smoothed result from last window
    };
    ProfileAccum m_profile[PROFILE_COUNT] = {};
    float        m_profileAccum = 0.f;
    int          m_profileSteps = 0;
    void         ProfileFlush(float elapsed);
};
