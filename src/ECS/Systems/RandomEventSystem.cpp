#include "RandomEventSystem.h"
#include "ECS/Components.h"
#include <algorithm>
#include <set>
#include <vector>
#include <string>

static constexpr float DROUGHT_MODIFIER  = 0.2f;   // production factor during drought
static constexpr float DROUGHT_DURATION  = 8.f;    // game-hours
static constexpr float BLIGHT_FRACTION   = 0.35f;  // fraction of food stockpile destroyed
static constexpr float BANDIT_DURATION   = 3.f;    // game-hours road is blocked
static constexpr float EVENT_MEAN_HOURS  = 72.f;   // ~3 game-days between events
static constexpr float EVENT_JITTER      = 36.f;   // ±jitter in game-hours

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
                        if (auto* dt = registry.try_get<DeprivationTimer>(npc)) {
                            dt->strikeDuration = STRIKE_DURATION_HOURS;
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
                "PLAGUE spreads from %s to %s [pop %d%s] — %d died",
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

            registry.view<Settlement>().each([&](auto e, Settlement& s) {
                if (s.modifierDuration > 0.f) return;  // already has an event

                // Compute profession diversity bitmask for this settlement
                int profMask = 0;
                registry.view<Profession, HomeSettlement>(
                    entt::exclude<Hauler, PlayerTag, BanditTag, ChildTag>).each(
                    [&](const Profession& prof, const HomeSettlement& hs) {
                        if (hs.settlement != e) return;
                        switch (prof.type) {
                            case ProfessionType::Farmer:      profMask |= 1; break;
                            case ProfessionType::WaterCarrier: profMask |= 2; break;
                            case ProfessionType::Lumberjack:   profMask |= 4; break;
                            default: break;
                        }
                    });
                if ((profMask & 7) != 7) return;  // not all 3 professions present

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
                        if (sk.farming < 0.7f && sk.water_drawing < 0.7f && sk.woodcutting < 0.7f) return;
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
    std::uniform_int_distribution<int>    skillIdxDist(0, 2);

    registry.view<DeprivationTimer, Skills, Money, Name>(
        entt::exclude<PlayerTag, BanditTag>)
        .each([&](auto e, DeprivationTimer& dt, Skills& skills, Money& money, const Name& name) {
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
                    int idx = skillIdxDist(m_rng);
                    float gain = skillGain_dist(m_rng);
                    int rt = (idx == 0) ? RES_FOOD :
                                      (idx == 1) ? RES_WATER : RES_WOOD;
                    skills.Advance(rt, gain);
                    if (log) {
                        const char* skillName = (idx == 0) ? "farming" :
                                                (idx == 1) ? "water drawing" : "woodcutting";
                        const char* settName = "the wilds";
                        if (const auto* hs = registry.try_get<HomeSettlement>(e))
                            if (hs->settlement != entt::null && registry.valid(hs->settlement))
                                if (const auto* stt = registry.try_get<Settlement>(hs->settlement))
                                    settName = stt->name.c_str();
                        char buf[120];
                        std::snprintf(buf, sizeof(buf), "%s had a skill insight in %s at %s",
                            name.value.c_str(), skillName, settName);
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
            if (!dt.wisdomFired) {
                const auto* age = registry.try_get<Age>(e);
                if (age && age->days > 70.f) {
                    float bestSkill = std::max({skills.farming, skills.water_drawing, skills.woodcutting});
                    if (bestSkill >= 0.6f) {
                        // Find the best skill type to transfer
                        int bestType = RES_FOOD;
                        if (skills.water_drawing >= skills.farming && skills.water_drawing >= skills.woodcutting)
                            bestType = RES_WATER;
                        else if (skills.woodcutting >= skills.farming)
                            bestType = RES_WOOD;

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
                                if (tSkills) {
                                    float& tVal = (bestType == RES_FOOD) ? tSkills->farming :
                                                  (bestType == RES_WATER) ? tSkills->water_drawing :
                                                  tSkills->woodcutting;
                                    tVal = std::min(0.8f, tVal + 0.1f);
                                    dt.wisdomFired = true;

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

void RandomEventSystem::TriggerEvent(entt::registry& registry, int day, int hour, const WorldSchema& schema) {
    auto lv = registry.view<EventLog>();
    EventLog* log = (lv.begin() == lv.end())
                    ? nullptr : &lv.get<EventLog>(*lv.begin());

    // Get current season for seasonal events
    SeasonID seasonId = 0;
    float    seasonHeatMod = 0.f;
    float    seasonProdMod = 1.f;
    float    seasonTemp    = 20.f;
    std::string seasonName = "Spring";
    {
        auto tmv = registry.view<TimeManager>();
        if (tmv.begin() != tmv.end()) {
            seasonId = tmv.get<TimeManager>(*tmv.begin()).CurrentSeason();
            seasonHeatMod = GetSeasonHeatDrainMod(seasonId, schema);
            seasonProdMod = GetSeasonProductionMod(seasonId, schema);
            seasonTemp    = (seasonId >= 0 && seasonId < (int)schema.seasons.size())
                            ? schema.seasons[seasonId].baseTemperature : 20.f;
            seasonName    = GetSeasonName(seasonId, schema);
        }
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

    // Count population at target once; appended as [pop N] in all settlement-specific log lines.
    int popCount = 0;
    registry.view<HomeSettlement>(entt::exclude<PlayerTag, Hauler>).each(
        [&](const HomeSettlement& hs) { if (hs.settlement == target) ++popCount; });

    // Build a pop tag string with optional trend indicator
    // Trend is (↑) when pop grew by ≥2, (↓) when pop fell by ≥2 since last sample
    std::string popTag = "[pop " + std::to_string(popCount);
    {
        auto prev = m_prevPop.find(target);
        if (prev != m_prevPop.end()) {
            int delta = popCount - prev->second;
            if (delta >= 2)       popTag += " \xe2\x86\x91";   // ↑
            else if (delta <= -2) popTag += " \xe2\x86\x93";   // ↓
        }
        popTag += "]";
    }

    switch (pickType(m_rng)) {

    case 0: {   // Drought — cripple production
        if (settl->modifierDuration > 0.f) break;   // already has an event
        settl->productionModifier = DROUGHT_MODIFIER;
        settl->modifierDuration   = DROUGHT_DURATION;
        settl->modifierName       = "Drought";
        settl->morale = std::max(0.f, settl->morale - 0.10f);  // drought demoralises
        if (log) log->Push(day, hour,
            "DROUGHT strikes " + settl->name + " ("
            + std::to_string((int)DROUGHT_DURATION) + "h) " + popTag);
        // Seed rumour into up to 2 NPCs at this settlement
        {
            std::vector<entt::entity> residents;
            registry.view<HomeSettlement>(entt::exclude<Hauler, PlayerTag>).each(
                [&](auto e, const HomeSettlement& hs) {
                    if (hs.settlement == target && !registry.any_of<Rumour>(e))
                        residents.push_back(e);
                });
            std::shuffle(residents.begin(), residents.end(), m_rng);
            int seedCount = std::min((int)residents.size(), 2);
            for (int k = 0; k < seedCount; ++k)
                registry.emplace<Rumour>(residents[k], Rumour{RumourType::DroughtNearby, target, 3});
        }
        SoftenRivalryOnSharedCrisis(registry, target, *settl, "Drought", log, day, hour);
        // Drought solidarity: residents with existing bonds pull together
        {
            std::vector<entt::entity> droughtResidents;
            registry.view<HomeSettlement, Relations>(
                entt::exclude<Hauler, PlayerTag, BanditTag>).each(
                [&](auto e, const HomeSettlement& hs, const Relations&) {
                    if (hs.settlement == target) droughtResidents.push_back(e);
                });
            for (size_t i = 0; i < droughtResidents.size(); ++i) {
                auto* relA = registry.try_get<Relations>(droughtResidents[i]);
                if (!relA) continue;
                for (size_t j = i + 1; j < droughtResidents.size(); ++j) {
                    auto itA = relA->affinity.find(droughtResidents[j]);
                    if (itA == relA->affinity.end() || itA->second < 0.3f) continue;
                    auto* relB = registry.try_get<Relations>(droughtResidents[j]);
                    if (!relB) continue;
                    auto itB = relB->affinity.find(droughtResidents[i]);
                    if (itB == relB->affinity.end() || itB->second < 0.3f) continue;
                    // Boost mutual affinity by +0.03 (cap 1.0)
                    itA->second = std::min(1.f, itA->second + 0.03f);
                    itB->second = std::min(1.f, itB->second + 0.03f);
                }
            }
            if (log)
                log->Push(day, hour, settl->name + " residents pull together during the drought.");
        }
        break;
    }

    case 1: {   // Blight — destroy food stockpile
        auto* sp = registry.try_get<Stockpile>(target);
        if (!sp) break;
        auto it = sp->quantities.find(RES_FOOD);
        if (it == sp->quantities.end() || it->second < 5.f) break;
        float lost = it->second * BLIGHT_FRACTION;
        it->second -= lost;
        settl->morale = std::max(0.f, settl->morale - 0.12f);  // food loss is demoralising
        if (log) log->Push(day, hour,
            "BLIGHT hits " + settl->name + " " + popTag + " — "
            + std::to_string((int)lost) + " food destroyed");
        // Blight solidarity: residents with existing bonds share what little remains
        {
            std::vector<entt::entity> blightResidents;
            registry.view<HomeSettlement, Relations>(
                entt::exclude<Hauler, PlayerTag, BanditTag>).each(
                [&](auto e, const HomeSettlement& hs, const Relations&) {
                    if (hs.settlement == target) blightResidents.push_back(e);
                });
            for (size_t i = 0; i < blightResidents.size(); ++i) {
                auto* relA = registry.try_get<Relations>(blightResidents[i]);
                if (!relA) continue;
                for (size_t j = i + 1; j < blightResidents.size(); ++j) {
                    auto itA = relA->affinity.find(blightResidents[j]);
                    if (itA == relA->affinity.end() || itA->second < 0.3f) continue;
                    auto* relB = registry.try_get<Relations>(blightResidents[j]);
                    if (!relB) continue;
                    auto itB = relB->affinity.find(blightResidents[i]);
                    if (itB == relB->affinity.end() || itB->second < 0.3f) continue;
                    // Boost mutual affinity by +0.02 (cap 1.0) — smaller than drought/plague
                    itA->second = std::min(1.f, itA->second + 0.02f);
                    itB->second = std::min(1.f, itB->second + 0.02f);
                }
            }
            if (log)
                log->Push(day, hour, settl->name + " residents share what little food remains.");
        }
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
                "PLAGUE erupts at %s %s — %d died, disease spreading via roads!",
                settl->name.c_str(), popTag.c_str(), killCount);
            log->Push(day, hour, buf);
        }
        // Seed rumour into up to 2 NPCs at this settlement
        {
            std::vector<entt::entity> residents;
            registry.view<HomeSettlement>(entt::exclude<Hauler, PlayerTag>).each(
                [&](auto e, const HomeSettlement& hs) {
                    if (hs.settlement == target && !registry.any_of<Rumour>(e))
                        residents.push_back(e);
                });
            std::shuffle(residents.begin(), residents.end(), m_rng);
            int seedCount = std::min((int)residents.size(), 2);
            for (int k = 0; k < seedCount; ++k)
                registry.emplace<Rumour>(residents[k], Rumour{RumourType::PlagueNearby, target, 3});
        }
        SoftenRivalryOnSharedCrisis(registry, target, *settl, "Plague", log, day, hour);
        // Plague solidarity: residents with existing bonds support each other
        {
            std::vector<entt::entity> plagueResidents;
            registry.view<HomeSettlement, Relations>(
                entt::exclude<Hauler, PlayerTag, BanditTag>).each(
                [&](auto e, const HomeSettlement& hs, const Relations&) {
                    if (hs.settlement == target) plagueResidents.push_back(e);
                });
            for (size_t i = 0; i < plagueResidents.size(); ++i) {
                auto* relA = registry.try_get<Relations>(plagueResidents[i]);
                if (!relA) continue;
                for (size_t j = i + 1; j < plagueResidents.size(); ++j) {
                    auto itA = relA->affinity.find(plagueResidents[j]);
                    if (itA == relA->affinity.end() || itA->second < 0.3f) continue;
                    auto* relB = registry.try_get<Relations>(plagueResidents[j]);
                    if (!relB) continue;
                    auto itB = relB->affinity.find(plagueResidents[i]);
                    if (itB == relB->affinity.end() || itB->second < 0.3f) continue;
                    itA->second = std::min(1.f, itA->second + 0.03f);
                    itB->second = std::min(1.f, itB->second + 0.03f);
                }
            }
            if (log)
                log->Push(day, hour, settl->name + " residents support each other through the plague.");
        }
        break;
    }

    case 4: {   // Trade boom — inject gold into settlement treasury
        static constexpr float BOOM_GOLD = 150.f;
        settl->treasury += BOOM_GOLD;
        if (log) log->Push(day, hour,
            "TRADE BOOM at " + settl->name + " " + popTag
            + " — treasury +" + std::to_string((int)BOOM_GOLD) + "g");
        break;
    }

    case 6: {   // Blizzard (Winter only) — blocks ALL roads for 4 game-hours
        if (seasonHeatMod < 0.8f) break;  // Blizzard only in cold seasons
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
        if (seasonName != "Spring") break;  // Spring flood only in spring
        auto* sp = registry.try_get<Stockpile>(target);
        if (!sp) break;
        auto it = sp->quantities.find(RES_FOOD);
        if (it == sp->quantities.end() || it->second < 5.f) break;
        float lost = it->second * 0.40f;
        it->second -= lost;
        if (log) log->Push(day, hour,
            "SPRING FLOOD at " + settl->name + " " + popTag + " — "
            + std::to_string((int)lost) + " food washed away");
        break;
    }

    case 8: {   // Harvest bounty (Autumn only) — production boost for 12 game-hours
        if (seasonProdMod < 1.1f) break;  // Harvest bounty only in high-production seasons
        if (settl->modifierDuration > 0.f) break;   // already has an event
        static constexpr float BOUNTY_MODIFIER  = 1.5f;
        static constexpr float BOUNTY_DURATION  = 12.f;
        settl->productionModifier = BOUNTY_MODIFIER;
        settl->modifierDuration   = BOUNTY_DURATION;
        settl->modifierName       = "Harvest Bounty";
        if (log) log->Push(day, hour,
            "HARVEST BOUNTY at " + settl->name + " " + popTag
            + " (+50% production, " + std::to_string((int)BOUNTY_DURATION) + "h)");
        // Seed GoodHarvest rumour on up to 2 NPCs at this settlement
        {
            std::vector<entt::entity> residents;
            registry.view<HomeSettlement>(entt::exclude<PlayerTag, Hauler>).each(
                [&](auto e, const HomeSettlement& hs) {
                    if (hs.settlement == target && !registry.any_of<Rumour>(e))
                        residents.push_back(e);
                });
            std::shuffle(residents.begin(), residents.end(), m_rng);
            int seedCount = std::min((int)residents.size(), 2);
            for (int k = 0; k < seedCount; ++k)
                registry.emplace<Rumour>(residents[k], Rumour{RumourType::GoodHarvest, target, 3});
        }
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
            Needs npcNeeds{ {
                Need{NeedType::Hunger, 0.6f, DRAIN_HUNGER, 0.3f, REFILL_H},
                Need{NeedType::Thirst, 0.6f, DRAIN_THIRST, 0.3f, REFILL_T},
                Need{NeedType::Energy, 0.8f, DRAIN_ENERGY, 0.3f, REFILL_E},
                Need{NeedType::Heat,   0.8f, DRAIN_HEAT,   0.3f, REFILL_HEAT}
            } };
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
            "MIGRATION WAVE: " + std::to_string(arrivals) + " arrived at " + settl->name
            + " [pop " + std::to_string(popCount + arrivals) + ([&]{
                auto prev = m_prevPop.find(target);
                if (prev != m_prevPop.end()) {
                    int d = (popCount + arrivals) - prev->second;
                    if (d >= 2) return " \xe2\x86\x91]";
                    if (d <= -2) return " \xe2\x86\x93]";
                }
                return "]";
            }()));
        break;
    }

    case 10: {  // Rainstorm — all settlements gain water; drought at target ends early
        // Rain doesn't discriminate — all settlements collect water.
        // Bonus: if the target settlement is in drought, the rain breaks it.
        static constexpr float RAIN_WATER = 25.f;   // water added to every settlement
        registry.view<Settlement, Stockpile>().each(
            [&](auto e, Settlement& rs, Stockpile& rsp) {
            rsp.quantities[RES_WATER] += RAIN_WATER;
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
        int scarcest    = RES_FOOD;
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

        const char* resName = (scarcest == RES_FOOD)  ? "food"  :
                              (scarcest == RES_WATER) ? "water" : "wood";
        if (log) log->Push(day, hour,
            "OFF-MAP CONVOY at " + settl->name + " " + popTag + " +"
            + std::to_string((int)CONVOY_AMOUNT)
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
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "FESTIVAL at %s %s — %d celebrating, treasury +%.0fg, production +35%% (%dh)",
                settl->name.c_str(), popTag.c_str(), celebrantCount, FESTIVAL_GOLD, (int)FESTIVAL_DURATION);
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
        auto fitFood = sp->quantities.find(RES_FOOD);
        if (fitFood != sp->quantities.end() && fitFood->second > 5.f) {
            foodLost = fitFood->second * fraction;
            fitFood->second -= foodLost;
        }
        auto fitWood = sp->quantities.find(RES_WOOD);
        if (fitWood != sp->quantities.end() && fitWood->second > 5.f) {
            woodLost = fitWood->second * fraction;
            fitWood->second -= woodLost;
        }

        int killed = KillFraction(registry, target, 0.05f);

        if (log) {
            char buf[180];
            std::snprintf(buf, sizeof(buf),
                "FIRE at %s %s — %.0f food, %.0f wood destroyed, %d died",
                settl->name.c_str(), popTag.c_str(), foodLost, woodLost, killed);
            log->Push(day, hour, buf);
        }
        break;
    }

    case 14: {  // Heat wave (Summer only) — production penalty + water shortage stress
        if (seasonTemp < 25.f) break;  // Heat wave only in hot seasons
        if (settl->modifierDuration > 0.f) break;   // already affected

        static constexpr float HEATWAVE_MODIFIER = 0.75f;
        static constexpr float HEATWAVE_DURATION = 10.f;   // game-hours

        settl->productionModifier = HEATWAVE_MODIFIER;
        settl->modifierDuration   = HEATWAVE_DURATION;
        settl->modifierName       = "Heat Wave";

        // Destroy 20-35% of water stockpile (evaporation / dehydration)
        auto* sp = registry.try_get<Stockpile>(target);
        if (sp) {
            auto it = sp->quantities.find(RES_WATER);
            if (it != sp->quantities.end() && it->second > 5.f) {
                std::uniform_real_distribution<float> evap(0.20f, 0.35f);
                float lost = it->second * evap(m_rng);
                it->second -= lost;
            }
        }

        if (log) {
            char buf[140];
            std::snprintf(buf, sizeof(buf),
                "HEAT WAVE strikes %s %s — production -25%%, water reserves drained (%dh)",
                settl->name.c_str(), popTag.c_str(), (int)HEATWAVE_DURATION);
            log->Push(day, hour, buf);
        }
        break;
    }

    case 15: {  // Lumber windfall (Spring/Autumn) — storm brings easy wood
        // Lumber windfall occurs in moderate seasons (not extreme heat or cold)
        if (seasonHeatMod >= 0.8f || (seasonHeatMod <= 0.01f && seasonProdMod >= 0.9f)) break;
        auto* sp = registry.try_get<Stockpile>(target);
        if (!sp) break;

        std::uniform_real_distribution<float> lumberAmt(50.f, 80.f);
        float windfall = lumberAmt(m_rng);
        sp->quantities[RES_WOOD] += windfall;

        if (log) {
            char buf[140];
            std::snprintf(buf, sizeof(buf),
                "LUMBER WINDFALL at %s %s — storm felled trees, +%.0f wood",
                settl->name.c_str(), popTag.c_str(), windfall);
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
        registry.emplace<Needs>(npc16, Needs{ {
            Need{NeedType::Hunger, 1.f, DRAIN_HUNGER, CRIT_THRESH, 0.004f},
            Need{NeedType::Thirst, 1.f, DRAIN_THIRST, CRIT_THRESH, 0.006f},
            Need{NeedType::Energy, 1.f, DRAIN_ENERGY, CRIT_THRESH, 0.002f},
            Need{NeedType::Heat,   1.f, DRAIN_HEAT,   CRIT_THRESH, 0.010f},
        } });
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
            char buf[140];
            std::snprintf(buf, sizeof(buf),
                "SKILLED IMMIGRANT: %s (%s) arrives at %s [pop %d%s] — skill %.0f%%",
                immName.c_str(), specialty16, settl->name.c_str(), popCount + 1,
                [&]{
                    auto prev = m_prevPop.find(target);
                    if (prev != m_prevPop.end()) {
                        int d = (popCount + 1) - prev->second;
                        if (d >= 2) return " \xe2\x86\x91";
                        if (d <= -2) return " \xe2\x86\x93";
                    }
                    return "";
                }(),
                highSkill * 100.f);
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
            char buf[140];
            std::snprintf(buf, sizeof(buf),
                "MARKET CRISIS at %s %s — panic buying, all prices spike %.1fx!",
                settl->name.c_str(), popTag.c_str(), spikeBase);
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
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "EARTHQUAKE at %s %s — %d facilit%s destroyed, all roads blocked (%dh)",
                settl->name.c_str(), popTag.c_str(), destroyed,
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
