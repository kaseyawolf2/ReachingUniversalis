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

    // Get EventLog for use in Update() body
    auto logView = registry.view<EventLog>();
    EventLog* log = (logView.begin() == logView.end()) ? nullptr
                  : &logView.get<EventLog>(*logView.begin());

    // Tick down active settlement modifiers (drought/plague recovery)
    // Also update morale drift and unrest threshold crossing.
    registry.view<Settlement>().each([&](auto e, Settlement& s) {
        if (s.modifierDuration > 0.f) {
            s.modifierDuration -= gameHoursDt;
            if (s.modifierDuration <= 0.f) {
                s.modifierDuration   = 0.f;
                s.productionModifier = 1.f;
                bool wasPlague = (s.modifierName == "Plague");
                if (log)
                    log->Push(tm.day, (int)tm.hourOfDay,
                        s.modifierName + " ends at " + s.name +
                        (wasPlague ? " — plague contained" : " — production restored"));
                s.modifierName.clear();
                if (wasPlague) m_plagueSpreadTimer.erase(e);
            }
        }

        // Morale drifts toward 0.5 baseline at ±0.5% per game-hour (slow, organic recovery)
        float drift = (0.5f - s.morale) * 0.005f * gameHoursDt;
        s.morale = std::max(0.f, std::min(1.f, s.morale + drift));

        // Unrest: log once when morale crosses below 0.3, and again on recovery above 0.4
        if (!s.unrest && s.morale < 0.3f) {
            s.unrest = true;
            if (log) log->Push(tm.day, (int)tm.hourOfDay,
                "UNREST in " + s.name + " — morale critical, production suffering");
        } else if (s.unrest && s.morale >= 0.4f) {
            s.unrest = false;
            if (log) log->Push(tm.day, (int)tm.hourOfDay,
                "Tensions ease in " + s.name + " — morale recovering");
        }

        // Drain work-stoppage cooldown
        if (s.strikeCooldown > 0.f)
            s.strikeCooldown = std::max(0.f, s.strikeCooldown - gameHoursDt);

        // Work stoppage: 5% chance per game-day when morale is critical
        static constexpr float STRIKE_PROB_PER_DAY  = 0.05f;
        static constexpr float STRIKE_DURATION_HOURS = 6.f;
        static constexpr float STRIKE_COOLDOWN_HOURS = 24.f;  // min hours between stoppages
        if (s.morale < 0.3f && s.strikeCooldown <= 0.f) {
            // Probability per tick scales linearly with elapsed game-time
            std::uniform_real_distribution<float> strikeChance(0.f, 1.f);
            float probThisTick = STRIKE_PROB_PER_DAY * gameHoursDt / 24.f;
            if (strikeChance(m_rng) < probThisTick) {
                s.strikeCooldown = STRIKE_COOLDOWN_HOURS;

                // Force all schedule-following NPCs at this settlement onto strike
                int strikerCount = 0;
                registry.view<Schedule, AgentState, HomeSettlement>(
                    entt::exclude<Hauler, PlayerTag>)
                    .each([&](auto npc, const Schedule&, AgentState& as,
                              const HomeSettlement& hs) {
                        if (hs.settlement != e) return;
                        // Only stop workers (don't interrupt sleep/migration/satisfying needs)
                        if (as.behavior == AgentBehavior::Working ||
                            as.behavior == AgentBehavior::Idle) {
                            as.behavior = AgentBehavior::Idle;
                        }
                        if (auto* dt = registry.try_get<DeprivationTimer>(npc)) {
                            dt->strikeDuration = STRIKE_DURATION_HOURS;
                        }
                        ++strikerCount;
                    });

                if (log && strikerCount > 0) {
                    char buf[100];
                    std::snprintf(buf, sizeof(buf),
                        "WORK STOPPAGE at %s — %d workers downed tools (%dh)",
                        s.name.c_str(), strikerCount, (int)STRIKE_DURATION_HOURS);
                    log->Push(tm.day, (int)tm.hourOfDay, buf);
                }
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

    // ---- Plague spreading ----
    // Each infected settlement tries to spread to a connected (non-blocked-road) neighbour
    // once per PLAGUE_SPREAD_INTERVAL game-hours.
    static constexpr float PLAGUE_SPREAD_INTERVAL = 20.f;   // game-hours between spread attempts
    static constexpr float PLAGUE_SPREAD_CHANCE   = 0.60f;  // 60% chance spread succeeds
    static constexpr float PLAGUE_MODIFIER        = 0.45f;  // production drop during plague
    static constexpr float PLAGUE_DURATION        = 72.f;   // game-hours plague lasts

    for (auto& [plagueSettl, timer] : m_plagueSpreadTimer) {
        if (!registry.valid(plagueSettl)) continue;
        timer -= gameHoursDt;
        if (timer > 0.f) continue;
        timer = PLAGUE_SPREAD_INTERVAL;

        // Find open-road neighbours of this settlement
        std::vector<entt::entity> neighbors;
        registry.view<Road>().each([&](const Road& r) {
            if (r.blocked) return;
            if (r.from == plagueSettl && registry.valid(r.to))   neighbors.push_back(r.to);
            if (r.to   == plagueSettl && registry.valid(r.from)) neighbors.push_back(r.from);
        });
        if (neighbors.empty()) continue;

        std::uniform_int_distribution<int>    pickN(0, (int)neighbors.size() - 1);
        std::uniform_real_distribution<float> chance(0.f, 1.f);
        if (chance(m_rng) > PLAGUE_SPREAD_CHANCE) continue;

        entt::entity target2 = neighbors[pickN(m_rng)];
        if (!registry.valid(target2)) continue;
        if (m_plagueSpreadTimer.count(target2)) continue;  // already infected

        auto* ts = registry.try_get<Settlement>(target2);
        if (!ts || ts->modifierDuration > 0.f) continue;  // already has another event

        ts->productionModifier = PLAGUE_MODIFIER;
        ts->modifierDuration   = PLAGUE_DURATION;
        ts->modifierName       = "Plague";
        m_plagueSpreadTimer[target2] = PLAGUE_SPREAD_INTERVAL;

        int killed = KillFraction(registry, target2, 0.10f);

        const auto* src = registry.try_get<Settlement>(plagueSettl);
        if (log) {
            char buf[120];
            std::snprintf(buf, sizeof(buf),
                "PLAGUE spreads from %s to %s — %d died",
                src ? src->name.c_str() : "?", ts->name.c_str(), killed);
            log->Push(tm.day, (int)tm.hourOfDay, buf);
        }
    }

    // Count down to next event
    m_nextEvent -= gameHoursDt;
    if (m_nextEvent <= 0.f) {
        // Schedule next
        std::uniform_real_distribution<float> jitter(-EVENT_JITTER, EVENT_JITTER);
        m_nextEvent = std::max(12.f, EVENT_MEAN_HOURS + jitter(m_rng));
        TriggerEvent(registry, tm.day, (int)tm.hourOfDay);
    }

    // ---- Per-NPC personal events ----
    // Each NPC rolls a small event every 12–48 game-hours (timer staggered at spawn).
    // Events: skill discovery, windfall, minor illness, good harvest.
    static constexpr float NPC_EVT_MIN = 12.f;
    static constexpr float NPC_EVT_MAX = 48.f;
    static constexpr float ILLNESS_DURATION = 6.f;     // game-hours
    static constexpr float HARVEST_DURATION = 4.f;     // game-hours

    std::uniform_real_distribution<float> nextEvtDist(NPC_EVT_MIN, NPC_EVT_MAX);
    std::uniform_int_distribution<int>    evtTypeDist(0, 3);
    std::uniform_real_distribution<float> windfall_dist(5.f, 15.f);
    std::uniform_real_distribution<float> skillGain_dist(0.08f, 0.12f);
    std::uniform_int_distribution<int>    needIdxDist(0, 2);   // Hunger/Thirst/Energy only
    std::uniform_int_distribution<int>    skillIdxDist(0, 2);

    registry.view<DeprivationTimer, Skills, Money, Name>(
        entt::exclude<PlayerTag, BanditTag>)
        .each([&](auto e, DeprivationTimer& dt, Skills& skills, Money& money, const Name& name) {
            // Drain timers regardless of whether an event fires
            if (dt.illnessTimer > 0.f)
                dt.illnessTimer = std::max(0.f, dt.illnessTimer - gameHoursDt);
            if (dt.harvestBonusTimer > 0.f)
                dt.harvestBonusTimer = std::max(0.f, dt.harvestBonusTimer - gameHoursDt);

            dt.personalEventTimer -= gameHoursDt;
            if (dt.personalEventTimer > 0.f) return;

            // Schedule next event
            dt.personalEventTimer = nextEvtDist(m_rng);

            switch (evtTypeDist(m_rng)) {
                case 0: {   // Skill discovery — +0.1 to a random skill
                    int idx = skillIdxDist(m_rng);
                    float gain = skillGain_dist(m_rng);
                    ResourceType rt = (idx == 0) ? ResourceType::Food :
                                      (idx == 1) ? ResourceType::Water : ResourceType::Wood;
                    skills.Advance(rt, gain);
                    if (log) {
                        const char* skillName = (idx == 0) ? "farming" :
                                                (idx == 1) ? "water drawing" : "woodcutting";
                        char buf[100];
                        std::snprintf(buf, sizeof(buf), "%s had a skill insight in %s",
                            name.value.c_str(), skillName);
                        log->Push(tm.day, (int)tm.hourOfDay, buf);
                    }
                    break;
                }
                case 1: {   // Windfall — find 5–15g on the road (exogenous injection)
                    float found = windfall_dist(m_rng);
                    money.balance += found;
                    if (log) {
                        char buf[100];
                        std::snprintf(buf, sizeof(buf), "%s found %.0fg on the road",
                            name.value.c_str(), found);
                        log->Push(tm.day, (int)tm.hourOfDay, buf);
                    }
                    break;
                }
                case 2: {   // Minor illness — one need drains 2× for 6 hours
                    if (dt.illnessTimer <= 0.f) {   // don't stack illnesses
                        dt.illnessTimer    = ILLNESS_DURATION;
                        dt.illnessNeedIdx  = needIdxDist(m_rng);
                        if (log) {
                            const char* needName =
                                (dt.illnessNeedIdx == 0) ? "hunger" :
                                (dt.illnessNeedIdx == 1) ? "thirst" : "fatigue";
                            char buf[100];
                            std::snprintf(buf, sizeof(buf),
                                "%s fell ill (%s doubled for %dh)",
                                name.value.c_str(), needName, (int)ILLNESS_DURATION);
                            log->Push(tm.day, (int)tm.hourOfDay, buf);
                        }
                    }
                    break;
                }
                case 3: {   // Good harvest — worker produces 1.5× for 4 hours
                    dt.harvestBonusTimer = HARVEST_DURATION;
                    // Only log occasionally to avoid spam (log ~1 in 3)
                    std::uniform_int_distribution<int> logChance(0, 2);
                    if (log && logChance(m_rng) == 0) {
                        char buf[100];
                        std::snprintf(buf, sizeof(buf),
                            "%s is having a great work day (+50%% for %dh)",
                            name.value.c_str(), (int)HARVEST_DURATION);
                        log->Push(tm.day, (int)tm.hourOfDay, buf);
                    }
                    break;
                }
            }
        });
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
    std::uniform_int_distribution<int> pickType(0, 17); // 0-5=always 6=winter 7=spring 8=autumn 9=off-map 10=rainstorm 11=festival 12=earthquake 13=fire 14=heatwave 15=lumberwindfall 16=skilled_immigrant 17=market_crisis

    entt::entity target = settlements[pickSettl(m_rng)];
    auto* settl = registry.try_get<Settlement>(target);
    if (!settl) return;

    switch (pickType(m_rng)) {

    case 0: {   // Drought — cripple production
        if (settl->modifierDuration > 0.f) break;   // already has an event
        settl->productionModifier = DROUGHT_MODIFIER;
        settl->modifierDuration   = DROUGHT_DURATION;
        settl->modifierName       = "Drought";
        settl->morale = std::max(0.f, settl->morale - 0.10f);  // drought demoralises
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
        settl->morale = std::max(0.f, settl->morale - 0.12f);  // food loss is demoralising
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

    case 3: {   // Plague outbreak — production debuff + death + spreading
        if (settl->modifierDuration > 0.f) break;   // already has an active event
        if (m_plagueSpreadTimer.count(target)) break; // already infected

        static constexpr float PLAGUE_INIT_MODIFIER = 0.45f;
        static constexpr float PLAGUE_INIT_DURATION = 72.f;
        settl->productionModifier = PLAGUE_INIT_MODIFIER;
        settl->modifierDuration   = PLAGUE_INIT_DURATION;
        settl->modifierName       = "Plague";
        settl->morale = std::max(0.f, settl->morale - 0.20f);  // plague is deeply demoralising
        m_plagueSpreadTimer[target] = 20.f;  // first spread attempt in 20 game-hours

        int killCount = KillFraction(registry, target, 0.15f);
        if (log) {
            char buf[120];
            std::snprintf(buf, sizeof(buf),
                "PLAGUE erupts at %s — %d died, disease spreading via roads!",
                settl->name.c_str(), killCount);
            log->Push(day, hour, buf);
        }
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
        const auto* tpos = registry.try_get<Position>(target);
        if (!tpos) break;
        // Check pop cap — don't overfill the settlement
        int curPop5 = 0;
        registry.view<HomeSettlement>(entt::exclude<PlayerTag, Hauler>).each(
            [&](const HomeSettlement& hs) { if (hs.settlement == target) ++curPop5; });
        int slots = settl->popCap - curPop5;
        if (slots <= 0) break;   // full — redirect event silently
        std::uniform_int_distribution<int> count_dist(3, 5);
        int arrivals = std::min(slots, count_dist(m_rng));

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
        std::uniform_real_distribution<float> skill_dist(0.30f, 0.70f);

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
            registry.emplace<Skills>(npc, Skills{ skill_dist(m_rng), skill_dist(m_rng), skill_dist(m_rng) });
        }
        if (log) log->Push(day, hour,
            "MIGRATION WAVE: " + std::to_string(arrivals) + " arrived at " + settl->name);
        break;
    }

    case 10: {  // Rainstorm — all settlements gain water; drought at target ends early
        // Rain doesn't discriminate — all settlements collect water.
        // Bonus: if the target settlement is in drought, the rain breaks it.
        static constexpr float RAIN_WATER = 25.f;   // water added to every settlement
        registry.view<Settlement, Stockpile>().each(
            [&](auto e, Settlement& rs, Stockpile& rsp) {
            rsp.quantities[ResourceType::Water] += RAIN_WATER;
            // Break drought at this settlement
            if (e == target && rs.modifierDuration > 0.f &&
                rs.modifierName.find("Drought") != std::string::npos) {
                rs.modifierDuration   = 0.f;
                rs.productionModifier = 1.f;
                rs.modifierName.clear();
            }
        });
        if (log) log->Push(day, hour,
            "RAINSTORM — all settlements +" + std::to_string((int)RAIN_WATER) + " water");
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

    case 11: {  // Festival — boosts treasury and gives short production bonus
        // Represent merchants and travelers visiting the settlement for a festival.
        // The influx of trade brings gold into the treasury; the festive mood
        // boosts production output for a short window.
        static constexpr float FESTIVAL_GOLD     = 120.f;
        static constexpr float FESTIVAL_MODIFIER = 1.35f;
        static constexpr float FESTIVAL_DURATION = 16.f;   // game-hours

        if (settl->modifierDuration > 0.f) break;   // already affected by another event
        {
            int spop = 0;
            registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
                [&](const HomeSettlement& hs) { if (hs.settlement == target) ++spop; });
            if (spop < 5) break;   // too small for a notable festival
        }

        settl->treasury          += FESTIVAL_GOLD;
        settl->productionModifier = FESTIVAL_MODIFIER;
        settl->modifierDuration   = FESTIVAL_DURATION;
        settl->modifierName       = "Festival";
        settl->morale = std::min(1.f, settl->morale + 0.15f);  // festivals lift spirits

        // Put all NPCs at this settlement into the Celebrating state; count them for the log
        int celebrantCount = 0;
        registry.view<AgentState, HomeSettlement>(
            entt::exclude<PlayerTag, Hauler>).each(
            [&](AgentState& as, const HomeSettlement& hs) {
                if (hs.settlement == target) {
                    as.behavior = AgentBehavior::Celebrating;
                    ++celebrantCount;
                }
            });

        if (log) {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                "FESTIVAL at %s — %d celebrating, treasury +%.0fg, production +35%% (%dh)",
                settl->name.c_str(), celebrantCount, FESTIVAL_GOLD, (int)FESTIVAL_DURATION);
            log->Push(day, hour, buf);
        }
        break;
    }

    case 13: {  // Fire — burns stored food and wood, kills a few NPCs
        auto* sp = registry.try_get<Stockpile>(target);
        if (!sp) break;

        std::uniform_real_distribution<float> burnFrac(0.35f, 0.55f);
        float fraction = burnFrac(m_rng);

        float foodLost = 0.f, woodLost = 0.f;
        auto fitFood = sp->quantities.find(ResourceType::Food);
        if (fitFood != sp->quantities.end() && fitFood->second > 5.f) {
            foodLost = fitFood->second * fraction;
            fitFood->second -= foodLost;
        }
        auto fitWood = sp->quantities.find(ResourceType::Wood);
        if (fitWood != sp->quantities.end() && fitWood->second > 5.f) {
            woodLost = fitWood->second * fraction;
            fitWood->second -= woodLost;
        }

        int killed = KillFraction(registry, target, 0.05f);

        if (log) {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "FIRE at %s — %.0f food, %.0f wood destroyed, %d died",
                settl->name.c_str(), foodLost, woodLost, killed);
            log->Push(day, hour, buf);
        }
        break;
    }

    case 14: {  // Heat wave (Summer only) — production penalty + water shortage stress
        if (season != Season::Summer) break;
        if (settl->modifierDuration > 0.f) break;   // already affected

        static constexpr float HEATWAVE_MODIFIER = 0.75f;
        static constexpr float HEATWAVE_DURATION = 10.f;   // game-hours

        settl->productionModifier = HEATWAVE_MODIFIER;
        settl->modifierDuration   = HEATWAVE_DURATION;
        settl->modifierName       = "Heat Wave";

        // Destroy 20-35% of water stockpile (evaporation / dehydration)
        auto* sp = registry.try_get<Stockpile>(target);
        if (sp) {
            auto it = sp->quantities.find(ResourceType::Water);
            if (it != sp->quantities.end() && it->second > 5.f) {
                std::uniform_real_distribution<float> evap(0.20f, 0.35f);
                float lost = it->second * evap(m_rng);
                it->second -= lost;
            }
        }

        if (log) {
            char buf[120];
            std::snprintf(buf, sizeof(buf),
                "HEAT WAVE strikes %s — production -25%%, water reserves drained (%dh)",
                settl->name.c_str(), (int)HEATWAVE_DURATION);
            log->Push(day, hour, buf);
        }
        break;
    }

    case 15: {  // Lumber windfall (Spring/Autumn) — storm brings easy wood
        if (season != Season::Spring && season != Season::Autumn) break;
        auto* sp = registry.try_get<Stockpile>(target);
        if (!sp) break;

        std::uniform_real_distribution<float> lumberAmt(50.f, 80.f);
        float windfall = lumberAmt(m_rng);
        sp->quantities[ResourceType::Wood] += windfall;

        if (log) {
            char buf[120];
            std::snprintf(buf, sizeof(buf),
                "LUMBER WINDFALL at %s — storm felled trees, +%.0f wood",
                settl->name.c_str(), windfall);
            log->Push(day, hour, buf);
        }
        break;
    }

    case 16: {  // Skilled Immigrant — a talented NPC arrives at the target settlement
        // Spawn one NPC with high skill in a random resource type.
        // This boosts production at under-populated or skill-poor settlements.
        const auto* spos = registry.try_get<Position>(target);
        if (!spos) break;

        // Count current pop to check cap
        int curPop16 = 0;
        registry.view<HomeSettlement>(entt::exclude<PlayerTag, Hauler>).each(
            [&](const HomeSettlement& hs) { if (hs.settlement == target) ++curPop16; });
        if (curPop16 >= settl->popCap) break;  // no room

        static constexpr float DRAIN_HUNGER = 0.00083f;
        static constexpr float DRAIN_THIRST = 0.00125f;
        static constexpr float DRAIN_ENERGY = 0.00050f;
        static constexpr float DRAIN_HEAT   = 0.00200f;
        static constexpr float CRIT_THRESH  = 0.3f;

        auto npc16 = registry.create();
        float angle16 = std::uniform_real_distribution<float>(0.f, 6.28f)(m_rng);
        registry.emplace<Position>(npc16,
            spos->x + std::cos(angle16) * 60.f,
            spos->y + std::sin(angle16) * 60.f);
        registry.emplace<Velocity>(npc16, 0.f, 0.f);
        registry.emplace<MoveSpeed>(npc16, 60.f);
        registry.emplace<Needs>(npc16, Needs{{
            Need{NeedType::Hunger, 1.f, DRAIN_HUNGER, CRIT_THRESH, 0.004f},
            Need{NeedType::Thirst, 1.f, DRAIN_THIRST, CRIT_THRESH, 0.006f},
            Need{NeedType::Energy, 1.f, DRAIN_ENERGY, CRIT_THRESH, 0.002f},
            Need{NeedType::Heat,   1.f, DRAIN_HEAT,   CRIT_THRESH, 0.010f},
        }});
        registry.emplace<AgentState>(npc16);
        registry.emplace<HomeSettlement>(npc16, HomeSettlement{ target });
        DeprivationTimer dt16;
        dt16.migrateThreshold = std::uniform_real_distribution<float>(2.f, 5.f)(m_rng) * 60.f;
        registry.emplace<DeprivationTimer>(npc16, dt16);
        registry.emplace<Schedule>(npc16);
        registry.emplace<Renderable>(npc16, WHITE, 6.f);
        Age age16; age16.days = 20.f + std::uniform_real_distribution<float>(0.f, 15.f)(m_rng);
        age16.maxDays = std::uniform_real_distribution<float>(60.f, 100.f)(m_rng);
        registry.emplace<Age>(npc16, age16);
        registry.emplace<Money>(npc16, Money{ 30.f });  // arrives with savings

        // High skill in one randomly chosen area (0.75+); normal in others
        static const char* FIRST_N[] = {
            "Aldric","Brom","Cedric","Daven","Edric","Finn","Gareth","Holt","Ivan","Jorin",
            "Kael","Lewin","Marden","Nolan","Oswin","Pell","Roran","Sven","Torben","Uric"
        };
        static const char* LAST_N[] = {
            "Smith","Miller","Cooper","Fletcher","Mason","Tanner","Ward","Thatcher",
            "Fisher","Baker","Forger","Webb","Stone","Holt","Reed","Marsh"
        };
        std::uniform_int_distribution<int> fn(0,19), ln(0,15);
        std::string immName = std::string(FIRST_N[fn(m_rng)]) + " " + LAST_N[ln(m_rng)];
        registry.emplace<Name>(npc16, Name{ immName });

        std::uniform_int_distribution<int> apt16(0, 2);
        int aptIdx16 = apt16(m_rng);
        float highSkill = 0.75f + std::uniform_real_distribution<float>(0.f, 0.20f)(m_rng);
        Skills sk16{ 0.35f, 0.35f, 0.35f };
        const char* specialty16 = "Farmer";
        if      (aptIdx16 == 0) { sk16.farming       = highSkill; specialty16 = "Farmer";       }
        else if (aptIdx16 == 1) { sk16.water_drawing  = highSkill; specialty16 = "Water Carrier"; }
        else                    { sk16.woodcutting    = highSkill; specialty16 = "Woodcutter";    }
        registry.emplace<Skills>(npc16, sk16);

        if (log) {
            char buf[120];
            std::snprintf(buf, sizeof(buf),
                "SKILLED IMMIGRANT: %s (%s) arrives at %s — skill %.0f%%",
                immName.c_str(), specialty16, settl->name.c_str(), highSkill * 100.f);
            log->Push(day, hour, buf);
        }
        break;
    }

    case 17: {  // Market Crisis — panic drives prices up 3× at one settlement temporarily
        // Sets a 3× modifier on all market prices at the target settlement.
        // Implemented by multiplying the current Market prices directly;
        // PriceSystem will gradually bring them back toward equilibrium.
        auto* mkt = registry.try_get<Market>(target);
        if (!mkt) break;

        // Only trigger if prices are currently reasonable (not already spiked)
        bool alreadySpiked = false;
        for (const auto& [rt, p] : mkt->price)
            if (p > 15.f) { alreadySpiked = true; break; }
        if (alreadySpiked) break;

        float spikeBase = std::uniform_real_distribution<float>(2.5f, 3.5f)(m_rng);
        for (auto& [rt, p] : mkt->price)
            p = std::min(20.f, p * spikeBase);

        if (log) {
            char buf[120];
            std::snprintf(buf, sizeof(buf),
                "MARKET CRISIS at %s — panic buying, all prices spike %.1f×!",
                settl->name.c_str(), spikeBase);
            log->Push(day, hour, buf);
        }
        break;
    }

    case 12: {  // Earthquake — destroys some facilities, blocks all roads for a time
        {
            int spop2 = 0;
            registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
                [&](const HomeSettlement& hs) { if (hs.settlement == target) ++spop2; });
            if (spop2 < 3) break;
        }

        static constexpr float QUAKE_ROAD_BLOCK_DURATION = 6.f;  // game-hours
        static constexpr float QUAKE_FACILITY_DESTROY_CHANCE = 0.30f;
        static constexpr float QUAKE_ROAD_DAMAGE = 0.30f;  // condition lost on connected roads

        // Block only roads connected to the epicenter settlement and damage their condition
        int blockedRoads = 0;
        registry.view<Road>().each([&](Road& road) {
            bool connected = (road.from == target || road.to == target);
            if (!connected) return;
            if (!road.blocked) {
                road.blocked     = true;
                road.banditTimer = QUAKE_ROAD_BLOCK_DURATION;
                ++blockedRoads;
            }
            // Earthquake physically damages the road surface
            road.condition = std::max(0.f, road.condition - QUAKE_ROAD_DAMAGE);
        });

        // Destroy a fraction of this settlement's non-shelter facilities
        std::vector<entt::entity> settlFacs;
        registry.view<ProductionFacility>().each(
            [&](auto fe, const ProductionFacility& fac) {
            if (fac.settlement == target && fac.baseRate > 0.f)
                settlFacs.push_back(fe);
        });
        std::uniform_real_distribution<float> chance2(0.f, 1.f);
        int destroyed = 0;
        for (auto fe : settlFacs) {
            if (chance2(m_rng) < QUAKE_FACILITY_DESTROY_CHANCE) {
                registry.destroy(fe);
                ++destroyed;
            }
        }

        if (log) {
            char buf[140];
            std::snprintf(buf, sizeof(buf),
                "EARTHQUAKE at %s — %d facilit%s destroyed, all roads blocked (%dh)",
                settl->name.c_str(), destroyed,
                destroyed == 1 ? "y" : "ies", (int)QUAKE_ROAD_BLOCK_DURATION);
            log->Push(day, hour, buf);
        }
        break;
    }
    }
}

