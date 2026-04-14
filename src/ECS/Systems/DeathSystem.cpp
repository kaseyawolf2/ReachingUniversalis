#include "DeathSystem.h"
#include "ECS/Components.h"
#include <vector>
#include <cstdio>
#include <algorithm>
#include <random>
#include <unordered_map>

// An NPC dies if any single need stays at 0 for this many gameDt seconds.
// 12 game-hours = 12 * 60 gameDt seconds at 1x speed.
static constexpr float DEATH_THRESHOLD = 12.f * 60.f;

// 1 game-day = 24 game-hours × 60 game-minutes = 1440 game-seconds
static constexpr float SECS_PER_GAME_DAY = 24.f * 60.f;

static std::mt19937 s_wisdomDeathRng{ std::random_device{}() };

void DeathSystem::Update(entt::registry& registry, float realDt) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    float gameDt = timeView.get<TimeManager>(*timeView.begin()).GameDt(realDt);
    if (gameDt <= 0.f) return;

    float agingDays = gameDt / SECS_PER_GAME_DAY;  // age advance per tick

    // Per-settlement entity index: built once per tick, used for inheritance,
    // family grief/dissolution, and settlement collapse pop counting.
    std::unordered_map<entt::entity, std::vector<entt::entity>> entitiesBySettlement;
    registry.view<HomeSettlement>().each(
        [&](auto entity, const HomeSettlement& hs) {
            if (hs.settlement != entt::null && registry.valid(hs.settlement))
                entitiesBySettlement[hs.settlement].push_back(entity);
        });

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

        // Ensure needsAtZero vector is large enough for all needs
        if (timer.needsAtZero.size() < needs.list.size())
            timer.needsAtZero.resize(needs.list.size(), 0.f);
        for (int i = 0; i < (int)needs.list.size(); ++i) {
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

        // ---- Elder wisdom fading: skilled elder death penalises bonded NPCs ----
        {
            const auto* deadAge = registry.try_get<Age>(e);
            const auto* deadSkills = registry.try_get<Skills>(e);
            if (deadAge && deadAge->days > 60.f && deadSkills && hs
                && hs->settlement != entt::null && registry.valid(hs->settlement)) {
                // Check if elder had any skill >= 0.8
                bool wasWise = (deadSkills->farming >= 0.8f || deadSkills->water_drawing >= 0.8f
                                || deadSkills->woodcutting >= 0.8f);
                if (wasWise) {
                    registry.view<Relations, Skills, HomeSettlement>(
                        entt::exclude<PlayerTag, BanditTag>).each(
                        [&](auto mourner, Relations& rel, Skills& mSkills, const HomeSettlement& mHome) {
                            if (mourner == e) return;
                            if (mHome.settlement != hs->settlement) return;
                            auto it = rel.affinity.find(e);
                            if (it == rel.affinity.end() || it->second < 0.6f) return;
                            // Apply wisdom grief: 3 days of growth penalty
                            mSkills.wisdomGriefDays = 3.f;
                            // Elder apprentice fast-track: if this was the mourner's mentor
                            if (mSkills.elderMentor == e) {
                                mSkills.tributeDays = 5.f;
                                mSkills.elderMentor = entt::null; // mentor is gone
                            }
                            // Track lineage: mourner carries the elder's legacy
                            // If the dying NPC was already carrying a legacy (died before mastery),
                            // pass the original elder's name to chain across generations.
                            mSkills.wisdomLineage = e;
                            if (deadSkills->wisdomLineage != entt::null && !deadSkills->wisdomLineageName.empty()) {
                                mSkills.wisdomLineageName = deadSkills->wisdomLineageName;
                            } else if (const auto* nm = registry.try_get<Name>(e)) {
                                mSkills.wisdomLineageName = nm->value;
                            } else {
                                mSkills.wisdomLineageName = "a wise elder";
                            }
                            // Log at 1-in-3 frequency
                            if (s_wisdomDeathRng() % 3 == 0) {
                                auto logVW = registry.view<EventLog>();
                                if (!logVW.empty()) {
                                    auto& tmW = timeView.get<TimeManager>(*timeView.begin());
                                    std::string mournerName = "An NPC";
                                    if (const auto* nm = registry.try_get<Name>(mourner)) mournerName = nm->value;
                                    std::string elderName = "an elder";
                                    if (const auto* nm = registry.try_get<Name>(e)) elderName = nm->value;
                                    std::string where = "settlement";
                                    if (const auto* s = registry.try_get<Settlement>(hs->settlement)) where = s->name;
                                    char wbuf[180];
                                    std::snprintf(wbuf, sizeof(wbuf), "%s mourns the loss of %s's guidance at %s",
                                                  mournerName.c_str(), elderName.c_str(), where.c_str());
                                    logVW.get<EventLog>(*logVW.begin()).Push(tmW.day, (int)tmW.hourOfDay, wbuf);
                                }
                            }
                        });
                }
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
                            std::snprintf(ebuf, sizeof(ebuf), "%s%s left an estate of %.0fg to %s.",
                                          who.c_str(), isElder ? " (elder)" : "",
                                          estate, settl->name.c_str());
                            logView3.get<EventLog>(*logView3.begin()).Push(
                                tm3.day, (int)tm3.hourOfDay, ebuf);
                        }

                        // ---- Friend inheritance: best friend at same settlement ----
                        float remaining = money->balance - estate;
                        if (remaining >= 5.f) {
                            static constexpr float FRIEND_INHERIT_THRESHOLD = 0.6f;
                            static constexpr float FRIEND_INHERIT_FRACTION = 0.25f;
                            const auto* deadRel = registry.try_get<Relations>(e);
                            if (deadRel) {
                                entt::entity bestFriend = entt::null;
                                float bestAff = FRIEND_INHERIT_THRESHOLD;
                                auto sit = entitiesBySettlement.find(hs->settlement);
                                if (sit != entitiesBySettlement.end()) {
                                    for (auto other : sit->second) {
                                        if (other == e || !registry.valid(other)) continue;
                                        bool isDying = false;
                                        for (auto dead : toRemove) if (dead == other) { isDying = true; break; }
                                        if (isDying) continue;
                                        auto ait = deadRel->affinity.find(other);
                                        if (ait != deadRel->affinity.end() && ait->second > bestAff) {
                                            bestAff = ait->second;
                                            bestFriend = other;
                                        }
                                    }
                                }
                                if (bestFriend != entt::null) {
                                    float friendShare = remaining * FRIEND_INHERIT_FRACTION;
                                    if (auto* friendMoney = registry.try_get<Money>(bestFriend)) {
                                        friendMoney->balance += friendShare;
                                        // Log
                                        auto logView7 = registry.view<EventLog>();
                                        if (!logView7.empty()) {
                                            auto& tm7 = timeView.get<TimeManager>(*timeView.begin());
                                            std::string friendName = "Someone";
                                            if (const auto* fn = registry.try_get<Name>(bestFriend))
                                                friendName = fn->value;
                                            std::string who2 = "Someone";
                                            if (const auto* nm2 = registry.try_get<Name>(e))
                                                who2 = nm2->value;
                                            char fbuf[160];
                                            std::snprintf(fbuf, sizeof(fbuf),
                                                "%s inherits %.0fg from %s's estate.",
                                                friendName.c_str(), friendShare, who2.c_str());
                                            logView7.get<EventLog>(*logView7.begin()).Push(
                                                tm7.day, (int)tm7.hourOfDay, fbuf);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ---- Family grief ----
        // Surviving family members grieve: skip social actions, drain settlement morale
        if (const auto* deadFt = registry.try_get<FamilyTag>(e)) {
            std::string deadName = "Someone";
            if (const auto* nm = registry.try_get<Name>(e)) deadName = nm->value;
            registry.view<FamilyTag, DeprivationTimer, Name>(entt::exclude<BanditTag>).each(
                [&](auto other, const FamilyTag& oFt, DeprivationTimer& oTmr, const Name& oNm) {
                    if (other == e) return;
                    for (auto dead : toRemove) if (dead == other) return;
                    if (oFt.name != deadFt->name) return;
                    oTmr.griefTimer = 4.f;  // 4 game-hours
                    oTmr.lastGriefDay = timeView.get<TimeManager>(*timeView.begin()).day;
                    auto logView5 = registry.view<EventLog>();
                    if (logView5.begin() != logView5.end()) {
                        auto& tm5 = timeView.get<TimeManager>(*timeView.begin());
                        std::string msg = oNm.value + " mourns the loss of " + deadName + ".";
                        logView5.get<EventLog>(*logView5.begin()).Push(
                            tm5.day, (int)tm5.hourOfDay, msg);
                    }
                });
        }

        // ---- Family dissolution ----
        // When the last adult FamilyTag-holder with a given name dies, dissolve the family:
        // remove FamilyTag from surviving children so they can form new families later.
        if (const auto* ft = registry.try_get<FamilyTag>(e)) {
            std::string familyName = ft->name;
            entt::entity homeSettl = (hs && hs->settlement != entt::null) ? hs->settlement : entt::null;

            // Count surviving adults with the same family name at the same settlement
            // (exclude all entities in toRemove, since they're also dying this tick)
            // Uses per-settlement index to avoid scanning all entities.
            int survivingAdults = 0;
            std::vector<entt::entity> orphanedChildren;
            if (homeSettl != entt::null) {
                auto sit = entitiesBySettlement.find(homeSettl);
                if (sit != entitiesBySettlement.end()) {
                    for (auto other : sit->second) {
                        if (other == e || !registry.valid(other)) continue;
                        bool isDying = false;
                        for (auto dead : toRemove) if (dead == other) { isDying = true; break; }
                        if (isDying) continue;
                        const auto* oFt = registry.try_get<FamilyTag>(other);
                        if (!oFt || oFt->name != familyName) continue;
                        if (registry.all_of<ChildTag>(other))
                            orphanedChildren.push_back(other);
                        else
                            ++survivingAdults;
                    }
                }
            }

            if (survivingAdults == 0) {
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

        // ---- Friend grief on death ----
        // NPCs with affinity ≥ 0.5 toward the deceased mourn: morale -0.03, clear helpedTimer.
        // Log for the 2 closest friends only.
        // Optimised: use dead NPC's own Relations to find friends, then check the
        // reverse affinity — O(friends) instead of O(all entities).
        {
            std::string deadName = "Someone";
            if (const auto* nm = registry.try_get<Name>(e)) deadName = nm->value;

            struct FriendGrief { entt::entity e; float affinity; };
            std::vector<FriendGrief> grievingFriends;

            const auto* deadRel = registry.try_get<Relations>(e);
            if (deadRel) {
                for (const auto& [other, deadAff] : deadRel->affinity) {
                    if (!registry.valid(other)) continue;
                    bool isDying = false;
                    for (auto dead : toRemove) if (dead == other) { isDying = true; break; }
                    if (isDying) continue;
                    // Check reverse: does 'other' consider the deceased a friend?
                    auto* otherRel = registry.try_get<Relations>(other);
                    if (!otherRel) continue;
                    auto it = otherRel->affinity.find(e);
                    if (it == otherRel->affinity.end() || it->second < 0.5f) continue;
                    // Apply grief effects
                    if (auto* oTmr = registry.try_get<DeprivationTimer>(other))
                        oTmr->helpedTimer = 0.f;
                    if (const auto* oHs = registry.try_get<HomeSettlement>(other)) {
                        if (oHs->settlement != entt::null && registry.valid(oHs->settlement)) {
                            if (auto* settl = registry.try_get<Settlement>(oHs->settlement))
                                settl->morale = std::max(0.f, settl->morale - 0.03f);
                        }
                    }
                    grievingFriends.push_back({other, it->second});
                }
            }

            // Log for the 2 closest friends
            if (!grievingFriends.empty()) {
                std::sort(grievingFriends.begin(), grievingFriends.end(),
                    [](const FriendGrief& a, const FriendGrief& b) { return a.affinity > b.affinity; });
                int logged = 0;
                auto logView6 = registry.view<EventLog>();
                if (!logView6.empty()) {
                    auto& tm6 = timeView.get<TimeManager>(*timeView.begin());
                    for (const auto& fg : grievingFriends) {
                        if (logged >= 2) break;
                        std::string friendName = "An NPC";
                        if (const auto* fn = registry.try_get<Name>(fg.e)) friendName = fn->value;
                        std::string settlName;
                        if (const auto* fhs = registry.try_get<HomeSettlement>(fg.e))
                            if (fhs->settlement != entt::null && registry.valid(fhs->settlement))
                                if (const auto* s = registry.try_get<Settlement>(fhs->settlement))
                                    settlName = " at " + s->name;
                        logView6.get<EventLog>(*logView6.begin()).Push(
                            tm6.day, (int)tm6.hourOfDay,
                            friendName + " mourns the loss of friend " + deadName + settlName + ".");
                        ++logged;
                    }
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
    // Rebuild pop counts from the settlement index (post-removal).
    auto logView2 = registry.view<EventLog>();
    if (!logView2.empty() && !toRemove.empty()) {
        auto& log2 = logView2.get<EventLog>(*logView2.begin());
        auto& tm2  = timeView.get<TimeManager>(*timeView.begin());

        // Post-death population: count surviving (valid, non-player) entities per settlement
        std::unordered_map<entt::entity, int> postPop;
        registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
            [&](const HomeSettlement& hs) {
                if (hs.settlement != entt::null && registry.valid(hs.settlement))
                    postPop[hs.settlement]++;
            });

        registry.view<Settlement>().each([&](auto settl, Settlement& s) {
            // Drain ruin timer each tick
            if (s.ruinTimer > 0.f) {
                s.ruinTimer = std::max(0.f, s.ruinTimer - gameDt / 60.f);  // gameDt in sim-secs; timer in game-hours
            }
            int pop = postPop.count(settl) ? postPop[settl] : 0;
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
