#include "World.h"

void World::Initialize() {
    // Place resource nodes at fixed positions on a 1280x720 world
    nodes = {
        { ResourceType::Food,    { 300.0f, 200.0f }, 30.0f, GREEN   },
        { ResourceType::Water,   { 900.0f, 500.0f }, 30.0f, SKYBLUE },
        { ResourceType::Shelter, { 600.0f, 560.0f }, 30.0f, BROWN   },
    };

    npc.Initialize({ 640.0f, 360.0f });
}

void World::Update(float dt) {
    npc.Update(dt, nodes);
}

void World::Draw() const {
    for (const auto& node : nodes) {
        DrawRectangle(
            (int)(node.position.x - 15),
            (int)(node.position.y - 15),
            30, 30,
            node.color
        );
        DrawRectangleLines(
            (int)(node.position.x - 15),
            (int)(node.position.y - 15),
            30, 30,
            WHITE
        );
        // Draw interaction radius (faint)
        DrawCircleLines(
            (int)node.position.x,
            (int)node.position.y,
            (int)node.interactionRadius,
            Fade(node.color, 0.3f)
        );
    }

    npc.Draw();
}
