#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

class ScheduleSystem {
public:
    void Update(entt::registry& registry, float realDt, const WorldSchema& schema);
};
