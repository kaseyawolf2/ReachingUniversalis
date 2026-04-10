#pragma once
#include <entt/entt.hpp>

class PlayerInputSystem {
public:
    void Update(entt::registry& registry, float realDt);
};
