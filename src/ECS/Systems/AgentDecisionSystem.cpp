#include "AgentDecisionSystem.h"
#include <cmath>
#include <limits>
#include <string>
#include "ECS/Components.h"

// Radius within which an NPC can interact with a production facility.
static constexpr float FACILITY_RANGE = 35.0f;
// Arrival threshold for reaching a settlement when migrating.
static constexpr float SETTLE_RANGE   = 130.0f;
// Note: migration threshold is now per-NPC (DeprivationTimer::migrateThreshold)
// so each NPC migrates at a different time, preventing mass simultaneous exodus.

// ---- Static helpers ----

static AgentBehavior BehaviorForNeed(NeedType type) {
    switch (type) {
        case NeedType::Hunger: return AgentBehavior::SeekingFood;
        case NeedType::Thirst: return AgentBehavior::SeekingWater;
        case NeedType::Energy: return AgentBehavior::SeekingSleep;
    }
    return AgentBehavior::Idle;
}

static ResourceType ResourceTypeForNeed(NeedType type) {
    switch (type) {
        case NeedType::Hunger: return ResourceType::Food;
        case NeedType::Thirst: return ResourceType::Water;
        case NeedType::Energy: return ResourceType::Shelter;
    }
    return ResourceType::Food;
}

static int NeedIndexForResource(ResourceType type) {
    switch (type) {
        case ResourceType::Food:    return (int)NeedType::Hunger;
        case ResourceType::Water:   return (int)NeedType::Thirst;
        case ResourceType::Shelter: return (int)NeedType::Energy;
    }
    return -1;
}

static bool InRange(float ax, float ay, float bx, float by, float r) {
    float dx = ax - bx, dy = ay - by;
    return (dx * dx + dy * dy) <= r * r;
}

static void MoveToward(Velocity& vel, const Position& from,
                        float tx, float ty, float speed) {
    float dx = tx - from.x, dy = ty - from.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.f) { vel.vx = vel.vy = 0.f; return; }
    vel.vx = (dx / dist) * speed;
    vel.vy = (dy / dist) * speed;
}

// ---- AgentDecisionSystem::FindNearestFacility ----

entt::entity AgentDecisionSystem::FindNearestFacility(entt::registry& registry,
                                                       ResourceType type,
                                                       entt::entity homeSettlement,
                                                       float px, float py) {
    entt::entity nearest  = entt::null;
    float        bestDist = std::numeric_limits<float>::max();

    auto view = registry.view<Position, ProductionFacility>();
    for (auto e : view) {
        const auto& fac = view.get<ProductionFacility>(e);
        if (fac.output != type) continue;
        if (fac.settlement != homeSettlement) continue;

        const auto& pos = view.get<Position>(e);
        float dx = pos.x - px, dy = pos.y - py;
        float dist = dx * dx + dy * dy;
        if (dist < bestDist) { bestDist = dist; nearest = e; }
    }
    return nearest;
}

// ---- AgentDecisionSystem::FindMigrationTarget ----
// Picks the reachable settlement with the most combined food + water stock.
// If the NPC has skills, adds an affinity bonus (20% of base score) for
// destinations whose primary facility matches the NPC's strongest skill.
// This makes skilled workers self-sort toward matching settlements over time.

