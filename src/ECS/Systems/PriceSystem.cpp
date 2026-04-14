#include "PriceSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <algorithm>
#include <map>
#include <cstdio>

// Stock levels that trigger price movement
static constexpr float PRICE_HIGH_STOCK = 100.f;   // above → price decays
static constexpr float PRICE_LOW_STOCK  =  15.f;   // below → price rises
static constexpr float PRICE_RISE_RATE  =  0.08f;  // fraction per game-hour when scarce
static constexpr float PRICE_DECAY_RATE =  0.04f;  // fraction per game-hour when abundant
static constexpr float PRICE_MIN        =   0.5f;
static constexpr float PRICE_MAX        =  25.f;   // raised slightly — seasonal spikes can push high

// Seasonal demand pressure: some resources are more valuable in certain seasons.
// Returns a target floor price below which the price won't fall via decay.
// Uses schema season properties: heatDrainMod drives wood demand, productionMod drives food floor.
static float SeasonPriceFloor(int res, SeasonID seasonId, const WorldSchema& schema) {
    float heatMod = 0.f, prodMod = 1.f;
    if (seasonId >= 0 && seasonId < (int)schema.seasons.size()) {
        heatMod = schema.seasons[seasonId].heatDrainMod;
        prodMod = schema.seasons[seasonId].productionMod;
    }
    if (res == RES_WOOD) {
        // Wood is essential in cold seasons; price floor rises with heat drain demand
        // heatDrainMod: 0.0 → floor 0.5, 0.15 → ~1.0, 0.4 → ~2.5, 1.0 → 5.0
        return 0.5f + 4.5f * heatMod;
    }
    if (res == RES_FOOD) {
        // Food is harder to produce in low-production seasons; floor rises when prodMod is low
        // prodMod 1.2 → 1.0, 0.2 → 2.0
        return (prodMod < 0.5f) ? 2.0f : 1.0f;
    }
    return PRICE_MIN;
}

// Fraction of price gap that closes per game-hour due to arbitrage pressure
// on an open road. Low enough to preserve meaningful differentials.
static constexpr float ARBITRAGE_RATE = 0.003f;

void PriceSystem::Update(entt::registry& registry, float realDt, const WorldSchema& schema) {
    auto tv = registry.view<TimeManager>();
    if (tv.begin() == tv.end()) return;
    const auto& tm = tv.get<TimeManager>(*tv.begin());
    float gameDt = tm.GameDt(realDt);
    if (gameDt <= 0.f) return;
    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;
    SeasonID seasonId = tm.CurrentSeason();

    // ---- Supply/demand price adjustment per settlement ----
    // Price spike cooldown: (entity, resourceType) → game-hours remaining
    static std::map<std::pair<entt::entity, int>, float> s_priceSpikeCooldown;
    // Drain cooldowns
    for (auto& [key, cd] : s_priceSpikeCooldown)
        cd = std::max(0.f, cd - gameHoursDt);

    auto* log = [&]() -> EventLog* {
        auto lv = registry.view<EventLog>();
        return (lv.begin() != lv.end()) ? &lv.get<EventLog>(*lv.begin()) : nullptr;
    }();

    registry.view<Market, Stockpile, Settlement>().each(
        [&](entt::entity e, Market& mkt, const Stockpile& sp, const Settlement& settl) {
            for (auto& [res, price] : mkt.price) {
                float oldPrice = price;
                auto it = sp.quantities.find(res);
                float stock = (it != sp.quantities.end()) ? it->second : 0.f;

                if (stock > PRICE_HIGH_STOCK)
                    price *= std::pow(1.f - PRICE_DECAY_RATE, gameHoursDt);
                else if (stock < PRICE_LOW_STOCK)
                    price *= std::pow(1.f + PRICE_RISE_RATE, gameHoursDt);

                float floor = SeasonPriceFloor(res, seasonId, schema);
                price = std::max(floor, std::min(PRICE_MAX, price));

                // Log price spikes > 20% (rate-limited to once per 12 game-hours per resource)
                if (oldPrice > 0.f && price > oldPrice * 1.2f) {
                    auto key = std::make_pair(e, static_cast<int>(res));
                    float& cd = s_priceSpikeCooldown[key];
                    if (cd <= 0.f && log) {
                        const char* resName = (res == RES_FOOD) ? "food"
                                            : (res == RES_WATER) ? "water" : "wood";
                        float pct = (price - oldPrice) / oldPrice * 100.f;
                        char buf[128];
                        std::snprintf(buf, sizeof(buf),
                            "Price spike: %s at %s now %.1fg (+%.0f%%)",
                            resName, settl.name.c_str(), price, pct);
                        log->Push(tm.day, (int)tm.hourOfDay, buf);
                        cd = 12.f;
                    }
                }
            }
        });

    // ---- Arbitrage convergence on open roads ----
    // For each open road, prices at both ends slowly converge toward each other.
    // This models the steady effect of hauler activity compressing trade margins.
    registry.view<Road>().each([&](const Road& road) {
        if (road.blocked) return;
        if (!registry.valid(road.from) || !registry.valid(road.to)) return;
        auto* mktA = registry.try_get<Market>(road.from);
        auto* mktB = registry.try_get<Market>(road.to);
        if (!mktA || !mktB) return;

        // Degraded roads have weaker arbitrage pressure — fewer haulers brave poor roads.
        // condition=1.0 → full rate; condition=0.15 → 25% rate.
        float condFactor = 0.25f + 0.75f * road.condition;
        float convergeFrac = std::min(1.f, ARBITRAGE_RATE * gameHoursDt * condFactor);

        // Alliance bonus: allied settlements (both scores > 0.5) converge 50% faster
        auto* sA = registry.try_get<Settlement>(road.from);
        auto* sB = registry.try_get<Settlement>(road.to);
        if (sA && sB) {
            auto itAB = sA->relations.find(road.to);
            auto itBA = sB->relations.find(road.from);
            bool allied = (itAB != sA->relations.end() && itAB->second > 0.5f)
                       && (itBA != sB->relations.end() && itBA->second > 0.5f);
            if (allied) convergeFrac = std::min(1.f, convergeFrac * 1.5f);
        }

        for (auto& [res, priceA] : mktA->price) {
            auto it = mktB->price.find(res);
            if (it == mktB->price.end()) continue;
            float& priceB = it->second;
            float mid     = (priceA + priceB) * 0.5f;
            priceA += (mid - priceA) * convergeFrac;
            priceB += (mid - priceB) * convergeFrac;
            // Re-apply floors after convergence
            priceA = std::max(SeasonPriceFloor(res, seasonId, schema), priceA);
            priceB = std::max(SeasonPriceFloor(res, seasonId, schema), priceB);
        }
    });
}
