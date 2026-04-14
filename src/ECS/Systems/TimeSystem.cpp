#include "TimeSystem.h"
#include "ECS/Components.h"

void TimeSystem::Advance(entt::registry& registry, float subDt, const WorldSchema& schema) {
    auto view = registry.view<TimeManager>();
    if (view.empty()) return;
    auto& tm = view.get<TimeManager>(*view.begin());

    // Each sub-tick advances by subDt real seconds worth of game time.
    // GAME_MINS_PER_REAL_SEC converts to game-minutes; /60 to game-hours.
    float gameHoursDt = subDt * GAME_MINS_PER_REAL_SEC / 60.0f;

    SeasonID prevSeason = tm.CurrentSeason();

    tm.gameSeconds += subDt * GAME_MINS_PER_REAL_SEC * 60.0f;
    tm.hourOfDay   += gameHoursDt;

    if (tm.hourOfDay >= 24.0f) {
        tm.hourOfDay -= 24.0f;
        tm.day       += 1;

        // Log season transitions on the day they begin
        SeasonID newSeason = tm.CurrentSeason();
        if (newSeason != prevSeason) {
            auto lv = registry.view<EventLog>();
            if (lv.begin() != lv.end()) {
                std::string msg = std::string("--- ") + GetSeasonName(newSeason, schema) + " begins ---";
                lv.get<EventLog>(*lv.begin()).Push(tm.day, (int)tm.hourOfDay, msg);
            }
        }
    }
}
