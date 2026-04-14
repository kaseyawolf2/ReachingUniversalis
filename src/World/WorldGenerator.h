#pragma once
#include <entt/entt.hpp>

struct WorldSchema;

class WorldGenerator {
public:
    // Populate the registry with the initial Alpha scenario entities.
    // WP0: 3 resource nodes + 1 NPC (visual parity with old OOP build).
    static void Populate(entt::registry& registry, const WorldSchema& schema);
};
