#pragma once
#include <entt/entt.hpp>

class TransportSystem {
public:
    void Update(entt::registry& registry, float realDt);

private:
    entt::entity FindRoadPartner(entt::registry& registry, entt::entity settlement);
};
