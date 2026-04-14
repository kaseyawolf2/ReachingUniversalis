#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

class ProductionSystem {
public:
    void Update(entt::registry& registry, float realDt, const WorldSchema& schema);
};