entt::entity AgentDecisionSystem::FindMigrationTarget(entt::registry& registry,
                                                        entt::entity homeSettlement,
                                                        const Skills* skills) {
    // Determine the NPC's strongest skill (if any) for affinity matching.
    ResourceType affinityType = ResourceType::Food;
    bool         hasAffinity  = false;
    if (skills) {
        float best = skills->farming;
        affinityType = ResourceType::Food;
        if (skills->water_drawing > best) { best = skills->water_drawing; affinityType = ResourceType::Water; }
        if (skills->woodcutting   > best) {                               affinityType = ResourceType::Wood;  }
        // Only apply affinity bonus if the skill is meaningfully developed (> 0.25)
        hasAffinity = (best > 0.25f);
    }

    entt::entity best      = entt::null;
    float        bestScore = -1.f;

    registry.view<Road>().each([&](const Road& road) {
        if (road.blocked) return;
        entt::entity dest = entt::null;
        if (road.from == homeSettlement) dest = road.to;
        else if (road.to == homeSettlement) dest = road.from;
        else return;

        if (!registry.valid(dest)) return;
        const auto* sp = registry.try_get<Stockpile>(dest);
        if (!sp) return;

        float food  = sp->quantities.count(ResourceType::Food)
                      ? sp->quantities.at(ResourceType::Food)  : 0.f;
        float water = sp->quantities.count(ResourceType::Water)
                      ? sp->quantities.at(ResourceType::Water) : 0.f;
        float wood  = sp->quantities.count(ResourceType::Wood)
                      ? sp->quantities.at(ResourceType::Wood)  : 0.f;
        float total = food + water;

        // In cold seasons (Autumn/Winter), wood stock matters for warmth.
        // Settlements with good wood reserve get a bonus — cold NPCs flee to warmth.
        auto tmv = registry.view<TimeManager>();
        if (!tmv.empty()) {
            Season season = tmv.get<TimeManager>(*tmv.begin()).CurrentSeason();
            float heatMult = SeasonHeatDrainMult(season);
            if (heatMult > 0.f) {
                total += wood * heatMult * 1.5f;  // weight wood by how cold the season is
            }
        }

        // Plague penalty: NPCs strongly avoid plague-infected destinations
        if (const auto* ds = registry.try_get<Settlement>(dest)) {
            if (ds->modifierName == "Plague")
                total *= 0.20f;  // 80% less attractive — flee or avoid
        }

        // Skill-affinity bonus: +20% score if destination primarily produces
        // the resource matching the NPC's strongest skill.
        if (hasAffinity) {
            registry.view<ProductionFacility>().each([&](const ProductionFacility& fac) {
                if (fac.settlement == dest && fac.output == affinityType && fac.baseRate > 0.f)
                    total *= 1.20f;
            });
        }

        if (total > bestScore) { bestScore = total; best = dest; }
    });
    return best;
}

// ---- Main update ----

