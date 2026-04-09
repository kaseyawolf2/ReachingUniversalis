#include "GameState.h"

void GameState::Initialize() {
    world.Initialize();
}

void GameState::Update(float dt) {
    world.Update(dt);
}

void GameState::Draw() const {
    world.Draw();
    hud.Draw(world.npc);
}
