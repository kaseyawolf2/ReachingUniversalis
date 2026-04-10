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

    // Collect valid settlements
    std::vector<entt::entity> settlements;
    registry.view<Settlement>().each([&](auto e, const Settlement&) {
        settlements.push_back(e);
    });
    if (settlements.empty()) return;

    std::uniform_int_distribution<int> pickSettl(0, (int)settlements.size() - 1);
    std::uniform_int_distribution<int> pickType(0, 5);  // 0=drought 1=blight 2=bandits 3=disease 4=trade_boom 5=migration

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

    case 5: {   // Migration wave — 3-5 NPCs arrive from outside
        std::uniform_int_distribution<int> count_dist(3, 5);
        int arrivals = count_dist(m_rng);
        const auto* tpos = registry.try_get<Position>(target);
        if (!tpos) break;

        static const float DRAIN_HUNGER = 0.00083f, DRAIN_THIRST = 0.00125f,
                           DRAIN_ENERGY = 0.00050f;
        static const float REFILL_H = 0.004f, REFILL_T = 0.006f, REFILL_E = 0.002f;
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
        std::uniform_real_distribution<float> mt_dist(2.f, 5.f);

        for (int i = 0; i < arrivals; ++i) {
            float ang = angle_dist(m_rng);
            auto npc = registry.create();
            registry.emplace<Position>(npc, tpos->x + std::cos(ang)*60.f,
                                            tpos->y + std::sin(ang)*60.f);
            registry.emplace<Velocity>(npc, 0.f, 0.f);
            registry.emplace<MoveSpeed>(npc, 60.f);
            registry.emplace<Needs>(npc, Needs{{
                Need{NeedType::Hunger, 0.6f, DRAIN_HUNGER, 0.3f, REFILL_H},
                Need{NeedType::Thirst, 0.6f, DRAIN_THIRST, 0.3f, REFILL_T},
                Need{NeedType::Energy, 0.8f, DRAIN_ENERGY, 0.3f, REFILL_E}
            }});
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
    }
}
