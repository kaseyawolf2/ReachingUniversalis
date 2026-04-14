#pragma once
#include <entt/entt.hpp>
#include "ECS/Components.h"

struct WorldSchema;

class AgentDecisionSystem {
public:
    void Update(entt::registry& registry, float realDt, const WorldSchema& schema);

    // Sub-block profiling: smoothed average microseconds per step (1-second window)
    static constexpr int SUB_PROFILE_COUNT = 7;
    struct SubProfile {
        const char* name = nullptr;
        float accumUs = 0.f;
        float avgUs   = 0.f;
    };
    SubProfile m_subProfile[SUB_PROFILE_COUNT] = {};
    float      m_subProfileAccum = 0.f;
    int        m_subProfileSteps = 0;
    void       SubProfileFlush();

private:
    const WorldSchema* m_schema = nullptr;  // set by Update() for private method access

    entt::entity FindNearestFacility(entt::registry& registry,
                                     int type,
                                     entt::entity homeSettlement,
                                     float px, float py);

    entt::entity FindMigrationTarget(entt::registry& registry,
                                     entt::entity homeSettlement,
                                     const Skills* skills = nullptr,
                                     const Profession* profession = nullptr,
                                     const MigrationMemory* memory = nullptr,
                                     int currentDay = 0,
                                     float lastSatisfaction = 0.5f,
                                     bool isLonely = false);
};
