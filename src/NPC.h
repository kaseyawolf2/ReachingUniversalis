#pragma once
#include "raylib.h"
#include "Need.h"
#include "ResourceNode.h"
#include <array>
#include <vector>

enum class NPCState { Idle, SeekingFood, SeekingWater, SeekingSleep, Satisfying };

class NPC {
public:
    Vector2 position;
    float moveSpeed;
    std::array<Need, 3> needs;
    NPCState currentState;
    ResourceNode* targetNode;  // non-owning observer pointer

    void Initialize(Vector2 startPos);
    void Update(float dt, const std::vector<ResourceNode>& nodes);
    void Draw() const;

    const char* GetStateLabel() const;

private:
    void UpdateNeeds(float dt);
    NPCState EvaluatePriority(const std::vector<ResourceNode>& nodes);
    void MoveToward(Vector2 target, float dt);
    void SatisfyNeed(float dt);
    ResourceNode* FindNearestNode(ResourceType type, const std::vector<ResourceNode>& nodes);
};
