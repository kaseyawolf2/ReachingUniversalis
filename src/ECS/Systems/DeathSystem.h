#pragma once
#include <entt/entt.hpp>
#include <unordered_set>
#include <vector>
#include <string>

struct WorldSchema;

class DeathSystem {
public:
    explicit DeathSystem(const WorldSchema& schema);

    void Update(entt::registry& registry, float realDt);

    int totalDeaths = 0;   // running tally for HUD display

private:
    // Cached need names indexed by NeedID, built once in the constructor.
    // Used in death-cause determination to avoid accessing schema per tick.
    std::vector<std::string> m_needNames;

    // Track which settlements have already been logged as collapsed
    // so we only fire the collapse event once per collapse episode.
    std::unordered_set<entt::entity> m_collapsed;
};
