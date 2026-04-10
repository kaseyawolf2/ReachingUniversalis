#pragma once
#include <entt/entt.hpp>

class TimeSystem {
public:
    // Advance game clock and handle keyboard input (pause, speed).
    void Update(entt::registry& registry, float realDt);
};
