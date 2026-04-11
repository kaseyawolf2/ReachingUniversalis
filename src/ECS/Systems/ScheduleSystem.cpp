#include "ScheduleSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <limits>
#include <random>

static constexpr float SLEEP_ARRIVE  = 25.f;   // distance at which NPC is "at settlement"
static constexpr float LEISURE_RADIUS = 80.f;  // wander radius around home settlement center

// Skill decay per game-hour while NOT actively working at a facility.
// At 0.005/day ≈ 0.0002/hr — slow enough that NPCs don't forget overnight,
// but fast enough that prolonged absence matters.
static constexpr float SKILL_DECAY_PER_HOUR = 0.005f / 24.f;

static std::mt19937                          s_rng{std::random_device{}()};
static std::uniform_real_distribution<float> s_angle(0.f, 6.28318f);
static std::uniform_real_distribution<float> s_radius(10.f, LEISURE_RADIUS);

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

        // Age affects work eligibility: children (<15 days) don't work;
        // elderly (>70 days) work reduced hours (7–12 only).
        bool workEligible = true;
        int  workEndAdj   = sched.workEnd + seasonWorkEndAdj;
        if (const auto* age = registry.try_get<Age>(entity)) {
            if (age->days < 15.f) workEligible = false;            // child
            else if (age->days > 70.f) workEndAdj = std::min(workEndAdj, 12); // elderly: half-shift
        }

        bool workTime  = workEligible &&
                         (hour >= sched.workStart && hour < workEndAdj);

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
        }

        // ---- Set Working during work hours (only when Idle — needs override this) ----
        if (workTime && state.behavior == AgentBehavior::Idle) {
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
                    if (auto* skills = registry.try_get<Skills>(entity))
                        skills->Advance(facType, SKILL_GAIN_PER_GAME_HOUR * gameHoursDt);
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
                // Child: passive growth (observing community)
                if (auto* skills = registry.try_get<Skills>(entity)) {
                    float grow = CHILD_SKILL_GAIN_PER_HOUR * gHrs;
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
                // Adult not working: decay
                if (auto* skills = registry.try_get<Skills>(entity)) {
                    float decay = SKILL_DECAY_PER_HOUR * gHrs;
                    skills->farming       = std::max(0.f, skills->farming       - decay);
                    skills->water_drawing = std::max(0.f, skills->water_drawing - decay);
                    skills->woodcutting   = std::max(0.f, skills->woodcutting   - decay);
                }
            }
        }

        // ---- Leisure wandering: between work shift end and sleep ----
        // Idle NPCs outside work hours pick a random spot near home and amble to it.
        // Velocity is only changed when they arrive or have no target — they finish
        // natural movement in between (AgentDecisionSystem may interrupt for needs).
        bool leisureTime = !sleepTime && !workTime;
        if (leisureTime && state.behavior == AgentBehavior::Idle &&
            home.settlement != entt::null && registry.valid(home.settlement)) {

            const auto& homePos = registry.get<Position>(home.settlement);
            // If standing still, pick a new wander destination
            if (std::abs(vel.vx) < 0.5f && std::abs(vel.vy) < 0.5f) {
                float ang = s_angle(s_rng);
                float rad = s_radius(s_rng);
                float wx  = homePos.x + std::cos(ang) * rad;
                float wy  = homePos.y + std::sin(ang) * rad;
                MoveToward(vel, pos, wx, wy, speed * 0.4f);
            }
        }
    }
}
