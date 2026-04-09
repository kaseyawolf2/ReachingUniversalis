#pragma once
#include "NPC.h"
#include "ResourceNode.h"
#include <vector>

class World {
public:
    std::vector<ResourceNode> nodes;
    NPC npc;

    void Initialize();
    void Update(float dt);
    void Draw() const;
};
