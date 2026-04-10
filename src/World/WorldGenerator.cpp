#include "WorldGenerator.h"
#include "ECS/Components.h"
#include "raylib.h"

// Map is 2400x720.
// Greenfield is the farming town on the left; Wellsworth is the well town on the right.
static constexpr float MAP_W = 2400.0f;
static constexpr float MAP_H  =  720.0f;

void WorldGenerator::Populate(entt::registry& registry) {

    // ---- Game clock ----
    auto clock = registry.create();
    registry.emplace<TimeManager>(clock);

    // ---- Camera ----
    auto camEntity = registry.create();
    auto& cs = registry.emplace<CameraState>(camEntity);
    cs.cam.offset = { 640.0f, 360.0f };
    cs.cam.target = { MAP_W * 0.5f, MAP_H * 0.5f };
    // Zoom out so the full 2400-unit map fits the 1280px window with margin.
    // At 0.5 zoom: viewport = 2560x1440 units — both settlements visible.
    cs.cam.zoom = 0.5f;

    // ---- Settlements ----

    auto greenfield = registry.create();
    registry.emplace<Position>(greenfield, 400.0f, 360.0f);
    registry.emplace<Settlement>(greenfield, Settlement{ "Greenfield", 120.0f });
    registry.emplace<Stockpile>(greenfield, Stockpile{{
        { ResourceType::Food,  0.0f },
        { ResourceType::Water, 0.0f }
    }});

    auto wellsworth = registry.create();
    registry.emplace<Position>(wellsworth, 2000.0f, 360.0f);
    registry.emplace<Settlement>(wellsworth, Settlement{ "Wellsworth", 120.0f });
    registry.emplace<Stockpile>(wellsworth, Stockpile{{
        { ResourceType::Food,  0.0f },
        { ResourceType::Water, 0.0f }
    }});

    // ---- Road ----
    auto road = registry.create();
    registry.emplace<Road>(road, Road{ greenfield, wellsworth, false });

    // ---- Production facilities: Greenfield has 2 Farms ----
    for (int i = 0; i < 2; ++i) {
        auto farm = registry.create();
        float ox = (i == 0) ? -50.0f : 50.0f;
        registry.emplace<Position>(farm, 400.0f + ox, 300.0f);
        registry.emplace<ProductionFacility>(farm,
            ProductionFacility{ ResourceType::Food, 1.0f, greenfield });
    }

    // ---- Production facilities: Wellsworth has 2 Wells ----
    for (int i = 0; i < 2; ++i) {
        auto well = registry.create();
        float ox = (i == 0) ? -50.0f : 50.0f;
        registry.emplace<Position>(well, 2000.0f + ox, 300.0f);
        registry.emplace<ProductionFacility>(well,
            ProductionFacility{ ResourceType::Water, 1.0f, wellsworth });
    }

    // ---- Player NPC — starts in Greenfield ----
    auto npc = registry.create();
    registry.emplace<Position>(npc, 400.0f, 360.0f);
    registry.emplace<Velocity>(npc, 0.0f, 0.0f);
    registry.emplace<MoveSpeed>(npc, 80.0f);
    registry.emplace<Needs>(npc, Needs{{
        Need{ NeedType::Hunger, 1.0f, 0.04f, 0.3f, 0.25f },
        Need{ NeedType::Thirst, 1.0f, 0.07f, 0.3f, 0.35f },
        Need{ NeedType::Energy, 1.0f, 0.02f, 0.3f, 0.15f }
    }});
    registry.emplace<AgentState>(npc);
    registry.emplace<HomeSettlement>(npc, HomeSettlement{ greenfield });
    registry.emplace<Renderable>(npc, WHITE, 10.0f);
    registry.emplace<PlayerTag>(npc);
}
