#pragma once
#include <entt/entt.hpp>

class MovementSystem {
public:
    void Update(entt::registry& registry, float dt);
};
