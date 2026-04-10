#include "CameraSystem.h"
#include "ECS/Components.h"
#include "raylib.h"
#include <cmath>

static constexpr float MAP_W    = 2400.0f;
static constexpr float MAP_H    =  720.0f;
static constexpr float LERP_SPD =    5.0f;  // camera follow smoothing factor

void CameraSystem::Update(entt::registry& registry, float realDt) {
    auto camView = registry.view<CameraState>();
    if (camView.empty()) return;
    auto& cs = camView.get<CameraState>(*camView.begin());

    // ---- Click-and-drag pan (middle mouse or right mouse) ----
    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = GetMouseDelta();
        if (delta.x != 0.f || delta.y != 0.f) {
            cs.followPlayer     = false;
            cs.cam.target.x    -= delta.x / cs.cam.zoom;
            cs.cam.target.y    -= delta.y / cs.cam.zoom;
        }
    }

    // ---- Manual pan — also disables player follow ----
    bool panning = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT) ||
                   IsKeyDown(KEY_UP)   || IsKeyDown(KEY_DOWN);
    if (panning) {
        cs.followPlayer = false;
        float speed = cs.panSpeed / cs.cam.zoom;
        if (IsKeyDown(KEY_LEFT))  cs.cam.target.x -= speed * realDt;
        if (IsKeyDown(KEY_RIGHT)) cs.cam.target.x += speed * realDt;
        if (IsKeyDown(KEY_UP))    cs.cam.target.y -= speed * realDt;
        if (IsKeyDown(KEY_DOWN))  cs.cam.target.y += speed * realDt;
    }

    // ---- Zoom with scroll wheel ----
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        cs.cam.zoom += wheel * 0.1f * cs.cam.zoom;
        if (cs.cam.zoom < cs.zoomMin) cs.cam.zoom = cs.zoomMin;
        if (cs.cam.zoom > cs.zoomMax) cs.cam.zoom = cs.zoomMax;
    }

    // ---- C: re-center on map and re-enable follow ----
    if (IsKeyPressed(KEY_C)) {
        cs.cam.target   = { MAP_W * 0.5f, MAP_H * 0.5f };
        cs.cam.zoom     = 0.5f;
        cs.followPlayer = false;
    }

    // ---- Follow player (lerp toward player position) ----
    if (cs.followPlayer) {
        auto playerView = registry.view<PlayerTag, Position>();
        if (playerView.begin() != playerView.end()) {
            const auto& ppos = playerView.get<Position>(*playerView.begin());
            float t = LERP_SPD * realDt;
            if (t > 1.f) t = 1.f;
            cs.cam.target.x += (ppos.x - cs.cam.target.x) * t;
            cs.cam.target.y += (ppos.y - cs.cam.target.y) * t;
        }
    }

    // ---- Clamp target to map bounds ----
    if (cs.cam.target.x < 0.0f)  cs.cam.target.x = 0.0f;
    if (cs.cam.target.x > MAP_W) cs.cam.target.x = MAP_W;
    if (cs.cam.target.y < 0.0f)  cs.cam.target.y = 0.0f;
    if (cs.cam.target.y > MAP_H) cs.cam.target.y = MAP_H;
}
