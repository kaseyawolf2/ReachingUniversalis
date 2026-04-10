#pragma once
#include <entt/entt.hpp>
#include "raylib.h"
#include "ECS/Systems/TimeSystem.h"
#include "ECS/Systems/NeedDrainSystem.h"
#include "ECS/Systems/ConsumptionSystem.h"
#include "ECS/Systems/AgentDecisionSystem.h"
#include "ECS/Systems/MovementSystem.h"
#include "ECS/Systems/ProductionSystem.h"
#include "ECS/Systems/ScheduleSystem.h"
#include "ECS/Systems/TransportSystem.h"
#include "ECS/Systems/DeathSystem.h"
#include "ECS/Systems/CameraSystem.h"
#include "ECS/Systems/RenderSystem.h"
#include "ECS/Systems/PlayerInputSystem.h"
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
    ConsumptionSystem   consumptionSystem;
    AgentDecisionSystem agentDecisionSystem;
    MovementSystem      movementSystem;
    ProductionSystem    productionSystem;
    ScheduleSystem      scheduleSystem;
    TransportSystem     transportSystem;
    DeathSystem         deathSystem;
    CameraSystem        cameraSystem;
    RenderSystem        renderSystem;
    PlayerInputSystem   playerInputSystem;
    HUD                 hud;
};
