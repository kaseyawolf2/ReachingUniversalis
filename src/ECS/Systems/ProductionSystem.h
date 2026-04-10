#pragma once
#include <entt/entt.hpp>

class ProductionSystem {
public:
    void Update(entt::registry& registry, float realDt);
};
