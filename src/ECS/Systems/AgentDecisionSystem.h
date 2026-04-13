#pragma once
#include <entt/entt.hpp>
#include "ECS/Components.h"

class AgentDecisionSystem {
public:
    void Update(entt::registry& registry, float realDt);

private:
    entt::entity FindNearestFacility(entt::registry& registry,
                                     ResourceType type,
                                     entt::entity homeSettlement,
                                     float px, float py);

    entt::entity FindMigrationTarget(entt::registry& registry,
                                     entt::entity homeSettlement,
                                     const Skills* skills = nullptr,
                                     const Profession* profession = nullptr,
                                     const MigrationMemory* memory = nullptr,
                                     int currentDay = 0,
                                     float lastSatisfaction = 0.5f);
};
