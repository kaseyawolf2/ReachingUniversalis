#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "ECS/Systems/TimeSystem.h"
#include "ECS/Systems/NeedDrainSystem.h"
#include "ECS/Systems/AgentDecisionSystem.h"
#include "ECS/Systems/MovementSystem.h"
#include "ECS/Systems/ProductionSystem.h"
#include "ECS/Systems/CameraSystem.h"
#include "ECS/Systems/RenderSystem.h"
#include "UI/HUD.h"

class GameState {
public:
    entt::registry registry;

    void  Initialize();
    void  Update(float dt);
    void  Draw();
    Color SkyColor() const;

private:
    TimeSystem          timeSystem;
    NeedDrainSystem     needDrainSystem;
    AgentDecisionSystem agentDecisionSystem;
    MovementSystem      movementSystem;
    ProductionSystem    productionSystem;
    CameraSystem        cameraSystem;
    RenderSystem        renderSystem;
    HUD                 hud;
};
