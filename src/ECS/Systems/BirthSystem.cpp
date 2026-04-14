#include "BirthSystem.h"
#include "ECS/Components.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <random>

// A new NPC may be born at a settlement when:
//   - Population at that settlement < MAX_POP_PER_SETTLEMENT
//   - Stockpile has at least BIRTH_FOOD_MIN food AND BIRTH_WATER_MIN water
//   - The birth accumulator has reached BIRTH_INTERVAL
//   - AND a random BIRTH_CHANCE roll succeeds (simulating NPCs deciding to have a child)
//
// Each birth costs BIRTH_FOOD_COST food and BIRTH_WATER_COST water.

// Default hard cap; actual cap is now stored in Settlement::popCap (can be expanded).
static constexpr int   MAX_POP_PER_SETTLEMENT = 35;
static constexpr float BIRTH_INTERVAL         = 3.f * 60.f;   // 3 game-hours
static constexpr float BIRTH_CHANCE           = 0.25f;        // 25% chance per interval
static constexpr float BIRTH_FOOD_MIN         = 30.f;
static constexpr float BIRTH_WATER_MIN        = 30.f;
static constexpr float BIRTH_FOOD_COST        = 10.f;
static constexpr float BIRTH_WATER_COST       = 10.f;

// Drain rates match WorldGenerator values
static constexpr float DRAIN_HUNGER = 0.00083f;
static constexpr float DRAIN_THIRST = 0.00125f;
static constexpr float DRAIN_ENERGY = 0.00050f;
static constexpr float DRAIN_HEAT   = 0.00200f;
static constexpr float REFILL_HUNGER = 0.004f;
static constexpr float REFILL_THIRST = 0.006f;
static constexpr float REFILL_ENERGY = 0.002f;
static constexpr float REFILL_HEAT   = 0.010f;
static constexpr float CRIT_THRESHOLD = 0.3f;

