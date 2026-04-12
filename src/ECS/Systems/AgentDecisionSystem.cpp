#include "AgentDecisionSystem.h"
#include <cmath>
#include <limits>
#include <random>
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
                                                        const Skills* skills,
                                                        const Profession* profession,
                                                        const MigrationMemory* memory) {
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

    // Determine profession-based affinity resource type (additional bonus on top of skills).
    ResourceType profAffinityType = ResourceType::Food;
    bool         hasProfAffinity  = false;
    if (profession) {
        switch (profession->type) {
            case ProfessionType::Farmer:       profAffinityType = ResourceType::Food;  hasProfAffinity = true; break;
            case ProfessionType::WaterCarrier: profAffinityType = ResourceType::Water; hasProfAffinity = true; break;
            case ProfessionType::Lumberjack:   profAffinityType = ResourceType::Wood;  hasProfAffinity = true; break;
            default: break;
        }
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

        // Profession-affinity bonus: additional +15% if profession matches settlement output.
        // Stacks with skill affinity — a skilled farmer who identifies as a Farmer
        // gets a combined 35% bonus toward farming settlements.
        if (hasProfAffinity) {
            registry.view<ProductionFacility>().each([&](const ProductionFacility& fac) {
                if (fac.settlement == dest && fac.output == profAffinityType && fac.baseRate > 0.f)
                    total *= 1.15f;
            });
        }

        // Migration memory bonus: prefer destinations remembered as having
        // cheaper food / water than the NPC's current home.
        // +20% if food was cheaper there; +10% if water was cheaper there.
        if (memory) {
            const auto* homeSett = registry.try_get<Settlement>(homeSettlement);
            const auto* destSett = registry.try_get<Settlement>(dest);
            if (homeSett && destSett) {
                const auto* homeSnap = memory->Get(homeSett->name);
                const auto* destSnap = memory->Get(destSett->name);
                if (homeSnap && destSnap) {
                    float memBonus = 1.f;
                    if (destSnap->food  < homeSnap->food)  memBonus += 0.20f;
                    if (destSnap->water < homeSnap->water) memBonus += 0.10f;
                    total *= memBonus;
                }
            }
        }

        if (total > bestScore) { bestScore = total; best = dest; }
    });
    return best;
}

// ---- Main update ----

