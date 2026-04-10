#include "CameraSystem.h"
#include "ECS/Components.h"
#include "raylib.h"

static constexpr float MAP_W = 2400.0f;
static constexpr float MAP_H =  720.0f;

void CameraSystem::Update(entt::registry& registry, float realDt) {
    auto view = registry.view<CameraState>();
    if (view.empty()) return;
    auto& cs = view.get<CameraState>(*view.begin());

    // ---- Pan with arrow keys (speed scales inversely with zoom) ----
    float speed = cs.panSpeed / cs.cam.zoom;
    if (IsKeyDown(KEY_LEFT))  cs.cam.target.x -= speed * realDt;
    if (IsKeyDown(KEY_RIGHT)) cs.cam.target.x += speed * realDt;
    if (IsKeyDown(KEY_UP))    cs.cam.target.y -= speed * realDt;
    if (IsKeyDown(KEY_DOWN))  cs.cam.target.y += speed * realDt;

    // ---- Zoom with scroll wheel ----
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        cs.cam.zoom += wheel * 0.1f * cs.cam.zoom;
        if (cs.cam.zoom < cs.zoomMin) cs.cam.zoom = cs.zoomMin;
        if (cs.cam.zoom > cs.zoomMax) cs.cam.zoom = cs.zoomMax;
    }

    // ---- Re-center on C ----
    if (IsKeyPressed(KEY_C)) {
        cs.cam.target = { MAP_W * 0.5f, MAP_H * 0.5f };
        cs.cam.zoom   = 1.0f;
    }

    // ---- Clamp target to map bounds ----
    if (cs.cam.target.x < 0.0f)   cs.cam.target.x = 0.0f;
    if (cs.cam.target.x > MAP_W)  cs.cam.target.x = MAP_W;
    if (cs.cam.target.y < 0.0f)   cs.cam.target.y = 0.0f;
    if (cs.cam.target.y > MAP_H)  cs.cam.target.y = MAP_H;
}
