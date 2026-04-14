#pragma once
#include <entt/entt.hpp>
#include <random>

struct WorldSchema;

// Simulates economic mobility: NPCs can become haulers when they accumulate
// enough capital, and haulers who go bankrupt return to common labor.
//
// This creates dynamic social stratification without any scripted logic.
// A successful farmer saves money → invests in a cart → becomes a merchant.
// A failed merchant loses capital → sells the cart → returns to day labor.

class EconomicMobilitySystem {
public:
    void Update(entt::registry& registry, float realDt, const WorldSchema& schema);

private:
    std::mt19937 m_rng{std::random_device{}()};

    float m_checkAccum = 0.f;   // accumulates game-hours since last full scan
};