void AgentDecisionSystem::Update(entt::registry& registry, float realDt) {
    // Charity frequency counter: counts lifetime charity acts per helper entity.
    // Pruned each frame for destroyed entities so it doesn't leak memory.
    static std::map<entt::entity, int> s_charityCount;
    for (auto it = s_charityCount.begin(); it != s_charityCount.end(); ) {
        if (!registry.valid(it->first)) it = s_charityCount.erase(it);
        else ++it;
    }

    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    const auto& tm = timeView.get<TimeManager>(*timeView.begin());
    float dt = tm.GameDt(realDt);
    if (dt <= 0.f) return;
    int currentHour = (int)tm.hourOfDay;

    // Exclude Haulers (TransportSystem handles them), Player (PlayerInputSystem),
    // and Bandits (handled in the bandit section at the end of Update).
    auto view = registry.view<Needs, AgentState, Position, Velocity,
                               MoveSpeed, HomeSettlement, DeprivationTimer>(
                    entt::exclude<Hauler, PlayerTag, BanditTag>);

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
        // CELEBRATING: move toward settlement centre at half speed.
        // Stays active while the home settlement has the "Festival" modifier.
        // Reverts to Idle when the festival ends.
        // ============================================================
        if (state.behavior == AgentBehavior::Celebrating) {
            // Personal celebration from a completed goal takes priority.
            bool personalCelebration = false;
            if (const auto* g = registry.try_get<Goal>(entity))
                personalCelebration = (g->celebrateTimer > 0.f);

            // Check if the festival is still active at home settlement
            bool festivalActive = false;
            if (home.settlement != entt::null && registry.valid(home.settlement)) {
                if (const auto* s = registry.try_get<Settlement>(home.settlement))
                    festivalActive = (s->modifierName == "Festival");
            }
            if (!festivalActive && !personalCelebration) {
                state.behavior = AgentBehavior::Idle;
                // Fall through to normal decision-making below
            } else {
                // Leave celebration if any need becomes critical (same check as WORKING)
                bool anyCritical = false;
                for (const auto& n : needs.list)
                    if (n.value < n.criticalThreshold) { anyCritical = true; break; }
                if (anyCritical) {
                    state.behavior = AgentBehavior::Idle;
                    // Fall through to normal seeking below; re-enters Celebrating next tick
                    // when no longer critical (festival still active)
                } else {
                    // Drift toward settlement centre at half speed
                    if (home.settlement != entt::null && registry.valid(home.settlement)) {
                        const auto& homePos = registry.get<Position>(home.settlement);
                        static constexpr float CELEBRATE_ARRIVE = 45.f;
                        float dx = homePos.x - pos.x, dy = homePos.y - pos.y;
                        if (dx*dx + dy*dy > CELEBRATE_ARRIVE * CELEBRATE_ARRIVE)
                            MoveToward(vel, pos, homePos.x, homePos.y, speed * 0.5f);
                        else
                            vel.vx = vel.vy = 0.f;
                    } else {
                        vel.vx = vel.vy = 0.f;
                    }
                    continue;
                }
            }
        }

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

                // Record new settlement's prices in migration memory on arrival.
                if (auto* mem = registry.try_get<MigrationMemory>(entity)) {
                    if (home.settlement != entt::null && registry.valid(home.settlement)) {
                        if (const auto* mkt = registry.try_get<Market>(home.settlement))
                            if (const auto* stt = registry.try_get<Settlement>(home.settlement))
                                mem->Record(stt->name,
                                    mkt->GetPrice(ResourceType::Food),
                                    mkt->GetPrice(ResourceType::Water),
                                    mkt->GetPrice(ResourceType::Wood));
                    }
                }
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
        // GRATITUDE: briefly move toward the NPC who recently helped you
        // ============================================================
        if (timer.gratitudeTimer > 0.f) {
            timer.gratitudeTimer -= dt;
            if (timer.gratitudeTarget != entt::null && registry.valid(timer.gratitudeTarget)) {
                const auto& tgtPos = registry.get<Position>(timer.gratitudeTarget);
                float gdx = tgtPos.x - pos.x, gdy = tgtPos.y - pos.y;
                float gdist2 = gdx*gdx + gdy*gdy;
                static constexpr float POLITE_DIST = 25.f;
                if (gdist2 <= POLITE_DIST * POLITE_DIST) {
                    // Close enough — stand still for the rest of the gratitude window
                    vel.vx = vel.vy = 0.f;
                } else {
                    MoveToward(vel, pos, tgtPos.x, tgtPos.y, speed * 0.7f);
                }
                state.behavior = AgentBehavior::Idle;
            } else {
                // Helper gone — cancel gratitude
                timer.gratitudeTimer  = 0.f;
                timer.gratitudeTarget = entt::null;
            }
            continue;
        }
        // Clear stale target once timer expires
        if (timer.gratitudeTarget != entt::null && timer.gratitudeTimer <= 0.f)
            timer.gratitudeTarget = entt::null;

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
            const auto* skills     = registry.try_get<Skills>(entity);
            const auto* profession = registry.try_get<Profession>(entity);
            auto*       memory     = registry.try_get<MigrationMemory>(entity);

            // Record current home prices before departing so the NPC can compare later.
            if (memory && home.settlement != entt::null && registry.valid(home.settlement)) {
                if (const auto* mkt  = registry.try_get<Market>(home.settlement))
                    if (const auto* stt = registry.try_get<Settlement>(home.settlement))
                        memory->Record(stt->name,
                            mkt->GetPrice(ResourceType::Food),
                            mkt->GetPrice(ResourceType::Water),
                            mkt->GetPrice(ResourceType::Wood));
            }

            entt::entity dest = FindMigrationTarget(registry, home.settlement, skills, profession, memory);
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

        // -- Theft from stockpile --
        // NPCs with very little money and no steal cooldown can steal 1 unit of their
        // most-needed resource from the home settlement stockpile.
        static constexpr float STEAL_MONEY_THRESHOLD = 5.f;   // below this balance → can steal
        static constexpr float STEAL_COOLDOWN_HOURS  = 48.f;  // game-hours between steals
        const auto* moneyComp = registry.try_get<Money>(entity);
        float currentBalance = moneyComp ? moneyComp->balance : 0.f;
        if (currentBalance < STEAL_MONEY_THRESHOLD && timer.stealCooldown <= 0.f &&
            home.settlement != entt::null && registry.valid(home.settlement))
        {
            // Find most-needed non-Heat resource (lowest need value)
            int   stealIdx = -1;
            float stealLow = std::numeric_limits<float>::max();
            for (int i = 0; i < (int)needs.list.size(); ++i) {
                const auto& n = needs.list[i];
                if (n.type == NeedType::Heat) continue;
                if (n.value < stealLow) { stealLow = n.value; stealIdx = i; }
            }
            if (stealIdx != -1) {
                ResourceType stealRes = ResourceTypeForNeed(needs.list[stealIdx].type);
                auto* sp  = registry.try_get<Stockpile>(home.settlement);
                auto* mkt = registry.try_get<Market>(home.settlement);
                auto* stl = registry.try_get<Settlement>(home.settlement);
                if (sp && sp->quantities.count(stealRes) && sp->quantities[stealRes] >= 1.f) {
                    sp->quantities[stealRes] -= 1.f;
                    // Credit the stolen unit to the thief's inventory-equivalent: increase need value
                    // (same as ConsumptionSystem does when food is consumed)
                    needs.list[stealIdx].value = std::min(1.f, needs.list[stealIdx].value + 0.3f);
                    // Deduct market value from settlement treasury (the settlement loses the good)
                    if (stl && mkt) {
                        float cost = mkt->GetPrice(stealRes);
                        stl->treasury -= cost;
                    }
                    timer.stealCooldown = STEAL_COOLDOWN_HOURS;
                    timer.theftCount++;

                    // Theft erodes social trust — lower settlement morale
                    if (stl) stl->morale = std::max(0.f, stl->morale - 0.05f);

                    // Social shame penalty: reduce all skills by 0.02 (ostracism effect)
                    if (auto* sk = registry.try_get<Skills>(entity)) {
                        sk->farming       = std::max(0.f, sk->farming       - 0.02f);
                        sk->water_drawing = std::max(0.f, sk->water_drawing - 0.02f);
                        sk->woodcutting   = std::max(0.f, sk->woodcutting   - 0.02f);
                    }

                    // Log the theft
                    auto lv = registry.view<EventLog>();
                    if (lv.begin() != lv.end()) {
                        auto tv = registry.view<TimeManager>();
                        if (tv.begin() != tv.end()) {
                            const auto& tm2 = tv.get<TimeManager>(*tv.begin());
                            std::string who = "NPC", where = "?";
                            if (auto* n = registry.try_get<Name>(entity))   who   = n->value;
                            if (stl)                                          where = stl->name;
                            std::string resName =
                                (stealRes == ResourceType::Food)    ? "food"    :
                                (stealRes == ResourceType::Water)   ? "water"   :
                                (stealRes == ResourceType::Wood)    ? "wood"    : "goods";
                            lv.get<EventLog>(*lv.begin()).Push(
                                tm2.day, (int)tm2.hourOfDay,
                                who + " stole " + resName + " from " + where + ".");

                            // Exile after 3 thefts: clear home settlement
                            if (timer.theftCount >= 3) {
                                std::string exileName = where;
                                home.settlement = entt::null;
                                lv.get<EventLog>(*lv.begin()).Push(
                                    tm2.day, (int)tm2.hourOfDay,
                                    who + " exiled from " + exileName + " for repeated theft.");
                            }
                        }
                    }
                }
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

            // ---- Evening gathering (hours 18–21) ----
            // Idle NPCs drift toward their home settlement centre at dusk,
            // making the world visually alive: people return home in the evening.
            if (currentHour >= 18 && currentHour < 21 &&
                home.settlement != entt::null && registry.valid(home.settlement)) {
                const auto& homePos = registry.get<Position>(home.settlement);
                static constexpr float GATHER_ARRIVE = 40.f;
                float dx = homePos.x - pos.x, dy = homePos.y - pos.y;
                float dist2 = dx*dx + dy*dy;
                if (dist2 > GATHER_ARRIVE * GATHER_ARRIVE) {
                    MoveToward(vel, pos, homePos.x, homePos.y, speed * 0.6f);
                } else {
                    vel.vx = vel.vy = 0.f;
                }
            } else {
                vel.vx = vel.vy = 0.f;
            }
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

    // ============================================================
    // GOSSIP / PRICE SHARING
    // When two NPCs from different settlements are within 30 units,
    // the "visitor's" home Market prices nudge 5% toward the local
    // settlement's prices. Runs at most once per 6 game-hours per NPC.
    // ============================================================
    static constexpr float GOSSIP_RADIUS    = 30.f;
    static constexpr float GOSSIP_NUDGE     = 0.05f;   // 5% nudge toward other's prices
    static constexpr float GOSSIP_COOLDOWN  = 6.f;     // game-hours between gossip events
    float gameHoursDt = dt * GAME_MINS_PER_REAL_SEC / 60.f;

    // Collect snapshot of NPC positions/home-settlements for the O(N²) check.
    // We use a simple vector to avoid re-querying inside nested loops.
    struct GossipEntry {
        entt::entity entity;
        float        x, y;
        entt::entity homeSettl;
    };
    std::vector<GossipEntry> gossipAgents;
    registry.view<Position, HomeSettlement, DeprivationTimer>(
        entt::exclude<Hauler, PlayerTag>).each(
        [&](auto e, const Position& p, const HomeSettlement& hs, DeprivationTimer& tmr) {
            // Drain cooldown every frame
            if (tmr.gossipCooldown > 0.f)
                tmr.gossipCooldown = std::max(0.f, tmr.gossipCooldown - gameHoursDt);
            if (hs.settlement != entt::null && registry.valid(hs.settlement))
                gossipAgents.push_back({ e, p.x, p.y, hs.settlement });
        });

    for (std::size_t i = 0; i < gossipAgents.size(); ++i) {
        auto& A = gossipAgents[i];
        auto* tmrA = registry.try_get<DeprivationTimer>(A.entity);
        if (!tmrA || tmrA->gossipCooldown > 0.f) continue;

        // A needs a Market at their home settlement to update
        auto* mktA = registry.try_get<Market>(A.homeSettl);
        if (!mktA) continue;

        for (std::size_t j = i + 1; j < gossipAgents.size(); ++j) {
            auto& B = gossipAgents[j];
            if (B.homeSettl == A.homeSettl) continue;   // same settlement — no gossip

            float dx = B.x - A.x, dy = B.y - A.y;
            if (dx*dx + dy*dy > GOSSIP_RADIUS * GOSSIP_RADIUS) continue;

            auto* tmrB = registry.try_get<DeprivationTimer>(B.entity);
            auto* mktB = registry.try_get<Market>(B.homeSettl);
            if (!mktB) continue;

            // Nudge A's home prices toward B's, and B's home prices toward A's.
            for (auto& [res, priceA] : mktA->price) {
                float priceB = mktB->GetPrice(res);
                priceA += (priceB - priceA) * GOSSIP_NUDGE;
            }
            // A learns B's settlement prices (for migration decision-making)
            if (auto* memA = registry.try_get<MigrationMemory>(A.entity)) {
                if (const auto* sttB = registry.try_get<Settlement>(B.homeSettl))
                    memA->Record(sttB->name,
                        mktB->GetPrice(ResourceType::Food),
                        mktB->GetPrice(ResourceType::Water),
                        mktB->GetPrice(ResourceType::Wood));
            }
            if (tmrB && tmrB->gossipCooldown <= 0.f) {
                for (auto& [res, priceB] : mktB->price) {
                    float priceA = mktA->GetPrice(res);
                    priceB += (priceA - priceB) * GOSSIP_NUDGE;
                }
                // B learns A's settlement prices
                if (auto* memB = registry.try_get<MigrationMemory>(B.entity)) {
                    if (const auto* sttA = registry.try_get<Settlement>(A.homeSettl))
                        memB->Record(sttA->name,
                            mktA->GetPrice(ResourceType::Food),
                            mktA->GetPrice(ResourceType::Water),
                            mktA->GetPrice(ResourceType::Wood));
                }
                tmrB->gossipCooldown = GOSSIP_COOLDOWN;
            }
            tmrA->gossipCooldown = GOSSIP_COOLDOWN;
            break;  // A gossips with at most one NPC per cooldown window
        }
    }

    // ============================================================
    // FAMILY PAIRING
    // Every 12 game-hours, find pairs of unpaired adults (age ≥ 18,
    // same settlement, no FamilyTag) and give them a shared FamilyTag.
    // The family name is the most common surname at that settlement;
    // on a tie the first NPC's surname is used.
    // ============================================================
    static constexpr float FAMILY_CHECK_INTERVAL = 12.f;   // game-hours
    static float s_familyAccum = 0.f;
    s_familyAccum += gameHoursDt;
    if (s_familyAccum >= FAMILY_CHECK_INTERVAL) {
        s_familyAccum -= FAMILY_CHECK_INTERVAL;

        // Collect unpaired adults grouped by settlement
        struct UnpairedAdult {
            entt::entity entity;
            std::string  surname;
        };
        std::map<entt::entity, std::vector<UnpairedAdult>> bySettlement;

        registry.view<Age, HomeSettlement, Name>(
            entt::exclude<FamilyTag, ChildTag, Hauler, PlayerTag>).each(
            [&](auto e, const Age& age, const HomeSettlement& hs, const Name& n) {
                if (age.days < 18.f) return;
                if (hs.settlement == entt::null || !registry.valid(hs.settlement)) return;
                std::string surname;
                auto sp = n.value.rfind(' ');
                if (sp != std::string::npos) surname = n.value.substr(sp + 1);
                bySettlement[hs.settlement].push_back({ e, surname });
            });

        for (auto& [settl, adults] : bySettlement) {
            if (adults.size() < 2) continue;

            // Build surname frequency map from ALL residents (adults + paired) for tie-breaking.
            std::map<std::string, int> surnameFreq;
            registry.view<HomeSettlement, Name>(entt::exclude<ChildTag, PlayerTag>).each(
                [&](const HomeSettlement& hs2, const Name& n2) {
                    if (hs2.settlement != settl) return;
                    auto sp2 = n2.value.rfind(' ');
                    if (sp2 != std::string::npos)
                        ++surnameFreq[n2.value.substr(sp2 + 1)];
                });

            // Pair adults two-by-two
            for (std::size_t i = 0; i + 1 < adults.size(); i += 2) {
                auto& A = adults[i];
                auto& B = adults[i + 1];

                // Determine family name: most common surname at this settlement
                std::string familyName = A.surname;
                if (!surnameFreq.empty()) {
                    auto best = std::max_element(surnameFreq.begin(), surnameFreq.end(),
                        [](const auto& x, const auto& y){ return x.second < y.second; });
                    if (!best->first.empty()) familyName = best->first;
                }
                if (familyName.empty()) familyName = B.surname;
                if (familyName.empty()) continue;

                registry.emplace_or_replace<FamilyTag>(A.entity, FamilyTag{ familyName });
                registry.emplace_or_replace<FamilyTag>(B.entity, FamilyTag{ familyName });
            }
        }
    }

    // ============================================================
    // CHARITY: NPC helps starving neighbour
    // A well-fed wealthy NPC (Hunger > 0.8, Money > 20g) within
    // CHARITY_RADIUS units of a starving NPC (Hunger < 0.2) gifts 5g.
    // The starving NPC uses it to buy food immediately (market purchase).
    // Happens at most once per 24 game-hours per helper.
    // ============================================================
    static constexpr float CHARITY_RADIUS   = 80.f;
    static constexpr float CHARITY_GIFT     = 5.f;
    static constexpr float CHARITY_COOLDOWN = 24.f;   // game-hours
    static constexpr float HUNGER_HELPER    = 0.8f;   // well-fed threshold
    static constexpr float HUNGER_STARVING  = 0.2f;   // starving threshold
    static constexpr float MONEY_HELPER_MIN = 20.f;   // must have at least this to donate

    // Drain charity timers and build candidate list
    struct CharityEntry {
        entt::entity entity;
        float        x, y;
        float        hunger;
        float        balance;
        entt::entity homeSettl;
        bool         canHelp;     // well-fed + rich + cooldown done
        bool         isStarving;
    };
    std::vector<CharityEntry> charityAgents;
    registry.view<Position, Needs, Money, HomeSettlement, DeprivationTimer>(
        entt::exclude<Hauler, PlayerTag, ChildTag>).each(
        [&](auto e, const Position& p, const Needs& n, const Money& m,
            const HomeSettlement& hs, DeprivationTimer& tmr) {
            tmr.charityTimer = std::max(0.f, tmr.charityTimer - gameHoursDt);
            tmr.helpedTimer  = std::max(0.f, tmr.helpedTimer  - gameHoursDt);
            if (hs.settlement == entt::null || !registry.valid(hs.settlement)) return;
            float hunger = n.list[(int)NeedType::Hunger].value;
            charityAgents.push_back({
                e, p.x, p.y, hunger, m.balance, hs.settlement,
                /*canHelp=*/  (hunger >= HUNGER_HELPER && m.balance >= MONEY_HELPER_MIN
                               && tmr.charityTimer <= 0.f),
                /*isStarving*/(hunger < HUNGER_STARVING)
            });
        });

    // Get EventLog for charity messages
    auto elv2 = registry.view<EventLog>();
    EventLog* charityLog = (elv2.begin() == elv2.end())
                           ? nullptr : &elv2.get<EventLog>(*elv2.begin());
    auto tmv2 = registry.view<TimeManager>();
    int  charityDay  = 1;
    int  charityHour = 0;
    if (tmv2.begin() != tmv2.end()) {
        const auto& ctm = tmv2.get<TimeManager>(*tmv2.begin());
        charityDay  = ctm.day;
        charityHour = (int)ctm.hourOfDay;
    }

    for (auto& helper : charityAgents) {
        if (!helper.canHelp) continue;

        for (auto& starving : charityAgents) {
            if (starving.entity == helper.entity) continue;
            if (!starving.isStarving) continue;

            float dx = starving.x - helper.x, dy = starving.y - helper.y;
            if (dx*dx + dy*dy > CHARITY_RADIUS * CHARITY_RADIUS) continue;

            // Transfer gold: helper → starving NPC (peer transfer, gold flow rule satisfied)
            auto* helperMoney   = registry.try_get<Money>(helper.entity);
            auto* starvingMoney = registry.try_get<Money>(starving.entity);
            auto* starvingTmr   = registry.try_get<DeprivationTimer>(starving.entity);
            if (!helperMoney || !starvingMoney) continue;

            helperMoney->balance  -= CHARITY_GIFT;
            starvingMoney->balance += CHARITY_GIFT;

            // Immediately buy food for the starving NPC at home market price
            auto* mkt = registry.try_get<Market>(starving.homeSettl);
            auto* sp  = registry.try_get<Stockpile>(starving.homeSettl);
            auto* sett = registry.try_get<Settlement>(starving.homeSettl);
            if (mkt && sp && sett) {
                float price = mkt->GetPrice(ResourceType::Food);
                if (starvingMoney->balance >= price) {
                    starvingMoney->balance -= price;
                    sett->treasury         += price;
                    sp->quantities[ResourceType::Food] += 1.f;
                }
            }

            // Reset the starving NPC's purchaseTimer so ConsumptionSystem acts promptly.
            // Mark them as recently helped (shown in HUD tooltip for 1 game-hour).
            // Set gratitude walk: move toward helper for 30–60 real-seconds.
            if (starvingTmr) {
                starvingTmr->purchaseTimer   = 0.f;
                starvingTmr->helpedTimer     = 1.f;   // 1 game-hour display window
                starvingTmr->gratitudeTarget = helper.entity;
                static std::uniform_real_distribution<float> s_gratDist(30.f, 60.f);
                static std::mt19937 s_gratRng{ std::random_device{}() };
                starvingTmr->gratitudeTimer  = s_gratDist(s_gratRng);
            }

            // Set helper cooldown
            auto* helperTmr = registry.try_get<DeprivationTimer>(helper.entity);
            if (helperTmr) helperTmr->charityTimer = CHARITY_COOLDOWN;
            helper.canHelp = false;   // don't help a second NPC this frame

            // Warmth "warm glow" buff: giving charity raises the helper's Heat need slightly.
            if (auto* helperNeeds = registry.try_get<Needs>(helper.entity)) {
                auto& heat = helperNeeds->list[(int)NeedType::Heat].value;
                heat = std::min(1.f, heat + 0.15f);
            }

            // Increment charity counter for this helper
            int charityN = ++s_charityCount[helper.entity];

            // Log — name both helper and recipient, settlement, and frequency
            if (charityLog) {
                std::string who      = "An NPC";
                std::string whom     = "a neighbour";
                std::string at       = "";
                if (const auto* n = registry.try_get<Name>(helper.entity))   who  = n->value;
                if (const auto* n = registry.try_get<Name>(starving.entity)) whom = n->value;
                if (sett) at = " at " + sett->name;
                std::string suffix = (charityN > 1)
                    ? " (x" + std::to_string(charityN) + ")" : "";
                charityLog->Push(charityDay, charityHour,
                    who + " helped " + whom + at + "." + suffix);
            }
            break;   // helper gives to at most one starving NPC per cooldown window
        }
    }

    // ============================================================
    // BANDIT PROMOTION & BEHAVIOUR
    // Exiles (home.settlement == entt::null) with balance < 2g for
    // 48+ game-hours become bandits (BanditTag). Bandits lurk near
    // the nearest Road midpoint and intercept haulers within 40 units,
    // stealing 30% of cargo (converted to gold at 3g/unit). Removed
    // when balance recovers above 20g.
    // ============================================================
    static constexpr float BANDIT_POVERTY_THRESH  = 2.f;    // gold below this → accrue poverty
    static constexpr float BANDIT_PROMOTE_HOURS   = 48.f;   // poverty hours before turning bandit
    static constexpr float BANDIT_RECOVER_BALANCE = 20.f;   // gold to go straight again
    static constexpr float BANDIT_INTERCEPT_RANGE = 40.f;   // units to intercept a hauler
    static constexpr float BANDIT_STEAL_FRACTION  = 0.30f;  // fraction of cargo to steal
    static constexpr float BANDIT_CARGO_GOLD_RATE = 3.f;    // gold per stolen cargo unit

    auto banditELV = registry.view<EventLog>();
    EventLog* banditLog = (banditELV.begin() == banditELV.end())
                          ? nullptr : &banditELV.get<EventLog>(*banditELV.begin());

    // Iterate all NPCs that could be bandits (includes current BanditTag entities).
    registry.view<Position, Velocity, MoveSpeed, Needs, AgentState,
                  HomeSettlement, DeprivationTimer, Money>(
        entt::exclude<Hauler, PlayerTag, ChildTag>).each(
        [&](auto e, Position& pos, Velocity& vel, const MoveSpeed& spd,
            Needs& /*needs*/, AgentState& state, HomeSettlement& home,
            DeprivationTimer& timer, Money& money)
        {
            bool isBanditNow = registry.all_of<BanditTag>(e);

            // NPCs with a home cannot be bandits
            if (home.settlement != entt::null && registry.valid(home.settlement)) {
                if (isBanditNow) {
                    registry.remove<BanditTag>(e);
                    isBanditNow = false;
                }
                timer.banditPovertyTimer = 0.f;
                return;
            }

            // Recover from banditry if they scraped together enough money
            if (isBanditNow && money.balance >= BANDIT_RECOVER_BALANCE) {
                registry.remove<BanditTag>(e);
                isBanditNow = false;
                timer.banditPovertyTimer = 0.f;
            }

            // Poverty accumulation → promotion
            if (!isBanditNow) {
                if (money.balance < BANDIT_POVERTY_THRESH) {
                    timer.banditPovertyTimer += gameHoursDt;
                    if (timer.banditPovertyTimer >= BANDIT_PROMOTE_HOURS) {
                        registry.emplace_or_replace<BanditTag>(e);
                        isBanditNow = true;
                        if (banditLog) {
                            std::string name = "An exile";
                            if (const auto* n = registry.try_get<Name>(e)) name = n->value;
                            banditLog->Push(charityDay, charityHour,
                                name + " has turned bandit.");
                        }
                    }
                } else {
                    timer.banditPovertyTimer = 0.f;
                }
            }

            if (!isBanditNow) return;

            // ---- Bandit movement: lurk near nearest road midpoint ----
            float bestRoadD2 = std::numeric_limits<float>::max();
            float lurk_x = pos.x, lurk_y = pos.y;
            registry.view<Road>().each([&](const Road& road) {
                if (road.blocked) return;
                const auto* pa = registry.try_get<Position>(road.from);
                const auto* pb = registry.try_get<Position>(road.to);
                if (!pa || !pb) return;
                float mx = (pa->x + pb->x) * 0.5f;
                float my = (pa->y + pb->y) * 0.5f;
                float dx2 = mx - pos.x, dy2 = my - pos.y;
                float d2  = dx2*dx2 + dy2*dy2;
                if (d2 < bestRoadD2) { bestRoadD2 = d2; lurk_x = mx; lurk_y = my; }
            });

            // ---- Try to intercept a nearby hauler ----
            bool intercepted = false;
            registry.view<Position, Hauler, Inventory, Money>(
                entt::exclude<PlayerTag>).each(
                [&](auto haulerE, const Position& hpos, const Hauler&,
                    Inventory& haulerInv, Money& /*haulerMoney*/)
                {
                    if (intercepted) return;
                    float dx3 = hpos.x - pos.x, dy3 = hpos.y - pos.y;
                    if (dx3*dx3 + dy3*dy3 > BANDIT_INTERCEPT_RANGE * BANDIT_INTERCEPT_RANGE)
                        return;

                    // Steal a fraction of each cargo type
                    float stolenGold = 0.f;
                    for (auto& [res, qty] : haulerInv.contents) {
                        if (qty <= 0) continue;
                        int stealQty = std::max(1, (int)(qty * BANDIT_STEAL_FRACTION));
                        stealQty = std::min(stealQty, qty);
                        haulerInv.contents[res] -= stealQty;
                        stolenGold += stealQty * BANDIT_CARGO_GOLD_RATE;
                    }
                    if (stolenGold > 0.f) {
                        money.balance += stolenGold;
                        intercepted    = true;
                        if (banditLog) {
                            std::string bandName = "A bandit";
                            if (const auto* n = registry.try_get<Name>(e)) bandName = n->value;
                            banditLog->Push(charityDay, charityHour,
                                bandName + " ambushed a hauler on the road.");
                        }
                        // Flee away from the hauler
                        float dist = std::sqrt(dx3*dx3 + dy3*dy3);
                        if (dist > 0.1f) {
                            vel.vx = -(dx3 / dist) * spd.value;
                            vel.vy = -(dy3 / dist) * spd.value;
                        }
                    }
                });

            if (!intercepted) {
                // Drift toward road lurk point
                static constexpr float LURK_ARRIVE = 20.f;
                float dx4 = lurk_x - pos.x, dy4 = lurk_y - pos.y;
                if (dx4*dx4 + dy4*dy4 > LURK_ARRIVE * LURK_ARRIVE)
                    MoveToward(vel, pos, lurk_x, lurk_y, spd.value * 0.8f);
                else
                    vel.vx = vel.vy = 0.f;
            }

            state.behavior = AgentBehavior::Idle;
        });

    // ============================================================
    // PERSONAL GOAL SYSTEM
    // Every frame: update progress for all NPCs with a Goal component.
    // When progress >= target: log a celebration, set Celebrating state
    // for 2 game-hours, then assign a fresh random goal.
    // ============================================================
    static std::mt19937       s_goalRng{ std::random_device{}() };
    static std::uniform_int_distribution<int> s_goalTypeDist(0, 3);

    registry.view<Goal>(entt::exclude<PlayerTag>).each(
        [&](auto e, Goal& goal)
        {
            // Drain personal celebration timer
            if (goal.celebrateTimer > 0.f) {
                goal.celebrateTimer = std::max(0.f, goal.celebrateTimer - gameHoursDt);
                return;  // still celebrating — skip progress check this tick
            }

            // Update progress for the active goal type
            switch (goal.type) {
                case GoalType::SaveGold:
                    if (const auto* m = registry.try_get<Money>(e))
                        goal.progress = m->balance;
                    break;
                case GoalType::ReachAge:
                    if (const auto* a = registry.try_get<Age>(e))
                        goal.progress = a->days;
                    break;
                case GoalType::FindFamily:
                    goal.progress = registry.all_of<FamilyTag>(e) ? 1.f : 0.f;
                    break;
                case GoalType::BecomeHauler:
                    goal.progress = registry.all_of<Hauler>(e) ? 1.f : 0.f;
                    break;
            }

            if (goal.progress < goal.target) return;  // not yet met

            // ---- Goal completed! ----
            // Log the event
            auto gelv = registry.view<EventLog>();
            if (gelv.begin() != gelv.end()) {
                std::string who = "An NPC";
                if (const auto* n = registry.try_get<Name>(e)) who = n->value;
                std::string msg;
                switch (goal.type) {
                    case GoalType::SaveGold:
                        msg = who + " reached their savings goal!";    break;
                    case GoalType::ReachAge:
                        msg = who + " celebrated a life milestone!";   break;
                    case GoalType::FindFamily:
                        msg = who + " found a family!";                break;
                    case GoalType::BecomeHauler:
                        msg = who + " achieved their dream of becoming a merchant!"; break;
                }
                gelv.get<EventLog>(*gelv.begin()).Push(charityDay, charityHour, msg);
            }

            // Start personal celebration: 2 game-hours for regular NPCs
            goal.celebrateTimer = 2.f;
            if (!registry.all_of<Hauler>(e)) {
                if (auto* st = registry.try_get<AgentState>(e))
                    st->behavior = AgentBehavior::Celebrating;
            }

            // Assign a new goal (avoid immediately re-assigning the same type)
            GoalType newType = static_cast<GoalType>(s_goalTypeDist(s_goalRng));
            if (newType == goal.type)
                newType = static_cast<GoalType>((static_cast<int>(newType) + 1) % 4);

            goal.type     = newType;
            goal.progress = 0.f;
            switch (newType) {
                case GoalType::SaveGold: {
                    float bal = registry.try_get<Money>(e)
                                ? registry.get<Money>(e).balance : 0.f;
                    goal.target = std::max(50.f, bal + 75.f);   // save 75g more
                    break;
                }
                case GoalType::ReachAge: {
                    float days = registry.try_get<Age>(e)
                                 ? registry.get<Age>(e).days : 0.f;
                    goal.target = days + 20.f;
                    break;
                }
                case GoalType::FindFamily:
                case GoalType::BecomeHauler:
                    goal.target = 1.f;
                    break;
            }
        });
}
