#include "ScheduleSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <unordered_map>

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

void ScheduleSystem::Update(entt::registry& registry, float realDt, const WorldSchema& schema) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    const auto& tm = timeView.get<TimeManager>(*timeView.begin());
    float gameDt = tm.GameDt(realDt);
    if (gameDt <= 0.f) return;

    int hour   = (int)tm.hourOfDay;
    SeasonID seasonId = tm.CurrentSeason();
    float seasonHeatMod = GetSeasonHeatDrainMod(seasonId, schema);

    // ---- Early exit: cache expensive pre-computations when hour hasn't changed ----
    static int      s_cachedHour = -1;
    static int      s_cachedDay  = -1;
    static SeasonID s_cachedSeason = 0;
    bool hourChanged = (hour != s_cachedHour || tm.day != s_cachedDay || seasonId != s_cachedSeason);
    if (hourChanged) {
        s_cachedHour   = hour;
        s_cachedDay    = tm.day;
        s_cachedSeason = seasonId;
    }

    // Pre-build set of facilities that have an elder worker (age > 60, Working)
    // within arrival range. Used for the +20% mentor skill gain bonus.
    // Only rebuild when the hour changes — elder positions are stable within an hour.
    static constexpr float ELDER_AGE = 60.f;
    static constexpr float MENTOR_ARRIVE2 = 30.f * 30.f; // same as WORK_ARRIVE
    struct ElderMentorInfo { std::string name; entt::entity facility; };
    static std::map<entt::entity, ElderMentorInfo> elderFacilities;
    if (hourChanged) {
        elderFacilities.clear();
        auto facView = registry.view<Position, ProductionFacility>();
        registry.view<AgentState, Position, Age>(entt::exclude<Hauler, PlayerTag, ChildTag>).each(
            [&](auto e, const AgentState& st, const Position& ep, const Age& age) {
            if (st.behavior != AgentBehavior::Working) return;
            if (age.days <= ELDER_AGE) return;
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

    // Pre-cache facility positions per settlement (rebuilt each hour)
    struct FacInfo { entt::entity entity; Position pos; int output; float baseRate; };
    static std::unordered_map<entt::entity, std::vector<FacInfo>> s_facBySettlement;
    if (hourChanged) {
        s_facBySettlement.clear();
        registry.view<Position, ProductionFacility>().each(
            [&](auto fe, const Position& fpos, const ProductionFacility& fac) {
                if (fac.baseRate <= 0.f) return;
                s_facBySettlement[fac.settlement].push_back({fe, fpos, fac.output, fac.baseRate});
            });
    }

    // Elder mentor log: once per game-day per facility
    static std::map<entt::entity, int> s_lastMentorLogDay;

    // Exclude haulers (TransportSystem owns them) and the player.
    auto view = registry.view<Schedule, AgentState, Position, Velocity,
                               MoveSpeed, HomeSettlement>(
                    entt::exclude<Hauler, PlayerTag>);

    for (auto entity : view) {
        auto& sched = view.get<Schedule>(entity);
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
                int primary = RES_FOOD;
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
        // Cold seasons (heatDrainMod >= 0.8) get shorter days like winter
        bool isColdSeason = (seasonHeatMod >= 0.8f);
        int seasonSleepAdj  = isColdSeason ? -2 : 0;  // negative = earlier sleep
        int seasonWakeAdj   = isColdSeason ?  1 : 0;  // positive = later wake
        int seasonWorkEndAdj = isColdSeason ? -2 : 0;  // negative = earlier end

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
                sched.consecutiveWorkHours = 0;
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
                    if (auto* sett = registry.try_get<Settlement>(home.settlement)) {
                        sett->morale = std::min(1.f, sett->morale + 0.05f);
                        // Log strike end once per settlement per tick
                        static std::map<entt::entity, int> s_strikeEndLogged;
                        if (s_strikeEndLogged[home.settlement] != tm.day * 100 + hour) {
                            s_strikeEndLogged[home.settlement] = tm.day * 100 + hour;
                            auto logv = registry.view<EventLog>();
                            if (!logv.empty()) {
                                char buf[128];
                                std::snprintf(buf, sizeof(buf),
                                    "%s strike ends \xe2\x80\x94 morale recovering (%d%%).",
                                    sett->name.c_str(), (int)(sett->morale * 100));
                                logv.get<EventLog>(*logv.begin()).Push(tm.day, hour, buf);
                            }
                        }
                    }
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
            sched.consecutiveWorkHours = 0;
        }

        // ---- Track consecutive work hours for overwork penalty ----
        if (hourChanged && state.behavior == AgentBehavior::Working) {
            sched.consecutiveWorkHours++;
        }

        // ---- Move Working NPCs toward their skill-matched facility in their settlement ----
        // NPCs prefer the facility that matches their strongest skill (aptitude-seeking):
        //   - a farmer prefers the farm, a water carrier prefers the well, etc.
        //   - if no matching facility exists here, fall back to nearest facility.
        // This makes skill specialisation visible: farmers cluster at farms, etc.
        if (state.behavior == AgentBehavior::Working &&
            home.settlement != entt::null && registry.valid(home.settlement)) {

            // Determine aptitude: resource type of the NPC's highest skill.
            int aptitude    = RES_FOOD;
            bool         hasAptitude = false;
            if (const auto* skills = registry.try_get<Skills>(entity)) {
                float mx = std::max({skills->farming, skills->water_drawing, skills->woodcutting});
                if (mx > 0.25f) {
                    hasAptitude = true;
                    if      (skills->water_drawing == mx) aptitude = RES_WATER;
                    else if (skills->woodcutting   == mx) aptitude = RES_WOOD;
                    // else stays Food/farming
                }
            }

            entt::entity chosenFac  = entt::null;
            float        chosenDist = std::numeric_limits<float>::max();
            entt::entity nearestFac = entt::null;
            float        nearestDist = std::numeric_limits<float>::max();
            int chosenType  = RES_FOOD;
            int nearestType = RES_FOOD;

            auto facIt = s_facBySettlement.find(home.settlement);
            if (facIt != s_facBySettlement.end()) {
                for (const auto& fi : facIt->second) {
                    float dx = fi.pos.x - pos.x, dy = fi.pos.y - pos.y;
                    float d  = dx*dx + dy*dy;
                    if (d < nearestDist) { nearestDist = d; nearestFac = fi.entity; nearestType = fi.output; }
                    if (hasAptitude && fi.output == aptitude && d < chosenDist) {
                        chosenDist = d; chosenFac = fi.entity; chosenType = fi.output;
                    }
                }
            }

            // Use aptitude-matched facility if found; fall back to nearest
            entt::entity workFac  = (chosenFac != entt::null) ? chosenFac  : nearestFac;
            int facType  = (chosenFac != entt::null) ? chosenType : nearestType;
            float        workDist = (chosenFac != entt::null) ? chosenDist : nearestDist;

            if (workFac != entt::null) {
                const auto& fpos = registry.get<Position>(workFac);
                static constexpr float WORK_ARRIVE = 30.f;
                if (workDist > WORK_ARRIVE * WORK_ARRIVE) {
                    MoveToward(vel, pos, fpos.x, fpos.y, speed * 0.8f);
                } else {
                    vel.vx = vel.vy = 0.f;

                    // ---- Update Profession to match current facility type ----
                    if (hourChanged) {
                        ProfessionType newProf = ProfessionForResource(facType);
                        if (auto* prof = registry.try_get<Profession>(entity)) {
                            if (prof->type != newProf && newProf != ProfessionType::Idle) {
                                static std::mt19937 s_profRng{ std::random_device{}() };
                                if (prof->type != ProfessionType::Idle && prof->type != ProfessionType::Hauler
                                    && s_profRng() % 2 == 0) {
                                    auto logV = registry.view<EventLog>();
                                    if (!logV.empty()) {
                                        std::string who = "NPC";
                                        if (const auto* nm = registry.try_get<Name>(entity))
                                            who = nm->value;
                                        std::string settlName = "settlement";
                                        if (home.settlement != entt::null && registry.valid(home.settlement))
                                            if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                                settlName = s->name;
                                        auto& tmRef = registry.view<TimeManager>().get<TimeManager>(
                                            *registry.view<TimeManager>().begin());
                                        std::string msg = who + " switched from " +
                                            ProfessionLabel(prof->type) + " to " +
                                            ProfessionLabel(newProf) + " at " + settlName + ".";
                                        logV.get<EventLog>(*logV.begin()).Push(
                                            tmRef.day, (int)tmRef.hourOfDay, msg);
                                    }
                                }
                                // Career changer adaptation: second+ career change
                                if (prof->prevType != ProfessionType::Idle &&
                                    prof->prevType != prof->type &&
                                    s_profRng() % 3 == 0) {
                                    auto logV2 = registry.view<EventLog>();
                                    if (!logV2.empty()) {
                                        std::string who2 = "NPC";
                                        if (const auto* nm = registry.try_get<Name>(entity))
                                            who2 = nm->value;
                                        std::string settlName2 = "settlement";
                                        if (home.settlement != entt::null && registry.valid(home.settlement))
                                            if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                                settlName2 = s->name;
                                        auto& tmRef2 = registry.view<TimeManager>().get<TimeManager>(
                                            *registry.view<TimeManager>().begin());
                                        std::string msg2 = who2 + " is finding their calling as a " +
                                            ProfessionLabel(newProf) + " after trying " +
                                            ProfessionLabel(prof->type) + " at " + settlName2 + ".";
                                        logV2.get<EventLog>(*logV2.begin()).Push(
                                            tmRef2.day, (int)tmRef2.hourOfDay, msg2);
                                    }
                                }
                                // Skill transfer: experienced workers carry knowledge to new careers
                                if (auto* sk = registry.try_get<Skills>(entity)) {
                                    // Check old profession's skill level
                                    float oldSkill = 0.f;
                                    switch (prof->type) {
                                        case ProfessionType::Farmer:      oldSkill = sk->farming; break;
                                        case ProfessionType::WaterCarrier: oldSkill = sk->water_drawing; break;
                                        case ProfessionType::Lumberjack:   oldSkill = sk->woodcutting; break;
                                        default: break;
                                    }
                                    if (oldSkill >= 0.5f) {
                                        // Grant +0.05 to the new profession's skill (capped at 0.5)
                                        switch (newProf) {
                                            case ProfessionType::Farmer:
                                                sk->farming = std::min(0.5f, sk->farming + 0.05f); break;
                                            case ProfessionType::WaterCarrier:
                                                sk->water_drawing = std::min(0.5f, sk->water_drawing + 0.05f); break;
                                            case ProfessionType::Lumberjack:
                                                sk->woodcutting = std::min(0.5f, sk->woodcutting + 0.05f); break;
                                            default: break;
                                        }
                                    }
                                }
                                prof->prevType = prof->type;
                                prof->type = newProf;
                                ++prof->careerChanges;
                            }
                        }
                    }

                    // ---- Shared workplace affinity gain ----
                    // NPCs working at the same facility build affinity over time.
                    // Only run the O(n) scan when the hour changes to avoid per-tick O(n²).
                    if (hourChanged) {
                        static constexpr float WORK_AFFINITY_PER_HOUR = 0.002f;
                        static constexpr float WORK_AFFINITY_CAP      = 0.5f;
                        static std::map<std::pair<entt::entity, entt::entity>, float> s_workAffinityGain;
                        // Track per-entity best coworker cumulative gain for workBestFriend
                        static std::map<entt::entity, std::pair<entt::entity, float>> s_bestCoworker;
                        auto* myRel = registry.try_get<Relations>(entity);
                        if (myRel) {
                            registry.view<AgentState, Position, HomeSettlement>(
                                entt::exclude<Hauler, PlayerTag, ChildTag>).each(
                                [&](auto other, const AgentState& oState, const Position& oPos,
                                    const HomeSettlement& oHome) {
                                if (other == entity) return;
                                if (oState.behavior != AgentBehavior::Working) return;
                                if (oHome.settlement != home.settlement) return;
                                float odx = oPos.x - fpos.x, ody = oPos.y - fpos.y;
                                if (odx*odx + ody*ody > WORK_ARRIVE * WORK_ARRIVE) return;
                                auto key = (entity < other)
                                    ? std::make_pair(entity, other)
                                    : std::make_pair(other, entity);
                                float& cumGain = s_workAffinityGain[key];
                                if (cumGain >= WORK_AFFINITY_CAP) return;
                                float gain = WORK_AFFINITY_PER_HOUR; // 1 hour's worth per call
                                float remaining = WORK_AFFINITY_CAP - cumGain;
                                if (gain > remaining) gain = remaining;
                                cumGain += gain;
                                myRel->affinity[other] = std::min(1.f, myRel->affinity[other] + gain);
                                if (auto* oRel = registry.try_get<Relations>(other))
                                    oRel->affinity[entity] = std::min(1.f, oRel->affinity[entity] + gain);
                                // Update workBestFriend for both NPCs
                                auto& bestA = s_bestCoworker[entity];
                                if (cumGain > bestA.second || !registry.valid(bestA.first)) {
                                    bestA = { other, cumGain };
                                    myRel->workBestFriend = other;
                                }
                                auto& bestB = s_bestCoworker[other];
                                if (cumGain > bestB.second || !registry.valid(bestB.first)) {
                                    bestB = { entity, cumGain };
                                    if (auto* oRel2 = registry.try_get<Relations>(other))
                                        oRel2->workBestFriend = entity;
                                }
                            });
                        }
                    }

                    // ---- Workplace rivalry ----
                    // Skilled workers at the same facility with matching profession skill ≥ 0.7
                    // may develop rivalry, decreasing affinity. Checked once per hour.
                    if (hourChanged) {
                        const auto* mySkills = registry.try_get<Skills>(entity);
                        const auto* myProf   = registry.try_get<Profession>(entity);
                        auto* myRel2 = registry.try_get<Relations>(entity);
                        if (mySkills && myProf && myRel2) {
                            // Determine this NPC's profession skill level
                            float myProfSkill = 0.f;
                            switch (myProf->type) {
                                case ProfessionType::Farmer:      myProfSkill = mySkills->farming; break;
                                case ProfessionType::WaterCarrier: myProfSkill = mySkills->water_drawing; break;
                                case ProfessionType::Lumberjack:   myProfSkill = mySkills->woodcutting; break;
                                default: break;
                            }
                            if (myProfSkill >= 0.7f) {
                                registry.view<AgentState, Position, HomeSettlement, Skills, Profession>(
                                    entt::exclude<Hauler, PlayerTag, ChildTag>).each(
                                    [&](auto other, const AgentState& oState, const Position& oPos,
                                        const HomeSettlement& oHome, const Skills& oSk, const Profession& oPr) {
                                    if (other <= entity) return; // avoid double-processing pairs
                                    if (oState.behavior != AgentBehavior::Working) return;
                                    if (oHome.settlement != home.settlement) return;
                                    if (oPr.type != myProf->type) return; // must be same profession
                                    float odx = oPos.x - fpos.x, ody = oPos.y - fpos.y;
                                    if (odx*odx + ody*ody > WORK_ARRIVE * WORK_ARRIVE) return;
                                    // Check other NPC's matching profession skill
                                    float otherSkill = 0.f;
                                    switch (oPr.type) {
                                        case ProfessionType::Farmer:      otherSkill = oSk.farming; break;
                                        case ProfessionType::WaterCarrier: otherSkill = oSk.water_drawing; break;
                                        case ProfessionType::Lumberjack:   otherSkill = oSk.woodcutting; break;
                                        default: break;
                                    }
                                    if (otherSkill < 0.7f) return;
                                    // 1-in-20 chance per hour
                                    if (s_rng() % 20 != 0) return;
                                    // Decrease affinity by 0.02 (floor 0.0) for both
                                    myRel2->affinity[other] = std::max(0.f, myRel2->affinity[other] - 0.02f);
                                    if (auto* oRel = registry.try_get<Relations>(other))
                                        oRel->affinity[entity] = std::max(0.f, oRel->affinity[entity] - 0.02f);
                                    // Log at 1-in-5 frequency
                                    if (s_rng() % 5 == 0) {
                                        auto logV = registry.view<EventLog>();
                                        if (logV.begin() != logV.end()) {
                                            std::string n1 = "NPC", n2 = "NPC";
                                            if (const auto* nm = registry.try_get<Name>(entity)) n1 = nm->value;
                                            if (const auto* nm2 = registry.try_get<Name>(other)) n2 = nm2->value;
                                            std::string where = "settlement";
                                            if (home.settlement != entt::null && registry.valid(home.settlement))
                                                if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                                    where = s->name;
                                            logV.get<EventLog>(*logV.begin()).Push(tm.day, hour,
                                                n1 + " and " + n2 + " compete at " + where + ".");
                                        }
                                    }
                                });
                            }
                        }
                    }

                    // ---- Rival profession taunt ----
                    // NPCs at the same facility with *different* professions and both
                    // skill ≥ 0.5 may taunt each other, decreasing mutual affinity.
                    if (hourChanged) {
                        const auto* mySkillsT = registry.try_get<Skills>(entity);
                        const auto* myProfT   = registry.try_get<Profession>(entity);
                        auto* myRelT = registry.try_get<Relations>(entity);
                        if (mySkillsT && myProfT && myRelT
                            && myProfT->type != ProfessionType::Idle
                            && myProfT->type != ProfessionType::Hauler) {
                            float mySkillT = 0.f;
                            switch (myProfT->type) {
                                case ProfessionType::Farmer:      mySkillT = mySkillsT->farming; break;
                                case ProfessionType::WaterCarrier: mySkillT = mySkillsT->water_drawing; break;
                                case ProfessionType::Lumberjack:   mySkillT = mySkillsT->woodcutting; break;
                                default: break;
                            }
                            if (mySkillT >= 0.5f) {
                                registry.view<AgentState, Position, HomeSettlement, Skills, Profession>(
                                    entt::exclude<Hauler, PlayerTag, ChildTag>).each(
                                    [&](auto other, const AgentState& oState, const Position& oPos,
                                        const HomeSettlement& oHome, const Skills& oSk, const Profession& oPr) {
                                    if (other <= entity) return;
                                    if (oState.behavior != AgentBehavior::Working) return;
                                    if (oHome.settlement != home.settlement) return;
                                    if (oPr.type == myProfT->type) return;  // same profession — handled by rivalry
                                    if (oPr.type == ProfessionType::Idle || oPr.type == ProfessionType::Hauler) return;
                                    float odx = oPos.x - fpos.x, ody = oPos.y - fpos.y;
                                    if (odx*odx + ody*ody > WORK_ARRIVE * WORK_ARRIVE) return;
                                    float otherSkillT = 0.f;
                                    switch (oPr.type) {
                                        case ProfessionType::Farmer:      otherSkillT = oSk.farming; break;
                                        case ProfessionType::WaterCarrier: otherSkillT = oSk.water_drawing; break;
                                        case ProfessionType::Lumberjack:   otherSkillT = oSk.woodcutting; break;
                                        default: break;
                                    }
                                    if (otherSkillT < 0.5f) return;
                                    // 1-in-25 chance per hour
                                    if (s_rng() % 25 != 0) return;
                                    // Elder mediation: elder at same facility may suppress the taunt
                                    if (s_rng() % 6 == 0) {
                                        entt::entity mediator = entt::null;
                                        registry.view<AgentState, Position, Age, Skills>(
                                            entt::exclude<Hauler, PlayerTag, ChildTag>).each(
                                            [&](auto me, const AgentState& meSt, const Position& mePos,
                                                const Age& meAge, const Skills& meSk) {
                                            if (mediator != entt::null) return;
                                            if (me == entity || me == other) return;
                                            if (meSt.behavior != AgentBehavior::Working) return;
                                            if (meAge.days <= 60.f) return;
                                            if (meSk.farming < 0.7f && meSk.water_drawing < 0.7f && meSk.woodcutting < 0.7f) return;
                                            float mdx = mePos.x - fpos.x, mdy = mePos.y - fpos.y;
                                            if (mdx*mdx + mdy*mdy > WORK_ARRIVE * WORK_ARRIVE) return;
                                            mediator = me;
                                        });
                                        if (mediator != entt::null) {
                                            // Both rivals gain +0.02 affinity toward the elder
                                            myRelT->affinity[mediator] = std::min(1.f, myRelT->affinity[mediator] + 0.02f);
                                            if (auto* oRel = registry.try_get<Relations>(other))
                                                oRel->affinity[mediator] = std::min(1.f, oRel->affinity[mediator] + 0.02f);
                                            // Log at full frequency
                                            auto logVM = registry.view<EventLog>();
                                            if (!logVM.empty()) {
                                                std::string elderNm = "An elder", n1m = "NPC", n2m = "NPC";
                                                if (const auto* nm = registry.try_get<Name>(mediator)) elderNm = nm->value;
                                                if (const auto* nm = registry.try_get<Name>(entity)) n1m = nm->value;
                                                if (const auto* nm = registry.try_get<Name>(other)) n2m = nm->value;
                                                logVM.get<EventLog>(*logVM.begin()).Push(tm.day, hour,
                                                    elderNm + " calms tensions between " + n1m + " and " + n2m + ".");
                                            }
                                            return; // taunt suppressed
                                        }
                                    }
                                    // Decrease mutual affinity by 0.01 (floor 0.0)
                                    myRelT->affinity[other] = std::max(0.f, myRelT->affinity[other] - 0.01f);
                                    if (auto* oRel = registry.try_get<Relations>(other))
                                        oRel->affinity[entity] = std::max(0.f, oRel->affinity[entity] - 0.01f);
                                    // Log the taunt
                                    auto logVT = registry.view<EventLog>();
                                    if (!logVT.empty()) {
                                        std::string n1 = "NPC", n2 = "NPC";
                                        if (const auto* nm = registry.try_get<Name>(entity)) n1 = nm->value;
                                        if (const auto* nm2 = registry.try_get<Name>(other)) n2 = nm2->value;
                                        const char* profStr =
                                            (oPr.type == ProfessionType::Farmer)      ? "farming" :
                                            (oPr.type == ProfessionType::WaterCarrier) ? "water-carrying" :
                                            "woodcutting";
                                        logVT.get<EventLog>(*logVT.begin()).Push(tm.day, hour,
                                            n1 + " teases " + n2 + " about their " + profStr + ".");
                                    }
                                });
                            }
                        }
                    }

                    // ---- Workplace reconciliation after taunt ----
                    // NPCs with different professions and strained relations (affinity < 0.1)
                    // who work together for 3+ consecutive hours may reconcile.
                    if (hourChanged) {
                        static std::map<std::pair<entt::entity, entt::entity>, int> s_reconHours;
                        // Prune stale entries periodically (when map gets large)
                        if (s_reconHours.size() > 500) {
                            for (auto it = s_reconHours.begin(); it != s_reconHours.end(); ) {
                                if (!registry.valid(it->first.first) || !registry.valid(it->first.second))
                                    it = s_reconHours.erase(it);
                                else ++it;
                            }
                        }
                        const auto* myProfR = registry.try_get<Profession>(entity);
                        auto* myRelR = registry.try_get<Relations>(entity);
                        if (myProfR && myRelR
                            && myProfR->type != ProfessionType::Idle
                            && myProfR->type != ProfessionType::Hauler) {
                            registry.view<AgentState, Position, HomeSettlement, Profession>(
                                entt::exclude<Hauler, PlayerTag, ChildTag>).each(
                                [&](auto other, const AgentState& oState, const Position& oPos,
                                    const HomeSettlement& oHome, const Profession& oPr) {
                                if (other <= entity) return;
                                if (oState.behavior != AgentBehavior::Working) return;
                                if (oHome.settlement != home.settlement) return;
                                if (oPr.type == myProfR->type) return; // same profession — not applicable
                                if (oPr.type == ProfessionType::Idle || oPr.type == ProfessionType::Hauler) return;
                                float odx = oPos.x - fpos.x, ody = oPos.y - fpos.y;
                                if (odx*odx + ody*ody > WORK_ARRIVE * WORK_ARRIVE) return;
                                // Check if both have strained relations
                                auto ait = myRelR->affinity.find(other);
                                float aff = (ait != myRelR->affinity.end()) ? ait->second : 0.f;
                                if (aff >= 0.1f) return; // Not strained enough
                                auto key = (entity < other)
                                    ? std::make_pair(entity, other)
                                    : std::make_pair(other, entity);
                                ++s_reconHours[key];
                                if (s_reconHours[key] < 3) return;
                                // 1-in-10 chance to reconcile
                                if (s_rng() % 10 != 0) return;
                                // Track repeated reconciliations for escalating bond
                                static std::map<std::pair<entt::entity,entt::entity>, int> s_reconCount;
                                ++s_reconCount[key];
                                int reconTimes = s_reconCount[key];
                                float affinityBoost = (reconTimes >= 2) ? 0.08f : 0.05f;
                                float glowDuration  = (reconTimes >= 2) ? 4.f   : 2.f;
                                // Reconcile: boost mutual affinity
                                myRelR->affinity[other] = std::min(1.f, aff + affinityBoost);
                                if (auto* oRel = registry.try_get<Relations>(other))
                                    oRel->affinity[entity] = std::min(1.f, oRel->affinity[entity] + affinityBoost);
                                s_reconHours.erase(key);
                                // Log
                                auto logVR = registry.view<EventLog>();
                                if (!logVR.empty()) {
                                    std::string n1 = "NPC", n2 = "NPC";
                                    if (const auto* nm = registry.try_get<Name>(entity)) n1 = nm->value;
                                    if (const auto* nm2 = registry.try_get<Name>(other)) n2 = nm2->value;
                                    std::string where = "settlement";
                                    if (home.settlement != entt::null && registry.valid(home.settlement))
                                        if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                            where = s->name;
                                    logVR.get<EventLog>(*logVR.begin()).Push(tm.day, hour,
                                        n1 + " and " + n2 + " put aside their differences at " + where + ".");
                                }
                                // Morale boost to home settlement
                                if (home.settlement != entt::null && registry.valid(home.settlement)) {
                                    if (auto* settl = registry.try_get<Settlement>(home.settlement))
                                        settl->morale = std::min(1.f, settl->morale + 0.01f);
                                    // Log harmony at 1-in-4 frequency
                                    if (s_rng() % 4 == 0) {
                                        auto hlogV = registry.view<EventLog>();
                                        if (!hlogV.empty()) {
                                            std::string where2 = "Settlement";
                                            if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                                where2 = s->name;
                                            hlogV.get<EventLog>(*hlogV.begin()).Push(tm.day, hour,
                                                where2 + " feels more harmonious");
                                        }
                                    }
                                }
                                // Set reconcileGlow on both NPCs
                                if (auto* tmrA = registry.try_get<DeprivationTimer>(entity))
                                    tmrA->reconcileGlow = glowDuration;
                                if (auto* tmrB = registry.try_get<DeprivationTimer>(other))
                                    tmrB->reconcileGlow = glowDuration;
                                // 3rd+ reconciliation: escalating friendship log
                                if (reconTimes >= 3) {
                                    auto ufLogV = registry.view<EventLog>();
                                    if (!ufLogV.empty()) {
                                        std::string un1 = "NPC", un2 = "NPC";
                                        if (const auto* nm = registry.try_get<Name>(entity)) un1 = nm->value;
                                        if (const auto* nm2 = registry.try_get<Name>(other)) un2 = nm2->value;
                                        std::string uw = "settlement";
                                        if (home.settlement != entt::null && registry.valid(home.settlement))
                                            if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                                uw = s->name;
                                        ufLogV.get<EventLog>(*ufLogV.begin()).Push(tm.day, hour,
                                            un1 + " and " + un2 + " are becoming unlikely friends at " + uw + ".");
                                    }
                                }
                            });
                        }
                    }

                    // ---- NPC work song: 3+ same-profession coworkers at same facility ----
                    if (hourChanged) {
                        const auto* mySongProf = registry.try_get<Profession>(entity);
                        if (mySongProf && mySongProf->type != ProfessionType::Idle
                            && mySongProf->type != ProfessionType::Hauler) {
                            // Gather same-profession coworkers within WORK_ARRIVE of this facility
                            std::vector<entt::entity> coworkers;
                            coworkers.push_back(entity);
                            registry.view<AgentState, Position, HomeSettlement, Profession>(
                                entt::exclude<Hauler, PlayerTag, ChildTag>).each(
                                [&](auto other, const AgentState& oState, const Position& oPos,
                                    const HomeSettlement& oHome, const Profession& oPr) {
                                if (other == entity) return;
                                if (oState.behavior != AgentBehavior::Working) return;
                                if (oHome.settlement != home.settlement) return;
                                if (oPr.type != mySongProf->type) return;
                                float odx = oPos.x - fpos.x, ody = oPos.y - fpos.y;
                                if (odx*odx + ody*ody > WORK_ARRIVE * WORK_ARRIVE) return;
                                coworkers.push_back(other);
                            });
                            // Seasonal work shanty: harvest seasons (high prodMod) = more frequent songs,
                            // cold seasons (high heatMod) = stronger bonds
                            float seasonProdMod = GetSeasonProductionMod(seasonId, schema);
                            int songChance = (seasonProdMod >= 1.1f) ? 15 : 30;
                            float songAffinityGain = (seasonHeatMod >= 0.8f) ? 0.02f : 0.01f;
                            if (coworkers.size() >= 3 && s_rng() % songChance == 0) {
                                // Boost all coworkers' mutual affinity
                                for (size_t i = 0; i < coworkers.size(); ++i) {
                                    auto* relI = registry.try_get<Relations>(coworkers[i]);
                                    if (!relI) continue;
                                    for (size_t j = i + 1; j < coworkers.size(); ++j) {
                                        relI->affinity[coworkers[j]] = std::min(1.f, relI->affinity[coworkers[j]] + songAffinityGain);
                                        if (auto* relJ = registry.try_get<Relations>(coworkers[j]))
                                            relJ->affinity[coworkers[i]] = std::min(1.f, relJ->affinity[coworkers[i]] + songAffinityGain);
                                    }
                                }
                                // Log the work song with seasonal variant
                                auto logVS = registry.view<EventLog>();
                                if (!logVS.empty()) {
                                    std::string who = "NPC";
                                    if (const auto* nm = registry.try_get<Name>(entity)) who = nm->value;
                                    std::string where = "settlement";
                                    if (home.settlement != entt::null && registry.valid(home.settlement))
                                        if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                            where = s->name;
                                    std::string songMsg;
                                    if (seasonProdMod >= 1.1f)
                                        songMsg = who + " leads a harvest shanty at " + where + ".";
                                    else if (seasonHeatMod >= 0.8f)
                                        songMsg = who + " leads a fireside song at " + where + ".";
                                    else
                                        songMsg = who + " leads a work song at " + where + ".";
                                    logVS.get<EventLog>(*logVS.begin()).Push(tm.day, hour, songMsg);
                                }
                                // Work song morale lift: 4+ coworkers boost settlement morale
                                if (coworkers.size() >= 4) {
                                    if (home.settlement != entt::null && registry.valid(home.settlement)) {
                                        if (auto* settl = registry.try_get<Settlement>(home.settlement))
                                            settl->morale = std::min(1.f, settl->morale + 0.01f);
                                    }
                                    if (s_rng() % 4 == 0) {
                                        auto logV2 = registry.view<EventLog>();
                                        if (!logV2.empty()) {
                                            std::string where2 = "Settlement";
                                            if (home.settlement != entt::null && registry.valid(home.settlement))
                                                if (const auto* s2 = registry.try_get<Settlement>(home.settlement))
                                                    where2 = s2->name;
                                            logV2.get<EventLog>(*logV2.begin()).Push(tm.day, hour,
                                                where2 + " hums along.");
                                        }
                                    }
                                }
                            }
                        }
                    }

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
                                            (facType == RES_FOOD)  ? "Farming" :
                                            (facType == RES_WATER) ? "Water"   : "Woodcutting";
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
