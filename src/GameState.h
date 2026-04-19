#pragma once
#include "raylib.h"
#include "Threading/InputSnapshot.h"
#include "Threading/RenderSnapshot.h"
#include "Threading/SimThread.h"
#include "World/WorldSchema.h"
#include "World/WorldLoader.h"
#include "ECS/Systems/RenderSystem.h"
#include "UI/HUD.h"
#include "UI/UIState.h"
#include <vector>

// GameState owns the two shared communication objects and the simulation thread.
// The main thread's responsibilities are:
//   1. Read Raylib input → write to InputSnapshot
//   2. Update camera (using player position from snapshot)
//   3. Render from the latest RenderSnapshot
//
// Everything else runs on the sim thread inside SimThread.

class GameState {
public:
    explicit GameState(const WorldSchema& schema,
                       std::vector<LoadWarning> loadWarnings = {});
    ~GameState();

    void  Update(float dt);
    void  Draw();
    Color SkyColor() const;

    // Expose snapshot for benchmark/diagnostic reads
    const RenderSnapshot& Snapshot() const { return m_snapshot; }

    // Set tick speed directly (for benchmark mode)
    void SetTickSpeed(int speed) { m_input.setTickSpeed.store(speed); }

private:
    void PollInput(float dt);

    InputSnapshot  m_input;
    RenderSnapshot m_snapshot;
    SimThread      m_simThread;
    const WorldSchema& m_schema;

    // Camera state
    Camera2D m_camera = {
        { 640.f, 360.f },  // offset: screen centre
        { 400.f, 360.f },  // target: start near Greenfield
        0.f, 0.5f          // rotation, zoom
    };
    float m_panSpeed     = 400.f;
    float m_zoomMin      = 0.25f;
    float m_zoomMax      = 3.0f;

    // Rendering
    RenderSystem m_renderSystem;
    HUD          m_hud;

    // All input-driven UI state (panel visibility, selection, scroll positions,
    // pending action display).  Main-thread only — never passed to SimThread.
    UIState m_uiState;
};
