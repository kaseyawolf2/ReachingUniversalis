#include "WorldGenerator.h"
#include "ECS/Components.h"
#include "raylib.h"
#include <array>
#include <cmath>
#include <random>

static constexpr float MAP_W = 2400.0f;
static constexpr float MAP_H  =  720.0f;

// Fixed-seed RNG for reproducible world generation
static std::mt19937 wg_rng{12345u};
static std::uniform_real_distribution<float> migrate_dist(1.f, 10.f);   // game-hours (wide range → staggered migration)
static std::uniform_real_distribution<float> wait_dist(0.f, 1.f);       // hauler stagger
static std::uniform_real_distribution<float> age_dist(0.f, 30.f);       // starting age days
static std::uniform_real_distribution<float> lifespan_dist(60.f, 100.f);// life expectancy days
static std::uniform_real_distribution<float> trait_dist(0.80f, 1.20f);  // ±20% personality trait variance

// ---- Name generation ----
static const std::array<const char*, 30> FIRST_NAMES = {
    "Aldric","Brom","Cedric","Daven","Edric","Finn","Gareth","Holt","Ivan","Jorin",
    "Kael","Lewin","Marden","Nolan","Oswin","Pell","Roran","Sven","Torben","Uric",
    "Vance","Wren","Xander","Yoric","Zane","Aela","Bryn","Clara","Dena","Elara"
};
static const std::array<const char*, 20> LAST_NAMES = {
    "Smith","Miller","Cooper","Fletcher","Mason","Tanner","Ward","Thatcher",
    "Fisher","Baker","Forger","Webb","Stone","Holt","Reed","Marsh","Wood",
    "Vale","Cross","Bridge"
};
static std::uniform_int_distribution<int> first_dist(0, (int)FIRST_NAMES.size()-1);
static std::uniform_int_distribution<int> last_dist(0,  (int)LAST_NAMES.size()-1);

static std::string MakeName() {
    return std::string(FIRST_NAMES[first_dist(wg_rng)]) + " " +
           LAST_NAMES[last_dist(wg_rng)];
}

// Drain rates: full need lasts ~20 game-hours (Hunger), ~13 (Thirst), ~33 (Energy).
// Heat drain is seasonal — ConsumptionSystem cancels it when Wood stockpile is available.
// At 1x: 1 gameDt second = 1 real second = 1 game minute.
static constexpr float DRAIN_HUNGER = 0.00083f;
static constexpr float DRAIN_THIRST = 0.00125f;
static constexpr float DRAIN_ENERGY = 0.00050f;
static constexpr float DRAIN_HEAT   = 0.00200f;   // fast drain — cancelled by wood fuel
static constexpr float REFILL_HUNGER = 0.004f;
static constexpr float REFILL_THIRST = 0.006f;
static constexpr float REFILL_ENERGY = 0.002f;
static constexpr float REFILL_HEAT   = 0.010f;    // warm up quickly when fuel available
static constexpr float CRIT_THRESHOLD = 0.3f;

