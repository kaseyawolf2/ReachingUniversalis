#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

class TransportSystem {
public:
    void Update(entt::registry& registry, float realDt, const WorldSchema& schema);

private:
    entt::entity FindRoadPartner(entt::registry& registry, entt::entity settlement);
};
