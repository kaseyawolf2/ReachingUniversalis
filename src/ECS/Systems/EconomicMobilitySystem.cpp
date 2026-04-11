#include "EconomicMobilitySystem.h"
#include "ECS/Components.h"
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

void EconomicMobilitySystem::Update(entt::registry& registry, float realDt) {
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
        [&](auto e, const Hauler&, const Money& money, const HomeSettlement&) {
        if (money.balance < BANKRUPTCY_THRESHOLD) {
            s_bankruptTimer[e] += CHECK_INTERVAL;
            if (s_bankruptTimer[e] >= BANKRUPTCY_HOURS)
                toDegrade.push_back(e);
        } else {
            s_bankruptTimer.erase(e);
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
        registry.remove<Hauler>(e);

        // Restore energy drain (haulers had it set to 0)
        if (auto* needs = registry.try_get<Needs>(e))
            needs->list[2].drainRate = DRAIN_ENERGY_NPC;

        // Give them a schedule (regular work hours)
        if (!registry.all_of<Schedule>(e))
            registry.emplace<Schedule>(e);

        // Log the event
        if (log) {
            std::string who = "Hauler";
            if (const auto* n = registry.try_get<Name>(e)) who = n->value;
            std::string where = "?";
            if (const auto* s = registry.try_get<Settlement>(home.settlement)) where = s->name;
            log->Push(tm.day, (int)tm.hourOfDay,
                who + " went bankrupt — returned to labor at " + where);
        }
        s_bankruptTimer.erase(e);
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

        ++haulerCount[hs.settlement];

        if (log) {
            std::string who = "NPC";
            if (const auto* n = registry.try_get<Name>(e)) who = n->value;
            std::string where = "?";
            if (const auto* s = registry.try_get<Settlement>(hs.settlement)) where = s->name;
            log->Push(tm.day, (int)tm.hourOfDay,
                who + " saved enough to become a hauler at " + where);
        }
    });
}
