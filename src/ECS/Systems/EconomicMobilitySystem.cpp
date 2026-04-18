#include "EconomicMobilitySystem.h"
#include "ECS/Components.h"
#include "World/WorldSchema.h"
#include <cmath>
#include <map>
#include <vector>

// Gold threshold for an NPC to consider becoming a hauler.
static constexpr float GRADUATION_THRESHOLD  = 100.f;
// Probability of graduating per eligible NPC per check interval.
static constexpr float GRADUATION_CHANCE     = 0.20f;
// Minimum age (days) before an NPC can become a hauler (young adults only).
static constexpr float MIN_HAULER_AGE        = 20.f;
// Maximum age for hauler graduation — elderly NPCs don't start new careers.
static constexpr float MAX_HAULER_AGE        = 65.f;
// Max haulers allowed per settlement (prevents market over-saturation).
static constexpr int   MAX_HAULERS_PER_SETTLEMENT = 10;

// Hauler bankruptcy threshold.
static constexpr float BANKRUPTCY_THRESHOLD  = 5.f;
// Hours a hauler must be below the threshold before going bankrupt.
static constexpr float BANKRUPTCY_HOURS      = 24.f;

// Check interval: run a full mobility scan every N game-hours.
static constexpr float CHECK_INTERVAL        = 6.f;

// Drain rates matching WorldGenerator values (used when restoring NPC status)
static constexpr float DRAIN_ENERGY_NPC = 0.00050f;

