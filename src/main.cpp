#include "raylib.h"
#include "GameState.h"

int main() {
    InitWindow(1280, 720, "ReachingUniversalis");
    SetTargetFPS(0);  // uncapped — simulation uses GetFrameTime() so it's frame-rate independent

    GameState state;

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
