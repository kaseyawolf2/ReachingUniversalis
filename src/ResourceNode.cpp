#include "ResourceNode.h"
#include <cmath>

bool ResourceNode::IsInRange(Vector2 agentPos) const {
    float dx = agentPos.x - position.x;
    float dy = agentPos.y - position.y;
    float distSq = dx * dx + dy * dy;
    return distSq <= interactionRadius * interactionRadius;
}
