#include "TimeSystem.h"
#include "ECS/Components.h"
#include "raylib.h"

void TimeSystem::HandleInput(entt::registry& registry) {
    auto view = registry.view<TimeManager>();
    if (view.empty()) return;
    auto& tm = view.get<TimeManager>(*view.begin());

    // ---- Pause / speed ----
    if (IsKeyPressed(KEY_SPACE)) tm.paused = !tm.paused;

    static constexpr int SPEEDS[] = { 1, 2, 4, 8, 16, 32, 64, 128 };
    static constexpr int NUM_SPEEDS = sizeof(SPEEDS) / sizeof(SPEEDS[0]);

    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
        for (int i = 0; i < NUM_SPEEDS - 1; ++i)
            if (tm.tickSpeed == SPEEDS[i]) { tm.tickSpeed = SPEEDS[i + 1]; break; }
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
        for (int i = NUM_SPEEDS - 1; i > 0; --i)
            if (tm.tickSpeed == SPEEDS[i]) { tm.tickSpeed = SPEEDS[i - 1]; break; }
    }

    // ---- B: toggle road blockade ----
    if (IsKeyPressed(KEY_B)) {
        auto roadView = registry.view<Road>();
        auto logView  = registry.view<EventLog>();
        EventLog* log = (logView.begin() == logView.end()) ? nullptr
                      : &logView.get<EventLog>(*logView.begin());
        for (auto re : roadView) {
            auto& road = roadView.get<Road>(re);
            road.blocked = !road.blocked;
            if (log) {
                int h = (int)tm.hourOfDay;
                log->Push(tm.day, h, road.blocked
                    ? "Road BLOCKED — haulers rerouting"
                    : "Road CLEARED — trade resumes");
            }
        }
    }
}

void TimeSystem::Advance(entt::registry& registry, float subDt) {
    auto view = registry.view<TimeManager>();
    if (view.empty()) return;
    auto& tm = view.get<TimeManager>(*view.begin());

    // Each sub-tick advances by subDt real seconds worth of game time.
    // GAME_MINS_PER_REAL_SEC converts to game-minutes; /60 to game-hours.
    float gameHoursDt = subDt * GAME_MINS_PER_REAL_SEC / 60.0f;

    tm.gameSeconds += subDt * GAME_MINS_PER_REAL_SEC * 60.0f;
    tm.hourOfDay   += gameHoursDt;

    if (tm.hourOfDay >= 24.0f) {
        tm.hourOfDay -= 24.0f;
        tm.day       += 1;
    }
}
