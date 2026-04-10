#include "ScheduleSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <limits>
#include <random>

static constexpr float SLEEP_ARRIVE  = 25.f;   // distance at which NPC is "at settlement"
static constexpr float LEISURE_RADIUS = 80.f;  // wander radius around home settlement center

// Wage paid from settlement treasury to each Working NPC per game-hour.
// Treasury must have at least WAGE_RESERVE before paying wages.
static constexpr float WAGE_PER_HOUR  = 0.25f;
static constexpr float WAGE_RESERVE   = 20.f;

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

    int hour = (int)tm.hourOfDay;

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

        bool sleepTime = (hour >= sched.sleepHour || hour < sched.wakeHour);

        // Age affects work eligibility: children (<15 days) don't work;
        // elderly (>70 days) work reduced hours (7–12 only).
        bool workEligible = true;
        int  workEndAdj   = sched.workEnd;
        if (const auto* age = registry.try_get<Age>(entity)) {
            if (age->days < 15.f) workEligible = false;            // child
            else if (age->days > 70.f) workEndAdj = 12;            // elderly: half-shift
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

        // ---- Move Working NPCs toward the nearest production facility in their settlement ----
        // This makes work visible — NPCs gravitate to farms/wells/mills during shifts.
        // Also advances the relevant skill while actively working at a facility.
        if (state.behavior == AgentBehavior::Working &&
            home.settlement != entt::null && registry.valid(home.settlement)) {

            entt::entity nearestFac  = entt::null;
            float        bestDistSq  = std::numeric_limits<float>::max();
            ResourceType facType     = ResourceType::Food;

            registry.view<Position, ProductionFacility>().each(
                [&](auto fe, const Position& fpos, const ProductionFacility& fac) {
                if (fac.settlement != home.settlement) return;
                if (fac.baseRate <= 0.f) return;   // skip shelter nodes
                float dx = fpos.x - pos.x, dy = fpos.y - pos.y;
                float d  = dx*dx + dy*dy;
                if (d < bestDistSq) { bestDistSq = d; nearestFac = fe; facType = fac.output; }
            });

            if (nearestFac != entt::null) {
                const auto& fpos = registry.get<Position>(nearestFac);
                static constexpr float WORK_ARRIVE = 30.f;
                if (bestDistSq > WORK_ARRIVE * WORK_ARRIVE) {
                    MoveToward(vel, pos, fpos.x, fpos.y, speed * 0.8f);
                } else {
                    vel.vx = vel.vy = 0.f;
                    // Skill advancement while at the work site (very slow: ~0.1 gain per game-day)
                    // gameDt is in real seconds; GAME_MINS_PER_REAL_SEC converts to game-minutes
                    // 0.1 per game-day = 0.1 / (24*60) per game-minute = ~6.9e-5 per game-min
                    static constexpr float SKILL_GAIN_PER_GAME_HOUR = 0.1f / 24.f;
                    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;
                    if (auto* skills = registry.try_get<Skills>(entity))
                        skills->Advance(facType, SKILL_GAIN_PER_GAME_HOUR * gameHoursDt);
                }
            }
        }

        // ---- Wage payment: treasury pays Working NPCs per game-hour ----
        float gameHoursDt2 = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;
        if (state.behavior == AgentBehavior::Working &&
            home.settlement != entt::null && registry.valid(home.settlement)) {
            if (auto* settl = registry.try_get<Settlement>(home.settlement)) {
                if (settl->treasury >= WAGE_RESERVE) {
                    float wage = WAGE_PER_HOUR * gameHoursDt2;
                    settl->treasury -= wage;
                    if (auto* money = registry.try_get<Money>(entity))
                        money->balance += wage;
                }
            }
        }

        // ---- Skill decay when not actively working at a facility ----
        // NPCs lose skill slowly through disuse; practice is the only way to
        // maintain mastery. Children are exempt — they grow into skills naturally.
        bool isChild = false;
        if (const auto* age = registry.try_get<Age>(entity))
            isChild = (age->days < 15.f);
        if (!isChild && state.behavior != AgentBehavior::Working) {
            if (auto* skills = registry.try_get<Skills>(entity)) {
                float decay = SKILL_DECAY_PER_HOUR * gameHoursDt2;
                skills->farming       = std::max(0.f, skills->farming       - decay);
                skills->water_drawing = std::max(0.f, skills->water_drawing - decay);
                skills->woodcutting   = std::max(0.f, skills->woodcutting   - decay);
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
