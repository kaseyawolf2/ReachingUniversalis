#pragma once
#include <thread>
#include <atomic>
#include <entt/entt.hpp>

#include "Threading/InputSnapshot.h"
#include "Threading/RenderSnapshot.h"

#include "ECS/Systems/TimeSystem.h"
#include "ECS/Systems/NeedDrainSystem.h"
#include "ECS/Systems/ConsumptionSystem.h"
#include "ECS/Systems/ScheduleSystem.h"
#include "ECS/Systems/AgentDecisionSystem.h"
#include "ECS/Systems/MovementSystem.h"
#include "ECS/Systems/ProductionSystem.h"
#include "ECS/Systems/TransportSystem.h"
#include "ECS/Systems/DeathSystem.h"

// SimThread owns the ECS registry and all simulation systems.
// It runs on a dedicated background thread so the render thread never stalls
// waiting for simulation work (and vice versa).
//
// Communication:
//   InputSnapshot  (main thread → sim thread) — keyboard/mouse intent
//   RenderSnapshot (sim thread → main thread) — drawable state each frame

class SimThread {
public:
    // input and snapshot must outlive this object.
    SimThread(InputSnapshot& input, RenderSnapshot& snapshot);
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

    InputSnapshot&  m_input;
    RenderSnapshot& m_snapshot;

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

    std::thread       m_thread;
    std::atomic<bool> m_running{false};

    // World-space click pending from main thread (protected by m_clickMutex)
    std::mutex        m_clickMutex;
    bool              m_pendingClick{false};
    float             m_clickX{0.f}, m_clickY{0.f};

    // Selected settlement entity (sim-thread-only state)
    entt::entity      m_selectedSettlement{entt::null};
};
