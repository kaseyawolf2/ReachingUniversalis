#include "NeedDrainSystem.h"
#include "ECS/Components.h"

void NeedDrainSystem::Update(entt::registry& registry, float realDt) {
    // Resolve game-time delta from the TimeManager singleton.
    // Needs drain at a consistent game-time rate regardless of tick speed.
    // If paused, gameDt == 0 and no draining occurs.
    float gameDt = realDt;  // fallback if no TimeManager present
    float energyDrainMult = 1.f;
    float heatDrainMult   = 0.f;
    auto timeView = registry.view<TimeManager>();
    if (!timeView.empty()) {
        const auto& tm = timeView.get<TimeManager>(*timeView.begin());
        gameDt          = tm.GameDt(realDt);
        Season season   = tm.CurrentSeason();
        energyDrainMult = SeasonEnergyDrainMult(season);
        heatDrainMult   = SeasonHeatDrainMult(season);
    }

    auto view = registry.view<Needs>();
    for (auto entity : view) {
        auto& needs = view.get<Needs>(entity);

        // If the entity is sleeping, restore energy instead of draining it.
        bool sleeping = false;
        if (const auto* state = registry.try_get<AgentState>(entity))
            sleeping = (state->behavior == AgentBehavior::Sleeping);

        for (auto& need : needs.list) {
            if (sleeping && need.type == NeedType::Energy) {
                need.value += need.refillRate * gameDt;
                if (need.value > 1.0f) need.value = 1.0f;
            } else {
                float mult = (need.type == NeedType::Energy) ? energyDrainMult :
                             (need.type == NeedType::Heat)   ? heatDrainMult   : 1.f;
                need.value -= need.drainRate * mult * gameDt;
                if (need.value < 0.0f) need.value = 0.0f;
            }
        }
    }
}
