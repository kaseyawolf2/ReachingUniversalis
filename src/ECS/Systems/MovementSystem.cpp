#include "MovementSystem.h"
#include "ECS/Components.h"

void MovementSystem::Update(entt::registry& registry, float realDt) {
    // Use game-time delta so movement pauses and scales with tick speed.
    float gameDt = realDt;
    auto timeView = registry.view<TimeManager>();
    if (!timeView.empty()) {
        gameDt = timeView.get<TimeManager>(*timeView.begin()).GameDt(realDt);
    }

    auto view = registry.view<Position, Velocity>();
    for (auto entity : view) {
        auto& pos = view.get<Position>(entity);
        auto& vel = view.get<Velocity>(entity);
        pos.x += vel.vx * gameDt;
        pos.y += vel.vy * gameDt;
    }
}
