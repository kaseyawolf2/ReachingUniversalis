#include "MovementSystem.h"
#include "ECS/Components.h"
#include <algorithm>

static constexpr float MAP_W  = 2400.f;
static constexpr float MAP_H  =  720.f;
static constexpr float MARGIN =    5.f;

void MovementSystem::Update(entt::registry& registry, float realDt) {
    float gameDt = realDt;
    auto timeView = registry.view<TimeManager>();
    if (!timeView.empty())
        gameDt = timeView.get<TimeManager>(*timeView.begin()).GameDt(realDt);

    auto view = registry.view<Position, Velocity>();
    for (auto entity : view) {
        auto& pos = view.get<Position>(entity);
        auto& vel = view.get<Velocity>(entity);

        // Age-based speed scaling for NPCs (not haulers or player)
        // Age is in game-days; maxDays ~60-100. Children < 15 days, elders > 55 days.
        float ageFactor = 1.f;
        if (auto* age = registry.try_get<Age>(entity)) {
            if (!registry.any_of<Hauler, PlayerTag>(entity)) {
                if (age->days < 15.f)      ageFactor = 0.8f;  // children
                else if (age->days > 55.f) ageFactor = 0.7f;  // elders
            }
        }

        pos.x += vel.vx * gameDt * ageFactor;
        pos.y += vel.vy * gameDt * ageFactor;

        // Clamp to map bounds — stop velocity on contact so agents don't
        // pile up pressing against the wall.
        if (pos.x < MARGIN)         { pos.x = MARGIN;         vel.vx = 0.f; }
        if (pos.x > MAP_W - MARGIN) { pos.x = MAP_W - MARGIN; vel.vx = 0.f; }
        if (pos.y < MARGIN)         { pos.y = MARGIN;         vel.vy = 0.f; }
        if (pos.y > MAP_H - MARGIN) { pos.y = MAP_H - MARGIN; vel.vy = 0.f; }
    }
}
