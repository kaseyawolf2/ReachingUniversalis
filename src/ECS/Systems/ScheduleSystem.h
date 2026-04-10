#pragma once
#include <entt/entt.hpp>

class ScheduleSystem {
public:
    void Update(entt::registry& registry, float realDt);
};
