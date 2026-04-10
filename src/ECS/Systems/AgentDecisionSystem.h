#pragma once
#include <entt/entt.hpp>
#include "ECS/Components.h"

class AgentDecisionSystem {
public:
    void Update(entt::registry& registry, float dt);

private:
    entt::entity FindNearestNode(entt::registry& registry,
                                 ResourceType type,
                                 float px, float py);
};
