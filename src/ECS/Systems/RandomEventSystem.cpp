#include "RandomEventSystem.h"
#include "DynBitset.h"
#include "ECS/Components.h"
#include "World/WorldSchema.h"
#include <algorithm>
#include <set>
#include <vector>
#include <string>

static constexpr float BANDIT_DURATION   = 3.f;    // fallback road block duration
static constexpr float EVENT_MEAN_HOURS  = 72.f;   // ~3 game-days between events
static constexpr float EVENT_JITTER      = 36.f;   // +/-jitter in game-hours

// Check if any rival settlements share the same crisis type; soften rivalry if so.
static void SoftenRivalryOnSharedCrisis(entt::registry& registry,
                                         entt::entity target,
                                         Settlement& settl,
                                         const std::string& crisisName,
                                         EventLog* log, int day, int hour) {
    static constexpr float RIVAL_THRESHOLD = -0.5f;
    static constexpr float RELATION_BOOST  =  0.15f;

    for (auto& [otherEnt, relation] : settl.relations) {
        if (relation >= RIVAL_THRESHOLD) continue;   // not a rival
        if (!registry.valid(otherEnt)) continue;
        auto* otherSettl = registry.try_get<Settlement>(otherEnt);
        if (!otherSettl) continue;
        if (otherSettl->modifierName != crisisName) continue;  // not same crisis

        // Both settlements suffer the same crisis — soften rivalry
        relation = std::min(1.f, relation + RELATION_BOOST);
        auto& reverse = otherSettl->relations[target];
        reverse = std::min(1.f, reverse + RELATION_BOOST);

        if (log) {
            log->Push(day, hour,
                settl.name + " and " + otherSettl->name +
                " set aside differences during " + crisisName);
        }
    }
}

