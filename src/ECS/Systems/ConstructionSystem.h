#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

// Allows prosperous settlements to autonomously build new production facilities.
// When a settlement has surplus gold AND a resource is chronically scarce (high
// price), it invests in a new facility for that resource type.
//
// This creates organic settlement growth — thriving communities expand capacity
// while struggling ones do not, creating self-reinforcing prosperity/decline cycles.

class ConstructionSystem {
public:
    void Update(entt::registry& registry, float realDt, const WorldSchema& schema);

private:
    float m_checkAccum    = 0.f;   // game-hours since last facility construction check
    float m_roadBuildAccum = 0.f;  // game-hours since last autonomous road-building check
};
