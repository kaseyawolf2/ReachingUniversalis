#include "raylib.h"
#include "GameState.h"

int main() {
    InitWindow(1280, 720, "ReachingUniversalis");
    SetTargetFPS(60);

    GameState state;
    state.Initialize();

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        state.Update(dt);

        BeginDrawing();
            ClearBackground(state.SkyColor());
            state.Draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
