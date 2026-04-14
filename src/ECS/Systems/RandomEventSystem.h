#pragma once
#include <entt/entt.hpp>
#include <map>
#include <set>
#include <utility>
#include <random>

struct WorldSchema;

// Fires random world events at intervals to stress the simulation:
//   Drought  — reduces all production at a settlement for several game-hours
//   Blight   — destroys a portion of a settlement's food stockpile
//   Bandits  — blocks the road for several game-hours (auto-clears)
//
// Events are logged to the EventLog component. Timers are ticked here so
// drought and bandit effects end automatically without player intervention.

class RandomEventSystem {
public:
    void Update(entt::registry& registry, float realDt, const WorldSchema& schema);

private:
    std::mt19937 m_rng{std::random_device{}()};
    float        m_nextEvent = 72.f;   // game-hours until next event (first fires at day 3)

    // Active plagues: settlement entity → game-hours until next spread attempt
    std::map<entt::entity, float> m_plagueSpreadTimer;

    // Canonical pairs (min id, max id) already logged to avoid duplicate rivalry/alliance spam
    std::set<std::pair<uint32_t,uint32_t>> m_loggedRivalries;
    std::set<std::pair<uint32_t,uint32_t>> m_loggedAlliances;
    std::set<std::pair<uint32_t,uint32_t>> m_loggedRivalryRecovery;  // tracks -0.3 recovery crossing

    void TriggerEvent(entt::registry& registry, int day, int hour, const WorldSchema& schema);
    // Kill killFraction of the settlement's population (excluding player).
    // Returns number of NPCs killed. Used by both initial outbreak and spread.
    int KillFraction(entt::registry& registry, entt::entity settl, float fraction);

    // Population trend tracking — sampled every 24 game-hours
    std::map<entt::entity, int> m_prevPop;
    float m_popSampleTimer = 0.f;   // game-hours until next sample
};
