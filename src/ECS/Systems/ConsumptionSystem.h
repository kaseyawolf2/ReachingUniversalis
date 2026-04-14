#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

class ConsumptionSystem {
public:
    explicit ConsumptionSystem(const WorldSchema& schema);

    void Update(entt::registry& registry, float realDt);

private:
    const WorldSchema& m_schema;

    static constexpr int NOT_CACHED = -2;
    int  m_hungerNeedId  = NOT_CACHED;
    int  m_thirstNeedId  = NOT_CACHED;
};
