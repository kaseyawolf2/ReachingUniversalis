#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

class TimeSystem {
public:
    // Advance the game clock by one sub-tick — call tickSpeed times per frame.
    void Advance(entt::registry& registry, float subDt, const WorldSchema& schema);
};
