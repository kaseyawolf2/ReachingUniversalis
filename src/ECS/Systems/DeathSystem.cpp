#include "DeathSystem.h"
#include "ECS/Components.h"
#include <vector>
#include <cstdio>
#include <algorithm>

// An NPC dies if any single need stays at 0 for this many gameDt seconds.
// 12 game-hours = 12 * 60 gameDt seconds at 1x speed.
static constexpr float DEATH_THRESHOLD = 12.f * 60.f;

// 1 game-day = 24 game-hours × 60 game-minutes = 1440 game-seconds
static constexpr float SECS_PER_GAME_DAY = 24.f * 60.f;

void DeathSystem::Update(entt::registry& registry, float realDt) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    float gameDt = timeView.get<TimeManager>(*timeView.begin()).GameDt(realDt);
    if (gameDt <= 0.f) return;

    float agingDays = gameDt / SECS_PER_GAME_DAY;  // age advance per tick

    std::vector<entt::entity> toRemove;

    // ---- Age-based natural death ----
    // Note: visual size scaling is handled per-frame in SimThread::WriteSnapshot
    // based on ageDays, so there is no need to modify rend->size here.
    registry.view<Age>().each([&](auto entity, Age& age) {
        age.days += agingDays;

        if (age.days >= age.maxDays) {
            toRemove.push_back(entity);
            auto logView = registry.view<EventLog>();
            if (!logView.empty()) {
                auto& tm2 = timeView.get<TimeManager>(*timeView.begin());
                std::string who = "NPC";
                if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                std::string settlStr;
                if (const auto* hs = registry.try_get<HomeSettlement>(entity))
                    if (hs->settlement != entt::null && registry.valid(hs->settlement))
                        if (const auto* s = registry.try_get<Settlement>(hs->settlement))
                            settlStr = " at " + s->name;
                char ageBuf[64];
                std::snprintf(ageBuf, sizeof(ageBuf), " died at age %d (old age)%s",
                              (int)age.days, settlStr.c_str());
                logView.get<EventLog>(*logView.begin()).Push(
                    tm2.day, (int)tm2.hourOfDay,
                    who + ageBuf);
            }
        }
    });

    // ---- Need-based death ----
    auto view = registry.view<Needs, DeprivationTimer>();
    for (auto entity : view) {
        if (!registry.valid(entity)) continue;   // may have been queued above
        // Skip if already queued for removal
        bool already = false;
        for (auto e : toRemove) if (e == entity) { already = true; break; }
        if (already) continue;

        auto& needs = view.get<Needs>(entity);
        auto& timer = view.get<DeprivationTimer>(entity);

        for (int i = 0; i < 4; ++i) {
            if (needs.list[i].value <= 0.001f) {
                timer.needsAtZero[i] += gameDt;
                if (timer.needsAtZero[i] >= DEATH_THRESHOLD) {
                    toRemove.push_back(entity);
                    const char* cause = (i == 0) ? "hunger" :
                                        (i == 1) ? "thirst" :
                                        (i == 2) ? "exhaustion" : "cold";
                    std::string who = "NPC";
                    if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                    int ageInt = 0;
                    if (const auto* ag = registry.try_get<Age>(entity)) ageInt = (int)ag->days;
                    std::string settlStr;
                    if (const auto* hs = registry.try_get<HomeSettlement>(entity))
                        if (hs->settlement != entt::null && registry.valid(hs->settlement))
                            if (const auto* s = registry.try_get<Settlement>(hs->settlement))
                                settlStr = " at " + s->name;
                    std::printf("[DEATH] %s died of %s, age %d\n", who.c_str(), cause, ageInt);
                    auto logView = registry.view<EventLog>();
                    if (!logView.empty()) {
                        auto& tm2 = timeView.get<TimeManager>(*timeView.begin());
                        char deathBuf[96];
                        std::snprintf(deathBuf, sizeof(deathBuf), " died of %s, age %d%s",
                                      cause, ageInt, settlStr.c_str());
                        logView.get<EventLog>(*logView.begin()).Push(
                            tm2.day, (int)tm2.hourOfDay,
                            who + deathBuf);
                    }
                    break;
                }
            } else {
                timer.needsAtZero[i] = 0.f;
            }
        }
    }

    // ---- Inheritance and morale impact ----
    // 50% of a deceased NPC's gold returns to their settlement.
    // Deaths from need-deprivation lower settlement morale — the community feels the loss.
    for (auto e : toRemove) {
        if (!registry.valid(e)) continue;
        const auto* hs = registry.try_get<HomeSettlement>(e);

        // Morale impact: traumatic deaths (need deprivation) hit harder than old age
        if (hs && hs->settlement != entt::null && registry.valid(hs->settlement)) {
            if (auto* settl = registry.try_get<Settlement>(hs->settlement)) {
                const auto* age = registry.try_get<Age>(e);
                bool wasOldAge = age && (age->days >= age->maxDays * 0.95f);
                float moralePenalty = wasOldAge ? 0.02f : 0.08f;
                settl->morale = std::max(0.f, settl->morale - moralePenalty);
            }
        }

        // Hauler cargo recovery: return goods in transit to home settlement
        if (const auto* inv = registry.try_get<Inventory>(e)) {
            if (!inv->contents.empty() && hs && hs->settlement != entt::null &&
                registry.valid(hs->settlement)) {
                if (auto* sp = registry.try_get<Stockpile>(hs->settlement)) {
                    for (const auto& [type, qty] : inv->contents)
                        sp->quantities[type] += qty;
                }
            }
        }

        if (const auto* money = registry.try_get<Money>(e)) {
            static constexpr float INHERITANCE_FRACTION       = 0.5f;
            static constexpr float ELDER_INHERITANCE_FRACTION = 0.8f;
            static constexpr float MIN_INHERITANCE            = 10.f;
            if (money->balance >= MIN_INHERITANCE) {
                if (hs && hs->settlement != entt::null && registry.valid(hs->settlement)) {
                    if (auto* settl = registry.try_get<Settlement>(hs->settlement)) {
                        const auto* agePtr = registry.try_get<Age>(e);
                        bool isElder = agePtr && (agePtr->days > 60.f);
                        float fraction = isElder ? ELDER_INHERITANCE_FRACTION : INHERITANCE_FRACTION;
                        float estate = money->balance * fraction;
                        settl->treasury += estate;

                        // Log the estate transfer
                        auto logView3 = registry.view<EventLog>();
                        if (!logView3.empty()) {
                            auto& tm3 = timeView.get<TimeManager>(*timeView.begin());
                            std::string who = "Someone";
                            if (const auto* nm = registry.try_get<Name>(e)) who = nm->value;
                            char ebuf[128];
                            std::snprintf(ebuf, sizeof(ebuf), "%s left an estate of %.0fg to %s.",
                                          who.c_str(), estate, settl->name.c_str());
                            logView3.get<EventLog>(*logView3.begin()).Push(
                                tm3.day, (int)tm3.hourOfDay, ebuf);
                        }
                    }
                }
            }
        }

        // ---- Family dissolution ----
        // When the last adult FamilyTag-holder with a given name dies, dissolve the family:
        // remove FamilyTag from surviving children so they can form new families later.
        if (const auto* ft = registry.try_get<FamilyTag>(e)) {
            std::string familyName = ft->name;
            entt::entity homeSettl = (hs && hs->settlement != entt::null) ? hs->settlement : entt::null;

            // Count surviving adults with the same family name at the same settlement
            // (exclude all entities in toRemove, since they're also dying this tick)
            int survivingAdults = 0;
            registry.view<FamilyTag, HomeSettlement, Age>(entt::exclude<ChildTag>).each(
                [&](auto other, const FamilyTag& oFt, const HomeSettlement& oHs, const Age& oAge) {
                    if (other == e) return;
                    // Skip other entities dying this tick
                    for (auto dead : toRemove) if (dead == other) return;
                    if (oFt.name != familyName) return;
                    if (oHs.settlement != homeSettl) return;
                    ++survivingAdults;
                });

            if (survivingAdults == 0) {
                // No adult family members left — dissolve: remove FamilyTag from children
                std::vector<entt::entity> orphanedChildren;
                registry.view<FamilyTag, ChildTag, HomeSettlement>().each(
                    [&](auto child, const FamilyTag& cFt, const HomeSettlement& cHs) {
                        if (cFt.name != familyName) return;
                        if (cHs.settlement != homeSettl) return;
                        orphanedChildren.push_back(child);
                    });
                for (auto child : orphanedChildren)
                    if (registry.valid(child)) registry.remove<FamilyTag>(child);

                // Log the family line ending
                auto logView4 = registry.view<EventLog>();
                if (!logView4.empty()) {
                    auto& tm4 = timeView.get<TimeManager>(*timeView.begin());
                    std::string settlName = "the world";
                    if (homeSettl != entt::null && registry.valid(homeSettl))
                        if (const auto* s = registry.try_get<Settlement>(homeSettl))
                            settlName = s->name;
                    logView4.get<EventLog>(*logView4.begin()).Push(
                        tm4.day, (int)tm4.hourOfDay,
                        "The " + familyName + " family line has ended at " + settlName + ".");
                }
            }
        }
    }

    for (auto e : toRemove) {
        if (registry.valid(e)) {
            ++totalDeaths;
            registry.destroy(e);
        }
    }

    // ---- Settlement collapse check ----
    // After deaths, log any settlement that just hit 0 population.
    auto logView2 = registry.view<EventLog>();
    if (!logView2.empty() && !toRemove.empty()) {
        auto& log2 = logView2.get<EventLog>(*logView2.begin());
        auto& tm2  = timeView.get<TimeManager>(*timeView.begin());

        registry.view<Settlement>().each([&](auto settl, Settlement& s) {
            // Drain ruin timer each tick
            if (s.ruinTimer > 0.f) {
                s.ruinTimer = std::max(0.f, s.ruinTimer - gameDt / 60.f);  // gameDt in sim-secs; timer in game-hours
            }
            int pop = 0;
            registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
                [&](const HomeSettlement& hs) { if (hs.settlement == settl) ++pop; });
            if (pop == 0 && !m_collapsed.count(settl)) {
                m_collapsed.insert(settl);
                s.ruinTimer = 300.f;  // 300 game-hours cooldown before repopulation
                log2.Push(tm2.day, (int)tm2.hourOfDay,
                    s.name + " has COLLAPSED — population zero");

                // Scatter any children still homed at this settlement:
                // clear their HomeSettlement and drop their follow target
                // so they become wanderers.
                int orphanCount = 0;
                registry.view<ChildTag, HomeSettlement, AgentState>().each(
                    [&](auto child, HomeSettlement& hs, AgentState& as) {
                        if (hs.settlement != settl) return;
                        hs.settlement = entt::null;
                        as.target     = entt::null;
                        ++orphanCount;
                    });
                if (orphanCount > 0) {
                    char obuf[128];
                    std::snprintf(obuf, sizeof(obuf), "%d children of %s orphaned and scattered.",
                                  orphanCount, s.name.c_str());
                    log2.Push(tm2.day, (int)tm2.hourOfDay, obuf);
                }
            } else if (pop > 0) {
                m_collapsed.erase(settl);  // recovered
            }
        });
    }
}
