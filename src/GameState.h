#pragma once
#include "World.h"
#include "HUD.h"

class GameState {
public:
    World world;
    HUD   hud;

    void Initialize();
    void Update(float dt);
    void Draw() const;
};
