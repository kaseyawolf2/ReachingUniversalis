#include "ScheduleSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <random>
#include <set>

static constexpr float SLEEP_ARRIVE  = 25.f;   // distance at which NPC is "at settlement"
static constexpr float LEISURE_RADIUS = 80.f;  // wander radius around home settlement center

// Skill decay per game-hour while NOT actively working at a facility.
// At 0.005/day ≈ 0.0002/hr — slow enough that NPCs don't forget overnight,
// but fast enough that prolonged absence matters.
static constexpr float SKILL_DECAY_PER_HOUR = 0.005f / 24.f;

static std::mt19937                          s_rng{std::random_device{}()};
static std::uniform_real_distribution<float> s_angle(0.f, 6.28318f);
static std::uniform_real_distribution<float> s_radius(10.f, LEISURE_RADIUS);
static std::uniform_real_distribution<float> s_scatter(5.f, 30.f);

static void MoveToward(Velocity& vel, const Position& from,
                        float tx, float ty, float speed) {
    float dx = tx - from.x, dy = ty - from.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.f) { vel.vx = vel.vy = 0.f; return; }
    vel.vx = (dx / dist) * speed;
    vel.vy = (dy / dist) * speed;
}

static bool InRange(const Position& a, const Position& b, float r) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return (dx * dx + dy * dy) <= r * r;
}

