#include "PlayerInputSystem.h"
#include "ECS/Components.h"
#include "raylib.h"
#include <cmath>

static constexpr float INTERACT_RADIUS = 140.f;  // must be within a settlement to interact

// Returns the nearest settlement entity within INTERACT_RADIUS of the player, or entt::null.
static entt::entity NearestSettlement(entt::registry& registry, const Position& playerPos) {
    entt::entity best = entt::null;
    float bestDist2   = INTERACT_RADIUS * INTERACT_RADIUS;

    auto view = registry.view<Position, Settlement>();
    for (auto entity : view) {
        const auto& pos = view.get<Position>(entity);
        float dx = pos.x - playerPos.x;
        float dy = pos.y - playerPos.y;
        float d2 = dx * dx + dy * dy;
        if (d2 <= bestDist2) { bestDist2 = d2; best = entity; }
    }
    return best;
}

void PlayerInputSystem::Update(entt::registry& registry, float realDt) {
    // Find player entity
    auto playerView = registry.view<PlayerTag, Position, Velocity, MoveSpeed, Needs, HomeSettlement>();
    if (playerView.begin() == playerView.end()) return;

    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    float gameDt = timeView.get<TimeManager>(*timeView.begin()).GameDt(realDt);

    auto  playerEntity = *playerView.begin();
    auto& pos   = playerView.get<Position>(playerEntity);
    auto& vel   = playerView.get<Velocity>(playerEntity);
    float speed = playerView.get<MoveSpeed>(playerEntity).value;

    // ---- WASD movement (uses gameDt so pausing freezes the player too) ----
    float dx = 0.f, dy = 0.f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dy -= 1.f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dy += 1.f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dx -= 1.f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dx += 1.f;

    // Normalise diagonal movement
    float len = std::sqrt(dx * dx + dy * dy);
    if (len > 0.f) {
        vel.vx = (dx / len) * speed;
        vel.vy = (dy / len) * speed;
    } else {
        vel.vx = vel.vy = 0.f;
    }

    // ---- E: consume 1 Food + 1 Water from nearby settlement stockpile ----
    if (IsKeyPressed(KEY_E)) {
        entt::entity settl = NearestSettlement(registry, pos);
        if (settl != entt::null) {
            auto* sp = registry.try_get<Stockpile>(settl);
            auto& needs = playerView.get<Needs>(playerEntity);
            if (sp) {
                // Consume food if hungry and available
                float& food = sp->quantities[ResourceType::Food];
                if (food >= 1.f) {
                    food -= 1.f;
                    for (auto& n : needs.list)
                        if (n.type == NeedType::Hunger) { n.value = std::min(1.f, n.value + n.refillRate * 30.f); break; }
                }
                // Consume water if thirsty and available
                float& water = sp->quantities[ResourceType::Water];
                if (water >= 1.f) {
                    water -= 1.f;
                    for (auto& n : needs.list)
                        if (n.type == NeedType::Thirst) { n.value = std::min(1.f, n.value + n.refillRate * 30.f); break; }
                }
            }
        }
    }

    // ---- R: respawn — teleport player to home settlement ----
    if (IsKeyPressed(KEY_R)) {
        auto& home = playerView.get<HomeSettlement>(playerEntity);
        if (home.settlement != entt::null && registry.valid(home.settlement)) {
            const auto& homePos = registry.get<Position>(home.settlement);
            pos.x = homePos.x;
            pos.y = homePos.y + 60.f;  // slightly below centre
        }
        vel.vx = vel.vy = 0.f;

        // Restore needs to full
        auto& needs = playerView.get<Needs>(playerEntity);
        for (auto& n : needs.list) n.value = 1.f;
    }

    // ---- F: toggle camera follow mode ----
    if (IsKeyPressed(KEY_F)) {
        auto camView = registry.view<CameraState>();
        if (!camView.empty()) {
            auto& cs = camView.get<CameraState>(*camView.begin());
            cs.followPlayer = !cs.followPlayer;
        }
    }
}
