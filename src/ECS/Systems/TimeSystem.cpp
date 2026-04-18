#include "TimeSystem.h"
#include "ECS/Components.h"
#include "World/WorldSchema.h"

void TimeSystem::Advance(entt::registry& registry, float subDt, const WorldSchema& schema) {
    auto view = registry.view<TimeManager>();
    if (view.empty()) return;
    auto& tm = view.get<TimeManager>(*view.begin());

    // Each sub-tick advances by subDt real seconds worth of game time.
    // GAME_MINS_PER_REAL_SEC converts to game-minutes; /60 to game-hours.
    float gameHoursDt = subDt * GAME_MINS_PER_REAL_SEC / 60.0f;

    SeasonID prevSeason = tm.CurrentSeason(schema.seasons);

    tm.gameSeconds += subDt * GAME_MINS_PER_REAL_SEC * 60.0f;
    tm.hourOfDay   += gameHoursDt;

    if (tm.hourOfDay >= 24.0f) {
        tm.hourOfDay -= 24.0f;
        tm.day       += 1;

        // Log season transitions on the day they begin
        SeasonID newSeason = tm.CurrentSeason(schema.seasons);
        if (newSeason != prevSeason) {
            auto lv = registry.view<EventLog>();
            if (lv.begin() != lv.end()) {
                const char* name = (newSeason >= 0 && newSeason < (int)schema.seasons.size())
                                   ? schema.seasons[newSeason].displayName.c_str() : "Unknown";
                std::string msg = std::string("--- ") + name + " begins ---";
                lv.get<EventLog>(*lv.begin()).Push(tm.day, (int)tm.hourOfDay, msg, "Time");
            }
        }
    }
}