void ScheduleSystem::Update(entt::registry& registry, float realDt) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    const auto& tm = timeView.get<TimeManager>(*timeView.begin());
    float gameDt = tm.GameDt(realDt);
    if (gameDt <= 0.f) return;

    int hour   = (int)tm.hourOfDay;
    Season season = tm.CurrentSeason();

    // Pre-build set of facilities that have an elder worker (age > 60, Working)
    // within arrival range. Used for the +20% mentor skill gain bonus.
    static constexpr float ELDER_AGE = 60.f;
    static constexpr float MENTOR_ARRIVE2 = 30.f * 30.f; // same as WORK_ARRIVE
    struct ElderMentorInfo { std::string name; entt::entity facility; };
    std::map<entt::entity, ElderMentorInfo> elderFacilities; // facility → elder info
    {
        auto facView = registry.view<Position, ProductionFacility>();
        registry.view<AgentState, Position, Age>(entt::exclude<Hauler, PlayerTag, ChildTag>).each(
            [&](auto e, const AgentState& st, const Position& ep, const Age& age) {
            if (st.behavior != AgentBehavior::Working) return;
            if (age.days <= ELDER_AGE) return;
            // Find which facility this elder is near
            for (auto fe : facView) {
                const auto& fp = facView.get<Position>(fe);
                float dx = ep.x - fp.x, dy = ep.y - fp.y;
                if (dx*dx + dy*dy <= MENTOR_ARRIVE2) {
                    if (elderFacilities.find(fe) == elderFacilities.end()) {
                        std::string ename = "An elder";
                        if (const auto* n = registry.try_get<Name>(e)) ename = n->value;
                        elderFacilities[fe] = {ename, fe};
                    }
                    break;
                }
            }
        });
    }

    // Elder mentor log: once per game-day per facility
    static std::map<entt::entity, int> s_lastMentorLogDay;

    // Exclude haulers (TransportSystem owns them) and the player.
    auto view = registry.view<Schedule, AgentState, Position, Velocity,
                               MoveSpeed, HomeSettlement>(
                    entt::exclude<Hauler, PlayerTag>);

    for (auto entity : view) {
        const auto& sched = view.get<Schedule>(entity);
        auto& state = view.get<AgentState>(entity);
        auto& pos   = view.get<Position>(entity);
        auto& vel   = view.get<Velocity>(entity);
        float speed = view.get<MoveSpeed>(entity).value;
        auto& home  = view.get<HomeSettlement>(entity);

        // ---- Age 15 graduation ----
        // When a child (ChildTag) reaches working age, boost their aptitude skill
        // toward the settlement's primary production and log the transition.
        if (registry.all_of<ChildTag>(entity)) {
            const auto* ageC = registry.try_get<Age>(entity);
            if (ageC && ageC->days >= 15.f) {
                // Find home settlement's primary production type
                ResourceType primary = ResourceType::Food;
                float bestRate = 0.f;
                if (home.settlement != entt::null && registry.valid(home.settlement)) {
                    registry.view<ProductionFacility>().each(
                        [&](const ProductionFacility& fac) {
                        if (fac.settlement == home.settlement && fac.baseRate > bestRate) {
                            bestRate = fac.baseRate;
                            primary  = fac.output;
                        }
                    });
                }
                if (auto* sk = registry.try_get<Skills>(entity))
                    sk->Advance(primary, 0.15f);

                // Capture the followed adult's name before removing ChildTag
                std::string raisedBy;
                if (state.target != entt::null && registry.valid(state.target))
                    if (const auto* pn = registry.try_get<Name>(state.target))
                        raisedBy = pn->value;

                // Inherit parent's last name (family lineage)
                if (!raisedBy.empty()) {
                    auto parentSpace = raisedBy.rfind(' ');
                    if (parentSpace != std::string::npos) {
                        std::string parentLast = raisedBy.substr(parentSpace + 1);
                        if (auto* n = registry.try_get<Name>(entity)) {
                            auto mySpace = n->value.rfind(' ');
                            if (mySpace != std::string::npos)
                                n->value = n->value.substr(0, mySpace + 1) + parentLast;
                            else
                                n->value += " " + parentLast;
                        }
                    }
                }

                registry.remove<ChildTag>(entity);

                auto logv2 = registry.view<EventLog>();
                if (!logv2.empty()) {
                    std::string who = "An NPC";
                    if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                    std::string where = "?";
                    if (home.settlement != entt::null && registry.valid(home.settlement))
                        if (const auto* s = registry.try_get<Settlement>(home.settlement))
                            where = s->name;
                    std::string msg = who + " came of age at " + where;
                    if (!raisedBy.empty())
                        msg += " (raised by " + raisedBy + ")";
                    // Append highest skill and value
                    if (const auto* sk = registry.try_get<Skills>(entity)) {
                        float best = sk->farming;
                        const char* skName = "Farming";
                        if (sk->water_drawing > best) { best = sk->water_drawing; skName = "Water"; }
                        if (sk->woodcutting   > best) { best = sk->woodcutting;   skName = "Woodcutting"; }
                        char skBuf[48];
                        std::snprintf(skBuf, sizeof(skBuf), " — best skill: %s %.0f%%", skName, best * 100.f);
                        msg += skBuf;
                    }
                    logv2.get<EventLog>(*logv2.begin()).Push(
                        tm.day, (int)tm.hourOfDay, msg);
                }
            }
        }

        // Seasonal day length adjustment:
        //   Winter: shorter days — sleep 2h earlier, wake 1h later, end work 2h earlier
        //   Summer: slightly longer productive hours — no adjustment needed (baseline)
        //   Spring/Autumn: base schedule
        int seasonSleepAdj  = (season == Season::Winter) ? -2 : 0;  // negative = earlier sleep
        int seasonWakeAdj   = (season == Season::Winter) ?  1 : 0;  // positive = later wake
        int seasonWorkEndAdj = (season == Season::Winter) ? -2 : 0; // negative = earlier end

        int effSleepHour = sched.sleepHour + seasonSleepAdj;
        int effWakeHour  = sched.wakeHour  + seasonWakeAdj;

        bool sleepTime = (hour >= effSleepHour || hour < effWakeHour);

        // Age affects work eligibility:
        //   < 12 days : young child — no work
        //  12–14 days : apprentice — 2-hour window (10:00–12:00) only
        //  > 60 days  : elder — retired, no work
        bool workEligible  = true;
        bool isApprentice  = false;
        int  workEndAdj    = sched.workEnd + seasonWorkEndAdj;
        if (const auto* age = registry.try_get<Age>(entity)) {
            if (age->days >= 12.f && age->days < 15.f) {
                isApprentice  = true;       // near-adult child: limited work window
                workEligible  = true;       // allow Working state
            } else if (age->days < 12.f) {
                workEligible  = false;      // young child: no work
            } else if (age->days > 60.f) {
                workEligible  = false;      // elder: fully retired
            }
        }

        // Apprentice work window: 10:00–12:00 only (2 game-hours).
        bool workTime = isApprentice
                        ? (hour >= 10 && hour < 12)
                        : (workEligible && hour >= sched.workStart && hour < workEndAdj);

        // ---- Transition into sleep ----
        if (sleepTime) {
            if (state.behavior != AgentBehavior::Sleeping) {
                state.behavior = AgentBehavior::Sleeping;
                state.target   = home.settlement;
                vel.vx = vel.vy = 0.f;
            }

            // Walk slowly toward settlement centre
            if (home.settlement != entt::null && registry.valid(home.settlement)) {
                const auto& homePos = registry.get<Position>(home.settlement);
                if (!InRange(pos, homePos, SLEEP_ARRIVE)) {
                    MoveToward(vel, pos, homePos.x, homePos.y, speed * 0.6f);
                } else {
                    vel.vx = vel.vy = 0.f;
                }
            }
            continue;
        }

        // ---- Wake up ----
        if (state.behavior == AgentBehavior::Sleeping) {
            state.behavior = AgentBehavior::Idle;
            state.target   = entt::null;
            vel.vx = vel.vy = 0.f;
            // Scatter waking NPCs so they don't all path-find from the exact same spot
            float ang = s_angle(s_rng);
            float rad = s_scatter(s_rng);
            pos.x += std::cos(ang) * rad;
            pos.y += std::sin(ang) * rad;
        }

        // ---- Drain work-stoppage strike timer ----
        bool onStrike = false;
        if (auto* dt = registry.try_get<DeprivationTimer>(entity)) {
            if (dt->strikeDuration > 0.f) {
                float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;
                dt->strikeDuration = std::max(0.f, dt->strikeDuration - gameHoursDt);
                if (dt->strikeDuration > 0.f) {
                    onStrike = true;
                } else {
                    // Strike just ended — nudge home settlement morale up
                    if (auto* sett = registry.try_get<Settlement>(home.settlement))
                        sett->morale = std::min(1.f, sett->morale + 0.05f);
                }
            }
        }

        // ---- Set Working during work hours (only when Idle — needs override this) ----
        // Strike NPCs refuse to work until their strikeDuration expires.
        if (workTime && state.behavior == AgentBehavior::Idle && !onStrike) {
            state.behavior = AgentBehavior::Working;
        }

        // ---- Clear Working when outside work hours ----
        if (!workTime && state.behavior == AgentBehavior::Working) {
            state.behavior = AgentBehavior::Idle;
        }

        // ---- Move Working NPCs toward their skill-matched facility in their settlement ----
        // NPCs prefer the facility that matches their strongest skill (aptitude-seeking):
        //   - a farmer prefers the farm, a water carrier prefers the well, etc.
        //   - if no matching facility exists here, fall back to nearest facility.
        // This makes skill specialisation visible: farmers cluster at farms, etc.
        if (state.behavior == AgentBehavior::Working &&
            home.settlement != entt::null && registry.valid(home.settlement)) {

            // Determine aptitude: resource type of the NPC's highest skill.
            ResourceType aptitude    = ResourceType::Food;
            bool         hasAptitude = false;
            if (const auto* skills = registry.try_get<Skills>(entity)) {
                float mx = std::max({skills->farming, skills->water_drawing, skills->woodcutting});
                if (mx > 0.25f) {
                    hasAptitude = true;
                    if      (skills->water_drawing == mx) aptitude = ResourceType::Water;
                    else if (skills->woodcutting   == mx) aptitude = ResourceType::Wood;
                    // else stays Food/farming
                }
            }

            entt::entity chosenFac  = entt::null;
            float        chosenDist = std::numeric_limits<float>::max();
            entt::entity nearestFac = entt::null;
            float        nearestDist = std::numeric_limits<float>::max();
            ResourceType chosenType  = ResourceType::Food;
            ResourceType nearestType = ResourceType::Food;

            registry.view<Position, ProductionFacility>().each(
                [&](auto fe, const Position& fpos, const ProductionFacility& fac) {
                if (fac.settlement != home.settlement) return;
                if (fac.baseRate <= 0.f) return;   // skip shelter nodes
                float dx = fpos.x - pos.x, dy = fpos.y - pos.y;
                float d  = dx*dx + dy*dy;
                if (d < nearestDist) { nearestDist = d; nearestFac = fe; nearestType = fac.output; }
                if (hasAptitude && fac.output == aptitude && d < chosenDist) {
                    chosenDist = d; chosenFac = fe; chosenType = fac.output;
                }
            });

            // Use aptitude-matched facility if found; fall back to nearest
            entt::entity workFac  = (chosenFac != entt::null) ? chosenFac  : nearestFac;
            ResourceType facType  = (chosenFac != entt::null) ? chosenType : nearestType;
            float        workDist = (chosenFac != entt::null) ? chosenDist : nearestDist;

            if (workFac != entt::null) {
                const auto& fpos = registry.get<Position>(workFac);
                static constexpr float WORK_ARRIVE = 30.f;
                if (workDist > WORK_ARRIVE * WORK_ARRIVE) {
                    MoveToward(vel, pos, fpos.x, fpos.y, speed * 0.8f);
                } else {
                    vel.vx = vel.vy = 0.f;
                    // Skill advancement while at the work site (very slow: ~0.1 gain per game-day)
                    static constexpr float SKILL_GAIN_PER_GAME_HOUR = 0.1f / 24.f;
                    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;
                    if (auto* skills = registry.try_get<Skills>(entity)) {
                        // +10% skill gain when the NPC's profession matches the facility type
                        float gainMult = 1.0f;
                        if (const auto* prof = registry.try_get<Profession>(entity))
                            if (prof->type == ProfessionForResource(facType))
                                gainMult = 1.1f;
                        // +20% skill gain when an elder (age > 60) is working at the same facility
                        auto mentorIt = elderFacilities.find(workFac);
                        if (mentorIt != elderFacilities.end()) {
                            // Don't mentor yourself
                            const auto* myAge = registry.try_get<Age>(entity);
                            if (!myAge || myAge->days <= ELDER_AGE)
                                gainMult *= 1.2f;
                            // Log once per game-day per facility
                            if (s_lastMentorLogDay[workFac] != tm.day) {
                                s_lastMentorLogDay[workFac] = tm.day;
                                auto logv = registry.view<EventLog>();
                                if (!logv.empty()) {
                                    const auto* sett = registry.try_get<Settlement>(home.settlement);
                                    std::string where = sett ? sett->name : "a settlement";
                                    logv.get<EventLog>(*logv.begin()).Push(tm.day, hour,
                                        mentorIt->second.name + " mentored workers at " + where + ".");
                                }
                            }
                        }
                        float before = skills->ForResource(facType);
                        skills->Advance(facType, SKILL_GAIN_PER_GAME_HOUR * gameHoursDt * gainMult);
                        float after = skills->ForResource(facType);

                        // Skill milestone log: Journeyman (0.5) and Master (0.9)
                        // Key: entity id * 10 + milestone index (0=Journeyman, 1=Master)
                        static std::set<uint64_t> s_milestones;
                        auto checkMilestone = [&](float threshold, int idx, const char* title) {
                            if (before < threshold && after >= threshold) {
                                uint64_t key = (uint64_t)entt::to_integral(entity) * 10 + idx;
                                if (s_milestones.insert(key).second) {
                                    auto lv = registry.view<EventLog>();
                                    if (!lv.empty()) {
                                        std::string who = "An NPC";
                                        if (const auto* n = registry.try_get<Name>(entity))
                                            who = n->value;
                                        const char* skName =
                                            (facType == ResourceType::Food)  ? "Farming" :
                                            (facType == ResourceType::Water) ? "Water"   : "Woodcutting";
                                        char buf[128];
                                        std::snprintf(buf, sizeof(buf),
                                            "%s reached %s %s.", who.c_str(), title, skName);
                                        lv.get<EventLog>(*lv.begin()).Push(
                                            tm.day, (int)tm.hourOfDay, buf);
                                    }
                                    // Journeyman (idx 0) and Master (idx 1) trigger visible celebration
                                    if (idx == 0 || idx == 1) {
                                        if (auto* as = registry.try_get<AgentState>(entity)) {
                                            as->behavior = AgentBehavior::Celebrating;
                                        }
                                        auto& tmr = registry.get_or_emplace<DeprivationTimer>(entity);
                                        tmr.skillCelebrateTimer = 0.5f; // 0.5 game-hours = ~30 real-seconds at 1×
                                    }
                                    // Master milestone boosts home settlement morale
                                    if (idx == 1) {
                                        if (const auto* hs = registry.try_get<HomeSettlement>(entity)) {
                                            if (hs->settlement != entt::null && registry.valid(hs->settlement)) {
                                                if (auto* settl = registry.try_get<Settlement>(hs->settlement))
                                                    settl->morale = std::min(1.f, settl->morale + 0.03f);
                                            }
                                        }
                                    }
                                }
                            }
                        };
                        checkMilestone(0.25f, 2, "Apprentice");
                        checkMilestone(0.5f,  0, "Journeyman");
                        checkMilestone(0.75f, 3, "Skilled");
                        checkMilestone(0.9f,  1, "Master");
                    }
                }
            }
        }

        // ---- Skill growth/decay ----
        // Children (< 15 days) passively grow all skills as they observe community work.
        // The skill that started highest (birth aptitude) grows to a higher cap (0.42)
        // while the other two cap at 0.35, reflecting natural specialisation tendency.
        // Adults not actively Working lose skill slowly (disuse decay).
        // Target: child reaches ~0.35 by age 15 if born at 0.08 (0.27 gain over 15 days).
        //   Gain: 0.27 / (15*24) ≈ 0.00075/hr (slightly faster than before to compensate
        //   for the lower starting value of 0.08 vs old 0.1)
        // Adults decay: 0.005/day = 0.000208/hr — slow, needs continuous practice to maintain.
        static constexpr float CHILD_SKILL_GAIN_PER_HOUR = 0.27f / (15.f * 24.f);
        static constexpr float CHILD_SKILL_CAP_BASE      = 0.35f;   // non-aptitude skills
        static constexpr float CHILD_SKILL_CAP_APTITUDE  = 0.42f;   // aptitude skill
        float gHrs = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;

        if (const auto* age2 = registry.try_get<Age>(entity)) {
            if (age2->days < 15.f) {
                // Child / apprentice: passive growth (observing community).
                // Apprentices in Working state get 2× the passive rate (hands-on learning).
                if (auto* skills = registry.try_get<Skills>(entity)) {
                    float multiplier = (isApprentice && state.behavior == AgentBehavior::Working)
                                       ? 2.0f : 1.0f;
                    float grow = CHILD_SKILL_GAIN_PER_HOUR * gHrs * multiplier;
                    // The highest skill is the aptitude — it gets a slightly higher cap.
                    float mx = std::max({skills->farming, skills->water_drawing, skills->woodcutting});
                    float capF = (skills->farming       == mx) ? CHILD_SKILL_CAP_APTITUDE : CHILD_SKILL_CAP_BASE;
                    float capW = (skills->water_drawing == mx) ? CHILD_SKILL_CAP_APTITUDE : CHILD_SKILL_CAP_BASE;
                    float capL = (skills->woodcutting   == mx) ? CHILD_SKILL_CAP_APTITUDE : CHILD_SKILL_CAP_BASE;
                    skills->farming       = std::min(capF, skills->farming       + grow);
                    skills->water_drawing = std::min(capW, skills->water_drawing + grow);
                    skills->woodcutting   = std::min(capL, skills->woodcutting   + grow);
                }
            } else if (state.behavior != AgentBehavior::Working) {
                // Adult not working: decay.
                // Elders (age > 65) decay twice as fast but retain tacit knowledge (floor 0.1).
                if (auto* skills = registry.try_get<Skills>(entity)) {
                    bool isElder = (age2->days > 65.f);
                    float decayMult = isElder ? 2.0f : 1.0f;
                    float floor     = isElder ? 0.1f  : 0.f;
                    float decay = SKILL_DECAY_PER_HOUR * gHrs * decayMult;
                    skills->farming       = std::max(floor, skills->farming       - decay);
                    skills->water_drawing = std::max(floor, skills->water_drawing - decay);
                    skills->woodcutting   = std::max(floor, skills->woodcutting   - decay);
                }
            }
        }

        // ---- Leisure wandering / child following ----
        // Idle NPCs outside work hours either follow the nearest adult (children)
        // or pick a random wander destination near home (adults).
        bool leisureTime = !sleepTime && !workTime;
        if (leisureTime && state.behavior == AgentBehavior::Idle &&
            home.settlement != entt::null && registry.valid(home.settlement)) {

            const auto& homePos = registry.get<Position>(home.settlement);

            if (!workEligible || isApprentice) {
                // Children (including apprentices): follow the nearest adult at their home settlement.
                // Cache the target in AgentState::target; re-evaluate when stale.
                bool needTarget = (state.target == entt::null ||
                                   !registry.valid(state.target));
                if (!needTarget) {
                    // Re-evaluate if cached adult is no longer at this settlement
                    if (const auto* ths = registry.try_get<HomeSettlement>(state.target))
                        needTarget = (ths->settlement != home.settlement);
                    else
                        needTarget = true;
                }
                if (needTarget) {
                    float        bestD2   = std::numeric_limits<float>::max();
                    entt::entity bestAdult = entt::null;
                    registry.view<Position, Age, HomeSettlement>(
                        entt::exclude<PlayerTag, Hauler>)
                        .each([&](auto ae, const Position& apos, const Age& aage,
                                  const HomeSettlement& ahs) {
                        if (ae == entity) return;
                        if (ahs.settlement != home.settlement) return;
                        if (aage.days < 15.f) return;   // skip other children
                        float dx = apos.x - pos.x, dy = apos.y - pos.y;
                        float d = dx*dx + dy*dy;
                        if (d < bestD2) { bestD2 = d; bestAdult = ae; }
                    });
                    state.target = bestAdult;
                }
                if (state.target != entt::null && registry.valid(state.target)) {
                    const auto& apos = registry.get<Position>(state.target);
                    float dx = apos.x - pos.x, dy = apos.y - pos.y;
                    if (dx*dx + dy*dy > 20.f * 20.f)
                        MoveToward(vel, pos, apos.x, apos.y, speed * 0.7f);
                    else
                        vel.vx = vel.vy = 0.f;
                } else {
                    // No adult found — wander near settlement centre (tighter radius)
                    if (std::abs(vel.vx) < 0.5f && std::abs(vel.vy) < 0.5f) {
                        float ang = s_angle(s_rng);
                        float rad = s_radius(s_rng) * 0.5f;
                        MoveToward(vel, pos,
                            homePos.x + std::cos(ang) * rad,
                            homePos.y + std::sin(ang) * rad,
                            speed * 0.4f);
                    }
                }
            } else {
                // Adults: normal leisure wandering
                // Evening (18–22): cluster closer to settlement centre
                if (std::abs(vel.vx) < 0.5f && std::abs(vel.vy) < 0.5f) {
                    float ang = s_angle(s_rng);
                    float rad = s_radius(s_rng);
                    if (hour >= 18 && hour < 22) rad *= 0.4f;
                    float wx  = homePos.x + std::cos(ang) * rad;
                    float wy  = homePos.y + std::sin(ang) * rad;
                    MoveToward(vel, pos, wx, wy, speed * 0.4f);
                }
            }
        }
    }
}
