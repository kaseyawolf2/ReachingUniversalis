#include "WorldGenerator.h"
#include "ECS/Components.h"
#include "raylib.h"
#include <cmath>

static constexpr float MAP_W = 2400.0f;
static constexpr float MAP_H  =  720.0f;

// Drain rates tuned so a full need lasts ~20 game-hours without supply.
// At 1x speed: 1 gameDt second = 1 real second = 1 game minute.
// 20 game-hours = 1200 gameDt seconds → drainRate = 1.0 / 1200 ≈ 0.00083
static constexpr float DRAIN_HUNGER = 0.00083f;   // ~20 game-hours to empty
static constexpr float DRAIN_THIRST = 0.00125f;   // ~13 game-hours to empty
static constexpr float DRAIN_ENERGY = 0.00050f;   // ~33 game-hours to empty

static constexpr float REFILL_HUNGER = 0.004f;    // ~4 min at facility to refill
static constexpr float REFILL_THIRST = 0.006f;
static constexpr float REFILL_ENERGY = 0.002f;

static constexpr float CRIT_THRESHOLD = 0.3f;

// Scatter NPCs around a settlement centre in rings.
static void SpawnNPCs(entt::registry& registry,
                      entt::entity settlement,
                      float cx, float cy,
                      int count) {
    for (int i = 0; i < count; ++i) {
        float angle = (float)i / count * 2.f * PI;
        float ring  = 40.f + (i % 3) * 22.f;

        auto npc = registry.create();
        registry.emplace<Position>(npc, cx + std::cos(angle) * ring,
                                        cy + std::sin(angle) * ring);
        registry.emplace<Velocity>(npc, 0.f, 0.f);
        registry.emplace<MoveSpeed>(npc, 60.f);
        registry.emplace<Needs>(npc, Needs{{
            Need{ NeedType::Hunger, 1.f, DRAIN_HUNGER, CRIT_THRESHOLD, REFILL_HUNGER },
            Need{ NeedType::Thirst, 1.f, DRAIN_THIRST, CRIT_THRESHOLD, REFILL_THIRST },
            Need{ NeedType::Energy, 1.f, DRAIN_ENERGY, CRIT_THRESHOLD, REFILL_ENERGY }
        }});
        registry.emplace<AgentState>(npc);
        registry.emplace<HomeSettlement>(npc, HomeSettlement{ settlement });
        registry.emplace<DeprivationTimer>(npc);
        registry.emplace<Renderable>(npc, WHITE, 6.f);
    }
}

void WorldGenerator::Populate(entt::registry& registry) {

    // ---- Game clock ----
    auto clock = registry.create();
    registry.emplace<TimeManager>(clock);

    // ---- Camera ----
    auto camEntity = registry.create();
    auto& cs = registry.emplace<CameraState>(camEntity);
    cs.cam.offset = { 640.0f, 360.0f };
    cs.cam.target = { MAP_W * 0.5f, MAP_H * 0.5f };
    cs.cam.zoom   = 0.5f;

    // ---- Settlements ----

    auto greenfield = registry.create();
    registry.emplace<Position>(greenfield, 400.f, 360.f);
    registry.emplace<Settlement>(greenfield, Settlement{ "Greenfield", 120.f });
    registry.emplace<Stockpile>(greenfield, Stockpile{{
        { ResourceType::Food,  20.f },
        { ResourceType::Water,  0.f }
    }});

    auto wellsworth = registry.create();
    registry.emplace<Position>(wellsworth, 2000.f, 360.f);
    registry.emplace<Settlement>(wellsworth, Settlement{ "Wellsworth", 120.f });
    registry.emplace<Stockpile>(wellsworth, Stockpile{{
        { ResourceType::Food,   0.f },
        { ResourceType::Water, 20.f }
    }});

    // ---- Road ----
    auto road = registry.create();
    registry.emplace<Road>(road, Road{ greenfield, wellsworth, false });

    // ---- Production facilities ----

    for (int i = 0; i < 2; ++i) {
        auto farm = registry.create();
        registry.emplace<Position>(farm, 400.f + (i == 0 ? -50.f : 50.f), 300.f);
        registry.emplace<ProductionFacility>(farm,
            ProductionFacility{ ResourceType::Food, 1.f, greenfield });
    }

    for (int i = 0; i < 2; ++i) {
        auto well = registry.create();
        registry.emplace<Position>(well, 2000.f + (i == 0 ? -50.f : 50.f), 300.f);
        registry.emplace<ProductionFacility>(well,
            ProductionFacility{ ResourceType::Water, 1.f, wellsworth });
    }

    // ---- NPC population ----
    SpawnNPCs(registry, greenfield, 400.f,  360.f, 20);
    SpawnNPCs(registry, wellsworth, 2000.f, 360.f, 20);

    // ---- Player (distinct: larger, brighter, starts in Greenfield) ----
    auto player = registry.create();
    registry.emplace<Position>(player, 400.f, 420.f);
    registry.emplace<Velocity>(player, 0.f, 0.f);
    registry.emplace<MoveSpeed>(player, 80.f);
    registry.emplace<Needs>(player, Needs{{
        Need{ NeedType::Hunger, 1.f, DRAIN_HUNGER, CRIT_THRESHOLD, REFILL_HUNGER },
        Need{ NeedType::Thirst, 1.f, DRAIN_THIRST, CRIT_THRESHOLD, REFILL_THIRST },
        Need{ NeedType::Energy, 1.f, DRAIN_ENERGY, CRIT_THRESHOLD, REFILL_ENERGY }
    }});
    registry.emplace<AgentState>(player);
    registry.emplace<HomeSettlement>(player, HomeSettlement{ greenfield });
    registry.emplace<DeprivationTimer>(player);
    registry.emplace<Renderable>(player, YELLOW, 10.f);
    registry.emplace<PlayerTag>(player);
}
