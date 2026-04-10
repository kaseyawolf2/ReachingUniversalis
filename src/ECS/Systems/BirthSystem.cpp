#include "BirthSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <map>
#include <random>

// A new NPC is born at a settlement when:
//   - Population at that settlement < MAX_POP_PER_SETTLEMENT
//   - Stockpile has at least BIRTH_FOOD_MIN food AND BIRTH_WATER_MIN water
//   - The birth accumulator (tracked per settlement) has reached BIRTH_INTERVAL
//
// Each birth costs BIRTH_FOOD_COST food and BIRTH_WATER_COST water.

static constexpr int   MAX_POP_PER_SETTLEMENT = 35;
static constexpr float BIRTH_INTERVAL         = 3.f * 60.f;   // 3 game-hours
static constexpr float BIRTH_FOOD_MIN         = 30.f;
static constexpr float BIRTH_WATER_MIN        = 30.f;
static constexpr float BIRTH_FOOD_COST        = 10.f;
static constexpr float BIRTH_WATER_COST       = 10.f;

// Drain rates match WorldGenerator values
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

void BirthSystem::Update(entt::registry& registry, float realDt) {
    auto tmv = registry.view<TimeManager>();
    if (tmv.begin() == tmv.end()) return;
    const auto& tm = tmv.get<TimeManager>(*tmv.begin());
    float gameDt = tm.GameDt(realDt);
    if (gameDt <= 0.f) return;
    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;

    // Count population per settlement
    std::map<entt::entity, int> popCount;
    registry.view<HomeSettlement>(entt::exclude<Hauler, PlayerTag>).each(
        [&](const HomeSettlement& hs) {
            popCount[hs.settlement]++;
        });

    // EventLog for birth announcements
    auto logv = registry.view<EventLog>();
    EventLog* log = (logv.begin() == logv.end())
                    ? nullptr : &logv.get<EventLog>(*logv.begin());

    // Check each settlement
    auto settlView = registry.view<Position, Settlement, Stockpile, BirthTracker>();
    for (auto settl : settlView) {
        auto& tracker  = settlView.get<BirthTracker>(settl);
        auto& stockpile = settlView.get<Stockpile>(settl);
        const auto& spos = settlView.get<Position>(settl);

        float food  = stockpile.quantities.count(ResourceType::Food)
                      ? stockpile.quantities[ResourceType::Food]  : 0.f;
        float water = stockpile.quantities.count(ResourceType::Water)
                      ? stockpile.quantities[ResourceType::Water] : 0.f;
        int   pop   = popCount.count(settl) ? popCount[settl] : 0;

        bool canBirth = (pop < MAX_POP_PER_SETTLEMENT)
                     && (food  >= BIRTH_FOOD_MIN)
                     && (water >= BIRTH_WATER_MIN);

        if (canBirth) {
            tracker.accumulator += gameHoursDt;
        } else {
            // Decay the timer when conditions aren't met so it doesn't
            // trigger immediately when conditions return after a drought.
            tracker.accumulator = std::max(0.f, tracker.accumulator - gameHoursDt * 0.5f);
        }

        if (tracker.accumulator >= BIRTH_INTERVAL) {
            tracker.accumulator -= BIRTH_INTERVAL;

            // Deduct birth cost
            stockpile.quantities[ResourceType::Food]  -= BIRTH_FOOD_COST;
            stockpile.quantities[ResourceType::Water] -= BIRTH_WATER_COST;

            // Spawn new NPC at a ring around the settlement centre
            float angle = (float)(pop % 20) / 20.f * 2.f * 3.14159f;
            float ring  = 50.f + (float)(pop % 3) * 22.f;
            // Randomise migration threshold so NPCs don't all flee simultaneously
            static std::mt19937 s_rng{std::random_device{}()};
            static std::uniform_real_distribution<float> s_dist(2.f, 5.f);
            DeprivationTimer dt;
            dt.migrateThreshold = s_dist(s_rng) * 60.f;  // 2–5 game-hours

            auto npc = registry.create();
            registry.emplace<Position>(npc,
                spos.x + std::cos(angle) * ring,
                spos.y + std::sin(angle) * ring);
            registry.emplace<Velocity>(npc, 0.f, 0.f);
            registry.emplace<MoveSpeed>(npc, 60.f);
            registry.emplace<Needs>(npc, MakeNeeds());
            registry.emplace<AgentState>(npc);
            registry.emplace<HomeSettlement>(npc, HomeSettlement{ settl });
            registry.emplace<DeprivationTimer>(npc, dt);
            registry.emplace<Schedule>(npc);
            registry.emplace<Renderable>(npc, WHITE, 6.f);

            if (log) {
                const auto& s = settlView.get<Settlement>(settl);
                log->Push(tm.day, (int)tm.hourOfDay,
                          "Birth at " + s.name + " (pop " + std::to_string(pop + 1) + ")");
            }
        }
    }
}
