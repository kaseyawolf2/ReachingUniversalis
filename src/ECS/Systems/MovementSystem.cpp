#include "MovementSystem.h"
#include "ECS/Components.h"

void MovementSystem::Update(entt::registry& registry, float dt) {
    auto view = registry.view<Position, Velocity>();
    for (auto entity : view) {
        auto& pos = view.get<Position>(entity);
        auto& vel = view.get<Velocity>(entity);
        pos.x += vel.vx * dt;
        pos.y += vel.vy * dt;
    }
}
