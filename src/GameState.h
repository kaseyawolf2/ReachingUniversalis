#pragma once
#include <entt/entt.hpp>
#include "ECS/Systems/NeedDrainSystem.h"
#include "ECS/Systems/AgentDecisionSystem.h"
#include "ECS/Systems/MovementSystem.h"
#include "ECS/Systems/RenderSystem.h"
#include "UI/HUD.h"

class GameState {
public:
    entt::registry registry;

    void Initialize();
    void Update(float dt);
    void Draw();

private:
    NeedDrainSystem     needDrainSystem;
    AgentDecisionSystem agentDecisionSystem;
    MovementSystem      movementSystem;
    RenderSystem        renderSystem;
    HUD                 hud;
};
