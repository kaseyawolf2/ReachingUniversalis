#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

class MovementSystem {
public:
    void Update(entt::registry& registry, float dt, const WorldSchema& schema);
};
