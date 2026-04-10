#include "RandomEventSystem.h"
#include "ECS/Components.h"
#include <algorithm>
#include <vector>
#include <string>

static constexpr float DROUGHT_MODIFIER  = 0.2f;   // production factor during drought
static constexpr float DROUGHT_DURATION  = 8.f;    // game-hours
static constexpr float BLIGHT_FRACTION   = 0.35f;  // fraction of food stockpile destroyed
static constexpr float BANDIT_DURATION   = 3.f;    // game-hours road is blocked
static constexpr float EVENT_MEAN_HOURS  = 72.f;   // ~3 game-days between events
static constexpr float EVENT_JITTER      = 36.f;   // ±jitter in game-hours

void RandomEventSystem::Update(entt::registry& registry, float realDt) {
    auto tv = registry.view<TimeManager>();
    if (tv.begin() == tv.end()) return;
    const auto& tm = tv.get<TimeManager>(*tv.begin());
    float gameDt = tm.GameDt(realDt);
    if (gameDt <= 0.f) return;
    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;

    // Tick down active settlement modifiers (drought recovery)
    registry.view<Settlement>().each([&](Settlement& s) {
        if (s.modifierDuration > 0.f) {
            s.modifierDuration -= gameHoursDt;
            if (s.modifierDuration <= 0.f) {
                s.modifierDuration   = 0.f;
                s.productionModifier = 1.f;

                auto lv = registry.view<EventLog>();
                if (lv.begin() != lv.end())
                    lv.get<EventLog>(*lv.begin()).Push(
                        tm.day, (int)tm.hourOfDay,
                        s.modifierName + " ends at " + s.name + " — production restored");

                s.modifierName.clear();
            }
        }
    });

    // Tick down bandit timers (auto-clear road)
    registry.view<Road>().each([&](Road& road) {
        if (road.banditTimer > 0.f) {
            road.banditTimer -= gameHoursDt;
            if (road.banditTimer <= 0.f) {
                road.banditTimer = 0.f;
                road.blocked     = false;

                auto lv = registry.view<EventLog>();
                if (lv.begin() != lv.end())
                    lv.get<EventLog>(*lv.begin()).Push(
                        tm.day, (int)tm.hourOfDay,
                        "Bandits dispersed — road reopened");
            }
        }
    });

    // Count down to next event
    m_nextEvent -= gameHoursDt;
    if (m_nextEvent > 0.f) return;

    // Schedule next
    std::uniform_real_distribution<float> jitter(-EVENT_JITTER, EVENT_JITTER);
    m_nextEvent = std::max(12.f, EVENT_MEAN_HOURS + jitter(m_rng));

    TriggerEvent(registry, tm.day, (int)tm.hourOfDay);
}

