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
static constexpr float PRICE_MAX        =  20.f;

void PriceSystem::Update(entt::registry& registry, float realDt) {
    auto tv = registry.view<TimeManager>();
    if (tv.begin() == tv.end()) return;
    float gameDt = tv.get<TimeManager>(*tv.begin()).GameDt(realDt);
    if (gameDt <= 0.f) return;
    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;

    registry.view<Market, Stockpile>().each(
        [&](Market& mkt, const Stockpile& sp) {
            for (auto& [res, price] : mkt.price) {
                auto it = sp.quantities.find(res);
                float stock = (it != sp.quantities.end()) ? it->second : 0.f;

                if (stock > PRICE_HIGH_STOCK)
                    price *= std::pow(1.f - PRICE_DECAY_RATE, gameHoursDt);
                else if (stock < PRICE_LOW_STOCK)
                    price *= std::pow(1.f + PRICE_RISE_RATE, gameHoursDt);

                price = std::max(PRICE_MIN, std::min(PRICE_MAX, price));
            }
        });
}
