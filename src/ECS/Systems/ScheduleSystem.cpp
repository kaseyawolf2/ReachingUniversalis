#include "ScheduleSystem.h"
#include "ECS/Components.h"
#include <cmath>

static constexpr float SLEEP_ARRIVE = 25.f;  // distance at which NPC is "at settlement"

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
        bool workTime  = (hour >= sched.workStart && hour < sched.workEnd);

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
    }
}
