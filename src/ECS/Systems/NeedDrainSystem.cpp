#include "NeedDrainSystem.h"
#include "ECS/Components.h"

// Extra drain multiplier applied to the player's needs when standing inside a
// plague-afflicted settlement (within its radius).
static constexpr float PLAGUE_PLAYER_DRAIN_MULT = 1.5f;

void NeedDrainSystem::Update(entt::registry& registry, float realDt, const WorldSchema& schema) {
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
        SeasonID season = tm.CurrentSeason(schema);
        energyDrainMult = SeasonEnergyDrainMult(season, schema);
        heatDrainMult   = SeasonHeatDrainMult(season, schema);
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
        bool sleeping    = false;
        bool celebrating = false;
        if (const auto* state = registry.try_get<AgentState>(entity)) {
            sleeping    = (state->behavior == AgentBehavior::Sleeping);
            celebrating = (state->behavior == AgentBehavior::Celebrating);
        }

        bool isPlayer = registry.all_of<PlayerTag>(entity);
        float plagueMult     = (isPlayer) ? playerPlagueMult : 1.f;
        float celebrateMult  = celebrating ? 0.5f : 1.f;

        // Check for minor illness on this entity (doubles drain on the affected need).
        const DeprivationTimer* depTimer = registry.try_get<DeprivationTimer>(entity);

        for (int i = 0; i < (int)needs.list.size(); ++i) {
            auto& need = needs.list[i];
            if (sleeping && need.type == NeedType::Energy) {
                need.value += need.refillRate * gameDt;
                if (need.value > 1.0f) need.value = 1.0f;
            } else {
                float mult = (need.type == NeedType::Energy) ? energyDrainMult :
                             (need.type == NeedType::Heat)   ? heatDrainMult   : 1.f;
                float illnessMult = (depTimer && depTimer->illnessTimer > 0.f
                                    && depTimer->illnessNeedIdx == i) ? 2.f : 1.f;
                need.value -= need.drainRate * mult * plagueMult * celebrateMult
                              * illnessMult * gameDt;
                if (need.value < 0.0f) need.value = 0.0f;
            }
        }

        // Worker fatigue: energy below 0.2 while working triggers production penalty
        if (auto* sched = registry.try_get<Schedule>(entity)) {
            float energy = ((int)NeedType::Energy < (int)needs.list.size())
                         ? needs.list[(int)NeedType::Energy].value : 1.f;
            if (const auto* state = registry.try_get<AgentState>(entity)) {
                if (state->behavior == AgentBehavior::Working && energy < 0.2f)
                    sched->fatigued = true;
            }
            if (energy > 0.5f)
                sched->fatigued = false;
        }
    }

    // ---- Skill degradation with age ----
    // Elders (65+ game-days) slowly lose skills, creating an economic lifecycle.
    auto skillAgeView = registry.view<Skills, Age>();
    for (auto entity : skillAgeView) {
        const auto& age = skillAgeView.get<Age>(entity);
        if (age.days > 65.f) {
            auto& sk = skillAgeView.get<Skills>(entity);
            float decay = 0.0002f * gameDt;
            sk.farming       = std::max(0.1f, sk.farming       - decay);
            sk.water_drawing = std::max(0.1f, sk.water_drawing - decay);
            sk.woodcutting   = std::max(0.1f, sk.woodcutting   - decay);
        }
    }
}
