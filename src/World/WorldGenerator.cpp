#include "WorldGenerator.h"
#include "ECS/Components.h"
#include "raylib.h"

void WorldGenerator::Populate(entt::registry& registry) {
    // ---- Resource nodes ----
    // Positions and colors mirror the old World::Initialize exactly.

    auto foodNode = registry.create();
    registry.emplace<Position>(foodNode, 300.0f, 200.0f);
    registry.emplace<ResourceNode>(foodNode, ResourceType::Food, 30.0f);
    registry.emplace<Renderable>(foodNode, GREEN, 15.0f);   // half-size = 15 → 30x30 square

    auto waterNode = registry.create();
    registry.emplace<Position>(waterNode, 900.0f, 500.0f);
    registry.emplace<ResourceNode>(waterNode, ResourceType::Water, 30.0f);
    registry.emplace<Renderable>(waterNode, SKYBLUE, 15.0f);

    auto shelterNode = registry.create();
    registry.emplace<Position>(shelterNode, 600.0f, 560.0f);
    registry.emplace<ResourceNode>(shelterNode, ResourceType::Shelter, 30.0f);
    registry.emplace<Renderable>(shelterNode, BROWN, 15.0f);

    // ---- NPC / player entity ----

    auto npc = registry.create();
    registry.emplace<Position>(npc, 640.0f, 360.0f);
    registry.emplace<Velocity>(npc, 0.0f, 0.0f);
    registry.emplace<MoveSpeed>(npc, 80.0f);
    registry.emplace<Needs>(npc, Needs{{
        Need{ NeedType::Hunger, 1.0f, 0.04f, 0.3f, 0.25f },
        Need{ NeedType::Thirst, 1.0f, 0.07f, 0.3f, 0.35f },
        Need{ NeedType::Energy, 1.0f, 0.02f, 0.3f, 0.15f }
    }});
    registry.emplace<AgentState>(npc);
    registry.emplace<Renderable>(npc, WHITE, 10.0f);
    registry.emplace<PlayerTag>(npc);
}
