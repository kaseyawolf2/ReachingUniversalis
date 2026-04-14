#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

// Adjusts market prices at each settlement based on stockpile levels.
// Prices rise when stock is scarce, fall when stock is abundant.
// This creates the price differentials that drive hauler profit-seeking.

class PriceSystem {
public:
    void Update(entt::registry& registry, float realDt, const WorldSchema& schema);
};
