#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

class NeedDrainSystem {
public:
    explicit NeedDrainSystem(const WorldSchema& schema);

    void Update(entt::registry& registry, float dt);

private:
    const WorldSchema& m_schema;

    int  m_energyNeedId = -1;
    int  m_heatNeedId   = -1;
};
