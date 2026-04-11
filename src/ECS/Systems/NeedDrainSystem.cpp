#include "NeedDrainSystem.h"
#include "ECS/Components.h"

// Extra drain multiplier applied to the player's needs when standing inside a
// plague-afflicted settlement (within its radius).
static constexpr float PLAGUE_PLAYER_DRAIN_MULT = 1.5f;

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

    // Check if the player is inside a plague-afflicted settlement (for extra drain).
    float playerPlagueMult = 1.f;
    {
        auto pv = registry.view<PlayerTag, Position>();
        if (pv.begin() != pv.end()) {
            const auto& pp = pv.get<Position>(*pv.begin());
            registry.view<Position, Settlement>().each(
                [&](const Position& sp, const Settlement& s) {
                if (s.modifierName == "Plague") {
                    float dx = pp.x - sp.x, dy = pp.y - sp.y;
                    if (dx*dx + dy*dy <= s.radius * s.radius)
                        playerPlagueMult = PLAGUE_PLAYER_DRAIN_MULT;
                }
            });
        }
    }

    auto view = registry.view<Needs>();
    for (auto entity : view) {
        auto& needs = view.get<Needs>(entity);

        // If the entity is sleeping, restore energy instead of draining it.
        bool sleeping = false;
        if (const auto* state = registry.try_get<AgentState>(entity))
            sleeping = (state->behavior == AgentBehavior::Sleeping);

        bool isPlayer = registry.all_of<PlayerTag>(entity);
        float plagueMult = (isPlayer) ? playerPlagueMult : 1.f;

        for (auto& need : needs.list) {
            if (sleeping && need.type == NeedType::Energy) {
                need.value += need.refillRate * gameDt;
                if (need.value > 1.0f) need.value = 1.0f;
            } else {
                float mult = (need.type == NeedType::Energy) ? energyDrainMult :
                             (need.type == NeedType::Heat)   ? heatDrainMult   : 1.f;
                need.value -= need.drainRate * mult * plagueMult * gameDt;
                if (need.value < 0.0f) need.value = 0.0f;
            }
        }
    }
}