void EconomicMobilitySystem::Update(entt::registry& registry, float realDt, const WorldSchema& schema) {
    auto tmv = registry.view<TimeManager>();
    if (tmv.begin() == tmv.end()) return;
    const auto& tm = tmv.get<TimeManager>(*tmv.begin());
    float gameDt = tm.GameDt(realDt);
    if (gameDt <= 0.f) return;

    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;
    m_checkAccum += gameHoursDt;
    if (m_checkAccum < CHECK_INTERVAL) return;
    m_checkAccum -= CHECK_INTERVAL;

    auto lv  = registry.view<EventLog>();
    EventLog* log = (lv.begin() == lv.end()) ? nullptr : &lv.get<EventLog>(*lv.begin());

    // ---- Count haulers per settlement for the cap check ----
    std::map<entt::entity, int> haulerCount;
    registry.view<Hauler, HomeSettlement>().each(
        [&](const Hauler&, const HomeSettlement& hs) {
        ++haulerCount[hs.settlement];
    });

    // ---- Hauler bankruptcy: check each hauler's balance ----
    // Add bankruptTimer to Hauler struct indirectly by storing per-entity map here.
    // (Avoids changing the Hauler struct for a single system's internal state.)
    static std::map<entt::entity, float> s_bankruptTimer;

    // Prune stale timer entries for destroyed entities
    for (auto it = s_bankruptTimer.begin(); it != s_bankruptTimer.end(); ) {
        if (!registry.valid(it->first)) it = s_bankruptTimer.erase(it);
        else ++it;
    }

    std::vector<entt::entity> toDegrade;
    registry.view<Hauler, Money, HomeSettlement>(entt::exclude<PlayerTag>).each(
        [&](auto e, Hauler& hauler, const Money& money, const HomeSettlement&) {
        if (money.balance < BANKRUPTCY_THRESHOLD) {
            s_bankruptTimer[e] += CHECK_INTERVAL;
            hauler.nearBankrupt = (s_bankruptTimer[e] >= BANKRUPTCY_HOURS * 0.75f);
            hauler.bankruptProgress = s_bankruptTimer[e];
            // Warn at 50% progress (once per bankruptcy cycle)
            if (!hauler.bankruptWarned && hauler.bankruptProgress >= BANKRUPTCY_HOURS * 0.5f) {
                hauler.bankruptWarned = true;
                auto lv = registry.view<EventLog>();
                auto tv = registry.view<TimeManager>();
                if (!lv.empty() && !tv.empty()) {
                    const auto& tm = tv.get<TimeManager>(*tv.begin());
                    std::string who = "A hauler";
                    if (const auto* n = registry.try_get<Name>(e))
                        who = n->value;
                    lv.get<EventLog>(*lv.begin()).Push(tm.day, (int)tm.hourOfDay,
                        who + " is struggling financially.", "Econ");
                }
            }
            if (s_bankruptTimer[e] >= BANKRUPTCY_HOURS)
                toDegrade.push_back(e);
        } else {
            s_bankruptTimer.erase(e);
            hauler.nearBankrupt = false;
            hauler.bankruptProgress = 0.f;
            hauler.bankruptWarned = false;
        }
    });

    for (auto e : toDegrade) {
        if (!registry.valid(e)) continue;
        const auto& home = registry.get<HomeSettlement>(e);

        // Clear cargo inventory and remove hauler-specific components
        if (auto* inv = registry.try_get<Inventory>(e)) {
            // Return cargo to home stockpile
            if (auto* sp = registry.try_get<Stockpile>(home.settlement)) {
                for (const auto& [res, qty] : inv->contents)
                    sp->quantities[res] += qty;
            }
            registry.remove<Inventory>(e);
        }

        // ---- Novice hauler bankruptcy sympathy ----
        // Friends at the same settlement donate gold to help a novice back on their feet.
        {
            const auto* haulerPtr = registry.try_get<Hauler>(e);
            if (haulerPtr && haulerPtr->lifetimeTrips < 10) {
                auto* bankruptMoney = registry.try_get<Money>(e);
                if (bankruptMoney) {
                    static constexpr float SYMPATHY_DONATION = 5.f;
                    static constexpr int   MAX_DONORS = 3;
                    static std::mt19937 s_sympathyRng{std::random_device{}()};
                    int donors = 0;
                    registry.view<Relations, Money, HomeSettlement>(
                        entt::exclude<PlayerTag>).each(
                        [&](auto other, Relations& rel, Money& otherMoney, const HomeSettlement& otherHome) {
                            if (donors >= MAX_DONORS) return;
                            if (other == e) return;
                            if (otherHome.settlement != home.settlement) return;
                            auto it = rel.affinity.find(e);
                            if (it == rel.affinity.end() || it->second < 0.4f) return;
                            if (otherMoney.balance < SYMPATHY_DONATION) return;
                            // Gold Flow Rule: balance-to-balance transfer
                            otherMoney.balance     -= SYMPATHY_DONATION;
                            bankruptMoney->balance += SYMPATHY_DONATION;
                            ++donors;
                            // Log at 1-in-3 frequency
                            if (s_sympathyRng() % 3 == 0 && log) {
                                std::string friendName = "A friend";
                                if (const auto* nm = registry.try_get<Name>(other)) friendName = nm->value;
                                std::string bankruptName = "a hauler";
                                if (const auto* nm = registry.try_get<Name>(e)) bankruptName = nm->value;
                                std::string where = "settlement";
                                if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                    where = s->name;
                                char sbuf[180];
                                std::snprintf(sbuf, sizeof(sbuf),
                                    "%s helps %s get back on their feet at %s",
                                    friendName.c_str(), bankruptName.c_str(), where.c_str());
                                log->Push(tm.day, (int)tm.hourOfDay, sbuf, "Econ");
                            }
                            // Community reputation: other NPCs at settlement respect the donor
                            registry.view<Relations, HomeSettlement>(
                                entt::exclude<PlayerTag>).each(
                                [&](auto witness, Relations& wRel, const HomeSettlement& wHome) {
                                    if (witness == other || witness == e) return;
                                    if (wHome.settlement != home.settlement) return;
                                    wRel.affinity[other] = std::min(1.f, wRel.affinity[other] + 0.01f);
                                });
                            // Log reputation at 1-in-5 frequency
                            if (s_sympathyRng() % 5 == 0 && log) {
                                std::string donorName = "A donor";
                                if (const auto* nm = registry.try_get<Name>(other)) donorName = nm->value;
                                std::string bankruptName2 = "a hauler";
                                if (const auto* nm = registry.try_get<Name>(e)) bankruptName2 = nm->value;
                                char rbuf[180];
                                std::snprintf(rbuf, sizeof(rbuf),
                                    "%s earns respect for helping %s",
                                    donorName.c_str(), bankruptName2.c_str());
                                log->Push(tm.day, (int)tm.hourOfDay, rbuf, "Econ");
                            }
                        });
                }
            }
        }

        registry.remove<Hauler>(e);

        // Restore energy drain (haulers had it set to 0)
        if (auto* needs = registry.try_get<Needs>(e))
            needs->list[2].drainRate = DRAIN_ENERGY_NPC;

        // Give them a schedule (regular work hours)
        if (!registry.all_of<Schedule>(e))
            registry.emplace<Schedule>(e);

        // Mark as bankruptcy survivor for extra skill growth
        if (auto* sb = registry.try_get<SocialBehavior>(e))
            sb->mood.bankruptSurvivor = true;

        // Log the event
        if (log) {
            std::string who = "Hauler";
            if (const auto* n = registry.try_get<Name>(e)) who = n->value;
            std::string where = "?";
            if (const auto* s = registry.try_get<Settlement>(home.settlement)) where = s->name;
            float bal = 0.f;
            if (const auto* m = registry.try_get<Money>(e)) bal = m->balance;
            char buf[160];
            std::snprintf(buf, sizeof(buf), "%s went bankrupt (%.0fg left) — returned to labor at %s",
                          who.c_str(), bal, where.c_str());
            log->Push(tm.day, (int)tm.hourOfDay, buf, "Econ");
        }
        s_bankruptTimer.erase(e);
        // Bankruptcy demoralises the home settlement
        if (auto* settl = registry.try_get<Settlement>(home.settlement))
            settl->morale = std::max(0.f, settl->morale - 0.03f);
        if (home.settlement != entt::null && haulerCount.count(home.settlement))
            --haulerCount[home.settlement];
    }

    // ---- NPC → Hauler graduation ----
    std::uniform_real_distribution<float> chance(0.f, 1.f);
    registry.view<Needs, Money, HomeSettlement, Age>(
        entt::exclude<Hauler, PlayerTag>).each(
        [&](auto e, const Needs&, const Money& money, const HomeSettlement& hs, const Age& age) {
        if (money.balance < GRADUATION_THRESHOLD) return;
        if (age.days < MIN_HAULER_AGE || age.days > MAX_HAULER_AGE) return;
        if (haulerCount[hs.settlement] >= MAX_HAULERS_PER_SETTLEMENT) return;
        if (chance(m_rng) > GRADUATION_CHANCE) return;

        // Graduate this NPC to hauler status
        Hauler h;
        h.state      = HaulerState::Idle;
        h.waitTimer  = 0.f;
        registry.emplace<Hauler>(e, h);
        {
            int oldProf = schema.FindIdleProfession();
            if (const auto* p = registry.try_get<Profession>(e)) oldProf = p->type;
            registry.emplace_or_replace<Profession>(e, Profession{ schema.FindHaulerProfession(), oldProf });
        }
        registry.emplace_or_replace<Inventory>(e, Inventory{ {}, 15 });

        // Haulers work around the clock — zero out energy drain
        if (auto* needs = registry.try_get<Needs>(e))
            needs->list[2].drainRate = 0.f;

        // Remove schedule (TransportSystem owns their behaviour)
        if (registry.all_of<Schedule>(e))
            registry.remove<Schedule>(e);

        // Change colour to sky-blue (hauler colour)
        if (auto* rend = registry.try_get<Renderable>(e))
            rend->color = SKYBLUE;

        // Celebrate graduation: 2 game-hours of celebration + morale boost
        if (auto* as = registry.try_get<AgentState>(e))
            as->behavior = AgentBehavior::Celebrating;
        if (auto* goal = registry.try_get<Goal>(e)) {
            goal->celebrateTimer = 2.f;
            // If the NPC's goal checks for the hauler profession, mark it complete
            if (goal->goalId >= 0 && goal->goalId < (int)schema.goals.size()
                && schema.goals[goal->goalId].checkTypeEnum == GoalCheckType::HasProfession
                && schema.goals[goal->goalId].targetProfessionId == schema.haulerProfessionId)
                goal->progress = goal->target;
        }
        if (auto* settl = registry.try_get<Settlement>(hs.settlement))
            settl->morale = std::min(1.f, settl->morale + 0.02f);

        ++haulerCount[hs.settlement];

        // Second-chance hauler graduation bonus: bankruptcy survivors get higher mentor bonus
        if (const auto* sb = registry.try_get<SocialBehavior>(e)) {
            if (sb->mood.bankruptSurvivor) {
                if (auto* hPtr = registry.try_get<Hauler>(e))
                    hPtr->mentorBonus = 0.15f;
                if (log) {
                    std::string who = "NPC";
                    if (const auto* n = registry.try_get<Name>(e)) who = n->value;
                    std::string where = "settlement";
                    if (const auto* s = registry.try_get<Settlement>(hs.settlement)) where = s->name;
                    char sbuf[180];
                    std::snprintf(sbuf, sizeof(sbuf), "%s returns to hauling with hard-won wisdom at %s.",
                                  who.c_str(), where.c_str());
                    log->Push(tm.day, (int)tm.hourOfDay, sbuf, "Econ");
                }
            }
        }

        if (log) {
            std::string who = "NPC";
            if (const auto* n = registry.try_get<Name>(e)) who = n->value;
            std::string where = "?";
            if (const auto* s = registry.try_get<Settlement>(hs.settlement)) where = s->name;
            char buf[160];
            std::snprintf(buf, sizeof(buf), "%s saved enough (%.0fg) to become a hauler at %s",
                          who.c_str(), money.balance, where.c_str());
            log->Push(tm.day, (int)tm.hourOfDay, buf, "Econ");
        }
    });
}
