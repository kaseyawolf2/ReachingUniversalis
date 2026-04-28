#include "NeedDrainSystem.h"
#include "ECS/Components.h"
#include "World/WorldSchema.h"
#include <cstdio>

// Extra drain multiplier applied to the player's needs when standing inside a
// plague-afflicted settlement (within its radius).
static constexpr float PLAGUE_PLAYER_DRAIN_MULT = 1.5f;

NeedDrainSystem::NeedDrainSystem(const WorldSchema& schema)
    : m_schema(schema)
{
    // Cache need IDs once at construction to avoid string map lookups every tick.
    m_energyNeedId = m_schema.FindNeed("Energy");
    m_heatNeedId   = m_schema.FindNeed("Heat");

    if (m_energyNeedId == INVALID_ID)
        fprintf(stderr, "[NeedDrainSystem] WARNING: cached NeedID for \"Energy\" is INVALID_ID (-1). "
                        "Seasonal energy drain and sleep refill will not function. "
                        "Check that a need named \"Energy\" exists in the schema TOML.\n");
    if (m_heatNeedId == INVALID_ID)
        fprintf(stderr, "[NeedDrainSystem] WARNING: cached NeedID for \"Heat\" is INVALID_ID (-1). "
                        "Seasonal heat drain will not function. "
                        "Check that a need named \"Heat\" exists in the schema TOML.\n");
}

void NeedDrainSystem::Update(entt::registry& registry, float realDt) {
    const int energyNeedId = m_energyNeedId;
    const int heatNeedId   = m_heatNeedId;

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
        SeasonID sid    = tm.CurrentSeason(m_schema.seasons);
        if (sid >= 0 && sid < (int)m_schema.seasons.size()) {
            energyDrainMult = m_schema.seasons[sid].energyDrainMod;
            heatDrainMult   = m_schema.seasons[sid].heatDrainMod;
        }
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
        const PersonalEventState* pesTimer = registry.try_get<PersonalEventState>(entity);

        for (int i = 0; i < (int)needs.list.size(); ++i) {
            auto& need = needs.list[i];
            if (sleeping && need.needId == energyNeedId) {
                need.value += need.refillRate * gameDt;
                if (need.value > 1.0f) need.value = 1.0f;
            } else {
                float mult = (need.needId == energyNeedId) ? energyDrainMult :
                             (need.needId == heatNeedId)   ? heatDrainMult   : 1.f;
                float illnessMult = (pesTimer && pesTimer->illnessTimer > 0.f
                                    && pesTimer->illnessNeedIdx == i) ? 2.f : 1.f;
                need.value -= need.drainRate * mult * plagueMult * celebrateMult
                              * illnessMult * gameDt;
                if (need.value < 0.0f) need.value = 0.0f;
            }
        }

        // Worker fatigue: energy below 0.2 while working triggers production penalty
        if (auto* sched = registry.try_get<Schedule>(entity)) {
            const Need* energyNeed = needs.ByID(energyNeedId);
            float energy = energyNeed ? energyNeed->value : 1.f;
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
            sk.DecayAll(decay, 0.1f);
        }
    }
}
