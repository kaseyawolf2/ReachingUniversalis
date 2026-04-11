#pragma once
#include <entt/entt.hpp>
#include <map>
#include <random>

// Fires random world events at intervals to stress the simulation:
//   Drought  — reduces all production at a settlement for several game-hours
//   Blight   — destroys a portion of a settlement's food stockpile
//   Bandits  — blocks the road for several game-hours (auto-clears)
//
// Events are logged to the EventLog component. Timers are ticked here so
// drought and bandit effects end automatically without player intervention.

class RandomEventSystem {
public:
    void Update(entt::registry& registry, float realDt);

private:
    std::mt19937 m_rng{std::random_device{}()};
    float        m_nextEvent = 72.f;   // game-hours until next event (first fires at day 3)

    // Active plagues: settlement entity → game-hours until next spread attempt
    std::map<entt::entity, float> m_plagueSpreadTimer;

    void TriggerEvent(entt::registry& registry, int day, int hour);
    // Kill killFraction of the settlement's population (excluding player).
    // Returns number of NPCs killed. Used by both initial outbreak and spread.
    int KillFraction(entt::registry& registry, entt::entity settl, float fraction);
};
