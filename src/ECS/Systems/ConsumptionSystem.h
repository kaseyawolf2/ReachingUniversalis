#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

class ConsumptionSystem {
public:
    void Update(entt::registry& registry, float realDt, const WorldSchema& schema);

private:
    int  m_hungerNeedId  = -1;
    int  m_thirstNeedId  = -1;
    bool m_needsCached   = false;
};
