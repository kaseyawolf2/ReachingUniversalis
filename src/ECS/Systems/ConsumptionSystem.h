#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

class ConsumptionSystem {
public:
    explicit ConsumptionSystem(const WorldSchema& schema);

    void Update(entt::registry& registry, float realDt, const WorldSchema& schema);

private:
    const WorldSchema& m_schema;

    int  m_hungerNeedId  = -1;
    int  m_thirstNeedId  = -1;
    bool m_needsCached   = false;
};
