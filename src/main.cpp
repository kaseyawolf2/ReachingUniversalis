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
            ClearBackground({ 30, 30, 30, 255 });
            state.Draw(); // non-const: systems need mutable registry access
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