void RandomEventSystem::TriggerEvent(entt::registry& registry, int day, int hour) {
    auto lv = registry.view<EventLog>();
    EventLog* log = (lv.begin() == lv.end())
                    ? nullptr : &lv.get<EventLog>(*lv.begin());

    // Get current season for seasonal events
    Season season = Season::Spring;
    {
        auto tmv = registry.view<TimeManager>();
        if (tmv.begin() != tmv.end())
            season = tmv.get<TimeManager>(*tmv.begin()).CurrentSeason();
    }

    // Collect valid settlements
    std::vector<entt::entity> settlements;
    registry.view<Settlement>().each([&](auto e, const Settlement&) {
        settlements.push_back(e);
    });
    if (settlements.empty()) return;

    std::uniform_int_distribution<int> pickSettl(0, (int)settlements.size() - 1);
    std::uniform_int_distribution<int> pickType(0, 9);  // 0-5=always 6=winter 7=spring 8=autumn 9=off-map convoy

    entt::entity target = settlements[pickSettl(m_rng)];
    auto* settl = registry.try_get<Settlement>(target);
    if (!settl) return;

    switch (pickType(m_rng)) {

    case 0: {   // Drought — cripple production
        if (settl->modifierDuration > 0.f) break;   // already has an event
        settl->productionModifier = DROUGHT_MODIFIER;
        settl->modifierDuration   = DROUGHT_DURATION;
        settl->modifierName       = "Drought";
        if (log) log->Push(day, hour,
            "DROUGHT strikes " + settl->name + " ("
            + std::to_string((int)DROUGHT_DURATION) + "h)");
        break;
    }

    case 1: {   // Blight — destroy food stockpile
        auto* sp = registry.try_get<Stockpile>(target);
        if (!sp) break;
        auto it = sp->quantities.find(ResourceType::Food);
        if (it == sp->quantities.end() || it->second < 5.f) break;
        float lost = it->second * BLIGHT_FRACTION;
        it->second -= lost;
        if (log) log->Push(day, hour,
            "BLIGHT hits " + settl->name + " — "
            + std::to_string((int)lost) + " food destroyed");
        break;
    }

    case 2: {   // Bandits — block one random open road temporarily
        std::vector<entt::entity> openRoads;
        registry.view<Road>().each([&](auto e, const Road& road) {
            if (!road.blocked && road.banditTimer <= 0.f)
                openRoads.push_back(e);
        });
        if (openRoads.empty()) break;
        std::uniform_int_distribution<int> pickRoad(0, (int)openRoads.size() - 1);
        auto& road = registry.get<Road>(openRoads[pickRoad(m_rng)]);
        road.blocked     = true;
        road.banditTimer = BANDIT_DURATION;
        if (log) log->Push(day, hour,
            "BANDITS blocking road ("
            + std::to_string((int)BANDIT_DURATION) + "h)");
        break;
    }

    case 3: {   // Disease outbreak — kills 15% of settlement population instantly
        std::vector<entt::entity> victims;
        registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
            [&](auto e, const HomeSettlement& hs) {
                if (hs.settlement == target) victims.push_back(e);
            });
        if (victims.size() < 3) break;   // too small to be meaningful
        std::shuffle(victims.begin(), victims.end(), m_rng);
        int killCount = std::max(1, (int)(victims.size() * 0.15f));
        for (int i = 0; i < killCount; ++i) {
            if (registry.valid(victims[i])) registry.destroy(victims[i]);
        }
        if (log) log->Push(day, hour,
            "DISEASE outbreak at " + settl->name
            + " — " + std::to_string(killCount) + " died");
        break;
    }

    case 4: {   // Trade boom — inject gold into settlement treasury
        static constexpr float BOOM_GOLD = 150.f;
        settl->treasury += BOOM_GOLD;
        if (log) log->Push(day, hour,
            "TRADE BOOM at " + settl->name
            + " — treasury +" + std::to_string((int)BOOM_GOLD) + "g");
        break;
    }

    case 6: {   // Blizzard (Winter only) — blocks ALL roads for 4 game-hours
        if (season != Season::Winter) break;
        static constexpr float BLIZZARD_DURATION = 4.f;
        int blockedCount = 0;
        registry.view<Road>().each([&](Road& road) {
            if (!road.blocked) {
                road.blocked     = true;
                road.banditTimer = BLIZZARD_DURATION;  // auto-clears after duration
                ++blockedCount;
            }
        });
        if (blockedCount > 0 && log)
            log->Push(day, hour,
                "BLIZZARD — all " + std::to_string(blockedCount) + " roads blocked ("
                + std::to_string((int)BLIZZARD_DURATION) + "h)");
        break;
    }

    case 7: {   // Spring flood — destroys 40% of food at a random settlement
        if (season != Season::Spring) break;
        auto* sp = registry.try_get<Stockpile>(target);
        if (!sp) break;
        auto it = sp->quantities.find(ResourceType::Food);
        if (it == sp->quantities.end() || it->second < 5.f) break;
        float lost = it->second * 0.40f;
        it->second -= lost;
        if (log) log->Push(day, hour,
            "SPRING FLOOD at " + settl->name + " — "
            + std::to_string((int)lost) + " food washed away");
        break;
    }

    case 8: {   // Harvest bounty (Autumn only) — production boost for 12 game-hours
        if (season != Season::Autumn) break;
        if (settl->modifierDuration > 0.f) break;   // already has an event
        static constexpr float BOUNTY_MODIFIER  = 1.5f;
        static constexpr float BOUNTY_DURATION  = 12.f;
        settl->productionModifier = BOUNTY_MODIFIER;
        settl->modifierDuration   = BOUNTY_DURATION;
        settl->modifierName       = "Harvest Bounty";
        if (log) log->Push(day, hour,
            "HARVEST BOUNTY at " + settl->name + " (+50% production, "
            + std::to_string((int)BOUNTY_DURATION) + "h)");
        break;
    }

    case 5: {   // Migration wave — 3-5 NPCs arrive from outside
        std::uniform_int_distribution<int> count_dist(3, 5);
        int arrivals = count_dist(m_rng);
        const auto* tpos = registry.try_get<Position>(target);
        if (!tpos) break;

        static const float DRAIN_HUNGER = 0.00083f, DRAIN_THIRST = 0.00125f,
                           DRAIN_ENERGY = 0.00050f, DRAIN_HEAT = 0.00200f;
        static const float REFILL_H = 0.004f, REFILL_T = 0.006f, REFILL_E = 0.002f,
                           REFILL_HEAT = 0.010f;
        static const char* FIRSTS[] = {
            "Aldric","Brom","Cedric","Daven","Edric","Finn","Gareth","Holt","Ivan","Jorin",
            "Kael","Lewin","Marden","Nolan","Oswin","Pell","Roran","Sven","Torben","Uric",
            "Vance","Wren","Xander","Yoric","Zane","Aela","Bryn","Clara","Dena","Elara"
        };
        static const char* LASTS[] = {
            "Smith","Miller","Cooper","Fletcher","Mason","Tanner","Ward","Thatcher",
            "Fisher","Baker","Forger","Webb","Stone","Holt","Reed","Marsh","Wood",
            "Vale","Cross","Bridge"
        };
        std::uniform_int_distribution<int> fd(0, 29), ld(0, 19);
        std::uniform_real_distribution<float> angle_dist(0.f, 6.28f);
        std::uniform_real_distribution<float> life_dist(60.f, 100.f);
        std::uniform_real_distribution<float> age_dist2(5.f, 25.f);
        std::uniform_real_distribution<float> mt_dist(1.f, 10.f);
        std::uniform_real_distribution<float> trait_dist(0.80f, 1.20f);

        for (int i = 0; i < arrivals; ++i) {
            float ang = angle_dist(m_rng);
            auto npc = registry.create();
            registry.emplace<Position>(npc, tpos->x + std::cos(ang)*60.f,
                                            tpos->y + std::sin(ang)*60.f);
            registry.emplace<Velocity>(npc, 0.f, 0.f);
            registry.emplace<MoveSpeed>(npc, 60.f);
            Needs npcNeeds{{
                Need{NeedType::Hunger, 0.6f, DRAIN_HUNGER, 0.3f, REFILL_H},
                Need{NeedType::Thirst, 0.6f, DRAIN_THIRST, 0.3f, REFILL_T},
                Need{NeedType::Energy, 0.8f, DRAIN_ENERGY, 0.3f, REFILL_E},
                Need{NeedType::Heat,   0.8f, DRAIN_HEAT,   0.3f, REFILL_HEAT}
            }};
            // Personality variation: ±20% drain rates
            for (auto& need : npcNeeds.list) need.drainRate *= trait_dist(m_rng);
            registry.emplace<Needs>(npc, npcNeeds);
            registry.emplace<AgentState>(npc);
            registry.emplace<HomeSettlement>(npc, HomeSettlement{target});
            DeprivationTimer dt; dt.migrateThreshold = mt_dist(m_rng) * 60.f;
            registry.emplace<DeprivationTimer>(npc, dt);
            registry.emplace<Schedule>(npc);
            registry.emplace<Renderable>(npc, WHITE, 6.f);
            registry.emplace<Money>(npc, Money{5.f});
            Age age; age.days = age_dist2(m_rng); age.maxDays = life_dist(m_rng);
            registry.emplace<Age>(npc, age);
            std::string nm = std::string(FIRSTS[fd(m_rng)]) + " " + LASTS[ld(m_rng)];
            registry.emplace<Name>(npc, Name{nm});
        }
        if (log) log->Push(day, hour,
            "MIGRATION WAVE: " + std::to_string(arrivals) + " arrived at " + settl->name);
        break;
    }

    case 9: {   // Off-map trade convoy — external market delivers scarce goods
        // Finds the most expensive resource at the target settlement (indicating
        // scarcity) and delivers a shipment — simulating trade from off-map regions.
        // The settlement treasury must pay for the delivery at market price.
        auto* mkt   = registry.try_get<Market>(target);
        auto* sp    = registry.try_get<Stockpile>(target);
        auto* settl2 = registry.try_get<Settlement>(target);
        if (!mkt || !sp || !settl2) break;

        // Find the most scarce (highest-priced) resource
        ResourceType scarcest    = ResourceType::Food;
        float        highestPrice = 0.f;
        for (const auto& [res, price] : mkt->price) {
            if (price > highestPrice) { highestPrice = price; scarcest = res; }
        }

        // Only dispatch a convoy when prices indicate real scarcity
        static constexpr float CONVOY_MIN_PRICE = 5.f;
        if (highestPrice < CONVOY_MIN_PRICE) break;

        // Cost = market price × quantity (convoy charges full price, no discount)
        static constexpr float CONVOY_AMOUNT = 50.f;
        float cost = highestPrice * CONVOY_AMOUNT;

        // If the settlement can't afford it, the convoy doesn't come
        if (settl2->treasury < cost) {
            if (log) log->Push(day, hour,
                "Convoy turned away from " + settl->name
                + " — treasury too low (" + std::to_string((int)settl2->treasury) + "g)");
            break;
        }

        settl2->treasury -= cost;
        sp->quantities[scarcest] += CONVOY_AMOUNT;

        const char* resName = (scarcest == ResourceType::Food)  ? "food"  :
                              (scarcest == ResourceType::Water) ? "water" : "wood";
        if (log) log->Push(day, hour,
            "OFF-MAP CONVOY at " + settl->name + " +" + std::to_string((int)CONVOY_AMOUNT)
            + " " + resName + " (paid " + std::to_string((int)cost) + "g)");
        break;
    }
    }
}
