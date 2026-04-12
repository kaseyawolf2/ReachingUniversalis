#pragma once
#include "raylib.h"
#include "Threading/InputSnapshot.h"
#include "Threading/RenderSnapshot.h"
#include "Threading/SimThread.h"
#include "ECS/Systems/RenderSystem.h"
#include "UI/HUD.h"

// GameState owns the two shared communication objects and the simulation thread.
// The main thread's responsibilities are:
//   1. Read Raylib input → write to InputSnapshot
//   2. Update camera (using player position from snapshot)
//   3. Render from the latest RenderSnapshot
//
// Everything else runs on the sim thread inside SimThread.

class GameState {
public:
    GameState();
    ~GameState();

    void  Update(float dt);
    void  Draw();
    Color SkyColor() const;

private:
    void PollInput(float dt);

    InputSnapshot  m_input;
    RenderSnapshot m_snapshot;
    SimThread      m_simThread;

    // Camera lives on the main thread — it responds to input immediately
    // without waiting for the sim thread.
    Camera2D m_camera = {
        { 640.f, 360.f },  // offset: screen centre
        { 400.f, 360.f },  // target: start near Greenfield
        0.f, 0.5f          // rotation, zoom
    };
    bool  m_followPlayer = true;
    float m_panSpeed     = 400.f;
    float m_zoomMin      = 0.25f;
    float m_zoomMax      = 3.0f;

    RenderSystem m_renderSystem;
    HUD          m_hud;

    // Road colour mode: false = safety (bandit-aware), true = condition-only
    bool  m_showRoadCondition = false;

    // Road-build mode: tracks the intermediate state between the first and second N key press.
    bool  m_roadBuildMode = false;
    float m_roadBuildSrcX = 0.f;
    float m_roadBuildSrcY = 0.f;
};
