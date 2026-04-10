#include "TimeSystem.h"
#include "ECS/Components.h"
#include "raylib.h"

void TimeSystem::Update(entt::registry& registry, float realDt) {
    auto view = registry.view<TimeManager>();
    if (view.empty()) return;

    auto entity = *view.begin();
    auto& tm    = view.get<TimeManager>(entity);

    // ---- Input: pause and tick speed ----
    if (IsKeyPressed(KEY_SPACE)) {
        tm.paused = !tm.paused;
    }
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
        if      (tm.tickSpeed == 1) tm.tickSpeed = 2;
        else if (tm.tickSpeed == 2) tm.tickSpeed = 4;
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
        if      (tm.tickSpeed == 4) tm.tickSpeed = 2;
        else if (tm.tickSpeed == 2) tm.tickSpeed = 1;
    }

    // ---- B: toggle road blockade ----
    if (IsKeyPressed(KEY_B)) {
        auto roadView = registry.view<Road>();
        auto logView  = registry.view<EventLog>();
        EventLog* log = logView.empty() ? nullptr
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

    if (tm.paused) return;

    // ---- Advance game clock ----
    // gameDt is the speed-scaled real dt (seconds).
    // GAME_MINS_PER_REAL_SEC converts it to game-minutes.
    // Dividing by 60 converts game-minutes to game-hours for hourOfDay.
    float gameDt       = tm.GameDt(realDt);
    float gameHoursDt  = gameDt * GAME_MINS_PER_REAL_SEC / 60.0f;

    tm.gameSeconds += gameDt * GAME_MINS_PER_REAL_SEC * 60.0f;
    tm.hourOfDay   += gameHoursDt;

    if (tm.hourOfDay >= 24.0f) {
        tm.hourOfDay -= 24.0f;
        tm.day       += 1;
    }
}
