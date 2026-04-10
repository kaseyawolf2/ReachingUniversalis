#include "NeedDrainSystem.h"
#include "ECS/Components.h"

void NeedDrainSystem::Update(entt::registry& registry, float dt) {
    auto view = registry.view<Needs>();
    for (auto entity : view) {
        auto& needs = view.get<Needs>(entity);
        for (auto& need : needs.list) {
            need.value -= need.drainRate * dt;
            if (need.value < 0.0f) need.value = 0.0f;
        }
    }
}
