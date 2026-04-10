#include "NeedDrainSystem.h"
#include "ECS/Components.h"

void NeedDrainSystem::Update(entt::registry& registry, float realDt) {
    // Resolve game-time delta from the TimeManager singleton.
    // Needs drain at a consistent game-time rate regardless of tick speed.
    // If paused, gameDt == 0 and no draining occurs.
    float gameDt = realDt;  // fallback if no TimeManager present
    auto timeView = registry.view<TimeManager>();
    if (!timeView.empty()) {
        gameDt = timeView.get<TimeManager>(*timeView.begin()).GameDt(realDt);
    }

    auto view = registry.view<Needs>();
    for (auto entity : view) {
        auto& needs = view.get<Needs>(entity);
        for (auto& need : needs.list) {
            need.value -= need.drainRate * gameDt;
            if (need.value < 0.0f) need.value = 0.0f;
        }
    }
}