// ---- KillFraction -------------------------------------------------------
// Kills `fraction` of the non-player NPCs homed at `settl`.
// Applies inheritance (50% gold → settlement treasury) and cargo recovery
// (hauler goods returned to home stockpile) matching DeathSystem logic.
// Returns the count killed.
int RandomEventSystem::KillFraction(entt::registry& registry,
                                     entt::entity settl, float fraction)
{
    std::vector<entt::entity> victims;
    registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
        [&](auto e, const HomeSettlement& hs) {
            if (hs.settlement == settl) victims.push_back(e);
        });
    if (victims.size() < 3) return 0;
    std::shuffle(victims.begin(), victims.end(), m_rng);
    int killCount = std::max(1, (int)(victims.size() * fraction));

    auto* sp    = registry.try_get<Stockpile>(settl);
    auto* settlC = registry.try_get<Settlement>(settl);

    for (int i = 0; i < killCount; ++i) {
        if (!registry.valid(victims[i])) continue;

        // Cargo recovery: hauler goods return to home stockpile
        if (sp) {
            if (const auto* inv = registry.try_get<Inventory>(victims[i]))
                for (const auto& [type, qty] : inv->contents)
                    sp->quantities[type] += qty;
        }

        // Inheritance: 50% of gold → settlement treasury
        static constexpr float INHERIT_FRAC = 0.5f;
        static constexpr float INHERIT_MIN  = 10.f;
        if (settlC) {
            if (const auto* money = registry.try_get<Money>(victims[i]))
                if (money->balance >= INHERIT_MIN)
                    settlC->treasury += money->balance * INHERIT_FRAC;
        }

        registry.destroy(victims[i]);
    }
    return killCount;
}
