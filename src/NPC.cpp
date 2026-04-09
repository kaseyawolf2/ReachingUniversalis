#include "NPC.h"
#include <algorithm>
#include <cmath>
#include <limits>

void NPC::Initialize(Vector2 startPos) {
    position  = startPos;
    moveSpeed = 80.0f;
    currentState = NPCState::Idle;
    targetNode   = nullptr;

    needs[0] = { NeedType::Hunger, 1.0f, 0.04f, 0.3f, 0.25f };
    needs[1] = { NeedType::Thirst, 1.0f, 0.07f, 0.3f, 0.35f };
    needs[2] = { NeedType::Energy, 1.0f, 0.02f, 0.3f, 0.15f };
}

void NPC::Update(float dt, const std::vector<ResourceNode>& nodes) {
    UpdateNeeds(dt);

    if (currentState == NPCState::Satisfying) {
        SatisfyNeed(dt);
        // If target node left range (shouldn't happen, but be safe)
        if (targetNode && !targetNode->IsInRange(position)) {
            currentState = NPCState::Idle;
            targetNode = nullptr;
        }
        return;
    }

    NPCState desired = EvaluatePriority(nodes);
    if (desired != currentState) {
        currentState = desired;
    }

    if (currentState != NPCState::Idle && targetNode != nullptr) {
        if (targetNode->IsInRange(position)) {
            currentState = NPCState::Satisfying;
        } else {
            MoveToward(targetNode->position, dt);
        }
    }
}

void NPC::UpdateNeeds(float dt) {
    for (auto& need : needs) {
        need.value -= need.drainRate * dt;
        if (need.value < 0.0f) need.value = 0.0f;
    }
}

NPCState NPC::EvaluatePriority(const std::vector<ResourceNode>& nodes) {
    // Find the most critical need (lowest value below its threshold)
    int criticalIndex = -1;
    float lowestValue = std::numeric_limits<float>::max();

    for (int i = 0; i < (int)needs.size(); ++i) {
        if (needs[i].value < needs[i].criticalThreshold && needs[i].value < lowestValue) {
            lowestValue = needs[i].value;
            criticalIndex = i;
        }
    }

    if (criticalIndex == -1) {
        targetNode = nullptr;
        return NPCState::Idle;
    }

    NeedType critical = needs[criticalIndex].type;
    ResourceType resType;
    NPCState seekState;

    switch (critical) {
        case NeedType::Hunger: resType = ResourceType::Food;    seekState = NPCState::SeekingFood;  break;
        case NeedType::Thirst: resType = ResourceType::Water;   seekState = NPCState::SeekingWater; break;
        case NeedType::Energy: resType = ResourceType::Shelter; seekState = NPCState::SeekingSleep; break;
        default:               return NPCState::Idle;
    }

    targetNode = FindNearestNode(resType, nodes);
    if (targetNode == nullptr) return NPCState::Idle;

    return seekState;
}

void NPC::MoveToward(Vector2 target, float dt) {
    float dx = target.x - position.x;
    float dy = target.y - position.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.0f) return;

    position.x += (dx / dist) * moveSpeed * dt;
    position.y += (dy / dist) * moveSpeed * dt;
}

void NPC::SatisfyNeed(float dt) {
    if (targetNode == nullptr) return;

    int idx = -1;
    switch (targetNode->type) {
        case ResourceType::Food:    idx = (int)NeedType::Hunger; break;
        case ResourceType::Water:   idx = (int)NeedType::Thirst; break;
        case ResourceType::Shelter: idx = (int)NeedType::Energy; break;
    }

    if (idx < 0) return;

    needs[idx].value += needs[idx].refillRate * dt;
    if (needs[idx].value >= 1.0f) {
        needs[idx].value = 1.0f;
        currentState = NPCState::Idle;
        targetNode = nullptr;
    }
}

ResourceNode* NPC::FindNearestNode(ResourceType type, const std::vector<ResourceNode>& nodes) {
    ResourceNode* nearest = nullptr;
    float bestDistSq = std::numeric_limits<float>::max();

    for (const auto& node : nodes) {
        if (node.type != type) continue;
        float dx = node.position.x - position.x;
        float dy = node.position.y - position.y;
        float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            nearest = const_cast<ResourceNode*>(&node);
        }
    }
    return nearest;
}

void NPC::Draw() const {
    DrawCircleV(position, 10.0f, WHITE);
    DrawCircleLines((int)position.x, (int)position.y, 10, LIGHTGRAY);
}

const char* NPC::GetStateLabel() const {
    switch (currentState) {
        case NPCState::Idle:         return "Idle";
        case NPCState::SeekingFood:  return "SeekingFood";
        case NPCState::SeekingWater: return "SeekingWater";
        case NPCState::SeekingSleep: return "SeekingSleep";
        case NPCState::Satisfying:   return "Satisfying";
        default:                     return "Unknown";
    }
}
