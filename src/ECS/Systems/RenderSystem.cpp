#include "RenderSystem.h"
#include "ECS/Components.h"
#include "raylib.h"

void RenderSystem::Draw(entt::registry& registry) {
    // Draw resource nodes: colored square + white border + faint interaction radius
    auto nodeView = registry.view<Position, ResourceNode, Renderable>();
    for (auto entity : nodeView) {
        const auto& pos  = nodeView.get<Position>(entity);
        const auto& node = nodeView.get<ResourceNode>(entity);
        const auto& rend = nodeView.get<Renderable>(entity);

        int half = (int)rend.size;
        DrawRectangle((int)(pos.x - half), (int)(pos.y - half),
                      half * 2, half * 2, rend.color);
        DrawRectangleLines((int)(pos.x - half), (int)(pos.y - half),
                           half * 2, half * 2, WHITE);
        DrawCircleLines((int)pos.x, (int)pos.y,
                        node.interactionRadius, Fade(rend.color, 0.3f));
    }

    // Draw agents: filled circle + grey outline
    auto agentView = registry.view<Position, AgentState, Renderable>();
    for (auto entity : agentView) {
        const auto& pos  = agentView.get<Position>(entity);
        const auto& rend = agentView.get<Renderable>(entity);

        DrawCircleV({pos.x, pos.y}, rend.size, rend.color);
        DrawCircleLines((int)pos.x, (int)pos.y, (int)rend.size, LIGHTGRAY);
    }
}
