#include "AgentDecisionSystem.h"
#include <cmath>
#include <limits>

// ---- Static helpers ----

static int NeedIndexForResource(ResourceType type) {
    switch (type) {
        case ResourceType::Food:    return (int)NeedType::Hunger;
        case ResourceType::Water:   return (int)NeedType::Thirst;
        case ResourceType::Shelter: return (int)NeedType::Energy;
    }
    return -1;
}

static ResourceType ResourceTypeForNeed(NeedType type) {
    switch (type) {
        case NeedType::Hunger: return ResourceType::Food;
        case NeedType::Thirst: return ResourceType::Water;
        case NeedType::Energy: return ResourceType::Shelter;
    }
    return ResourceType::Food;
}

static AgentBehavior BehaviorForNeed(NeedType type) {
    switch (type) {
        case NeedType::Hunger: return AgentBehavior::SeekingFood;
        case NeedType::Thirst: return AgentBehavior::SeekingWater;
        case NeedType::Energy: return AgentBehavior::SeekingSleep;
    }
    return AgentBehavior::Idle;
}

static bool IsInRange(float ax, float ay, float bx, float by, float radius) {
    float dx = ax - bx;
    float dy = ay - by;
    return (dx * dx + dy * dy) <= radius * radius;
}

// ---- System ----

void AgentDecisionSystem::Update(entt::registry& registry, float dt) {
    auto view = registry.view<Needs, AgentState, Position, Velocity, MoveSpeed>();

    for (auto entity : view) {
        auto& needs = view.get<Needs>(entity);
        auto& state = view.get<AgentState>(entity);
        auto& pos   = view.get<Position>(entity);
        auto& vel   = view.get<Velocity>(entity);
        float speed = view.get<MoveSpeed>(entity).value;

        // ---- Satisfying state: refill need then check completion ----
        if (state.behavior == AgentBehavior::Satisfying) {
            if (state.target == entt::null || !registry.valid(state.target)) {
                state.behavior = AgentBehavior::Idle;
                vel.vx = vel.vy = 0.0f;
                continue;
            }

            auto& targetPos = registry.get<Position>(state.target);
            auto& node      = registry.get<ResourceNode>(state.target);

            if (!IsInRange(pos.x, pos.y, targetPos.x, targetPos.y, node.interactionRadius)) {
                // Left the node's range — go back to seeking
                state.behavior = AgentBehavior::Idle;
                state.target   = entt::null;
                vel.vx = vel.vy = 0.0f;
                continue;
            }

            int idx = NeedIndexForResource(node.type);
            if (idx >= 0) {
                needs.list[idx].value += needs.list[idx].refillRate * dt;
                if (needs.list[idx].value >= 1.0f) {
                    needs.list[idx].value = 1.0f;
                    state.behavior = AgentBehavior::Idle;
                    state.target   = entt::null;
                    vel.vx = vel.vy = 0.0f;
                }
            }
            continue;
        }

        // ---- Idle / Seeking: decide what to do ----

        int criticalIdx = -1;
        float lowestVal = std::numeric_limits<float>::max();
        for (int i = 0; i < (int)needs.list.size(); ++i) {
            const auto& n = needs.list[i];
            if (n.value < n.criticalThreshold && n.value < lowestVal) {
                lowestVal    = n.value;
                criticalIdx  = i;
            }
        }

        if (criticalIdx == -1) {
            state.behavior = AgentBehavior::Idle;
            state.target   = entt::null;
            vel.vx = vel.vy = 0.0f;
            continue;
        }

        ResourceType resType = ResourceTypeForNeed(needs.list[criticalIdx].type);
        state.behavior       = BehaviorForNeed(needs.list[criticalIdx].type);

        entt::entity targetNode = FindNearestNode(registry, resType, pos.x, pos.y);
        if (targetNode == entt::null) {
            state.behavior = AgentBehavior::Idle;
            vel.vx = vel.vy = 0.0f;
            continue;
        }

        state.target = targetNode;

        auto& targetPos = registry.get<Position>(targetNode);
        auto& nodeComp  = registry.get<ResourceNode>(targetNode);

        if (IsInRange(pos.x, pos.y, targetPos.x, targetPos.y, nodeComp.interactionRadius)) {
            state.behavior = AgentBehavior::Satisfying;
            vel.vx = vel.vy = 0.0f;
        } else {
            float dx   = targetPos.x - pos.x;
            float dy   = targetPos.y - pos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            vel.vx = (dx / dist) * speed;
            vel.vy = (dy / dist) * speed;
        }
    }
}

entt::entity AgentDecisionSystem::FindNearestNode(entt::registry& registry,
                                                   ResourceType type,
                                                   float px, float py) {
    entt::entity nearest = entt::null;
    float bestDistSq = std::numeric_limits<float>::max();

    auto nodeView = registry.view<Position, ResourceNode>();
    for (auto entity : nodeView) {
        const auto& node = nodeView.get<ResourceNode>(entity);
        if (node.type != type) continue;

        const auto& npos = nodeView.get<Position>(entity);
        float dx = npos.x - px;
        float dy = npos.y - py;
        float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            nearest    = entity;
        }
    }
    return nearest;
}