static Needs MakeNeeds() {
    return Needs{{
        Need{ NeedType::Hunger, 1.f, DRAIN_HUNGER, CRIT_THRESHOLD, REFILL_HUNGER },
        Need{ NeedType::Thirst, 1.f, DRAIN_THIRST, CRIT_THRESHOLD, REFILL_THIRST },
        Need{ NeedType::Energy, 1.f, DRAIN_ENERGY, CRIT_THRESHOLD, REFILL_ENERGY },
        Need{ NeedType::Heat,   1.f, DRAIN_HEAT,   CRIT_THRESHOLD, REFILL_HEAT   }
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
        auto npcNeeds = MakeNeeds();
        // Personality variation: ±20% on need drain rates — some NPCs are sturdier or frailer
        for (auto& need : npcNeeds.list)
            need.drainRate *= trait_dist(wg_rng);
        registry.emplace<Needs>(npc, npcNeeds);
        registry.emplace<AgentState>(npc);
        registry.emplace<HomeSettlement>(npc, HomeSettlement{ settlement });
        // Randomise migration threshold: NPCs flee at different times (1–10 game-hours)
        DeprivationTimer dt;
        dt.migrateThreshold = migrate_dist(wg_rng) * 60.f;
        registry.emplace<DeprivationTimer>(npc, dt);
        registry.emplace<Schedule>(npc);
        registry.emplace<Renderable>(npc, WHITE, 6.f);
        registry.emplace<Money>(npc, Money{ 10.f });   // small starting purse
        Age age;
        age.days    = age_dist(wg_rng);
        age.maxDays = lifespan_dist(wg_rng);
        registry.emplace<Age>(npc, age);
        registry.emplace<Name>(npc, Name{ MakeName() });
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
        // Haulers work around the clock with no sleep schedule — zero energy drain
        // so DeathSystem never kills them from exhaustion.
        registry.get<Needs>(h).list[2].drainRate = 0.f;
        registry.emplace<AgentState>(h);
        registry.emplace<HomeSettlement>(h, HomeSettlement{ settlement });
        DeprivationTimer hdt;
        hdt.migrateThreshold = migrate_dist(wg_rng) * 60.f;
        registry.emplace<DeprivationTimer>(h, hdt);
        registry.emplace<Inventory>(h, Inventory{ {}, 15 });
        // Stagger hauler wait timers so they don't all rush for cargo simultaneously
        Hauler haulerComp;
        haulerComp.waitTimer = wait_dist(wg_rng);
        registry.emplace<Hauler>(h, haulerComp);
        registry.emplace<Money>(h);   // starting wallet: 50 gold
        registry.emplace<Renderable>(h, SKYBLUE, 7.f);
        Age hage;
        hage.days    = age_dist(wg_rng);
        hage.maxDays = lifespan_dist(wg_rng);
        registry.emplace<Age>(h, hage);
        registry.emplace<Name>(h, Name{ MakeName() });
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
        { ResourceType::Water,  80.f },   // raised from 20 — gives haulers time to supply
        { ResourceType::Wood,    0.f }
    }});
    // Market: Food cheap (surplus), Water expensive (scarce), Wood scarce
    registry.emplace<Market>(greenfield, Market{{
        { ResourceType::Food,  2.0f },
        { ResourceType::Water, 8.0f },
        { ResourceType::Wood,  6.0f }
    }});

    auto wellsworth = registry.create();
    registry.emplace<Position>(wellsworth, 2000.f, 360.f);
    registry.emplace<Settlement>(wellsworth, Settlement{ "Wellsworth", 120.f });
    registry.emplace<BirthTracker>(wellsworth);
    registry.emplace<StockpileAlert>(wellsworth);
    registry.emplace<Stockpile>(wellsworth, Stockpile{{
        { ResourceType::Food,   20.f },
        { ResourceType::Water, 120.f },
        { ResourceType::Wood,    0.f }
    }});
    // Market: Water cheap (surplus), Food expensive (scarce), Wood scarce
    registry.emplace<Market>(wellsworth, Market{{
        { ResourceType::Food,  8.0f },
        { ResourceType::Water, 2.0f },
        { ResourceType::Wood,  6.0f }
    }});

    auto millhaven = registry.create();
    registry.emplace<Position>(millhaven, 1200.f, 200.f);
    registry.emplace<Settlement>(millhaven, Settlement{ "Millhaven", 120.f });
    registry.emplace<BirthTracker>(millhaven);
    registry.emplace<StockpileAlert>(millhaven);
    registry.emplace<Stockpile>(millhaven, Stockpile{{
        { ResourceType::Food,   30.f },
        { ResourceType::Water,  30.f },
        { ResourceType::Wood,  120.f }
    }});
    // Market: Wood cheap (surplus), Food/Water mid-priced (imported)
    registry.emplace<Market>(millhaven, Market{{
        { ResourceType::Food,  5.0f },
        { ResourceType::Water, 5.0f },
        { ResourceType::Wood,  1.5f }
    }});

    // ---- Roads ----
    registry.emplace<Road>(registry.create(), Road{ greenfield, wellsworth, false });
    registry.emplace<Road>(registry.create(), Road{ greenfield, millhaven,  false });
    registry.emplace<Road>(registry.create(), Road{ millhaven,  wellsworth, false });

    // ---- Production facilities ----
    // Farms require 0.15 water per food unit produced — supply-chain dependency
    // At full workers (2 farms × baseRate 4 × scale 2.0): 2.4 water/hr consumed by farms.
    // Greenfield starts with 80 water, giving haulers time to establish water imports.
    for (int i = 0; i < 2; ++i) {
        auto farm = registry.create();
        registry.emplace<Position>(farm, 400.f + (i == 0 ? -50.f : 50.f), 290.f);
        registry.emplace<ProductionFacility>(farm,
            ProductionFacility{ ResourceType::Food, 4.f, greenfield,
                                {{ ResourceType::Water, 0.15f }} });
    }
    for (int i = 0; i < 2; ++i) {
        auto well = registry.create();
        registry.emplace<Position>(well, 2000.f + (i == 0 ? -50.f : 50.f), 290.f);
        registry.emplace<ProductionFacility>(well,
            ProductionFacility{ ResourceType::Water, 4.f, wellsworth, {} });
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
    // Millhaven: 2 lumber mills + shelter
    for (int i = 0; i < 2; ++i) {
        auto lmill = registry.create();
        registry.emplace<Position>(lmill, 1200.f + (i == 0 ? -50.f : 50.f), 130.f);
        registry.emplace<ProductionFacility>(lmill,
            ProductionFacility{ ResourceType::Wood, 3.f, millhaven });
    }
    {
        auto rest = registry.create();
        registry.emplace<Position>(rest, 1200.f, 290.f);
        registry.emplace<ProductionFacility>(rest,
            ProductionFacility{ ResourceType::Shelter, 0.f, millhaven });
    }

    // ---- Population ----
    SpawnNPCs(registry, greenfield, 400.f,  360.f, 20);
    SpawnNPCs(registry, wellsworth, 2000.f, 360.f, 20);
    SpawnNPCs(registry, millhaven,  1200.f, 200.f, 20);

    // ---- Haulers (6 per settlement, shown in sky blue) ----
    SpawnHaulers(registry, greenfield, 400.f,  360.f, 6);
    SpawnHaulers(registry, wellsworth, 2000.f, 360.f, 6);
    SpawnHaulers(registry, millhaven,  1200.f, 200.f, 6);

    // ---- Player ----
    auto player = registry.create();
    registry.emplace<Position>(player, 400.f, 420.f);
    registry.emplace<Velocity>(player, 0.f, 0.f);
    registry.emplace<MoveSpeed>(player, 100.f);
    registry.emplace<Needs>(player, MakeNeeds());
    registry.emplace<AgentState>(player);
    registry.emplace<HomeSettlement>(player, HomeSettlement{ greenfield });
    registry.emplace<DeprivationTimer>(player);
    registry.emplace<Inventory>(player, Inventory{ {}, 15 });   // 15-unit carry capacity
    registry.emplace<Money>(player);                            // 50 gold starting wallet
    registry.emplace<Renderable>(player, YELLOW, 10.f);
    registry.emplace<PlayerTag>(player);
    registry.emplace<Name>(player, Name{ "You" });   // player always named "You"
    // Player ages like any NPC — starts young, dies of old age eventually
    Age playerAge;
    playerAge.days    = 0.f;
    playerAge.maxDays = lifespan_dist(wg_rng);
    registry.emplace<Age>(player, playerAge);
}
