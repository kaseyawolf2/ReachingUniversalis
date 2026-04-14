#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

class NeedDrainSystem {
public:
    void Update(entt::registry& registry, float dt, const WorldSchema& schema);
};
