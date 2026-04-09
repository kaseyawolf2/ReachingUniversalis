#pragma once
#include "raylib.h"
#include "Need.h"

enum class ResourceType { Food, Water, Shelter };

struct ResourceNode {
    ResourceType type;
    Vector2 position;
    float interactionRadius;
    Color color;

    bool IsInRange(Vector2 agentPos) const;
};
