#include "GameState.h"
#include "World/WorldGenerator.h"

void GameState::Initialize() {
    WorldGenerator::Populate(registry);
}

void GameState::Update(float dt) {
    needDrainSystem.Update(registry, dt);
    agentDecisionSystem.Update(registry, dt);
    movementSystem.Update(registry, dt);
}

void GameState::Draw() {
    renderSystem.Draw(registry);
    hud.Draw(registry);
}
