#pragma once
#include <entt/entt.hpp>
#include <unordered_set>

struct WorldSchema;

class DeathSystem {
public:
    void Update(entt::registry& registry, float realDt, const WorldSchema& schema);

    int totalDeaths = 0;   // running tally for HUD display

private:
    // Track which settlements have already been logged as collapsed
    // so we only fire the collapse event once per collapse episode.
    std::unordered_set<entt::entity> m_collapsed;
};
