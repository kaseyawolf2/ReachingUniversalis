#include "WorldGenerator.h"
#include "ECS/Components.h"
#include "raylib.h"
#include <cmath>

static constexpr float MAP_W = 2400.0f;
static constexpr float MAP_H  =  720.0f;

// Drain rates: full need lasts ~20 game-hours (Hunger), ~13 (Thirst), ~33 (Energy)
// At 1x: 1 gameDt second = 1 real second = 1 game minute.
static constexpr float DRAIN_HUNGER = 0.00083f;
static constexpr float DRAIN_THIRST = 0.00125f;
static constexpr float DRAIN_ENERGY = 0.00050f;
static constexpr float REFILL_HUNGER = 0.004f;
static constexpr float REFILL_THIRST = 0.006f;
static constexpr float REFILL_ENERGY = 0.002f;
static constexpr float CRIT_THRESHOLD = 0.3f;

static Needs MakeNeeds() {
    return Needs{{
        Need{ NeedType::Hunger, 1.f, DRAIN_HUNGER, CRIT_THRESHOLD, REFILL_HUNGER },
        Need{ NeedType::Thirst, 1.f, DRAIN_THIRST, CRIT_THRESHOLD, REFILL_THIRST },
        Need{ NeedType::Energy, 1.f, DRAIN_ENERGY, CRIT_THRESHOLD, REFILL_ENERGY }
    }};
}

static void SpawnNPCs(entt::registry& registry,
                      entt::entity settlement,
                      float cx, float cy, int count) {
    for (int i = 0; i < count; ++i) {
        float angle = (float)i / count * 2.f * PI;
        float ring  = 40.f + (i % 3) * 22.f;
        auto npc = registry.create();
        registry.emplace<Position>(npc, cx + std::cos(angle) * ring,
                                        cy + std::sin(angle) * ring);
        registry.emplace<Velocity>(npc, 0.f, 0.f);
        registry.emplace<MoveSpeed>(npc, 60.f);
        registry.emplace<Needs>(npc, MakeNeeds());
        registry.emplace<AgentState>(npc);
        registry.emplace<HomeSettlement>(npc, HomeSettlement{ settlement });
        registry.emplace<DeprivationTimer>(npc);
        registry.emplace<Schedule>(npc);
        registry.emplace<Renderable>(npc, WHITE, 6.f);
    }
}

static void SpawnHaulers(entt::registry& registry,
                         entt::entity settlement,
                         float cx, float cy, int count) {
    for (int i = 0; i < count; ++i) {
        float angle = (float)i / count * 2.f * PI + 0.5f;
        auto h = registry.create();
        registry.emplace<Position>(h, cx + std::cos(angle) * 90.f,
                                      cy + std::sin(angle) * 90.f);
        registry.emplace<Velocity>(h, 0.f, 0.f);
        registry.emplace<MoveSpeed>(h, 70.f);
        registry.emplace<Needs>(h, MakeNeeds());
        registry.emplace<AgentState>(h);
        registry.emplace<HomeSettlement>(h, HomeSettlement{ settlement });
        registry.emplace<DeprivationTimer>(h);
        registry.emplace<Inventory>(h, Inventory{ {}, 15 });
        registry.emplace<Hauler>(h);
        registry.emplace<Renderable>(h, SKYBLUE, 7.f);
    }
}

void WorldGenerator::Populate(entt::registry& registry) {

    // ---- Game clock ----
    registry.emplace<TimeManager>(registry.create());

    // ---- Event log ----
    registry.emplace<EventLog>(registry.create());

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
    registry.emplace<BirthTracker>(greenfield);
    registry.emplace<StockpileAlert>(greenfield);
    registry.emplace<Stockpile>(greenfield, Stockpile{{
        { ResourceType::Food,  120.f },
        { ResourceType::Water,  20.f }
    }});

    auto wellsworth = registry.create();
    registry.emplace<Position>(wellsworth, 2000.f, 360.f);
    registry.emplace<Settlement>(wellsworth, Settlement{ "Wellsworth", 120.f });
    registry.emplace<BirthTracker>(wellsworth);
    registry.emplace<StockpileAlert>(wellsworth);
    registry.emplace<Stockpile>(wellsworth, Stockpile{{
        { ResourceType::Food,   20.f },
        { ResourceType::Water, 120.f }
    }});

    // ---- Road ----
    registry.emplace<Road>(registry.create(), Road{ greenfield, wellsworth, false });

    // ---- Production facilities ----
    for (int i = 0; i < 2; ++i) {
        auto farm = registry.create();
        registry.emplace<Position>(farm, 400.f + (i == 0 ? -50.f : 50.f), 290.f);
        registry.emplace<ProductionFacility>(farm,
            ProductionFacility{ ResourceType::Food, 4.f, greenfield });
    }
    for (int i = 0; i < 2; ++i) {
        auto well = registry.create();
        registry.emplace<Position>(well, 2000.f + (i == 0 ? -50.f : 50.f), 290.f);
        registry.emplace<ProductionFacility>(well,
            ProductionFacility{ ResourceType::Water, 4.f, wellsworth });
    }
    // Shelter/rest spots — Energy need satisfaction
    {
        auto rest = registry.create();
        registry.emplace<Position>(rest, 400.f, 430.f);
        registry.emplace<ProductionFacility>(rest,
            ProductionFacility{ ResourceType::Shelter, 0.f, greenfield });
    }
    {
        auto rest = registry.create();
        registry.emplace<Position>(rest, 2000.f, 430.f);
        registry.emplace<ProductionFacility>(rest,
            ProductionFacility{ ResourceType::Shelter, 0.f, wellsworth });
    }

    // ---- Population ----
    SpawnNPCs(registry, greenfield, 400.f,  360.f, 20);
    SpawnNPCs(registry, wellsworth, 2000.f, 360.f, 20);

    // ---- Haulers (6 per settlement, shown in sky blue) ----
    SpawnHaulers(registry, greenfield, 400.f,  360.f, 6);
    SpawnHaulers(registry, wellsworth, 2000.f, 360.f, 6);

    // ---- Player ----
    auto player = registry.create();
    registry.emplace<Position>(player, 400.f, 420.f);
    registry.emplace<Velocity>(player, 0.f, 0.f);
    registry.emplace<MoveSpeed>(player, 100.f);
    registry.emplace<Needs>(player, MakeNeeds());
    registry.emplace<AgentState>(player);
    registry.emplace<HomeSettlement>(player, HomeSettlement{ greenfield });
    registry.emplace<DeprivationTimer>(player);
    registry.emplace<Inventory>(player);
    registry.emplace<Renderable>(player, YELLOW, 10.f);
    registry.emplace<PlayerTag>(player);
}
