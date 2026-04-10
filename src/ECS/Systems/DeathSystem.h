#pragma once
#include <entt/entt.hpp>

class DeathSystem {
public:
    void Update(entt::registry& registry, float realDt);

    int totalDeaths = 0;   // running tally for HUD display
};
