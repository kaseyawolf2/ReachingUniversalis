#include "PriceSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <algorithm>

// Stock levels that trigger price movement
static constexpr float PRICE_HIGH_STOCK = 100.f;   // above → price decays
static constexpr float PRICE_LOW_STOCK  =  15.f;   // below → price rises
static constexpr float PRICE_RISE_RATE  =  0.08f;  // fraction per game-hour when scarce
static constexpr float PRICE_DECAY_RATE =  0.04f;  // fraction per game-hour when abundant
static constexpr float PRICE_MIN        =   0.5f;
static constexpr float PRICE_MAX        =  25.f;   // raised slightly — seasonal spikes can push high

// Seasonal demand pressure: some resources are more valuable in certain seasons.
// Returns a target floor price below which the price won't fall via decay.
static float SeasonPriceFloor(ResourceType res, Season season) {
    if (res == ResourceType::Wood) {
        // Wood is essential in cold seasons; price floor rises with demand
        switch (season) {
            case Season::Spring: return 1.0f;
            case Season::Summer: return 0.5f;
            case Season::Autumn: return 2.5f;
            case Season::Winter: return 5.0f;
        }
    }
    if (res == ResourceType::Food) {
        // Food is harder to produce in winter; minimum viable price
        switch (season) {
            case Season::Winter: return 2.0f;
            default:             return 1.0f;
        }
    }
    return PRICE_MIN;
}

void PriceSystem::Update(entt::registry& registry, float realDt) {
    auto tv = registry.view<TimeManager>();
    if (tv.begin() == tv.end()) return;
    const auto& tm = tv.get<TimeManager>(*tv.begin());
    float gameDt = tm.GameDt(realDt);
    if (gameDt <= 0.f) return;
    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;
    Season season = tm.CurrentSeason();

    registry.view<Market, Stockpile>().each(
        [&](Market& mkt, const Stockpile& sp) {
            for (auto& [res, price] : mkt.price) {
                auto it = sp.quantities.find(res);
                float stock = (it != sp.quantities.end()) ? it->second : 0.f;

                if (stock > PRICE_HIGH_STOCK)
                    price *= std::pow(1.f - PRICE_DECAY_RATE, gameHoursDt);
                else if (stock < PRICE_LOW_STOCK)
                    price *= std::pow(1.f + PRICE_RISE_RATE, gameHoursDt);

                // Apply seasonal floor — prices can't drop below seasonal demand level
                float floor = SeasonPriceFloor(res, season);
                price = std::max(floor, std::min(PRICE_MAX, price));
            }
        });
}