void RandomEventSystem::Update(entt::registry& registry, float realDt, const WorldSchema& schema) {
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
                bool wasFestival = (s.modifierName == "Festival" || s.modifierName == "Harvest Festival");
                if (log)
                    log->Push(tm.day, (int)tm.hourOfDay,
                        s.modifierName + " ends at " + s.name +
                        (wasPlague ? " — plague contained" : " — production restored"));
                // Post-festival afterglow: morale drift halved for 12 game-hours
                if (wasFestival)
                    s.afterglowHours = 12.f;
                // Post-crisis community gathering: surviving drought/plague strengthens bonds
                bool wasDrought = (s.modifierName == "Drought");
                if (wasDrought || wasPlague) {
                    std::vector<entt::entity> survivors;
                    registry.view<HomeSettlement, Relations>(
                        entt::exclude<Hauler, PlayerTag, BanditTag>).each(
                        [&](auto npc, const HomeSettlement& hs, const Relations&) {
                            if (hs.settlement == e) survivors.push_back(npc);
                        });
                    for (size_t i = 0; i < survivors.size(); ++i) {
                        auto* relA = registry.try_get<Relations>(survivors[i]);
                        if (!relA) continue;
                        for (size_t j = i + 1; j < survivors.size(); ++j) {
                            auto itA = relA->affinity.find(survivors[j]);
                            if (itA == relA->affinity.end() || itA->second < 0.2f) continue;
                            auto* relB = registry.try_get<Relations>(survivors[j]);
                            if (!relB) continue;
                            auto itB = relB->affinity.find(survivors[i]);
                            if (itB == relB->affinity.end() || itB->second < 0.2f) continue;
                            itA->second = std::min(1.f, itA->second + 0.02f);
                            itB->second = std::min(1.f, itB->second + 0.02f);
                        }
                    }
                    if (log) {
                        std::string crisis = wasDrought ? "drought" : "plague";
                        log->Push(tm.day, (int)tm.hourOfDay,
                            s.name + " celebrates surviving the " + crisis);
                    }
                }
                s.modifierName.clear();
                if (wasPlague) m_plagueSpreadTimer.erase(e);
            }
        }

        // Tick down post-festival afterglow
        if (s.afterglowHours > 0.f)
            s.afterglowHours = std::max(0.f, s.afterglowHours - gameHoursDt);

        // Morale drifts toward 0.5 baseline at ±0.5% per game-hour (slow, organic recovery)
        // During afterglow, drift is halved to preserve festival morale boost longer.
        float driftRate = (s.afterglowHours > 0.f) ? 0.0025f : 0.005f;
        float drift = (0.5f - s.morale) * driftRate * gameHoursDt;
        s.morale = std::max(0.f, std::min(1.f, s.morale + drift));

        // Trade volume counter: reset every 24 game-hours
        s.tradeVolumeTimer += gameHoursDt;
        if (s.tradeVolumeTimer >= 24.f) {
            s.tradeVolume = 0;
            s.importCount = 0;
            s.exportCount = 0;
            s.desperatePurchases = 0;
            s.tradeVolumeTimer -= 24.f;
        }

        // Bounty pool: settlements accumulate 0.5g per adjacent-road bandit per game-hour.
        // Gold is drawn from treasury; pool is paid to the player on bandit confrontation.
        {
            int adjBandits = 0;
            registry.view<Road>().each([&](const Road& road) {
                if (road.blocked) return;
                if (road.from != e && road.to != e) return;
                // Count bandits near this road midpoint
                const auto* pa = registry.try_get<Position>(road.from);
                const auto* pb = registry.try_get<Position>(road.to);
                if (!pa || !pb) return;
                float mx = (pa->x + pb->x) * 0.5f;
                float my = (pa->y + pb->y) * 0.5f;
                registry.view<Position, BanditTag>().each(
                    [&](const Position& bp) {
                        float dx = bp.x - mx, dy = bp.y - my;
                        if (dx*dx + dy*dy < 100.f * 100.f) ++adjBandits;
                    });
            });
            if (adjBandits > 0) {
                float bountyAdd = 0.5f * adjBandits * gameHoursDt;
                float fromTreasury = std::min(bountyAdd, s.treasury);
                s.treasury   -= fromTreasury;
                s.bountyPool += fromTreasury;
            }
        }

        // Bonus morale recovery when all three stockpiles are above 80 units.
        // Rewards players who maintain surpluses; gives morale a second recovery path.
        static constexpr float STOCKPILE_ABUNDANCE = 80.f;
        if (const auto* sp = registry.try_get<Stockpile>(e)) {
            auto foodIt  = sp->quantities.find(RES_FOOD);
            auto waterIt = sp->quantities.find(RES_WATER);
            auto woodIt  = sp->quantities.find(RES_WOOD);
            bool abundant = (foodIt  != sp->quantities.end() && foodIt->second  >= STOCKPILE_ABUNDANCE) &&
                            (waterIt != sp->quantities.end() && waterIt->second >= STOCKPILE_ABUNDANCE) &&
                            (woodIt  != sp->quantities.end() && woodIt->second  >= STOCKPILE_ABUNDANCE);
            // One-shot abundance log: fires once per abundance period
            static std::set<entt::entity> s_loggedAbundance;
            static constexpr float SCARCITY_RESET = 40.f;
            if (abundant) {
                s.morale = std::min(1.f, s.morale + 0.002f * gameHoursDt);
                if (s_loggedAbundance.insert(e).second && log) {
                    std::string where = s.name;
                    log->Push(tm.day, (int)tm.hourOfDay,
                        "Prosperity: " + where + " has abundant stores — morale rising.");
                }
            } else if (s_loggedAbundance.erase(e)) {
                // Log once when a settlement first drops out of abundance
                if (log)
                    log->Push(tm.day, (int)tm.hourOfDay,
                        "Abundance fading at " + s.name + " — stores declining.");
            }
            // Scarcity: any stockpile below 10 drains morale
            static constexpr float SCARCITY_THRESHOLD = 10.f;
            bool scarce = (foodIt  == sp->quantities.end() || foodIt->second  < SCARCITY_THRESHOLD) ||
                          (waterIt == sp->quantities.end() || waterIt->second < SCARCITY_THRESHOLD) ||
                          (woodIt  == sp->quantities.end() || woodIt->second  < SCARCITY_THRESHOLD);
            if (scarce)
                s.morale = std::max(0.f, s.morale - 0.003f * gameHoursDt);

            // One-shot scarcity log per resource: bitmask tracks which resources have been logged
            static std::map<entt::entity, int> s_loggedScarcity;
            static constexpr float SCARCITY_CLEAR = 20.f;
            int& mask = s_loggedScarcity[e];
            bool foodLow  = (foodIt  == sp->quantities.end() || foodIt->second  < SCARCITY_THRESHOLD);
            bool waterLow = (waterIt == sp->quantities.end() || waterIt->second < SCARCITY_THRESHOLD);
            bool woodLow  = (woodIt  == sp->quantities.end() || woodIt->second  < SCARCITY_THRESHOLD);
            // Detect and log recoveries before clearing bits
            int recovering = 0;
            if ((mask & 1) && !foodLow  && foodIt  != sp->quantities.end() && foodIt->second  >= SCARCITY_CLEAR) recovering |= 1;
            if ((mask & 2) && !waterLow && waterIt != sp->quantities.end() && waterIt->second >= SCARCITY_CLEAR) recovering |= 2;
            if ((mask & 4) && !woodLow  && woodIt  != sp->quantities.end() && woodIt->second  >= SCARCITY_CLEAR) recovering |= 4;
            if (recovering && log) {
                std::string resources;
                if (recovering & 1) resources += "food";
                if (recovering & 2) { if (!resources.empty()) resources += ", "; resources += "water"; }
                if (recovering & 4) { if (!resources.empty()) resources += ", "; resources += "wood"; }
                log->Push(tm.day, (int)tm.hourOfDay,
                    "Recovery: " + s.name + " " + resources + " stores recovering.");
            }
            // Morale bump: recovering from scarcity is a small community boost
            if (recovering) {
                int recoveredCount = ((recovering & 1) ? 1 : 0)
                                   + ((recovering & 2) ? 1 : 0)
                                   + ((recovering & 4) ? 1 : 0);
                s.morale = std::min(1.f, s.morale + 0.01f * recoveredCount);
            }
            mask &= ~recovering;
            // Log newly scarce resources
            int newBits = 0;
            if (foodLow  && !(mask & 1)) newBits |= 1;
            if (waterLow && !(mask & 2)) newBits |= 2;
            if (woodLow  && !(mask & 4)) newBits |= 4;
            if (newBits && log) {
                std::string resources;
                if (newBits & 1) resources += "food";
                if (newBits & 2) { if (!resources.empty()) resources += ", "; resources += "water"; }
                if (newBits & 4) { if (!resources.empty()) resources += ", "; resources += "wood"; }
                log->Push(tm.day, (int)tm.hourOfDay,
                    "Shortage: " + s.name + " running low on " + resources + ".");
            }
            mask |= newBits;
        }

        // Unrest: log once when morale crosses below 0.3, and again on recovery above 0.4
        if (!s.unrest && s.morale < 0.3f) {
            s.unrest = true;
            if (log) {
                int pop = 0;
                registry.view<HomeSettlement>(entt::exclude<PlayerTag, Hauler>).each(
                    [&](const HomeSettlement& hs) { if (hs.settlement == e) ++pop; });
                char buf[120];
                std::snprintf(buf, sizeof(buf),
                    "UNREST in %s [pop %d] — morale %d%%, production suffering",
                    s.name.c_str(), pop, (int)(s.morale * 100));
                log->Push(tm.day, (int)tm.hourOfDay, buf);
            }
        } else if (s.unrest && s.morale >= 0.4f) {
            s.unrest = false;
            if (log) {
                int pop = 0;
                registry.view<HomeSettlement>(entt::exclude<PlayerTag, Hauler>).each(
                    [&](const HomeSettlement& hs) { if (hs.settlement == e) ++pop; });
                char buf[120];
                std::snprintf(buf, sizeof(buf),
                    "Tensions ease in %s [pop %d] — morale recovering (%d%%)",
                    s.name.c_str(), pop, (int)(s.morale * 100));
                log->Push(tm.day, (int)tm.hourOfDay, buf);
            }
        }

        // Drain work-stoppage cooldown
        if (s.strikeCooldown > 0.f)
            s.strikeCooldown = std::max(0.f, s.strikeCooldown - gameHoursDt);

        // Drift inter-settlement relations toward neutral (0) at 0.3% per game-hour.
        // Also prune entries pointing to destroyed settlement entities.
        // Log first-time rivalry/alliance threshold crossings.
        static constexpr float RELATION_DRIFT       = 0.003f;
        static constexpr float RIVALRY_THRESHOLD     = -0.5f;
        static constexpr float RIVALRY_RECOVERY      = -0.3f;
        static constexpr float ALLIANCE_THRESHOLD    =  0.5f;
        for (auto it = s.relations.begin(); it != s.relations.end(); ) {
            if (!registry.valid(it->first)) {
                it = s.relations.erase(it);
                continue;
            }
            if (it->second > 0.f) it->second = std::max(0.f, it->second - RELATION_DRIFT * gameHoursDt);
            else                  it->second = std::min(0.f, it->second + RELATION_DRIFT * gameHoursDt);

            // Canonical pair to avoid logging both A→B and B→A
            uint32_t idA = static_cast<uint32_t>(entt::to_integral(e));
            uint32_t idB = static_cast<uint32_t>(entt::to_integral(it->first));
            auto key = (idA < idB) ? std::make_pair(idA, idB) : std::make_pair(idB, idA);

            if (log) {
                const auto* otherSettl = registry.try_get<Settlement>(it->first);
                if (otherSettl) {
                    // Rivalry onset: crosses below -0.5
                    if (it->second <= RIVALRY_THRESHOLD && !m_loggedRivalries.count(key)) {
                        m_loggedRivalries.insert(key);
                        m_loggedAlliances.erase(key);
                        m_loggedRivalryRecovery.erase(key);  // allow recovery log later
                        log->Push(tm.day, (int)tm.hourOfDay,
                            "RIVALRY: " + s.name + " and " + otherSettl->name +
                            " relations deteriorate \xe2\x80\x94 tariffs imposed");
                    }
                    // Rivalry recovery: crosses above -0.3 after being in rivalry
                    if (it->second >= RIVALRY_RECOVERY && m_loggedRivalries.count(key)
                        && !m_loggedRivalryRecovery.count(key)) {
                        m_loggedRivalryRecovery.insert(key);
                        m_loggedRivalries.erase(key);  // allow re-trigger if they drift hostile again
                        log->Push(tm.day, (int)tm.hourOfDay,
                            "Relations improving between " + s.name + " and " + otherSettl->name);
                    } else if (it->second > RIVALRY_THRESHOLD && it->second < RIVALRY_RECOVERY) {
                        // Between -0.5 and -0.3: keep rivalry flag but no recovery yet
                    } else if (it->second > RIVALRY_RECOVERY) {
                        m_loggedRivalries.erase(key);
                    }

                    // Alliance onset: crosses above +0.5
                    if (it->second >= ALLIANCE_THRESHOLD && !m_loggedAlliances.count(key)) {
                        m_loggedAlliances.insert(key);
                        m_loggedRivalries.erase(key);
                        log->Push(tm.day, (int)tm.hourOfDay,
                            "Alliance formed: " + s.name + " & " + otherSettl->name);
                    } else if (it->second < ALLIANCE_THRESHOLD) {
                        m_loggedAlliances.erase(key);
                    }
                }
            }

            ++it;
        }

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
                        if (auto* pes = registry.try_get<PersonalEventState>(npc)) {
                            pes->strikeDuration = STRIKE_DURATION_HOURS;
                        }
                        ++strikerCount;
                    });

                if (log && strikerCount > 0) {
                    char buf[140];
                    std::snprintf(buf, sizeof(buf),
                        "Workers strike at %s \xe2\x80\x94 %d workers walk out (morale: %d%%)",
                        s.name.c_str(), strikerCount, (int)(s.morale * 100));
                    log->Push(tm.day, (int)tm.hourOfDay, buf);
                }
            }
        }
    });

    // ---- Settlement rivalry: tick down and trigger ----
    // Tick down active rivalries first
    registry.view<Settlement>().each([&](auto e, Settlement& s) {
        if (s.rivalryTimer > 0.f) {
            s.rivalryTimer -= gameHoursDt;
            if (s.rivalryTimer <= 0.f) {
                if (log)
                    log->Push(tm.day, (int)tm.hourOfDay,
                        "Rivalry fades between " + s.name + " and " + s.rivalWith);
                s.rivalryTimer  = 0.f;
                s.rivalWith.clear();
                s.rivalEntity   = entt::null;
            }
        }
    });

    // Check for new rivalries between connected settlements
    {
        static constexpr float RIVALRY_DURATION = 24.f;  // game-hours
        static constexpr float RIVALRY_CHECK_PROB = 0.02f; // 2% per game-hour per eligible pair
        registry.view<Road>().each([&](const Road& road) {
            if (road.blocked) return;
            auto* sa = registry.try_get<Settlement>(road.from);
            auto* sb = registry.try_get<Settlement>(road.to);
            if (!sa || !sb) return;
            // Both must have morale > 0.7, neither already in a rivalry
            if (sa->morale <= 0.7f || sb->morale <= 0.7f) return;
            if (sa->rivalryTimer > 0.f || sb->rivalryTimer > 0.f) return;
            // Count populations
            int popA = 0, popB = 0;
            registry.view<HomeSettlement>(entt::exclude<PlayerTag, Hauler>).each(
                [&](const HomeSettlement& hs) {
                    if (hs.settlement == road.from) ++popA;
                    if (hs.settlement == road.to)   ++popB;
                });
            if (popA <= 15 || popB <= 15) return;
            // Probability roll
            std::uniform_real_distribution<float> chance(0.f, 1.f);
            if (chance(m_rng) >= RIVALRY_CHECK_PROB * gameHoursDt) return;
            // Trigger rivalry on both settlements
            sa->rivalWith   = sb->name;
            sa->rivalEntity = road.to;
            sa->rivalryTimer = RIVALRY_DURATION;
            sb->rivalWith   = sa->name;
            sb->rivalEntity = road.from;
            sb->rivalryTimer = RIVALRY_DURATION;
            if (log)
                log->Push(tm.day, (int)tm.hourOfDay,
                    sa->name + " and " + sb->name + " are competing for regional dominance.");
        });
    }

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

    // ---- Spreading events (data-driven plague-like spreading) ----
    // Find the spreading event definition from schema (if any).
    // Each infected settlement tries to spread to a connected neighbour.
    const EventDef* spreadDef = nullptr;
    for (const auto& ev : schema.events)
        if (ev.spreads) { spreadDef = &ev; break; }

    float spreadInterval = spreadDef ? spreadDef->spreadInterval : 20.f;
    float spreadChanceVal = spreadDef ? spreadDef->spreadChance : 0.60f;
    float spreadModifier = spreadDef ? spreadDef->effectValue : 0.45f;
    float spreadDuration = spreadDef ? spreadDef->durationHours : 72.f;
    float spreadKillFrac = spreadDef ? spreadDef->spreadKillFraction : 0.10f;
    std::string spreadName = spreadDef ? spreadDef->displayName : "Plague";

    for (auto& [plagueSettl, timer] : m_plagueSpreadTimer) {
        if (!registry.valid(plagueSettl)) continue;
        timer -= gameHoursDt;
        if (timer > 0.f) continue;
        timer = spreadInterval;

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
        if (chance(m_rng) > spreadChanceVal) continue;

        entt::entity target2 = neighbors[pickN(m_rng)];
        if (!registry.valid(target2)) continue;
        if (m_plagueSpreadTimer.count(target2)) continue;  // already infected

        auto* ts = registry.try_get<Settlement>(target2);
        if (!ts || ts->modifierDuration > 0.f) continue;  // already has another event

        ts->productionModifier = spreadModifier;
        ts->modifierDuration   = spreadDuration;
        ts->modifierName       = spreadName;
        m_plagueSpreadTimer[target2] = spreadInterval;

        int killed = KillFraction(registry, target2, spreadKillFrac);

        // Count destination population for log context
        int destPop = 0;
        registry.view<HomeSettlement>(entt::exclude<PlayerTag, Hauler>).each(
            [&](const HomeSettlement& hs) { if (hs.settlement == target2) ++destPop; });
        // Build trend indicator from last sample
        const char* destTrend = "";
        {
            auto prev = m_prevPop.find(target2);
            if (prev != m_prevPop.end()) {
                int d = destPop - prev->second;
                if (d >= 2) destTrend = " \xe2\x86\x91";
                else if (d <= -2) destTrend = " \xe2\x86\x93";
            }
        }

        const auto* src = registry.try_get<Settlement>(plagueSettl);
        if (log) {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "%s spreads from %s to %s [pop %d%s] -- %d died",
                spreadName.c_str(),
                src ? src->name.c_str() : "?", ts->name.c_str(),
                destPop, destTrend, killed);
            log->Push(tm.day, (int)tm.hourOfDay, buf);
        }
    }

    // Sample settlement populations every 24 game-hours for trend tracking
    m_popSampleTimer -= gameHoursDt;
    if (m_popSampleTimer <= 0.f) {
        m_popSampleTimer = 24.f;
        m_prevPop.clear();
        registry.view<Settlement>().each([&](auto e, const Settlement&) {
            int count = 0;
            registry.view<HomeSettlement>(entt::exclude<PlayerTag, Hauler>).each(
                [&](const HomeSettlement& hs) { if (hs.settlement == e) ++count; });
            m_prevPop[e] = count;
        });
    }

    // ---- Diversity festival: once per game-day, diverse settlements may celebrate ----
    {
        static int s_lastFestivalDay = -1;
        if (tm.day != s_lastFestivalDay) {
            s_lastFestivalDay = tm.day;
            static constexpr float HARVEST_FEST_MORALE   = 0.05f;
            static constexpr float HARVEST_FEST_AFFINITY = 0.02f;
            static constexpr float HARVEST_FEST_DURATION = 4.f;  // game-hours

            // Pre-compute full profession bitmask (constant per schema)
            DynBitset fullProfMask2(schema.professions.size());
            for (auto& pd : schema.professions)
                if (pd.producesResource != INVALID_ID) fullProfMask2.set(pd.id);

            registry.view<Settlement>().each([&](auto e, Settlement& s) {
                if (s.modifierDuration > 0.f) return;  // already has an event

                // Compute profession diversity bitmask for this settlement
                DynBitset profMask;
                registry.view<Profession, HomeSettlement>(
                    entt::exclude<Hauler, PlayerTag, BanditTag, ChildTag>).each(
                    [&](const Profession& prof, const HomeSettlement& hs) {
                        if (hs.settlement != e) return;
                        if (prof.type >= 0 && prof.type < (int)schema.professions.size()
                            && schema.professions[prof.type].producesResource != INVALID_ID)
                            profMask |= DynBitset::singleBit(prof.type);
                    });
                // Check all producing professions are present
                if (!fullProfMask2.any() || !profMask.containsAll(fullProfMask2)) return;

                // 1-in-200 chance per game-day
                if (m_rng() % 200 != 0) return;

                // Trigger Harvest Festival
                s.modifierName     = "Harvest Festival";
                s.modifierDuration = HARVEST_FEST_DURATION;
                s.productionModifier = 1.f;  // no production change
                s.morale = std::min(1.f, s.morale + HARVEST_FEST_MORALE);

                // Boost affinity between all resident pairs
                std::vector<entt::entity> residents;
                registry.view<Relations, HomeSettlement>(
                    entt::exclude<PlayerTag, BanditTag>).each(
                    [&](auto re, Relations&, const HomeSettlement& hs) {
                        if (hs.settlement == e) residents.push_back(re);
                    });
                for (size_t i = 0; i < residents.size(); ++i) {
                    auto& relA = registry.get<Relations>(residents[i]);
                    for (size_t j = i + 1; j < residents.size(); ++j) {
                        relA.affinity[residents[j]] =
                            std::min(1.f, relA.affinity[residents[j]] + HARVEST_FEST_AFFINITY);
                        auto& relB = registry.get<Relations>(residents[j]);
                        relB.affinity[residents[i]] =
                            std::min(1.f, relB.affinity[residents[i]] + HARVEST_FEST_AFFINITY);
                    }
                }

                if (log)
                    log->Push(tm.day, (int)tm.hourOfDay,
                        s.name + " celebrates its diverse workforce!");
            });
        }
    }

    // ---- Elder storytelling event ----
    // Once per day, each settlement rolls a 1-in-200 chance. If triggered, a skilled
    // elder tells tales: attendees with affinity >= 0.2 gain +0.02 mutual affinity,
    // the elder gains +0.01 toward all attendees, settlement morale +0.02.
    {
        static int s_lastStoryDay = -1;
        if (tm.day != s_lastStoryDay) {
            s_lastStoryDay = tm.day;

            registry.view<Settlement>().each([&](auto settlE, Settlement& s) {
                // 1-in-200 chance per game-day
                if (m_rng() % 200 != 0) return;

                // Find a skilled elder at this settlement
                entt::entity storyteller = entt::null;
                registry.view<Age, Skills, HomeSettlement, Relations>(
                    entt::exclude<PlayerTag, BanditTag, ChildTag>).each(
                    [&](auto npc, const Age& age, const Skills& sk, const HomeSettlement& hs, Relations&) {
                        if (storyteller != entt::null) return; // already found one
                        if (hs.settlement != settlE) return;
                        if (age.days <= 60.f) return;
                        if (!sk.AnyAbove(0.7f)) return;
                        storyteller = npc;
                    });
                if (storyteller == entt::null) return;

                // Gather attendees: NPCs with affinity >= 0.2 toward the elder
                auto& elderRel = registry.get<Relations>(storyteller);
                std::vector<entt::entity> attendees;
                registry.view<Relations, HomeSettlement>(
                    entt::exclude<PlayerTag, BanditTag>).each(
                    [&](auto npc, Relations& rel, const HomeSettlement& hs) {
                        if (npc == storyteller) return;
                        if (hs.settlement != settlE) return;
                        auto it = rel.affinity.find(storyteller);
                        if (it != rel.affinity.end() && it->second >= 0.2f)
                            attendees.push_back(npc);
                    });
                if (attendees.empty()) return;

                // Boost mutual affinity +0.02 among all attendees
                for (size_t i = 0; i < attendees.size(); ++i) {
                    auto& relA = registry.get<Relations>(attendees[i]);
                    for (size_t j = i + 1; j < attendees.size(); ++j) {
                        relA.affinity[attendees[j]] = std::min(1.f, relA.affinity[attendees[j]] + 0.02f);
                        auto& relB = registry.get<Relations>(attendees[j]);
                        relB.affinity[attendees[i]] = std::min(1.f, relB.affinity[attendees[i]] + 0.02f);
                    }
                }

                // Elder gains +0.01 affinity toward each attendee
                for (auto att : attendees)
                    elderRel.affinity[att] = std::min(1.f, elderRel.affinity[att] + 0.01f);

                // Morale boost
                s.morale = std::min(1.f, s.morale + 0.02f);

                // Log
                if (log) {
                    std::string elderName = "An elder";
                    if (const auto* nm = registry.try_get<Name>(storyteller)) elderName = nm->value;
                    log->Push(tm.day, (int)tm.hourOfDay,
                        elderName + " tells tales of the old days at " + s.name + ".");
                }
            });
        }
    }

    // Count down to next event
    m_nextEvent -= gameHoursDt;
    if (m_nextEvent <= 0.f) {
        // Schedule next
        std::uniform_real_distribution<float> jitter(-EVENT_JITTER, EVENT_JITTER);
        m_nextEvent = std::max(12.f, EVENT_MEAN_HOURS + jitter(m_rng));
        TriggerEvent(registry, tm.day, (int)tm.hourOfDay, schema);
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
    registry.view<PersonalEventState, Skills, Money, Name>(
        entt::exclude<PlayerTag, BanditTag>)
        .each([&](auto e, PersonalEventState& dt, Skills& skills, Money& money, const Name& name) {
            // Drain timers regardless of whether an event fires
            static std::set<entt::entity> s_currentlyIll;
            if (dt.illnessTimer > 0.f) {
                s_currentlyIll.insert(e);
                dt.illnessTimer = std::max(0.f, dt.illnessTimer - gameHoursDt);
                if (dt.illnessTimer <= 0.f && s_currentlyIll.erase(e) && log) {
                    const auto* hs = registry.try_get<HomeSettlement>(e);
                    const char* settName = "the wilds";
                    if (hs && hs->settlement != entt::null && registry.valid(hs->settlement))
                        if (const auto* stt = registry.try_get<Settlement>(hs->settlement))
                            settName = stt->name.c_str();
                    char buf[120];
                    std::snprintf(buf, sizeof(buf), "%s recovered from illness at %s",
                        name.value.c_str(), settName);
                    log->Push(tm.day, (int)tm.hourOfDay, buf);
                }
            }
            if (dt.harvestBonusTimer > 0.f)
                dt.harvestBonusTimer = std::max(0.f, dt.harvestBonusTimer - gameHoursDt);

            // Suffering log: when contentment < 0.2, log once until recovery above 0.5
            if (log) {
                static std::set<entt::entity> s_desperateLogged;
                if (const auto* needs = registry.try_get<Needs>(e)) {
                    float contentment = needs->list[0].value * 0.3f
                                      + needs->list[1].value * 0.3f
                                      + needs->list[2].value * 0.2f
                                      + needs->list[3].value * 0.2f;
                    if (contentment < 0.2f && !s_desperateLogged.count(e)) {
                        s_desperateLogged.insert(e);
                        const auto* hs = registry.try_get<HomeSettlement>(e);
                        const char* settName = "the wilds";
                        if (hs && hs->settlement != entt::null && registry.valid(hs->settlement))
                            if (const auto* stt = registry.try_get<Settlement>(hs->settlement))
                                settName = stt->name.c_str();
                        char buf[100];
                        std::snprintf(buf, sizeof(buf), "%s is desperate at %s",
                            name.value.c_str(), settName);
                        log->Push(tm.day, (int)tm.hourOfDay, buf);
                    } else if (contentment >= 0.5f) {
                        s_desperateLogged.erase(e);
                    }
                }
                // Prune dead entities from the set
                for (auto it = s_desperateLogged.begin(); it != s_desperateLogged.end(); )
                    if (!registry.valid(*it)) it = s_desperateLogged.erase(it); else ++it;
            }

            dt.personalEventTimer -= gameHoursDt;
            if (dt.personalEventTimer > 0.f) return;

            // Schedule next event
            dt.personalEventTimer = nextEvtDist(m_rng);

            switch (evtTypeDist(m_rng)) {
                case 0: {   // Skill discovery — +0.1 to a random skill
                    int nsk = skills.Size();
                    int idx = (nsk > 0) ? std::uniform_int_distribution<int>(0, nsk - 1)(m_rng) : 0;
                    float gain = skillGain_dist(m_rng);
                    if (idx >= 0 && idx < nsk)
                        skills.Set(idx, std::min(1.f, skills.Get(idx) + gain));
                    if (log) {
                        std::string skillName = (idx >= 0 && idx < (int)schema.skills.size())
                            ? schema.skills[idx].displayName : "skill";
                        const char* settName = "the wilds";
                        if (const auto* hs = registry.try_get<HomeSettlement>(e))
                            if (hs->settlement != entt::null && registry.valid(hs->settlement))
                                if (const auto* stt = registry.try_get<Settlement>(hs->settlement))
                                    settName = stt->name.c_str();
                        char buf[120];
                        std::snprintf(buf, sizeof(buf), "%s had a skill insight in %s at %s",
                            name.value.c_str(), skillName.c_str(), settName);
                        log->Push(tm.day, (int)tm.hourOfDay, buf);
                    }
                    break;
                }
                case 1: {   // Windfall — find 5–15g on the road (exogenous injection)
                    float found = windfall_dist(m_rng);
                    money.balance += found;
                    if (log) {
                        const char* nearName = "";
                        if (const auto* hs = registry.try_get<HomeSettlement>(e)) {
                            if (hs->settlement != entt::null && registry.valid(hs->settlement)) {
                                if (const auto* stt = registry.try_get<Settlement>(hs->settlement))
                                    nearName = stt->name.c_str();
                            }
                        }
                        char buf[120];
                        if (nearName[0] != '\0')
                            std::snprintf(buf, sizeof(buf), "%s found %.0fg on the road near %s",
                                name.value.c_str(), found, nearName);
                        else
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
                            const char* settName = "the wilds";
                            if (const auto* hs = registry.try_get<HomeSettlement>(e))
                                if (hs->settlement != entt::null && registry.valid(hs->settlement))
                                    if (const auto* stt = registry.try_get<Settlement>(hs->settlement))
                                        settName = stt->name.c_str();
                            char buf[120];
                            std::snprintf(buf, sizeof(buf),
                                "%s fell ill (%s doubled for %dh) at %s",
                                name.value.c_str(), needName, (int)ILLNESS_DURATION, settName);
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

            // Elder wisdom transfer — one-time event for elders age > 70 with a skill ≥ 0.6
            auto* socialBeh = registry.try_get<SocialBehavior>(e);
            if (socialBeh && !socialBeh->wisdomFired) {
                const auto* age = registry.try_get<Age>(e);
                if (age && age->days > 70.f) {
                    float bestSkill = skills.BestValue();
                    if (bestSkill >= 0.6f) {
                        int bestSkillId = skills.BestSkillId();
                        int bestType = (bestSkillId >= 0 && bestSkillId < (int)schema.skills.size())
                            ? schema.skills[bestSkillId].forResource : RES_FOOD;

                        // Find a younger co-settled NPC to receive the knowledge
                        const auto* hs = registry.try_get<HomeSettlement>(e);
                        if (hs && hs->settlement != entt::null && registry.valid(hs->settlement)) {
                            std::vector<entt::entity> candidates;
                            registry.view<Age, Skills, HomeSettlement, Name>(
                                entt::exclude<PlayerTag, BanditTag>).each(
                                [&](auto ce, const Age& ca, const Skills&, const HomeSettlement& chs, const Name&) {
                                if (ce == e) return;
                                if (chs.settlement != hs->settlement) return;
                                if (ca.days >= 60.f) return;  // only younger NPCs
                                candidates.push_back(ce);
                            });
                            if (!candidates.empty()) {
                                std::uniform_int_distribution<int> pick(0, (int)candidates.size() - 1);
                                auto target = candidates[pick(m_rng)];
                                auto* tSkills = registry.try_get<Skills>(target);
                                if (tSkills && bestSkillId != INVALID_ID) {
                                    tSkills->Set(bestSkillId, std::min(0.8f, tSkills->Get(bestSkillId) + 0.1f));
                                    socialBeh->wisdomFired = true;

                                    if (log) {
                                        const auto* stt = registry.try_get<Settlement>(hs->settlement);
                                        const char* settName = stt ? stt->name.c_str() : "the wilds";
                                        const auto* tName = registry.try_get<Name>(target);
                                        char buf[160];
                                        std::snprintf(buf, sizeof(buf),
                                            "Elder %s passed their knowledge to %s at %s.",
                                            name.value.c_str(),
                                            tName ? tName->value.c_str() : "a younger worker",
                                            settName);
                                        log->Push(tm.day, (int)tm.hourOfDay, buf);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        });
}

// Helper: map rumour type string from schema to RumourType enum.
static RumourType ParseRumourType(const std::string& s) {
    if (s == "PlagueNearby")  return RumourType::PlagueNearby;
    if (s == "DroughtNearby") return RumourType::DroughtNearby;
    if (s == "BanditRoads")   return RumourType::BanditRoads;
    if (s == "GoodHarvest")   return RumourType::GoodHarvest;
    return RumourType::DroughtNearby;  // fallback
}

// Helper: seed rumours at a settlement.
static void SeedRumours(entt::registry& registry, entt::entity target,
                        const std::string& rumourTypeStr, int seedCount,
                        std::mt19937& rng) {
    if (rumourTypeStr.empty() || seedCount <= 0) return;
    RumourType rt = ParseRumourType(rumourTypeStr);
    std::vector<entt::entity> residents;
    registry.view<HomeSettlement>(entt::exclude<Hauler, PlayerTag>).each(
        [&](auto e, const HomeSettlement& hs) {
            if (hs.settlement == target && !registry.any_of<Rumour>(e))
                residents.push_back(e);
        });
    std::shuffle(residents.begin(), residents.end(), rng);
    int count = std::min((int)residents.size(), seedCount);
    for (int k = 0; k < count; ++k)
        registry.emplace<Rumour>(residents[k], Rumour{rt, target, 3});
}

// Helper: apply solidarity affinity boost among residents with existing bonds.
static void ApplySolidarity(entt::registry& registry, entt::entity target,
                            float boost) {
    if (boost <= 0.f) return;
    std::vector<entt::entity> residents;
    registry.view<HomeSettlement, Relations>(
        entt::exclude<Hauler, PlayerTag, BanditTag>).each(
        [&](auto e, const HomeSettlement& hs, const Relations&) {
            if (hs.settlement == target) residents.push_back(e);
        });
    for (size_t i = 0; i < residents.size(); ++i) {
        auto* relA = registry.try_get<Relations>(residents[i]);
        if (!relA) continue;
        for (size_t j = i + 1; j < residents.size(); ++j) {
            auto itA = relA->affinity.find(residents[j]);
            if (itA == relA->affinity.end() || itA->second < 0.3f) continue;
            auto* relB = registry.try_get<Relations>(residents[j]);
            if (!relB) continue;
            auto itB = relB->affinity.find(residents[i]);
            if (itB == relB->affinity.end() || itB->second < 0.3f) continue;
            itA->second = std::min(1.f, itA->second + boost);
            itB->second = std::min(1.f, itB->second + boost);
        }
    }
}

// Helper: build population tag string with trend indicator.
static std::string BuildPopTag(int popCount,
                               const std::map<entt::entity, int>& prevPop,
                               entt::entity target) {
    std::string popTag = "[pop " + std::to_string(popCount);
    auto prev = prevPop.find(target);
    if (prev != prevPop.end()) {
        int delta = popCount - prev->second;
        if (delta >= 2)       popTag += " \xe2\x86\x91";   // up arrow
        else if (delta <= -2) popTag += " \xe2\x86\x93";   // down arrow
    }
    popTag += "]";
    return popTag;
}

// Helper: spawn an NPC at a settlement (used by migration and skilled immigrant events).
static entt::entity SpawnNPC(entt::registry& registry, entt::entity target,
                             const Position& tpos, const WorldSchema& schema,
                             std::mt19937& rng, float startGold,
                             float startNeedValue, bool skilled, int aptitudeSkillId) {
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

    float angle = std::uniform_real_distribution<float>(0.f, 6.28f)(rng);
    auto npc = registry.create();
    registry.emplace<Position>(npc, tpos.x + std::cos(angle) * 60.f,
                                    tpos.y + std::sin(angle) * 60.f);
    registry.emplace<Velocity>(npc, 0.f, 0.f);
    registry.emplace<MoveSpeed>(npc, 60.f);
    // Build needs from schema definitions
    Needs npcNeeds;
    for (const auto& nd : schema.needs) {
        // Energy and Heat-like needs (index >= 2) start slightly higher
        float sv = (nd.id >= 2) ? std::min(1.f, startNeedValue + 0.2f) : startNeedValue;
        npcNeeds.list.emplace_back(nd.id, sv, nd.drainRate, nd.criticalThreshold, nd.refillRate);
    }
    // Personality variation: +/-20% drain rates
    std::uniform_real_distribution<float> trait_dist(0.80f, 1.20f);
    for (auto& need : npcNeeds.list) need.drainRate *= trait_dist(rng);
    registry.emplace<Needs>(npc, npcNeeds);
    registry.emplace<AgentState>(npc);
    registry.emplace<HomeSettlement>(npc, HomeSettlement{target});
    DeprivationTimer dt;
    dt.migrateThreshold = std::uniform_real_distribution<float>(
        skilled ? 2.f : 1.f, skilled ? 5.f : 10.f)(rng) * 60.f;
    registry.emplace<DeprivationTimer>(npc, dt);
    registry.emplace<SocialBehavior>(npc);
    registry.emplace<GriefState>(npc);
    registry.emplace<TheftRecord>(npc);
    registry.emplace<CharityState>(npc);
    registry.emplace<BanditState>(npc);
    registry.emplace<PersonalEventState>(npc);
    registry.emplace<Schedule>(npc);
    registry.emplace<Renderable>(npc, WHITE, 6.f);
    registry.emplace<Money>(npc, Money{startGold});
    Age age;
    age.days = std::uniform_real_distribution<float>(skilled ? 20.f : 5.f,
                                                      skilled ? 35.f : 25.f)(rng);
    age.maxDays = std::uniform_real_distribution<float>(60.f, 100.f)(rng);
    registry.emplace<Age>(npc, age);

    std::uniform_int_distribution<int> fd(0, 29), ld(0, 19);
    std::string nm = std::string(FIRSTS[fd(rng)]) + " " + LASTS[ld(rng)];
    registry.emplace<Name>(npc, Name{nm});

    if (skilled) {
        float highSkill = 0.75f + std::uniform_real_distribution<float>(0.f, 0.20f)(rng);
        Skills sk = Skills::Make(schema, 0.35f);
        if (aptitudeSkillId >= 0 && aptitudeSkillId < (int)sk.levels.size())
            sk.levels[aptitudeSkillId] = highSkill;
        registry.emplace<Skills>(npc, std::move(sk));
    } else {
        std::uniform_real_distribution<float> skill_dist(0.30f, 0.70f);
        Skills sk = Skills::Make(schema);
        for (int si = 0; si < (int)schema.skills.size(); ++si)
            sk.levels[si] = std::clamp(schema.skills[si].startValue + (skill_dist(rng) - 0.5f), 0.f, 1.f);
        registry.emplace<Skills>(npc, std::move(sk));
    }

    return npc;
}

void RandomEventSystem::TriggerEvent(entt::registry& registry, int day, int hour, const WorldSchema& schema) {
    auto lv = registry.view<EventLog>();
    EventLog* log = (lv.begin() == lv.end())
                    ? nullptr : &lv.get<EventLog>(*lv.begin());

    // Get current season for seasonal events
    SeasonID seasonId = 0;
    {
        auto tmv = registry.view<TimeManager>();
        if (tmv.begin() != tmv.end())
            seasonId = tmv.get<TimeManager>(*tmv.begin()).CurrentSeason(schema.seasons);
    }
    static const SeasonDef s_defaultSeason{};
    const SeasonDef& seasonDef = (seasonId >= 0 && seasonId < (int)schema.seasons.size())
                                 ? schema.seasons[seasonId] : s_defaultSeason;

    // Collect valid settlements
    std::vector<entt::entity> settlements;
    registry.view<Settlement>().each([&](auto e, const Settlement&) {
        settlements.push_back(e);
    });
    if (settlements.empty()) return;
    if (schema.events.empty()) return;

    std::uniform_int_distribution<int> pickSettl(0, (int)settlements.size() - 1);
    entt::entity target = settlements[pickSettl(m_rng)];
    auto* settl = registry.try_get<Settlement>(target);
    if (!settl) return;

    // Count population at target once
    int popCount = 0;
    registry.view<HomeSettlement>(entt::exclude<PlayerTag, Hauler>).each(
        [&](const HomeSettlement& hs) { if (hs.settlement == target) ++popCount; });

    std::string popTag = BuildPopTag(popCount, m_prevPop, target);

    // Build list of eligible events based on season constraints and min population
    std::vector<int> eligible;
    for (int i = 0; i < (int)schema.events.size(); ++i) {
        const auto& ev = schema.events[i];
        // Check minimum population
        if (popCount < ev.minPopulation) continue;
        // Check season constraints
        if (ev.seasonMinHeatDrain >= 0.f && seasonDef.heatDrainMod < ev.seasonMinHeatDrain) continue;
        if (ev.seasonMaxHeatDrain < 999.0f && seasonDef.heatDrainMod > ev.seasonMaxHeatDrain) continue;
        if (ev.seasonMinTemp > -999.0f && seasonDef.baseTemperature < ev.seasonMinTemp) continue;
        if (ev.seasonMinProdMod >= 0.f && seasonDef.productionMod < ev.seasonMinProdMod) continue;
        eligible.push_back(i);
    }
    if (eligible.empty()) return;

    // Pick a random eligible event using weighted selection based on ev.chance
    float totalWeight = 0.f;
    for (int idx : eligible)
        totalWeight += schema.events[idx].chance;
    if (totalWeight <= 0.f) return;  // no valid weights; skip to avoid UB in uniform_real_distribution(0,0)
    std::uniform_real_distribution<float> weightDist(0.f, totalWeight);
    float roll = weightDist(m_rng);
    int pickedIdx = eligible.back();  // fallback to last
    float cumulative = 0.f;
    for (int idx : eligible) {
        cumulative += schema.events[idx].chance;
        if (roll <= cumulative) { pickedIdx = idx; break; }
    }
    const EventDef& ev = schema.events[pickedIdx];

    // ---- Apply event effects based on effectType enum (resolved at load time) ----

    switch (ev.effectEnum) {
    case EventEffectType::ProductionModifier: {
        // Duration-based production modifier requires no existing modifier
        if (ev.durationHours > 0.f) {
            if (settl->modifierDuration > 0.f) return;  // already has an active event
            // Plague-specific: check not already infected
            if (ev.spreads && m_plagueSpreadTimer.count(target)) return;

            settl->productionModifier = ev.effectValue;
            settl->modifierDuration   = ev.durationHours;
            settl->modifierName       = ev.displayName;

            // Plague spreading setup
            if (ev.spreads)
                m_plagueSpreadTimer[target] = ev.spreadInterval;
        }

        // Kill fraction (Plague)
        int killed = 0;
        if (ev.killFraction > 0.f)
            killed = KillFraction(registry, target, ev.killFraction);

        // Morale impact
        if (ev.moraleImpact != 0.f)
            settl->morale = std::clamp(settl->morale + ev.moraleImpact, 0.f, 1.f);

        // Destroy a specific resource by fraction (e.g. HeatWave destroys water)
        if (ev.destroyResourceId != INVALID_ID && ev.destroyFraction > 0.f) {
            auto* sp = registry.try_get<Stockpile>(target);
            if (sp) {
                auto it = sp->quantities.find(ev.destroyResourceId);
                if (it != sp->quantities.end() && it->second > 5.f) {
                    float lost = it->second * ev.destroyFraction;
                    it->second -= lost;
                }
            }
        }

        // Festival: treasury boost and celebration
        if (ev.treasuryChange != 0.f)
            settl->treasury += ev.treasuryChange;

        int celebrantCount = 0;
        if (ev.triggersCelebration) {
            registry.view<AgentState, HomeSettlement>(
                entt::exclude<PlayerTag, Hauler>).each(
                [&](AgentState& as, const HomeSettlement& hs) {
                    if (hs.settlement == target) {
                        as.behavior = AgentBehavior::Celebrating;
                        ++celebrantCount;
                    }
                });
        }

        // Rumour seeding
        SeedRumours(registry, target, ev.rumourType, ev.rumourSeeds, m_rng);

        // Crisis solidarity
        if (ev.moraleImpact < 0.f)
            SoftenRivalryOnSharedCrisis(registry, target, *settl, ev.displayName, log, day, hour);

        if (ev.triggersSolidarity)
            ApplySolidarity(registry, target, ev.solidarityBoost);

        // Log
        if (log) {
            char buf[200];
            if (ev.spreads) {
                std::snprintf(buf, sizeof(buf), "%s %s -- %d died, disease spreading via roads!",
                    ev.displayName.c_str(), popTag.c_str(), killed);
                log->Push(day, hour, settl->name + ": " + buf);
            } else if (ev.triggersCelebration) {
                std::snprintf(buf, sizeof(buf),
                    "%s at %s %s -- %d celebrating, treasury +%.0fg, production x%.2f (%dh)",
                    ev.displayName.c_str(), settl->name.c_str(), popTag.c_str(),
                    celebrantCount, ev.treasuryChange, ev.effectValue, (int)ev.durationHours);
                log->Push(day, hour, buf);
            } else {
                std::snprintf(buf, sizeof(buf), "%s at %s %s (%dh)",
                    ev.displayName.c_str(), settl->name.c_str(), popTag.c_str(),
                    (int)ev.durationHours);
                log->Push(day, hour, buf);
            }
            if (ev.triggersSolidarity && !ev.spreads) {
                log->Push(day, hour, settl->name + " residents pull together during the "
                    + ev.displayName + ".");
            }
        }
        break;
    }
    // --- Stockpile destroy events (Blight, Spring Flood) ---
    case EventEffectType::StockpileDestroy: {
        auto* sp = registry.try_get<Stockpile>(target);
        if (!sp) return;
        int resId = (ev.targetResourceId != INVALID_ID) ? ev.targetResourceId : RES_FOOD;
        auto it = sp->quantities.find(resId);
        if (it == sp->quantities.end() || it->second < 5.f) return;
        float lost = it->second * ev.effectValue;
        it->second -= lost;

        if (ev.moraleImpact != 0.f)
            settl->morale = std::clamp(settl->morale + ev.moraleImpact, 0.f, 1.f);

        if (ev.triggersSolidarity)
            ApplySolidarity(registry, target, ev.solidarityBoost);

        if (log) {
            std::string resName = (ev.targetResourceId != INVALID_ID
                && ev.targetResourceId < (int)schema.resources.size())
                ? schema.resources[ev.targetResourceId].displayName : "food";
            log->Push(day, hour,
                ev.displayName + " hits " + settl->name + " " + popTag + " -- "
                + std::to_string((int)lost) + " " + resName + " destroyed");
            if (ev.triggersSolidarity)
                log->Push(day, hour, settl->name + " residents share what little remains.");
        }
        break;
    }
    // --- Road block events (Bandits, Blizzard) ---
    case EventEffectType::RoadBlock: {
        float blockDur = ev.roadBlockDuration > 0.f ? ev.roadBlockDuration : BANDIT_DURATION;
        if (ev.blockAllRoads) {
            int blockedCount = 0;
            registry.view<Road>().each([&](Road& road) {
                if (!road.blocked) {
                    road.blocked     = true;
                    road.banditTimer = blockDur;
                    ++blockedCount;
                }
            });
            if (blockedCount > 0 && log)
                log->Push(day, hour,
                    ev.displayName + " -- all " + std::to_string(blockedCount)
                    + " roads blocked (" + std::to_string((int)blockDur) + "h)");
        } else {
            // Block one random open road
            std::vector<entt::entity> openRoads;
            registry.view<Road>().each([&](auto e, const Road& road) {
                if (!road.blocked && road.banditTimer <= 0.f)
                    openRoads.push_back(e);
            });
            if (openRoads.empty()) return;
            std::uniform_int_distribution<int> pickRoad(0, (int)openRoads.size() - 1);
            auto& road = registry.get<Road>(openRoads[pickRoad(m_rng)]);
            road.blocked     = true;
            road.banditTimer = blockDur;
            if (log) log->Push(day, hour,
                ev.displayName + " blocking road ("
                + std::to_string((int)blockDur) + "h)");
        }
        break;
    }
    // --- Treasury boost (Trade Boom) ---
    case EventEffectType::TreasuryBoost: {
        float gold = ev.treasuryChange > 0.f ? ev.treasuryChange : ev.effectValue;
        settl->treasury += gold;
        if (log) log->Push(day, hour,
            ev.displayName + " at " + settl->name + " " + popTag
            + " -- treasury +" + std::to_string((int)gold) + "g");
        break;
    }
    // --- Spawn NPCs (Migration Wave, Skilled Immigrant) ---
    case EventEffectType::SpawnNpcs: {
        const auto* tpos = registry.try_get<Position>(target);
        if (!tpos) return;

        int curPop = 0;
        registry.view<HomeSettlement>(entt::exclude<PlayerTag, Hauler>).each(
            [&](const HomeSettlement& hs) { if (hs.settlement == target) ++curPop; });
        int slots = settl->popCap - curPop;
        if (slots <= 0) return;

        if (ev.spawnSkilled) {
            // Skilled immigrant: spawn one high-skill NPC
            int nSkills = (int)schema.skills.size();
            std::uniform_int_distribution<int> apt(0, std::max(0, nSkills - 1));
            int aptIdx = apt(m_rng);
            auto npc = SpawnNPC(registry, target, *tpos, schema, m_rng,
                                30.f, 1.0f, true, aptIdx);

            float highSkill = 0.f;
            if (auto* sk = registry.try_get<Skills>(npc))
                if (aptIdx >= 0 && aptIdx < (int)sk->levels.size())
                    highSkill = sk->levels[aptIdx];
            const char* specialty = (aptIdx >= 0 && aptIdx < (int)schema.skills.size())
                ? schema.skills[aptIdx].displayName.c_str() : "Worker";
            const auto* nm = registry.try_get<Name>(npc);

            if (log) {
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                    "%s: %s (%s) arrives at %s %s -- skill %.0f%%",
                    ev.displayName.c_str(),
                    nm ? nm->value.c_str() : "Unknown",
                    specialty, settl->name.c_str(),
                    BuildPopTag(popCount + 1, m_prevPop, target).c_str(),
                    highSkill * 100.f);
                log->Push(day, hour, buf);
            }
        } else {
            // Migration wave: spawn 3-5 NPCs
            int maxArrivals = (int)ev.effectValue;
            std::uniform_int_distribution<int> count_dist(
                std::max(1, maxArrivals - 2), maxArrivals);
            int arrivals = std::min(slots, count_dist(m_rng));
            for (int i = 0; i < arrivals; ++i)
                SpawnNPC(registry, target, *tpos, schema, m_rng,
                         5.f, 0.6f, false, INVALID_ID);

            if (log) log->Push(day, hour,
                ev.displayName + ": " + std::to_string(arrivals)
                + " arrived at " + settl->name + " "
                + BuildPopTag(popCount + arrivals, m_prevPop, target));
        }
        break;
    }
    // --- Stockpile add (Rainstorm, Lumber Windfall) ---
    case EventEffectType::StockpileAdd: {
        int addResId = (ev.addResourceId != INVALID_ID) ? ev.addResourceId : RES_WATER;
        float amount = ev.addAmount;
        // Lumber windfall: randomise amount slightly
        if (amount > 0.f && !ev.affectsAllSettlements) {
            float variation = amount * 0.25f;
            amount = std::uniform_real_distribution<float>(
                amount - variation, amount + variation)(m_rng);
        }

        if (ev.affectsAllSettlements) {
            registry.view<Settlement, Stockpile>().each(
                [&](auto e, Settlement& rs, Stockpile& rsp) {
                    rsp.quantities[addResId] += amount;
                    // Break drought at target
                    if (ev.breaksDrought && e == target && rs.modifierDuration > 0.f &&
                        rs.modifierName.find("Drought") != std::string::npos) {
                        rs.modifierDuration   = 0.f;
                        rs.productionModifier = 1.f;
                        rs.modifierName.clear();
                    }
                });
            if (log) {
                std::string resName = (ev.addResourceId != INVALID_ID
                    && ev.addResourceId < (int)schema.resources.size())
                    ? schema.resources[ev.addResourceId].name : "resource";
                log->Push(day, hour,
                    ev.displayName + " -- all settlements +"
                    + std::to_string((int)amount) + " " + resName);
            }
        } else {
            auto* sp = registry.try_get<Stockpile>(target);
            if (!sp) return;
            sp->quantities[addResId] += amount;
            if (log) {
                std::string resName = (ev.addResourceId != INVALID_ID
                    && ev.addResourceId < (int)schema.resources.size())
                    ? schema.resources[ev.addResourceId].name : "resource";
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                    "%s at %s %s -- +%.0f %s",
                    ev.displayName.c_str(), settl->name.c_str(), popTag.c_str(),
                    amount, resName.c_str());
                log->Push(day, hour, buf);
            }
        }
        break;
    }
    // --- Convoy (Off-Map Trade) ---
    case EventEffectType::Convoy: {
        auto* mkt  = registry.try_get<Market>(target);
        auto* sp   = registry.try_get<Stockpile>(target);
        if (!mkt || !sp) return;

        // Find the most scarce (highest-priced) resource
        int scarcest = RES_FOOD;
        float highestPrice = 0.f;
        for (const auto& [res, price] : mkt->price)
            if (price > highestPrice) { highestPrice = price; scarcest = res; }

        float minPrice = ev.convoyMinPrice > 0.f ? ev.convoyMinPrice : 5.f;
        if (highestPrice < minPrice) return;

        float amount = ev.convoyAmount > 0.f ? ev.convoyAmount : 50.f;
        float cost = highestPrice * amount;

        if (settl->treasury < cost) {
            if (log) log->Push(day, hour,
                "Convoy turned away from " + settl->name
                + " -- treasury too low (" + std::to_string((int)settl->treasury) + "g)");
            return;
        }

        settl->treasury -= cost;
        sp->quantities[scarcest] += amount;

        std::string resName = (scarcest >= 0 && scarcest < (int)schema.resources.size())
            ? schema.resources[scarcest].name : "goods";
        if (log) log->Push(day, hour,
            ev.displayName + " at " + settl->name + " " + popTag + " +"
            + std::to_string((int)amount) + " " + resName
            + " (paid " + std::to_string((int)cost) + "g)");
        break;
    }
    // --- Earthquake ---
    case EventEffectType::Earthquake: {
        // Block connected roads and damage their condition
        float blockDur = ev.roadBlockDuration > 0.f ? ev.roadBlockDuration : 6.f;
        int blockedRoads = 0;
        registry.view<Road>().each([&](Road& road) {
            bool connected = (road.from == target || road.to == target);
            if (!connected) return;
            if (!road.blocked) {
                road.blocked     = true;
                road.banditTimer = blockDur;
                ++blockedRoads;
            }
            if (ev.roadDamage > 0.f)
                road.condition = std::max(0.f, road.condition - ev.roadDamage);
        });

        // Destroy a fraction of this settlement's facilities
        float destroyChance = ev.facilityDestroyChance > 0.f ? ev.facilityDestroyChance : 0.30f;
        std::vector<entt::entity> settlFacs;
        registry.view<ProductionFacility>().each(
            [&](auto fe, const ProductionFacility& fac) {
                if (fac.settlement == target && fac.baseRate > 0.f)
                    settlFacs.push_back(fe);
            });
        std::uniform_real_distribution<float> chance2(0.f, 1.f);
        int destroyed = 0;
        for (auto fe : settlFacs) {
            if (chance2(m_rng) < destroyChance) {
                registry.destroy(fe);
                ++destroyed;
            }
        }

        if (log) {
            char buf[180];
            std::snprintf(buf, sizeof(buf),
                "%s at %s %s -- %d facilit%s destroyed, roads blocked (%dh)",
                ev.displayName.c_str(), settl->name.c_str(), popTag.c_str(),
                destroyed, destroyed == 1 ? "y" : "ies", (int)blockDur);
            log->Push(day, hour, buf);
        }
        break;
    }
    // --- Fire ---
    case EventEffectType::Fire: {
        auto* sp = registry.try_get<Stockpile>(target);
        if (!sp) return;

        // Destroy resources specified in the destroyResources vector
        std::string lostDesc;
        for (const auto& [resId, baseFrac] : ev.destroyResources) {
            float frac = baseFrac + std::uniform_real_distribution<float>(-0.10f, 0.10f)(m_rng);
            frac = std::clamp(frac, 0.f, 1.f);
            auto fit = sp->quantities.find(resId);
            if (fit != sp->quantities.end() && fit->second > 5.f) {
                float lost = fit->second * frac;
                fit->second -= lost;
                if (!lostDesc.empty()) lostDesc += ", ";
                std::string resName = (resId >= 0 && resId < (int)schema.resources.size())
                    ? schema.resources[resId].name : "resource";
                lostDesc += std::to_string((int)lost) + " " + resName;
            }
        }

        int killed = 0;
        if (ev.killFraction > 0.f)
            killed = KillFraction(registry, target, ev.killFraction);

        if (log) {
            char buf[200];
            std::snprintf(buf, sizeof(buf),
                "%s at %s %s -- %s destroyed, %d died",
                ev.displayName.c_str(), settl->name.c_str(), popTag.c_str(),
                lostDesc.empty() ? "nothing" : lostDesc.c_str(), killed);
            log->Push(day, hour, buf);
        }
        break;
    }
    // --- Price spike (Market Crisis) ---
    case EventEffectType::PriceSpike: {
        auto* mkt = registry.try_get<Market>(target);
        if (!mkt) return;

        // Only trigger if prices are currently reasonable
        bool alreadySpiked = false;
        for (const auto& [rt, p] : mkt->price)
            if (p > 15.f) { alreadySpiked = true; break; }
        if (alreadySpiked) return;

        float spikeBase = ev.priceSpikeMultiplier > 0.f ? ev.priceSpikeMultiplier : 3.0f;
        float actualSpike = std::uniform_real_distribution<float>(
            std::max(1.0f, spikeBase - 0.5f), spikeBase + 0.5f)(m_rng);
        for (auto& [rt, p] : mkt->price)
            p = std::min(20.f, p * actualSpike);

        if (log) {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "%s at %s %s -- panic buying, all prices spike %.1fx!",
                ev.displayName.c_str(), settl->name.c_str(), popTag.c_str(), actualSpike);
            log->Push(day, hour, buf);
        }
        break;
    }
    // --- Morale boost (standalone, no production modifier) ---
    case EventEffectType::MoraleBoost: {
        if (ev.moraleImpact != 0.f)
            settl->morale = std::clamp(settl->morale + ev.moraleImpact, 0.f, 1.f);
        if (ev.treasuryChange != 0.f)
            settl->treasury += ev.treasuryChange;
        if (log) log->Push(day, hour,
            ev.displayName + " at " + settl->name + " " + popTag);
        break;
    }
    case EventEffectType::None:
        break;
    } // end switch
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
