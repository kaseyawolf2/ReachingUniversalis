#pragma once
#include <entt/entt.hpp>

class TimeSystem {
public:
    // Advance the game clock by one sub-tick — call tickSpeed times per frame.
    void Advance(entt::registry& registry, float subDt);
};