void AgentDecisionSystem::Update(entt::registry& registry, float realDt) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    float dt = timeView.get<TimeManager>(*timeView.begin()).GameDt(realDt);
    if (dt <= 0.f) return;

    // Exclude Haulers (TransportSystem handles them) and Player (PlayerInputSystem).
    auto view = registry.view<Needs, AgentState, Position, Velocity,
                               MoveSpeed, HomeSettlement, DeprivationTimer>(
                    entt::exclude<Hauler, PlayerTag>);

    for (auto entity : view) {
        auto& needs  = view.get<Needs>(entity);
        auto& state  = view.get<AgentState>(entity);
        auto& pos    = view.get<Position>(entity);
        auto& vel    = view.get<Velocity>(entity);
        float speed  = view.get<MoveSpeed>(entity).value;
        auto& home   = view.get<HomeSettlement>(entity);
        auto& timer  = view.get<DeprivationTimer>(entity);

        // ============================================================
        // SLEEPING: ScheduleSystem owns this state — skip entirely
        // ============================================================
        if (state.behavior == AgentBehavior::Sleeping) continue;

        // ============================================================
        // WORKING: only interrupt if a need is critical
        // ============================================================
        if (state.behavior == AgentBehavior::Working) {
            bool anyCritical = false;
            for (const auto& n : needs.list)
                if (n.value < n.criticalThreshold) { anyCritical = true; break; }
            if (!anyCritical) continue;
            // Critical need — fall through to seeking logic below
            state.behavior = AgentBehavior::Idle;
        }

        // ============================================================
        // MIGRATING: move toward target settlement, settle on arrival
        // ============================================================
        if (state.behavior == AgentBehavior::Migrating) {
            if (state.target == entt::null || !registry.valid(state.target)) {
                state.behavior = AgentBehavior::Idle;
                vel.vx = vel.vy = 0.f;
                continue;
            }
            const auto& destPos = registry.get<Position>(state.target);
            if (InRange(pos.x, pos.y, destPos.x, destPos.y, SETTLE_RANGE)) {
                // Arrived — adopt new home
                home.settlement       = state.target;
                timer.stockpileEmpty  = 0.f;
                state.behavior        = AgentBehavior::Idle;
                state.target          = entt::null;
                vel.vx = vel.vy       = 0.f;
            } else {
                MoveToward(vel, pos, destPos.x, destPos.y, speed);
            }
            continue;
        }

        // ============================================================
        // SATISFYING: refill need at the facility, check completion
        // ============================================================
        if (state.behavior == AgentBehavior::Satisfying) {
            if (state.target == entt::null || !registry.valid(state.target)) {
                state.behavior = AgentBehavior::Idle;
                vel.vx = vel.vy = 0.f;
                continue;
            }
            const auto& facPos = registry.get<Position>(state.target);
            const auto& fac    = registry.get<ProductionFacility>(state.target);

            if (!InRange(pos.x, pos.y, facPos.x, facPos.y, FACILITY_RANGE)) {
                state.behavior = AgentBehavior::Idle;
                state.target   = entt::null;
                vel.vx = vel.vy = 0.f;
                continue;
            }

            int idx = NeedIndexForResource(fac.output);
            if (idx >= 0) {
                needs.list[idx].value += needs.list[idx].refillRate * dt;
                if (needs.list[idx].value >= 1.f) {
                    needs.list[idx].value = 1.f;
                    state.behavior = AgentBehavior::Idle;
                    state.target   = entt::null;
                    vel.vx = vel.vy = 0.f;
                }
            }
            continue;
        }

        // ============================================================
        // IDLE / SEEKING: decide what to do this tick
        // ============================================================

        // -- Check migration trigger first --
        // NPCs in a plague settlement are more fearful and migrate at half the normal threshold.
        float effectiveMigrateThreshold = timer.migrateThreshold;
        if (const auto* hs = registry.try_get<Settlement>(home.settlement))
            if (hs->modifierName == "Plague")
                effectiveMigrateThreshold *= 0.50f;

        if (timer.stockpileEmpty >= effectiveMigrateThreshold) {
            const auto* skills = registry.try_get<Skills>(entity);
            entt::entity dest = FindMigrationTarget(registry, home.settlement, skills);
            if (dest != entt::null) {
                state.behavior       = AgentBehavior::Migrating;
                state.target         = dest;
                timer.stockpileEmpty = 0.f;

                // Log migration event
                auto lv = registry.view<EventLog>();
                if (lv.begin() != lv.end()) {
                    auto tv = registry.view<TimeManager>();
                    if (tv.begin() != tv.end()) {
                        const auto& tm2 = tv.get<TimeManager>(*tv.begin());
                        std::string from = "?", to = "?", who = "NPC";
                        if (auto* s = registry.try_get<Settlement>(home.settlement))
                            from = s->name;
                        if (auto* s = registry.try_get<Settlement>(dest))
                            to = s->name;
                        if (auto* n = registry.try_get<Name>(entity))
                            who = n->value;
                        lv.get<EventLog>(*lv.begin()).Push(
                            tm2.day, (int)tm2.hourOfDay,
                            who + " migrating " + from + " → " + to);
                    }
                }
                continue;
            }
        }

        // -- Find the most critical need --
        // Heat is handled passively by ConsumptionSystem (Wood stockpile → warmth),
        // so we skip it here — NPCs don't "seek" a heating facility.
        int   critIdx = -1;
        float lowest  = std::numeric_limits<float>::max();
        for (int i = 0; i < (int)needs.list.size(); ++i) {
            const auto& n = needs.list[i];
            if (n.type == NeedType::Heat) continue;   // passive need
            if (n.value < n.criticalThreshold && n.value < lowest) {
                lowest  = n.value;
                critIdx = i;
            }
        }

        if (critIdx == -1) {
            state.behavior = AgentBehavior::Idle;
            state.target   = entt::null;
            vel.vx = vel.vy = 0.f;
            continue;
        }

        ResourceType resType = ResourceTypeForNeed(needs.list[critIdx].type);
        state.behavior       = BehaviorForNeed(needs.list[critIdx].type);

        entt::entity fac = FindNearestFacility(registry, resType,
                                                home.settlement, pos.x, pos.y);
        if (fac == entt::null) {
            state.behavior = AgentBehavior::Idle;
            vel.vx = vel.vy = 0.f;
            continue;
        }

        state.target = fac;
        const auto& facPos = registry.get<Position>(fac);

        if (InRange(pos.x, pos.y, facPos.x, facPos.y, FACILITY_RANGE)) {
            state.behavior = AgentBehavior::Satisfying;
            vel.vx = vel.vy = 0.f;
        } else {
            MoveToward(vel, pos, facPos.x, facPos.y, speed);
        }
    }
}