static Needs MakeNeeds() {
    return Needs{{
        Need{ NeedType::Hunger, 1.f, DRAIN_HUNGER, CRIT_THRESHOLD, REFILL_HUNGER },
        Need{ NeedType::Thirst, 1.f, DRAIN_THIRST, CRIT_THRESHOLD, REFILL_THIRST },
        Need{ NeedType::Energy, 1.f, DRAIN_ENERGY, CRIT_THRESHOLD, REFILL_ENERGY },
        Need{ NeedType::Heat,   1.f, DRAIN_HEAT,   CRIT_THRESHOLD, REFILL_HEAT   }
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

        float food  = stockpile.quantities.count(RES_FOOD)
                      ? stockpile.quantities[RES_FOOD]  : 0.f;
        float water = stockpile.quantities.count(RES_WATER)
                      ? stockpile.quantities[RES_WATER] : 0.f;
        float wood  = stockpile.quantities.count(RES_WOOD)
                      ? stockpile.quantities[RES_WOOD]  : 0.f;
        int   pop   = popCount.count(settl) ? popCount[settl] : 0;

        // In cold seasons, require some wood reserve before having children
        float heatMult = SeasonHeatDrainMult(tm.CurrentSeason());
        bool  woodOk   = (heatMult <= 0.f) || (wood >= 10.f);

        // Use the settlement's dynamic pop cap (expandable via housing construction)
        const auto& settlComp = settlView.get<Settlement>(settl);
        int effectivePopCap = settlComp.popCap;

        bool canBirth = (pop < effectivePopCap)
                     && (food  >= BIRTH_FOOD_MIN)
                     && (water >= BIRTH_WATER_MIN)
                     && woodOk
                     && (settlComp.ruinTimer <= 0.f);  // no births during ruin cooldown

        if (canBirth) {
            tracker.accumulator += gameHoursDt;
        } else {
            // Decay the timer when conditions aren't met so it doesn't
            // trigger immediately when conditions return after a drought.
            tracker.accumulator = std::max(0.f, tracker.accumulator - gameHoursDt * 0.5f);
        }

        if (tracker.accumulator >= BIRTH_INTERVAL) {
            tracker.accumulator -= BIRTH_INTERVAL;

            // NPCs decide whether to have a child based on settlement prosperity.
            // Wealth threshold: if treasury > 200g and food + water surplus is good,
            // birth chance increases. Struggling settlements hold back.
            static std::mt19937 s_rng{std::random_device{}()};
            static std::uniform_real_distribution<float> s_dist(1.f, 10.f);
            static std::uniform_real_distribution<float> chance_dist(0.f, 1.f);

            float birthChance = BIRTH_CHANCE;
            const auto& settlComp2 = settlView.get<Settlement>(settl);
            if (settlComp2.treasury > 300.f && food > 80.f && water > 80.f)
                birthChance = 0.50f;  // prosperous: more eager
            else if (settlComp2.treasury < 50.f || food < 40.f || water < 40.f)
                birthChance = 0.10f;  // struggling: reluctant

            if (chance_dist(s_rng) > birthChance) continue;

            // Deduct birth cost
            stockpile.quantities[RES_FOOD]  -= BIRTH_FOOD_COST;
            stockpile.quantities[RES_WATER] -= BIRTH_WATER_COST;

            // Determine this settlement's primary profession from its highest-rate facility.
            ProfessionType settlProfession = ProfessionType::Idle;
            {
                float maxRate = 0.f;
                registry.view<ProductionFacility>().each(
                    [&](const ProductionFacility& fac) {
                        if (fac.settlement != settl) return;
                        if (fac.baseRate > maxRate) {
                            maxRate = fac.baseRate;
                            settlProfession = ProfessionForResource(fac.output);
                        }
                    });
            }

            // Spawn new NPC at a ring around the settlement centre
            float angle = (float)(pop % 20) / 20.f * 2.f * 3.14159f;
            float ring  = 50.f + (float)(pop % 3) * 22.f;
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
            registry.emplace<Relations>(npc);
            registry.emplace<Renderable>(npc, WHITE, 6.f);
            // New NPC starts at age 0 with a random life expectancy
            static std::uniform_real_distribution<float> lifespan(60.f, 100.f);
            Age npcAge;
            npcAge.days    = 0.f;
            npcAge.maxDays = lifespan(s_rng);
            registry.emplace<Age>(npc, npcAge);
            // Give the newborn a name using the same pool as WorldGenerator
            // (uses a local RNG seeded differently so names don't repeat predictably)
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
            static std::uniform_int_distribution<int> fd(0, 29);
            static std::uniform_int_distribution<int> ld(0, 19);

            // Find the most common adult surname at this settlement.
            // 50% chance the newborn inherits it, tying them to existing family lines.
            std::string familySurname;
            {
                std::map<std::string, int> surnameCount;
                registry.view<HomeSettlement, Name>(entt::exclude<ChildTag, PlayerTag>).each(
                    [&](const HomeSettlement& hs, const Name& n) {
                    if (hs.settlement != settl) return;
                    auto sp = n.value.rfind(' ');
                    if (sp != std::string::npos)
                        ++surnameCount[n.value.substr(sp + 1)];
                });
                if (!surnameCount.empty() && chance_dist(s_rng) < 0.5f) {
                    auto it = std::max_element(surnameCount.begin(), surnameCount.end(),
                        [](const auto& a, const auto& b) { return a.second < b.second; });
                    familySurname = it->first;
                }
            }

            std::string npcName = std::string(FIRSTS[fd(s_rng)]) + " " +
                (familySurname.empty() ? LASTS[ld(s_rng)] : familySurname);
            registry.emplace<Name>(npc, Name{ npcName });
            // Newborns inherit a small starting purse — participating in the
            // emergency market purchase system from birth.
            registry.emplace<Money>(npc, Money{ 5.f });
            // Personality variation: ±20% on drain rates for each newborn.
            static std::uniform_real_distribution<float> trait_dist(0.80f, 1.20f);
            auto& newNeeds = registry.get<Needs>(npc);
            for (auto& need : newNeeds.list)
                need.drainRate *= trait_dist(s_rng);

            // Newborns have a random skill aptitude: one skill starts slightly higher
            // (0.15) while the others start a little lower (0.08). This bias persists
            // through childhood passive growth, creating gentle specialisation by
            // the time the NPC enters the workforce. The aptitude also influences which
            // settlement they'll prefer when migrating (skill-aware migration targeting).
            static std::uniform_int_distribution<int> apt_dist(0, 2);
            int aptIdx = apt_dist(s_rng);
            Skills npcSkills{ 0.08f, 0.08f, 0.08f };
            if      (aptIdx == 0) npcSkills.farming       = 0.15f;
            else if (aptIdx == 1) npcSkills.water_drawing = 0.15f;
            else                  npcSkills.woodcutting   = 0.15f;
            registry.emplace<Skills>(npc, npcSkills);
            registry.emplace<Profession>(npc, Profession{ settlProfession });
            registry.emplace<ChildTag>(npc);

            // Inherit FamilyTag from any paired adult at this settlement (if one exists).
            {
                std::map<std::string, int> familyFreq;
                registry.view<HomeSettlement, FamilyTag>(entt::exclude<ChildTag, PlayerTag>).each(
                    [&](const HomeSettlement& hs2, const FamilyTag& ft) {
                        if (hs2.settlement != settl || ft.name.empty()) return;
                        ++familyFreq[ft.name];
                    });
                if (!familyFreq.empty()) {
                    auto best = std::max_element(familyFreq.begin(), familyFreq.end(),
                        [](const auto& a, const auto& b){ return a.second < b.second; });
                    registry.emplace<FamilyTag>(npc, FamilyTag{ best->first });
                }
            }

            // Find the wealthiest adult at this settlement to name as parent.
            std::string parentName;
            entt::entity parentEntity = entt::null;
            {
                float bestBalance = -1.f;
                registry.view<HomeSettlement, Money, Name>(entt::exclude<ChildTag, PlayerTag>).each(
                    [&](auto pe, const HomeSettlement& hs, const Money& m, const Name& n) {
                        if (hs.settlement != settl) return;
                        if (m.balance > bestBalance) {
                            bestBalance = m.balance;
                            parentName  = n.value;
                            parentEntity = pe;
                        }
                    });
            }

            // Births lift settlement morale slightly — new life is cause for hope
            {
                auto& sc = settlView.get<Settlement>(settl);
                sc.morale = std::min(1.f, sc.morale + 0.03f);
            }

            if (log) {
                const auto& s = settlView.get<Settlement>(settl);
                std::string msg = "Born: " + npcName + " at " + s.name;
                if (!parentName.empty()) msg += " (to " + parentName + ")";
                log->Push(tm.day, (int)tm.hourOfDay, msg);
            }

            // Friendship bonus: friends of the parent celebrate, boosting morale
            if (parentEntity != entt::null && registry.valid(parentEntity)) {
                const auto* parentRel = registry.try_get<Relations>(parentEntity);
                if (parentRel) {
                    int boosts = 0;
                    for (const auto& [friendEnt, aff] : parentRel->affinity) {
                        if (boosts >= 2) break;
                        if (aff < 0.5f) continue;
                        if (!registry.valid(friendEnt)) continue;
                        // Must be at the same settlement
                        const auto* fhs = registry.try_get<HomeSettlement>(friendEnt);
                        if (!fhs || fhs->settlement != settl) continue;
                        auto& sc = settlView.get<Settlement>(settl);
                        sc.morale = std::min(1.f, sc.morale + 0.01f);
                        ++boosts;
                        if (log) {
                            std::string friendName = "A friend";
                            if (const auto* fn = registry.try_get<Name>(friendEnt))
                                friendName = fn->value;
                            log->Push(tm.day, (int)tm.hourOfDay,
                                friendName + " celebrates " + parentName + "'s new child.");
                        }
                    }
                }
            }

            // Twins: 10% chance of a second birth at the same time
            // (only if pop cap not exceeded and stockpile has room)
            static constexpr float TWIN_CHANCE = 0.10f;
            int popNow = popCount.count(settl) ? popCount[settl] + 1 : 1;
            if (chance_dist(s_rng) < TWIN_CHANCE
                && popNow < effectivePopCap
                && stockpile.quantities[RES_FOOD]  >= BIRTH_FOOD_COST
                && stockpile.quantities[RES_WATER] >= BIRTH_WATER_COST) {

                stockpile.quantities[RES_FOOD]  -= BIRTH_FOOD_COST;
                stockpile.quantities[RES_WATER] -= BIRTH_WATER_COST;

                float angle2 = angle + 3.14159f;  // opposite side of the settlement
                DeprivationTimer dt2;
                dt2.migrateThreshold = s_dist(s_rng) * 60.f;

                auto npc2 = registry.create();
                registry.emplace<Position>(npc2,
                    spos.x + std::cos(angle2) * ring,
                    spos.y + std::sin(angle2) * ring);
                registry.emplace<Velocity>(npc2, 0.f, 0.f);
                registry.emplace<MoveSpeed>(npc2, 60.f);
                registry.emplace<Needs>(npc2, MakeNeeds());
                registry.emplace<AgentState>(npc2);
                registry.emplace<HomeSettlement>(npc2, HomeSettlement{ settl });
                registry.emplace<DeprivationTimer>(npc2, dt2);
                registry.emplace<Schedule>(npc2);
                registry.emplace<Relations>(npc2);
                registry.emplace<Renderable>(npc2, WHITE, 6.f);
                Age twinAge; twinAge.days = 0.f; twinAge.maxDays = lifespan(s_rng);
                registry.emplace<Age>(npc2, twinAge);
                std::string twinName = std::string(FIRSTS[fd(s_rng)]) + " " +
                    (familySurname.empty() ? LASTS[ld(s_rng)] : familySurname);
                registry.emplace<Name>(npc2, Name{ twinName });
                registry.emplace<Money>(npc2, Money{ 5.f });
                auto& twinNeeds = registry.get<Needs>(npc2);
                for (auto& need : twinNeeds.list) need.drainRate *= trait_dist(s_rng);
                // Twin shares same aptitude bias as first sibling (family trait)
                Skills twinSkills{ 0.08f, 0.08f, 0.08f };
                if      (aptIdx == 0) twinSkills.farming       = 0.15f;
                else if (aptIdx == 1) twinSkills.water_drawing = 0.15f;
                else                  twinSkills.woodcutting   = 0.15f;
                registry.emplace<Skills>(npc2, twinSkills);
                registry.emplace<Profession>(npc2, Profession{ settlProfession });
                registry.emplace<ChildTag>(npc2);

                // Inherit FamilyTag for twin (same logic as first sibling)
                {
                    std::map<std::string, int> familyFreq2;
                    registry.view<HomeSettlement, FamilyTag>(entt::exclude<ChildTag, PlayerTag>).each(
                        [&](const HomeSettlement& hs2, const FamilyTag& ft) {
                            if (hs2.settlement != settl || ft.name.empty()) return;
                            ++familyFreq2[ft.name];
                        });
                    if (!familyFreq2.empty()) {
                        auto best = std::max_element(familyFreq2.begin(), familyFreq2.end(),
                            [](const auto& a, const auto& b){ return a.second < b.second; });
                        registry.emplace<FamilyTag>(npc2, FamilyTag{ best->first });
                    }
                }

                if (log) {
                    const auto& s = settlView.get<Settlement>(settl);
                    std::string msg = "Born (twins!): " + npcName + " & " + twinName + " at " + s.name;
                    if (!parentName.empty()) msg += " (to " + parentName + ")";
                    log->Push(tm.day, (int)tm.hourOfDay, msg);
                }
            }
        }
    }
}
