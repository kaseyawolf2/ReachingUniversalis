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
                lv.get<EventLog>(*lv.begin()).Push(tm.day, (int)tm.hourOfDay, msg);
            }
        }
    }

    // Classify the current season regime from thresholds (checked high→low).
    // Result stored in TimeManager so WriteSnapshot() can copy it without
    // rebuilding the classification — zero per-frame string allocation.
    SeasonID curSeason = tm.CurrentSeason(schema.seasons);
    if (curSeason >= 0 && curSeason < (int)schema.seasons.size()) {
        const auto& sdef = schema.seasons[curSeason];
        const auto& th   = schema.seasonThresholds;
        if (sdef.heatDrainMod >= th.harshCold)
            tm.seasonRegime = SeasonRegime::HarshCold;
        else if (sdef.heatDrainMod >= th.moderateCold)
            tm.seasonRegime = SeasonRegime::ModerateCold;
        else if (sdef.heatDrainMod >= th.coldSeason)
            tm.seasonRegime = SeasonRegime::Cold;
        else if (sdef.heatDrainMod > th.mildCold)
            tm.seasonRegime = SeasonRegime::MildCold;
        else if (sdef.productionMod >= th.harvestSeason)
            tm.seasonRegime = SeasonRegime::Harvest;
        else if (sdef.productionMod <= th.lowProduction)
            tm.seasonRegime = SeasonRegime::LowProduction;
        else
            tm.seasonRegime = SeasonRegime::Mild;
    }
}
