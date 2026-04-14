#include "AgentDecisionSystem.h"
#include <chrono>
#include <cmath>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "ECS/Components.h"

// Radius within which an NPC can interact with a production facility.
static constexpr float FACILITY_RANGE = 35.0f;
// Arrival threshold for reaching a settlement when migrating.
static constexpr float SETTLE_RANGE   = 130.0f;
// Affinity threshold above which two NPCs are considered friends.
static constexpr float FRIEND_THRESHOLD = 0.5f;
// Note: migration threshold is now per-NPC (DeprivationTimer::migrateThreshold)
// so each NPC migrates at a different time, preventing mass simultaneous exodus.

// ---- File-scope caches (populated once per tick in Update, used by helpers) ----
static std::vector<std::pair<float,float>> s_banditPositions;

// Harmony per settlement, refreshed once per game-hour in FindMigrationTarget.
struct HarmonyCache { int hour = -1; int day = -1; std::unordered_map<entt::entity, float> values; };
static HarmonyCache s_harmonyCache;

// ---- Static helpers ----

static AgentBehavior BehaviorForNeed(NeedType type) {
    switch (type) {
        case NeedType::Hunger: return AgentBehavior::SeekingFood;
        case NeedType::Thirst: return AgentBehavior::SeekingWater;
        case NeedType::Energy: return AgentBehavior::SeekingSleep;
    }
    return AgentBehavior::Idle;
}

static ResourceType ResourceTypeForNeed(NeedType type) {
    switch (type) {
        case NeedType::Hunger: return ResourceType::Food;
        case NeedType::Thirst: return ResourceType::Water;
        case NeedType::Energy: return ResourceType::Shelter;
    }
    return ResourceType::Food;
}

static int NeedIndexForResource(ResourceType type) {
    switch (type) {
        case ResourceType::Food:    return (int)NeedType::Hunger;
        case ResourceType::Water:   return (int)NeedType::Thirst;
        case ResourceType::Shelter: return (int)NeedType::Energy;
    }
    return -1;
}

static bool InRange(float ax, float ay, float bx, float by, float r) {
    float dx = ax - bx, dy = ay - by;
    return (dx * dx + dy * dy) <= r * r;
}

static void MoveToward(Velocity& vel, const Position& from,
                        float tx, float ty, float speed) {
    float dx = tx - from.x, dy = ty - from.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.f) { vel.vx = vel.vy = 0.f; return; }
    vel.vx = (dx / dist) * speed;
    vel.vy = (dy / dist) * speed;
}

// ---- AgentDecisionSystem::FindNearestFacility ----
// Cached per (settlement, resourceType) and invalidated once per game-hour.
// Facilities are static, so the nearest one to any NPC at a given settlement
// is the same regardless of NPC position within the settlement.

entt::entity AgentDecisionSystem::FindNearestFacility(entt::registry& registry,
                                                       ResourceType type,
                                                       entt::entity homeSettlement,
                                                       float px, float py) {
    // --- Cache keyed by (settlement, resourceType), invalidated per game-hour ---
    struct FacCacheKey {
        entt::entity settlement;
        ResourceType resType;
        bool operator==(const FacCacheKey& o) const {
            return settlement == o.settlement && resType == o.resType;
        }
    };
    struct FacCacheKeyHash {
        size_t operator()(const FacCacheKey& k) const {
            return std::hash<uint32_t>()((uint32_t)k.settlement) ^
                   (std::hash<int>()((int)k.resType) << 16);
        }
    };
    struct FacCacheEntry { entt::entity facility; int hour; int day; };
    static std::unordered_map<FacCacheKey, FacCacheEntry, FacCacheKeyHash> s_facCache;

    int curHour = -1, curDay = -1;
    {
        auto tmv = registry.view<TimeManager>();
        if (!tmv.empty()) {
            const auto& tm = tmv.get<TimeManager>(*tmv.begin());
            curHour = (int)tm.hourOfDay;
            curDay  = tm.day;
        }
    }

    FacCacheKey key{homeSettlement, type};
    auto it = s_facCache.find(key);
    if (it != s_facCache.end() && it->second.hour == curHour && it->second.day == curDay) {
        return it->second.facility;
    }

    // Cache miss — do the full scan
    entt::entity nearest  = entt::null;
    float        bestDist = std::numeric_limits<float>::max();

    auto view = registry.view<Position, ProductionFacility>();
    for (auto e : view) {
        const auto& fac = view.get<ProductionFacility>(e);
        if (fac.output != type) continue;
        if (fac.settlement != homeSettlement) continue;

        const auto& pos = view.get<Position>(e);
        float dx = pos.x - px, dy = pos.y - py;
        float dist = dx * dx + dy * dy;
        if (dist < bestDist) { bestDist = dist; nearest = e; }
    }

    s_facCache[key] = {nearest, curHour, curDay};
    return nearest;
}

// ---- AgentDecisionSystem::FindMigrationTarget ----
// Picks the reachable settlement with the most combined food + water stock.
// If the NPC has skills, adds an affinity bonus (20% of base score) for
// destinations whose primary facility matches the NPC's strongest skill.
// This makes skilled workers self-sort toward matching settlements over time.

entt::entity AgentDecisionSystem::FindMigrationTarget(entt::registry& registry,
                                                        entt::entity homeSettlement,
                                                        const Skills* skills,
                                                        const Profession* profession,
                                                        const MigrationMemory* memory,
                                                        int currentDay,
                                                        float lastSatisfaction,
                                                        bool isLonely) {
    // ---- Cache: per source settlement per game-hour ----
    // NPC-specific modifiers (skills, memory, satisfaction) cause minor score variation
    // but the base destination ranking is settlement-dependent. Caching avoids repeated
    // O(roads × facilities × bandits) scans for co-migration candidates.
    struct MigCache { int hour = -1; int day = -1; entt::entity dest = entt::null; };
    static std::unordered_map<entt::entity, MigCache> s_migCache;
    int cacheHour = -1, cacheDay = -1;
    {
        auto tmv = registry.view<TimeManager>();
        if (!tmv.empty()) {
            const auto& tmData = tmv.get<TimeManager>(*tmv.begin());
            cacheHour = (int)tmData.hourOfDay;
            cacheDay  = tmData.day;
            auto it = s_migCache.find(homeSettlement);
            if (it != s_migCache.end() && it->second.hour == cacheHour && it->second.day == cacheDay) {
                return it->second.dest;
            }
        }
    }

    // Determine the NPC's strongest skill (if any) for affinity matching.
    ResourceType affinityType = ResourceType::Food;
    bool         hasAffinity  = false;
    if (skills) {
        float best = skills->farming;
        affinityType = ResourceType::Food;
        if (skills->water_drawing > best) { best = skills->water_drawing; affinityType = ResourceType::Water; }
        if (skills->woodcutting   > best) {                               affinityType = ResourceType::Wood;  }
        // Only apply affinity bonus if the skill is meaningfully developed (> 0.25)
        hasAffinity = (best > 0.25f);
    }

    // Determine profession-based affinity resource type (additional bonus on top of skills).
    ResourceType profAffinityType = ResourceType::Food;
    bool         hasProfAffinity  = false;
    if (profession) {
        switch (profession->type) {
            case ProfessionType::Farmer:       profAffinityType = ResourceType::Food;  hasProfAffinity = true; break;
            case ProfessionType::WaterCarrier: profAffinityType = ResourceType::Water; hasProfAffinity = true; break;
            case ProfessionType::Lumberjack:   profAffinityType = ResourceType::Wood;  hasProfAffinity = true; break;
            default: break;
        }
    }

    // Scarcity nudge: if home settlement has any stockpile below 10, all
    // destinations become more attractive (+0.3 per scarce resource).
    float scarcityNudge = 0.f;
    {
        static constexpr float SCARCITY_THRESHOLD = 10.f;
        const auto* homeSp = registry.try_get<Stockpile>(homeSettlement);
        if (homeSp) {
            for (auto rt : { ResourceType::Food, ResourceType::Water, ResourceType::Wood }) {
                auto it = homeSp->quantities.find(rt);
                if (it == homeSp->quantities.end() || it->second < SCARCITY_THRESHOLD)
                    scarcityNudge += 0.3f;
            }
        }
    }

    // Morale push: low morale drives NPCs away, high morale keeps them
    float moralePush = 0.f;
    if (const auto* homeSett = registry.try_get<Settlement>(homeSettlement)) {
        if (homeSett->morale < 0.25f)      moralePush = 0.3f;
        else if (homeSett->morale > 0.7f)  moralePush = -0.2f;
    }

    // Satisfaction push: consistently unsatisfied NPCs seek better settlements
    float satisfactionPush = (lastSatisfaction < 0.3f) ? 0.2f : 0.f;

    // Loneliness push: isolated NPCs seek communities
    float lonelinessPush = isLonely ? 0.15f : 0.f;

    // Pre-compute harmony per settlement (cached once per game-hour).
    // Harmony = (mutual friendship pairs * 2) / (pop * (pop - 1)), same formula as WriteSnapshot.
    if (s_harmonyCache.hour != cacheHour || s_harmonyCache.day != cacheDay) {
        s_harmonyCache.values.clear();
        s_harmonyCache.hour = cacheHour;
        s_harmonyCache.day  = cacheDay;
        // Group NPCs by home settlement
        std::unordered_map<entt::entity, std::vector<entt::entity>> settlResidents;
        registry.view<HomeSettlement, Relations>(entt::exclude<PlayerTag, BanditTag>).each(
            [&](auto re, const HomeSettlement& rh, const Relations&) {
                settlResidents[rh.settlement].push_back(re);
            });
        for (const auto& [sett, residents] : settlResidents) {
            int friendPairs = 0;
            for (size_t i = 0; i < residents.size(); ++i) {
                const auto& relA = registry.get<Relations>(residents[i]);
                for (size_t j = i + 1; j < residents.size(); ++j) {
                    auto itA = relA.affinity.find(residents[j]);
                    if (itA == relA.affinity.end() || itA->second < FRIEND_THRESHOLD) continue;
                    const auto& relB = registry.get<Relations>(residents[j]);
                    auto itB = relB.affinity.find(residents[i]);
                    if (itB != relB.affinity.end() && itB->second >= FRIEND_THRESHOLD)
                        ++friendPairs;
                }
            }
            int pop = (int)residents.size();
            float harmony = (pop >= 2) ? (friendPairs * 2.0f) / std::max(1, pop * (pop - 1)) : 0.f;
            s_harmonyCache.values[sett] = harmony;
        }
    }

    entt::entity best      = entt::null;
    float        bestScore = -1.f;

    registry.view<Road>().each([&](const Road& road) {
        if (road.blocked) return;
        entt::entity dest = entt::null;
        if (road.from == homeSettlement) dest = road.to;
        else if (road.to == homeSettlement) dest = road.from;
        else return;

        if (!registry.valid(dest)) return;
        const auto* sp = registry.try_get<Stockpile>(dest);
        if (!sp) return;

        float food  = sp->quantities.count(ResourceType::Food)
                      ? sp->quantities.at(ResourceType::Food)  : 0.f;
        float water = sp->quantities.count(ResourceType::Water)
                      ? sp->quantities.at(ResourceType::Water) : 0.f;
        float wood  = sp->quantities.count(ResourceType::Wood)
                      ? sp->quantities.at(ResourceType::Wood)  : 0.f;
        float total = food + water;

        // In cold seasons (Autumn/Winter), wood stock matters for warmth.
        // Settlements with good wood reserve get a bonus — cold NPCs flee to warmth.
        auto tmv = registry.view<TimeManager>();
        if (!tmv.empty()) {
            Season season = tmv.get<TimeManager>(*tmv.begin()).CurrentSeason();
            float heatMult = SeasonHeatDrainMult(season);
            if (heatMult > 0.f) {
                total += wood * heatMult * 1.5f;  // weight wood by how cold the season is
            }
        }

        // Plague penalty: NPCs strongly avoid plague-infected destinations
        if (const auto* ds = registry.try_get<Settlement>(dest)) {
            if (ds->modifierName == "Plague")
                total *= 0.20f;  // 80% less attractive — flee or avoid
        }

        // Skill-affinity bonus: +20% score if destination primarily produces
        // the resource matching the NPC's strongest skill.
        if (hasAffinity) {
            registry.view<ProductionFacility>().each([&](const ProductionFacility& fac) {
                if (fac.settlement == dest && fac.output == affinityType && fac.baseRate > 0.f)
                    total *= 1.20f;
            });
        }

        // Profession-affinity bonus: additional +15% if profession matches settlement output.
        // Stacks with skill affinity — a skilled farmer who identifies as a Farmer
        // gets a combined 35% bonus toward farming settlements.
        if (hasProfAffinity) {
            registry.view<ProductionFacility>().each([&](const ProductionFacility& fac) {
                if (fac.settlement == dest && fac.output == profAffinityType && fac.baseRate > 0.f)
                    total *= 1.15f;
            });
        }

        // Seasonal migration penalty: Winter travel is harsh — destinations
        // are less attractive, making NPCs more inclined to stay put.
        {
            auto tmv2 = registry.view<TimeManager>();
            if (!tmv2.empty()) {
                Season s = tmv2.get<TimeManager>(*tmv2.begin()).CurrentSeason();
                if (s == Season::Winter) total *= 0.8f;
            }
        }

        // Migration memory bonus: prefer destinations remembered as having
        // cheaper food / water than the NPC's current home.
        // +20% if food was cheaper there; +10% if water was cheaper there.
        if (memory) {
            const auto* homeSett = registry.try_get<Settlement>(homeSettlement);
            const auto* destSett = registry.try_get<Settlement>(dest);
            if (homeSett && destSett) {
                const auto* homeSnap = memory->Get(homeSett->name);
                const auto* destSnap = memory->Get(destSett->name);
                if (homeSnap && destSnap) {
                    float memBonus = 1.f;
                    if (destSnap->food  < homeSnap->food)  memBonus += 0.20f;
                    if (destSnap->water < homeSnap->water) memBonus += 0.10f;
                    // Stale memory decay: halve bonus if destination info is > 30 days old
                    if (currentDay > 0 && destSnap->lastVisitedDay > 0 &&
                        currentDay - destSnap->lastVisitedDay > 30) {
                        memBonus = 1.f + (memBonus - 1.f) * 0.5f;
                    }
                    total *= memBonus;
                }
            }
        }

        // Bandit danger penalty: -5% per bandit lurking near this road
        {
            const auto* pa = registry.try_get<Position>(road.from);
            const auto* pb = registry.try_get<Position>(road.to);
            if (pa && pb) {
                float mx = (pa->x + pb->x) * 0.5f;
                float my = (pa->y + pb->y) * 0.5f;
                int banditCount = 0;
                for (const auto& [bx, by] : s_banditPositions) {
                    float bdx = bx - mx, bdy = by - my;
                    if (bdx*bdx + bdy*bdy < 100.f * 100.f) ++banditCount;
                }
                if (banditCount > 0)
                    total *= std::max(0.2f, 1.f - 0.05f * banditCount);
            }
        }

        // Scarcity at home makes all destinations more attractive
        total += scarcityNudge;

        // Morale push: low morale pushes NPCs out, high morale anchors them
        total += moralePush;

        // Satisfaction push: unsatisfied NPCs more likely to migrate
        total += satisfactionPush;

        // Loneliness push: isolated NPCs seek communities
        total += lonelinessPush;

        // Harmony bonus: NPCs prefer socially cohesive destinations
        {
            auto hIt = s_harmonyCache.values.find(dest);
            if (hIt != s_harmonyCache.values.end() && hIt->second > 0.f) {
                total += 0.1f * hIt->second;
            }
        }

        if (total > bestScore) { bestScore = total; best = dest; }
    });
    if (cacheHour >= 0)
        s_migCache[homeSettlement] = {cacheHour, cacheDay, best};
    return best;
}

// ---- Main update ----

void AgentDecisionSystem::Update(entt::registry& registry, float realDt) {
    // Sub-block profiling setup
    using Clock = std::chrono::steady_clock;
    if (m_subProfile[0].name == nullptr) {
        const char* names[SUB_PROFILE_COUNT] = {
            "AD:PreLoop",  "AD:MainLoop", "AD:Orphan",
            "AD:Gossip",   "AD:Social",   "AD:Bandits",
            "AD:Goals"
        };
        for (int i = 0; i < SUB_PROFILE_COUNT; ++i)
            m_subProfile[i].name = names[i];
    }
    auto spStart = Clock::now();
    auto spLap   = spStart;
    auto spFlush = [&](int idx) {
        auto now = Clock::now();
        m_subProfile[idx].accumUs += std::chrono::duration<float, std::micro>(now - spLap).count();
        spLap = now;
    };

    // Charity frequency counter: counts lifetime charity acts per helper entity.
    // Pruned each frame for destroyed entities so it doesn't leak memory.
    static std::map<entt::entity, int> s_charityCount;
    for (auto it = s_charityCount.begin(); it != s_charityCount.end(); ) {
        if (!registry.valid(it->first)) it = s_charityCount.erase(it);
        else ++it;
    }

    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    const auto& tm = timeView.get<TimeManager>(*timeView.begin());
    float dt = tm.GameDt(realDt);
    if (dt <= 0.f) return;
    int currentHour = (int)tm.hourOfDay;
    static int s_frameCounter = 0;
    ++s_frameCounter;

    // ---- Per-entity settlement cache: avoids repeated try_get<HomeSettlement> in friend scans ----
    static std::unordered_map<entt::entity, entt::entity> s_entitySettlement;
    s_entitySettlement.clear();
    registry.view<HomeSettlement>().each(
        [&](entt::entity e, const HomeSettlement& hs) {
            s_entitySettlement[e] = hs.settlement;
        });

    // ---- Bandit position cache: avoids repeated view iteration for proximity checks ----
    s_banditPositions.clear();
    registry.view<BanditTag, Position>().each(
        [&](const Position& bp) {
            s_banditPositions.emplace_back(bp.x, bp.y);
        });

    // ---- Reputation decay: all NPCs drift toward 0 over time ----
    static constexpr float REP_DECAY_PER_HOUR = 0.01f;
    {
        float gameHoursDt = dt * GAME_MINS_PER_REAL_SEC / 60.f;
        float decay = REP_DECAY_PER_HOUR * gameHoursDt;
        registry.view<Reputation>().each([&](Reputation& rep) {
            if (rep.score > 0.f)
                rep.score = std::max(0.f, rep.score - decay);
            else if (rep.score < 0.f)
                rep.score = std::min(0.f, rep.score + decay);
        });
    }

    // ---- Friendship decay over distance: once per game-day ----
    {
        static int s_lastFriendDecayDay = -1;
        if (tm.day != s_lastFriendDecayDay) {
            s_lastFriendDecayDay = tm.day;
            registry.view<Relations, HomeSettlement>().each(
                [&](entt::entity e, Relations& rel, const HomeSettlement& home) {
                    for (auto& [other, aff] : rel.affinity) {
                        if (aff <= 0.f) continue;
                        auto sit = s_entitySettlement.find(other);
                        if (sit == s_entitySettlement.end() || sit->second != home.settlement) {
                            aff = std::max(0.f, aff - 0.005f);
                        }
                    }
                });
        }
    }

    // ---- Age-based skill growth: once per game-day ----
    // Adult working NPCs gain +0.001 per day in their profession's matching skill.
    // Master teaching bonus: +0.002 instead of +0.001 when a master (skill >= 0.9)
    // of the same profession is at the same settlement.
    {
        static int s_lastSkillGrowthDay = -1;
        if (tm.day != s_lastSkillGrowthDay) {
            s_lastSkillGrowthDay = tm.day;
            static constexpr float SKILL_GROWTH        = 0.001f;
            static constexpr float MASTER_SKILL_GROWTH = 0.002f;
            static constexpr float MASTER_THRESHOLD    = 0.9f;
            static constexpr float SKILL_RUST          = 0.0005f;
            static constexpr float SKILL_RUST_FLOOR    = 0.3f;
            static constexpr float LOYALTY_BONUS       = 0.0005f;

            // Build per-settlement master profession set
            // Bit flags: 1=Farmer, 2=WaterCarrier, 4=Lumberjack
            std::unordered_map<entt::entity, int> masterFlags;
            registry.view<Skills, Profession, HomeSettlement>(
                entt::exclude<ChildTag, Hauler, PlayerTag, BanditTag>).each(
                [&](const Skills& sk, const Profession& prof, const HomeSettlement& hs) {
                    if (hs.settlement == entt::null) return;
                    int flag = 0;
                    if (prof.type == ProfessionType::Farmer && sk.farming >= MASTER_THRESHOLD) flag = 1;
                    else if (prof.type == ProfessionType::WaterCarrier && sk.water_drawing >= MASTER_THRESHOLD) flag = 2;
                    else if (prof.type == ProfessionType::Lumberjack && sk.woodcutting >= MASTER_THRESHOLD) flag = 4;
                    if (flag) masterFlags[hs.settlement] |= flag;
                });

            // Pre-compute expert flags: settlement → profBits for NPCs with skill >= 0.8
            // (distinct from masterFlags which requires 0.9 — this is for teaching chain)
            std::unordered_map<entt::entity, int> expertFlags;
            registry.view<Skills, Profession, HomeSettlement>(
                entt::exclude<ChildTag, Hauler, PlayerTag, BanditTag>).each(
                [&](const Skills& sk, const Profession& prof, const HomeSettlement& hs) {
                    if (hs.settlement == entt::null) return;
                    int flag = 0;
                    if (prof.type == ProfessionType::Farmer && sk.farming >= 0.8f) flag = 1;
                    else if (prof.type == ProfessionType::WaterCarrier && sk.water_drawing >= 0.8f) flag = 2;
                    else if (prof.type == ProfessionType::Lumberjack && sk.woodcutting >= 0.8f) flag = 4;
                    if (flag) expertFlags[hs.settlement] |= flag;
                });

            // Pre-compute active mentoring: settlement → profMask of professions
            // where an elder (age > 60) AND a child (age 12-14) of matching profession coexist.
            // Used for mentorship rivalry: non-mentor skilled NPCs train harder.
            std::unordered_map<entt::entity, int> elderProfBySettl;
            registry.view<Age, Profession, HomeSettlement>(
                entt::exclude<ChildTag, Hauler, PlayerTag, BanditTag>).each(
                [&](const Age& age, const Profession& prof, const HomeSettlement& hs) {
                    if (age.days > 60.f && hs.settlement != entt::null
                        && prof.type != ProfessionType::Idle) {
                        int flag = (prof.type == ProfessionType::Farmer) ? 1 :
                                   (prof.type == ProfessionType::WaterCarrier) ? 2 :
                                   (prof.type == ProfessionType::Lumberjack) ? 4 : 0;
                        if (flag) elderProfBySettl[hs.settlement] |= flag;
                    }
                });
            std::unordered_map<entt::entity, int> childProfBySettl;
            registry.view<ChildTag, Age, Profession, HomeSettlement>().each(
                [&](auto, const Age& age, const Profession& prof, const HomeSettlement& hs) {
                    if (age.days >= 12.f && age.days <= 14.f && hs.settlement != entt::null
                        && prof.type != ProfessionType::Idle) {
                        int flag = (prof.type == ProfessionType::Farmer) ? 1 :
                                   (prof.type == ProfessionType::WaterCarrier) ? 2 :
                                   (prof.type == ProfessionType::Lumberjack) ? 4 : 0;
                        if (flag) childProfBySettl[hs.settlement] |= flag;
                    }
                });
            // Active mentoring = intersection of elder and child profession bits per settlement
            std::unordered_map<entt::entity, int> activeMentoring;
            for (const auto& [settl, elderMask] : elderProfBySettl) {
                auto cit = childProfBySettl.find(settl);
                if (cit != childProfBySettl.end()) {
                    int overlap = elderMask & cit->second;
                    if (overlap) activeMentoring[settl] = overlap;
                }
            }
            static std::mt19937 s_rivalRng{ std::random_device{}() };

            // Pre-compute skilled elders per settlement for elder wisdom boost.
            // Key: settlement → vector of (entity, profFlag) for elders with skill >= 0.8
            struct ElderInfo { entt::entity e; int profFlag; };
            std::unordered_map<entt::entity, std::vector<ElderInfo>> skilledElders;
            registry.view<Skills, Profession, Age, HomeSettlement>(
                entt::exclude<ChildTag, Hauler, PlayerTag, BanditTag>).each(
                [&](auto elder, const Skills& sk, const Profession& prof, const Age& age, const HomeSettlement& hs) {
                    if (age.days <= 60.f || hs.settlement == entt::null) return;
                    int flag = 0;
                    float skill = 0.f;
                    switch (prof.type) {
                        case ProfessionType::Farmer:      flag = 1; skill = sk.farming; break;
                        case ProfessionType::WaterCarrier: flag = 2; skill = sk.water_drawing; break;
                        case ProfessionType::Lumberjack:   flag = 4; skill = sk.woodcutting; break;
                        default: break;
                    }
                    if (flag && skill >= 0.8f)
                        skilledElders[hs.settlement].push_back({elder, flag});
                });
            static std::mt19937 s_wisdomRng{ std::random_device{}() };

            // Log RNG
            static std::mt19937 s_teachRng{ std::random_device{}() };
            auto logV2 = registry.view<EventLog>();

            registry.view<Skills, Profession, Age, HomeSettlement>(
                entt::exclude<ChildTag, Hauler, PlayerTag, BanditTag>).each(
                [&](auto e, Skills& sk, const Profession& prof, const Age& age, const HomeSettlement& hs) {
                    if (age.days > 60.f) return;  // elders don't grow skills
                    // Check if settlement has a master of this profession
                    int profFlag = (prof.type == ProfessionType::Farmer) ? 1 :
                                   (prof.type == ProfessionType::WaterCarrier) ? 2 :
                                   (prof.type == ProfessionType::Lumberjack) ? 4 : 0;
                    bool hasMaster = false;
                    if (profFlag && hs.settlement != entt::null) {
                        auto mit = masterFlags.find(hs.settlement);
                        hasMaster = (mit != masterFlags.end() && (mit->second & profFlag));
                    }
                    // Don't boost masters themselves
                    float growth = SKILL_GROWTH;
                    // Loyalty bonus: NPCs who never changed profession grow faster
                    bool loyal = (prof.prevType == prof.type || prof.prevType == ProfessionType::Idle);
                    if (loyal) growth += LOYALTY_BONUS;
                    // Mentorship rivalry: skilled NPCs (≥ 0.7) at settlements with
                    // active mentoring of their profession train 10% harder.
                    bool rivalryActive = false;
                    if (profFlag && hs.settlement != entt::null) {
                        float activeSkill = 0.f;
                        switch (prof.type) {
                            case ProfessionType::Farmer:      activeSkill = sk.farming; break;
                            case ProfessionType::WaterCarrier: activeSkill = sk.water_drawing; break;
                            case ProfessionType::Lumberjack:   activeSkill = sk.woodcutting; break;
                            default: break;
                        }
                        if (activeSkill >= 0.7f) {
                            auto ait = activeMentoring.find(hs.settlement);
                            if (ait != activeMentoring.end() && (ait->second & profFlag)) {
                                growth *= 1.1f;
                                rivalryActive = true;
                            }
                        }
                    }

                    // Elder wisdom: adult NPCs with strong affinity toward a skilled
                    // elder of the same profession get extra daily skill growth.
                    // Also tracks the highest-affinity elder as elderMentor.
                    if (profFlag && hs.settlement != entt::null) {
                        auto eit = skilledElders.find(hs.settlement);
                        if (eit != skilledElders.end()) {
                            const auto* rel = registry.try_get<Relations>(e);
                            if (rel) {
                                float bestAffinity = 0.f;
                                entt::entity bestElder = entt::null;
                                for (const auto& info : eit->second) {
                                    if (!(info.profFlag & profFlag)) continue;
                                    auto ait2 = rel->affinity.find(info.e);
                                    if (ait2 != rel->affinity.end() && ait2->second >= 0.6f) {
                                        growth += 0.0003f;
                                        // Track highest-affinity elder
                                        if (ait2->second > bestAffinity) {
                                            bestAffinity = ait2->second;
                                            bestElder = info.e;
                                        }
                                        // Log at 1-in-10
                                        if (s_wisdomRng() % 10 == 0 && !logV2.empty()) {
                                            std::string who = "An NPC";
                                            if (const auto* n = registry.try_get<Name>(e)) who = n->value;
                                            std::string elder = "an elder";
                                            if (const auto* n = registry.try_get<Name>(info.e)) elder = n->value;
                                            std::string where = "settlement";
                                            if (const auto* s = registry.try_get<Settlement>(hs.settlement)) where = s->name;
                                            char buf[180];
                                            std::snprintf(buf, sizeof(buf), "%s draws on %s's wisdom at %s",
                                                          who.c_str(), elder.c_str(), where.c_str());
                                            logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay, buf);
                                        }
                                        break;  // Only one elder wisdom bonus per NPC per day
                                    }
                                }
                                // Update elderMentor to highest-affinity skilled elder
                                if (bestElder != entt::null) {
                                    sk.elderMentor = bestElder;
                                    if (const auto* nm = registry.try_get<Name>(bestElder))
                                        sk.elderMentorName = nm->value;
                                    else
                                        sk.elderMentorName = "a wise elder";
                                }
                            }
                        }
                    }

                    // Bankruptcy survivor determination: extra growth for resilient NPCs
                    if (const auto* dt = registry.try_get<DeprivationTimer>(e)) {
                        if (dt->bankruptSurvivor) {
                            growth += 0.0002f;
                            // Log at 1-in-8 frequency (once per day per NPC)
                            if (s_teachRng() % 8 == 0 && !logV2.empty()) {
                                std::string who = "NPC";
                                if (const auto* nm = registry.try_get<Name>(e)) who = nm->value;
                                std::string where = "settlement";
                                if (hs.settlement != entt::null && registry.valid(hs.settlement))
                                    if (const auto* s = registry.try_get<Settlement>(hs.settlement))
                                        where = s->name;
                                logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay,
                                    who + " works with renewed determination at " + where + ".");
                            }
                        }
                    }

                    // Elder wisdom grief: reduced growth when mourning a wise elder
                    if (sk.wisdomGriefDays > 0.f) {
                        sk.wisdomGriefDays = std::max(0.f, sk.wisdomGriefDays - 1.f); // tick down 1 day per day
                        growth = std::max(0.f, growth - 0.0002f);
                    }

                    // Elder apprentice fast-track: accelerated growth after mentor dies
                    if (sk.tributeDays > 0.f) {
                        sk.tributeDays = std::max(0.f, sk.tributeDays - 1.f);
                        growth += 0.0003f;
                        if (s_teachRng() % 4 == 0 && !logV2.empty()) {
                            std::string who = "NPC";
                            if (const auto* nm = registry.try_get<Name>(e)) who = nm->value;
                            std::string mentorNm = sk.elderMentorName.empty() ? "a wise elder" : sk.elderMentorName;
                            std::string where = "settlement";
                            if (hs.settlement != entt::null && registry.valid(hs.settlement))
                                if (const auto* s = registry.try_get<Settlement>(hs.settlement))
                                    where = s->name;
                            logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay,
                                who + " redoubles their efforts in memory of " + mentorNm + " at " + where + ".");
                        }
                    }

                    // Mastery teaching chain: experts (skill >= 0.8) teach novices (skill < 0.5)
                    if (profFlag && hs.settlement != entt::null) {
                        float activeSkill2 = 0.f;
                        switch (prof.type) {
                            case ProfessionType::Farmer:      activeSkill2 = sk.farming; break;
                            case ProfessionType::WaterCarrier: activeSkill2 = sk.water_drawing; break;
                            case ProfessionType::Lumberjack:   activeSkill2 = sk.woodcutting; break;
                            default: break;
                        }
                        if (activeSkill2 < 0.5f) {
                            auto xit = expertFlags.find(hs.settlement);
                            if (xit != expertFlags.end() && (xit->second & profFlag)) {
                                growth += 0.0004f;
                                if (s_teachRng() % 10 == 0 && !logV2.empty()) {
                                    std::string who = "An expert";
                                    std::string novice = "a novice";
                                    if (const auto* n = registry.try_get<Name>(e)) novice = n->value;
                                    std::string where = "settlement";
                                    if (const auto* s = registry.try_get<Settlement>(hs.settlement)) where = s->name;
                                    char buf[180];
                                    std::snprintf(buf, sizeof(buf), "An expert shares tips with %s at %s",
                                                  novice.c_str(), where.c_str());
                                    logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay, buf);
                                }
                                // Jealousy reconciliation: novice with low affinity toward expert warms through learning
                                auto* novRel = registry.try_get<Relations>(e);
                                if (novRel) {
                                    for (auto& [target, aff] : novRel->affinity) {
                                        if (aff >= 0.2f) continue; // not jealousy-strained
                                        if (!registry.valid(target)) continue;
                                        const auto* tHome = registry.try_get<HomeSettlement>(target);
                                        if (!tHome || tHome->settlement != hs.settlement) continue;
                                        const auto* tProf = registry.try_get<Profession>(target);
                                        if (!tProf || tProf->type != prof.type) continue;
                                        const auto* tSk = registry.try_get<Skills>(target);
                                        if (!tSk) continue;
                                        float tSkill = 0.f;
                                        switch (prof.type) {
                                            case ProfessionType::Farmer:      tSkill = tSk->farming; break;
                                            case ProfessionType::WaterCarrier: tSkill = tSk->water_drawing; break;
                                            case ProfessionType::Lumberjack:   tSkill = tSk->woodcutting; break;
                                            default: break;
                                        }
                                        if (tSkill >= 0.8f) {
                                            aff = std::min(1.f, aff + 0.02f);
                                            if (s_teachRng() % 8 == 0 && !logV2.empty()) {
                                                std::string novName = "A novice";
                                                if (const auto* nm = registry.try_get<Name>(e)) novName = nm->value;
                                                std::string expName = "an expert";
                                                if (const auto* nm2 = registry.try_get<Name>(target)) expName = nm2->value;
                                                std::string wh = "settlement";
                                                if (const auto* s = registry.try_get<Settlement>(hs.settlement)) wh = s->name;
                                                logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay,
                                                    novName + " warms to " + expName + " through learning at " + wh + ".");
                                            }
                                            break; // only reconcile with one expert per day
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Jealousy-driven skill motivation: low affinity toward a skilled rival pushes harder training
                    if (profFlag && hs.settlement != entt::null) {
                        const auto* myRel = registry.try_get<Relations>(e);
                        if (myRel) {
                            bool jealousMotivated = false;
                            entt::entity rivalE = entt::null;
                            for (const auto& [target, aff] : myRel->affinity) {
                                if (aff >= 0.1f) continue; // not jealous
                                if (!registry.valid(target)) continue;
                                const auto* tHome = registry.try_get<HomeSettlement>(target);
                                if (!tHome || tHome->settlement != hs.settlement) continue;
                                const auto* tProf = registry.try_get<Profession>(target);
                                if (!tProf || tProf->type != prof.type) continue;
                                const auto* tSk = registry.try_get<Skills>(target);
                                if (!tSk) continue;
                                float rivalSkill = 0.f;
                                switch (prof.type) {
                                    case ProfessionType::Farmer:      rivalSkill = tSk->farming; break;
                                    case ProfessionType::WaterCarrier: rivalSkill = tSk->water_drawing; break;
                                    case ProfessionType::Lumberjack:   rivalSkill = tSk->woodcutting; break;
                                    default: break;
                                }
                                if (rivalSkill >= 0.8f) {
                                    jealousMotivated = true;
                                    rivalE = target;
                                    break;
                                }
                            }
                            if (jealousMotivated) {
                                growth += 0.0003f;
                                if (s_teachRng() % 12 == 0 && !logV2.empty()) {
                                    std::string who = "NPC";
                                    if (const auto* nm = registry.try_get<Name>(e)) who = nm->value;
                                    std::string rival = "a rival";
                                    if (rivalE != entt::null)
                                        if (const auto* rn = registry.try_get<Name>(rivalE)) rival = rn->value;
                                    std::string where = "settlement";
                                    if (registry.valid(hs.settlement))
                                        if (const auto* s = registry.try_get<Settlement>(hs.settlement))
                                            where = s->name;
                                    logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay,
                                        who + " trains harder to surpass " + rival + " at " + where + ".");
                                }
                            }
                        }
                    }

                    // Capture pre-growth skill for loyalty streak crossing detection
                    float preActiveSkill = 0.f;
                    switch (prof.type) {
                        case ProfessionType::Farmer:      preActiveSkill = sk.farming; break;
                        case ProfessionType::WaterCarrier: preActiveSkill = sk.water_drawing; break;
                        case ProfessionType::Lumberjack:   preActiveSkill = sk.woodcutting; break;
                        default: break;
                    }
                    switch (prof.type) {
                        case ProfessionType::Farmer:
                            if (hasMaster && sk.farming < MASTER_THRESHOLD) growth = MASTER_SKILL_GROWTH + (loyal ? LOYALTY_BONUS : 0.f);
                            sk.farming = std::min(1.f, sk.farming + growth);
                            break;
                        case ProfessionType::WaterCarrier:
                            if (hasMaster && sk.water_drawing < MASTER_THRESHOLD) growth = MASTER_SKILL_GROWTH + (loyal ? LOYALTY_BONUS : 0.f);
                            sk.water_drawing = std::min(1.f, sk.water_drawing + growth);
                            break;
                        case ProfessionType::Lumberjack:
                            if (hasMaster && sk.woodcutting < MASTER_THRESHOLD) growth = MASTER_SKILL_GROWTH + (loyal ? LOYALTY_BONUS : 0.f);
                            sk.woodcutting = std::min(1.f, sk.woodcutting + growth);
                            break;
                        default: break;
                    }
                    // Loyalty streak: log when a loyal NPC crosses the 0.7 skill threshold
                    {
                        float postActiveSkill = 0.f;
                        const char* profName = nullptr;
                        switch (prof.type) {
                            case ProfessionType::Farmer:      postActiveSkill = sk.farming; profName = "farmer"; break;
                            case ProfessionType::WaterCarrier: postActiveSkill = sk.water_drawing; profName = "water-carrier"; break;
                            case ProfessionType::Lumberjack:   postActiveSkill = sk.woodcutting; profName = "lumberjack"; break;
                            default: break;
                        }
                        if (loyal && profName && preActiveSkill < 0.7f && postActiveSkill >= 0.7f
                            && !logV2.empty() && s_teachRng() % 5 == 0) {
                            std::string who = "NPC";
                            if (const auto* nm = registry.try_get<Name>(e)) who = nm->value;
                            std::string where = "settlement";
                            if (hs.settlement != entt::null && registry.valid(hs.settlement))
                                if (const auto* s = registry.try_get<Settlement>(hs.settlement))
                                    where = s->name;
                            logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay,
                                who + " is a dedicated " + profName + " at " + where + ".");
                        }
                    }
                    // Skill recovery celebration: log when active skill rises back above 0.5
                    {
                        float postActiveSkill2 = 0.f;
                        const char* recSkill = nullptr;
                        switch (prof.type) {
                            case ProfessionType::Farmer:      postActiveSkill2 = sk.farming; recSkill = "farming"; break;
                            case ProfessionType::WaterCarrier: postActiveSkill2 = sk.water_drawing; recSkill = "water-drawing"; break;
                            case ProfessionType::Lumberjack:   postActiveSkill2 = sk.woodcutting; recSkill = "woodcutting"; break;
                            default: break;
                        }
                        if (recSkill && preActiveSkill < 0.5f && postActiveSkill2 >= 0.5f) {
                            // Morale boost: recovering NPCs lift community spirits
                            if (hs.settlement != entt::null && registry.valid(hs.settlement))
                                if (auto* settl = registry.try_get<Settlement>(hs.settlement))
                                    settl->morale = std::min(1.f, settl->morale + 0.02f);
                            // Log at 1-in-5 frequency
                            if (!logV2.empty() && s_teachRng() % 5 == 0) {
                                std::string who = "NPC";
                                if (const auto* nm = registry.try_get<Name>(e)) who = nm->value;
                                std::string where = "settlement";
                                if (hs.settlement != entt::null && registry.valid(hs.settlement))
                                    if (const auto* s = registry.try_get<Settlement>(hs.settlement))
                                        where = s->name;
                                logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay,
                                    who + " regains their " + recSkill + " proficiency at " + where + ".");
                            }
                        }
                    }
                    // Profession pride announcement: when skill crosses 0.8, boost same-prof affinity
                    {
                        float postSkill3 = 0.f;
                        const char* prideProfName = nullptr;
                        switch (prof.type) {
                            case ProfessionType::Farmer:      postSkill3 = sk.farming; prideProfName = "farming"; break;
                            case ProfessionType::WaterCarrier: postSkill3 = sk.water_drawing; prideProfName = "water-carrying"; break;
                            case ProfessionType::Lumberjack:   postSkill3 = sk.woodcutting; prideProfName = "woodcutting"; break;
                            default: break;
                        }
                        if (prideProfName && preActiveSkill < 0.8f && postSkill3 >= 0.8f) {
                            // Boost affinity +0.02 toward all same-profession NPCs at same settlement
                            auto* myRel = registry.try_get<Relations>(e);
                            if (myRel && hs.settlement != entt::null) {
                                registry.view<Profession, HomeSettlement, Relations>(
                                    entt::exclude<ChildTag, PlayerTag, BanditTag>).each(
                                    [&](auto other, const Profession& oPr, const HomeSettlement& oHs, Relations& oRel) {
                                        if (other == e) return;
                                        if (oHs.settlement != hs.settlement) return;
                                        if (oPr.type != prof.type) return;
                                        myRel->affinity[other] = std::min(1.f, myRel->affinity[other] + 0.02f);
                                        oRel.affinity[e] = std::min(1.f, oRel.affinity[e] + 0.02f);
                                    });
                            }
                            // Log at 1-in-3 frequency
                            if (!logV2.empty() && s_teachRng() % 3 == 0) {
                                std::string who = "NPC";
                                if (const auto* nm = registry.try_get<Name>(e)) who = nm->value;
                                std::string where = "settlement";
                                if (hs.settlement != entt::null && registry.valid(hs.settlement))
                                    if (const auto* s = registry.try_get<Settlement>(hs.settlement))
                                        where = s->name;
                                char buf[180];
                                std::snprintf(buf, sizeof(buf), "%s proudly declares mastery of %s at %s",
                                              who.c_str(), prideProfName, where.c_str());
                                logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay, buf);
                            }
                            // Wisdom lineage: NPC carries on a deceased elder's legacy
                            if (sk.wisdomLineage != entt::null) {
                                if (!logV2.empty()) {
                                    std::string who2 = "NPC";
                                    if (const auto* nm = registry.try_get<Name>(e)) who2 = nm->value;
                                    std::string elderName2 = sk.wisdomLineageName.empty() ? "a wise elder" : sk.wisdomLineageName;
                                    std::string where2 = "settlement";
                                    if (hs.settlement != entt::null && registry.valid(hs.settlement))
                                        if (const auto* s = registry.try_get<Settlement>(hs.settlement))
                                            where2 = s->name;
                                    logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay,
                                        who2 + " carries on " + elderName2 + "'s legacy at " + where2 + ".");
                                }
                                sk.wisdomLineage = entt::null;
                                sk.wisdomLineageName.clear();
                            }
                            // Profession pride jealousy: nearby same-profession NPCs with skill 0.6–0.79 may envy
                            if (hs.settlement != entt::null) {
                                registry.view<Profession, Skills, HomeSettlement, Relations>(
                                    entt::exclude<ChildTag, PlayerTag, BanditTag>).each(
                                    [&](auto jealousE, const Profession& jPr, const Skills& jSk,
                                        const HomeSettlement& jHs, Relations& jRel) {
                                        if (jealousE == e) return;
                                        if (jHs.settlement != hs.settlement) return;
                                        if (jPr.type != prof.type) return;
                                        float jSkill = 0.f;
                                        switch (jPr.type) {
                                            case ProfessionType::Farmer:      jSkill = jSk.farming; break;
                                            case ProfessionType::WaterCarrier: jSkill = jSk.water_drawing; break;
                                            case ProfessionType::Lumberjack:   jSkill = jSk.woodcutting; break;
                                            default: break;
                                        }
                                        if (jSkill < 0.6f || jSkill >= 0.8f) return;
                                        // 1-in-4 chance to feel jealousy
                                        if (s_teachRng() % 4 != 0) return;
                                        jRel.affinity[e] = std::max(0.f, jRel.affinity[e] - 0.01f);
                                        // Log at 1-in-6 frequency
                                        if (s_teachRng() % 6 == 0 && !logV2.empty()) {
                                            std::string jName = "An NPC";
                                            if (const auto* nm = registry.try_get<Name>(jealousE)) jName = nm->value;
                                            std::string mName = "a master";
                                            if (const auto* nm = registry.try_get<Name>(e)) mName = nm->value;
                                            std::string where2 = "settlement";
                                            if (const auto* s = registry.try_get<Settlement>(hs.settlement))
                                                where2 = s->name;
                                            char jbuf[180];
                                            std::snprintf(jbuf, sizeof(jbuf), "%s envies %s's skill at %s",
                                                          jName.c_str(), mName.c_str(), where2.c_str());
                                            logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay, jbuf);
                                        }
                                    });
                            }
                        }
                    }
                    // Master retention: mark NPC as settled master when any skill reaches 0.9
                    if (!registry.all_of<Hauler>(e)) {
                        if (auto* dt = registry.try_get<DeprivationTimer>(e)) {
                            if (!dt->masterSettled && (sk.farming >= MASTER_THRESHOLD ||
                                sk.water_drawing >= MASTER_THRESHOLD || sk.woodcutting >= MASTER_THRESHOLD))
                                dt->masterSettled = true;
                        }
                    }
                    // Skill rust: inactive skills decay slowly (floor 0.3)
                    float preFarm = sk.farming, preWater = sk.water_drawing, preWood = sk.woodcutting;
                    if (prof.type != ProfessionType::Farmer)
                        sk.farming = std::max(SKILL_RUST_FLOOR, sk.farming - SKILL_RUST);
                    if (prof.type != ProfessionType::WaterCarrier)
                        sk.water_drawing = std::max(SKILL_RUST_FLOOR, sk.water_drawing - SKILL_RUST);
                    if (prof.type != ProfessionType::Lumberjack)
                        sk.woodcutting = std::max(SKILL_RUST_FLOOR, sk.woodcutting - SKILL_RUST);

                    // Skill rust notification: log when a skill drops below 0.5
                    if (!logV2.empty() && s_teachRng() % 5 == 0) {
                        const char* rustSkill = nullptr;
                        if (preFarm >= 0.5f && sk.farming < 0.5f) rustSkill = "farming";
                        else if (preWater >= 0.5f && sk.water_drawing < 0.5f) rustSkill = "water-drawing";
                        else if (preWood >= 0.5f && sk.woodcutting < 0.5f) rustSkill = "woodcutting";
                        if (rustSkill) {
                            std::string who = "NPC";
                            if (const auto* nm = registry.try_get<Name>(e)) who = nm->value;
                            std::string where = "settlement";
                            if (hs.settlement != entt::null && registry.valid(hs.settlement))
                                if (const auto* s = registry.try_get<Settlement>(hs.settlement))
                                    where = s->name;
                            logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay,
                                who + "'s " + rustSkill + " is getting rusty at " + where + ".");
                        }
                    }

                    // Log at 1-in-10 frequency
                    if (hasMaster && growth > SKILL_GROWTH && !logV2.empty()) {
                        if (s_teachRng() % 10 == 0) {
                            const auto* nm = registry.try_get<Name>(e);
                            std::string settlName = "settlement";
                            if (hs.settlement != entt::null && registry.valid(hs.settlement))
                                if (const auto* s = registry.try_get<Settlement>(hs.settlement))
                                    settlName = s->name;
                            const char* skillName = (prof.type == ProfessionType::Farmer) ? "farming" :
                                                    (prof.type == ProfessionType::WaterCarrier) ? "water-drawing" :
                                                    "woodcutting";
                            std::string msg = "A master inspires " +
                                std::string(nm ? nm->value : "an NPC") +
                                "'s " + skillName + " at " + settlName + ".";
                            logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay, msg);
                        }
                    }
                    // Mentorship rivalry log: 1-in-8 frequency
                    if (rivalryActive && !logV2.empty() && s_rivalRng() % 8 == 0) {
                        std::string who = "NPC";
                        if (const auto* nm = registry.try_get<Name>(e)) who = nm->value;
                        // Find the mentor (elder) name at this settlement
                        std::string mentorName = "a mentor";
                        registry.view<Age, Profession, HomeSettlement, Name>(
                            entt::exclude<ChildTag, Hauler, PlayerTag, BanditTag>).each(
                            [&](const Age& mAge, const Profession& mProf, const HomeSettlement& mHs, const Name& mNm) {
                                if (mAge.days > 60.f && mProf.type == prof.type
                                    && mHs.settlement == hs.settlement) {
                                    mentorName = mNm.value;
                                }
                            });
                        logV2.get<EventLog>(*logV2.begin()).Push(tm.day, (int)tm.hourOfDay,
                            who + " trains harder, inspired by " + mentorName + "'s teaching.");
                    }
                });
        }
    }

    // ---- Mourning procession: 3+ grieving NPCs at same settlement gather to honour the fallen elder ----
    {
        static std::set<entt::entity> s_honouredElders;
        static int s_lastProcessionDay = -1;
        if (tm.day != s_lastProcessionDay) {
            s_lastProcessionDay = tm.day;

            // Count grieving NPCs per settlement, tracking which elder(s) they mourn
            struct GriefInfo { entt::entity elder; int count; };
            std::unordered_map<entt::entity, GriefInfo> griefBySettl; // settlement → info
            registry.view<Skills, HomeSettlement, AgentState, DeprivationTimer>(
                entt::exclude<Hauler, PlayerTag, BanditTag>).each(
                [&](auto e, const Skills& sk, const HomeSettlement& hs, const AgentState&, const DeprivationTimer&) {
                    if (sk.wisdomGriefDays <= 0.f || hs.settlement == entt::null) return;
                    auto& gi = griefBySettl[hs.settlement];
                    gi.count++;
                    if (sk.wisdomLineage != entt::null)
                        gi.elder = sk.wisdomLineage; // last one wins — all mourn the same elder
                });

            auto logVP = registry.view<EventLog>();
            for (auto& [settlE, gi] : griefBySettl) {
                if (gi.count < 3) continue;
                if (gi.elder != entt::null && s_honouredElders.count(gi.elder)) continue;
                if (gi.elder != entt::null)
                    s_honouredElders.insert(gi.elder);

                // Set all grieving NPCs at this settlement to Celebrating for 1 game-hour
                // and boost mutual affinity +0.02
                std::vector<entt::entity> participants;
                registry.view<Skills, HomeSettlement, AgentState, DeprivationTimer>(
                    entt::exclude<Hauler, PlayerTag, BanditTag>).each(
                    [&](auto e, const Skills& sk, const HomeSettlement& hs, AgentState& st, DeprivationTimer& dt) {
                        if (sk.wisdomGriefDays <= 0.f || hs.settlement != settlE) return;
                        st.behavior = AgentBehavior::Celebrating;
                        dt.skillCelebrateTimer = 1.0f; // 1 game-hour
                        participants.push_back(e);
                    });

                // Mutual affinity boost among participants
                for (size_t i = 0; i < participants.size(); ++i) {
                    auto* relA = registry.try_get<Relations>(participants[i]);
                    if (!relA) continue;
                    for (size_t j = i + 1; j < participants.size(); ++j) {
                        auto* relB = registry.try_get<Relations>(participants[j]);
                        relA->affinity[participants[j]] = std::min(1.f, relA->affinity[participants[j]] + 0.02f);
                        if (relB)
                            relB->affinity[participants[i]] = std::min(1.f, relB->affinity[participants[i]] + 0.02f);
                    }
                }

                // Log once
                if (!logVP.empty()) {
                    std::string where = "a settlement";
                    if (registry.valid(settlE))
                        if (const auto* s = registry.try_get<Settlement>(settlE))
                            where = s->name;
                    std::string elderName = "an elder";
                    // Try to get elder name from one of the participants' wisdomLineageName
                    for (auto p : participants) {
                        const auto* pSk = registry.try_get<Skills>(p);
                        if (pSk && !pSk->wisdomLineageName.empty()) {
                            elderName = pSk->wisdomLineageName;
                            break;
                        }
                    }
                    logVP.get<EventLog>(*logVP.begin()).Push(tm.day, (int)tm.hourOfDay,
                        where + " gathers to honour " + elderName + "'s memory.");
                }
                // Morale boost from collective mourning
                if (registry.valid(settlE)) {
                    if (auto* settl = registry.try_get<Settlement>(settlE))
                        settl->morale = std::min(1.f, settl->morale + 0.02f);
                }
                // Log community healing at 1-in-3 frequency
                static std::mt19937 s_procMoraleRng{ std::random_device{}() };
                if (s_procMoraleRng() % 3 == 0 && !logVP.empty()) {
                    std::string where2 = "A settlement";
                    if (registry.valid(settlE))
                        if (const auto* s = registry.try_get<Settlement>(settlE))
                            where2 = s->name;
                    logVP.get<EventLog>(*logVP.begin()).Push(tm.day, (int)tm.hourOfDay,
                        where2 + " finds solace in shared grief.");
                }
            }

            // Prune s_honouredElders to prevent unbounded growth (keep last 50)
            while (s_honouredElders.size() > 50)
                s_honouredElders.erase(s_honouredElders.begin());
        }
    }

    // ---- Mentor-apprentice: elders (age > 60) mentor children (age 12-14) at the same settlement ----
    {
        static int s_lastMentorDay = -1;
        if (tm.day != s_lastMentorDay) {
            s_lastMentorDay = tm.day;
            static constexpr float MENTOR_SKILL_GROWTH = 0.003f;
            static std::mt19937 s_mentorRng{ std::random_device{}() };

            // Build per-settlement elder profession set: settlement → vector of (entity, profType)
            struct ElderInfo { entt::entity e; ProfessionType prof; };
            std::unordered_map<entt::entity, std::vector<ElderInfo>> eldersBySettl;
            registry.view<Age, Profession, HomeSettlement>(
                entt::exclude<ChildTag, Hauler, PlayerTag, BanditTag>).each(
                [&](auto e, const Age& age, const Profession& prof, const HomeSettlement& hs) {
                    if (age.days > 60.f && hs.settlement != entt::null
                        && prof.type != ProfessionType::Idle)
                        eldersBySettl[hs.settlement].push_back({e, prof.type});
                });

            // For each child apprentice (age 12-14), find a matching elder mentor
            auto logV = registry.view<EventLog>();
            auto childView = registry.view<ChildTag, Age, Skills, Profession, HomeSettlement, Name>();
            for (auto child : childView) {
                const auto& age = childView.get<Age>(child);
                auto& sk = childView.get<Skills>(child);
                const auto& prof = childView.get<Profession>(child);
                const auto& hs = childView.get<HomeSettlement>(child);
                const auto& childName = childView.get<Name>(child);
                {
                    if (age.days < 12.f || age.days > 14.f) return;
                    if (hs.settlement == entt::null) return;
                    if (prof.type == ProfessionType::Idle) return;
                    auto it = eldersBySettl.find(hs.settlement);
                    if (it == eldersBySettl.end()) return;

                    // Find an elder with matching profession
                    for (const auto& elder : it->second) {
                        if (elder.prof != prof.type) continue;
                        // Apply mentor skill growth
                        switch (prof.type) {
                            case ProfessionType::Farmer:
                                sk.farming = std::min(1.f, sk.farming + MENTOR_SKILL_GROWTH); break;
                            case ProfessionType::WaterCarrier:
                                sk.water_drawing = std::min(1.f, sk.water_drawing + MENTOR_SKILL_GROWTH); break;
                            case ProfessionType::Lumberjack:
                                sk.woodcutting = std::min(1.f, sk.woodcutting + MENTOR_SKILL_GROWTH); break;
                            default: break;
                        }
                        // Log at 1-in-5 frequency
                        if (!logV.empty() && s_mentorRng() % 5 == 0) {
                            std::string elderName = "An elder";
                            if (const auto* nm = registry.try_get<Name>(elder.e))
                                elderName = nm->value;
                            const char* skillName = (prof.type == ProfessionType::Farmer) ? "farming" :
                                                    (prof.type == ProfessionType::WaterCarrier) ? "water-drawing" :
                                                    "woodcutting";
                            std::string settlName = "settlement";
                            if (registry.valid(hs.settlement))
                                if (const auto* s = registry.try_get<Settlement>(hs.settlement))
                                    settlName = s->name;
                            logV.get<EventLog>(*logV.begin()).Push(tm.day, (int)tm.hourOfDay,
                                elderName + " mentors " + childName.value + " in " +
                                skillName + " at " + settlName + ".");
                        }
                        break; // one mentor per child per day
                    }
                }
            }
        }
    }

    // ---- Wealthy NPC celebration: one-time log when balance crosses 500g ----
    {
        auto logV = registry.view<EventLog>();
        registry.view<Money, DeprivationTimer, HomeSettlement, Name>(
            entt::exclude<PlayerTag, Hauler, BanditTag>).each(
            [&](auto e, const Money& m, DeprivationTimer& tmr,
                const HomeSettlement& hs, const Name& name) {
            if (tmr.wealthCelebrated) return;
            if (m.balance < 500.f) return;
            tmr.wealthCelebrated = true;
            if (!logV.empty()) {
                std::string stl = "their settlement";
                if (hs.settlement != entt::null && registry.valid(hs.settlement))
                    if (const auto* s = registry.try_get<Settlement>(hs.settlement))
                        stl = s->name;
                char buf[160];
                std::snprintf(buf, sizeof(buf), "%s has become wealthy at %s!",
                              name.value.c_str(), stl.c_str());
                logV.get<EventLog>(*logV.begin()).Push(tm.day, currentHour, buf);
            }
        });
    }

    // Cache player position for NPC-to-player proximity checks
    entt::entity playerEntity = entt::null;
    Position playerPos{};
    {
        auto pv = registry.view<PlayerTag, Position>();
        if (pv.begin() != pv.end()) {
            playerEntity = *pv.begin();
            playerPos = pv.get<Position>(playerEntity);
        }
    }

    // Pre-compute grieving NPCs per settlement for vigil gathering
    std::map<entt::entity, std::vector<entt::entity>> grievingBySettlement;
    registry.view<HomeSettlement, DeprivationTimer>(
        entt::exclude<Hauler, PlayerTag, BanditTag>).each(
        [&](auto e, const HomeSettlement& hs, const DeprivationTimer& tmr) {
        if (tmr.griefTimer > 0.f && hs.settlement != entt::null && registry.valid(hs.settlement))
            grievingBySettlement[hs.settlement].push_back(e);
    });

    spFlush(0); // AD:PreLoop

    // Exclude Haulers (TransportSystem handles them), Player (PlayerInputSystem),
    // and Bandits (handled in the bandit section at the end of Update).
    auto view = registry.view<Needs, AgentState, Position, Velocity,
                               MoveSpeed, HomeSettlement, DeprivationTimer>(
                    entt::exclude<Hauler, PlayerTag, BanditTag>);

    for (auto entity : view) {
        auto& needs  = view.get<Needs>(entity);
        auto& state  = view.get<AgentState>(entity);
        auto& pos    = view.get<Position>(entity);
        auto& vel    = view.get<Velocity>(entity);
        float speed  = view.get<MoveSpeed>(entity).value;
        auto& home   = view.get<HomeSettlement>(entity);
        auto& timer  = view.get<DeprivationTimer>(entity);

        // ============================================================
        // SLEEPING: ScheduleSystem owns this state — skip entirely
        // ============================================================
        if (state.behavior == AgentBehavior::Sleeping) continue;

        // ============================================================
        // PANIC: flee from bandits — skip normal decisions while active
        // ============================================================
        if (timer.panicTimer > 0.f) {
            timer.panicTimer -= realDt;
            if (timer.panicTimer <= 0.f) {
                timer.panicTimer = 0.f;
                vel.vx = vel.vy = 0.f;
            }
            continue;
        }

        // ============================================================
        // POST-THEFT FLEE: sprint away from home settlement
        // ============================================================
        if (timer.fleeTimer > 0.f) {
            timer.fleeTimer -= realDt;
            if (home.settlement != entt::null && registry.valid(home.settlement)) {
                if (const auto* sp = registry.try_get<Position>(home.settlement)) {
                    float dx = pos.x - sp->x;
                    float dy = pos.y - sp->y;
                    float len = std::sqrt(dx * dx + dy * dy);
                    if (len > 1.f) {
                        vel.vx = (dx / len) * speed;
                        vel.vy = (dy / len) * speed;
                    }
                }
            }
            continue;  // skip all other decision-making while fleeing
        }

        // ============================================================
        // FAMILY VISIT: NPC is travelling to visit family at another settlement
        // ============================================================
        if (timer.visitTimer > 0.f) {
            float gameMinDt = dt * GAME_MINS_PER_REAL_SEC;
            timer.visitTimer -= gameMinDt;
            if (timer.visitTimer <= 0.f) {
                // Visit over — return home
                timer.visitTimer  = 0.f;
                timer.visitTarget = entt::null;
                if (home.settlement != entt::null && registry.valid(home.settlement)) {
                    const auto& homePos = registry.get<Position>(home.settlement);
                    MoveToward(vel, pos, homePos.x, homePos.y, speed);
                }
            } else if (timer.visitTarget != entt::null && registry.valid(timer.visitTarget)) {
                const auto& tgtPos = registry.get<Position>(timer.visitTarget);
                float vdx = tgtPos.x - pos.x, vdy = tgtPos.y - pos.y;
                if (vdx*vdx + vdy*vdy > 30.f * 30.f)
                    MoveToward(vel, pos, tgtPos.x, tgtPos.y, speed * 0.8f);
                else
                    vel.vx = vel.vy = 0.f;  // arrived — wait out the timer
            }
            continue;
        }

        // ============================================================
        // CELEBRATING: move toward settlement centre at half speed.
        // Stays active while the home settlement has the "Festival" modifier.
        // Reverts to Idle when the festival ends.
        // ============================================================
        if (state.behavior == AgentBehavior::Celebrating) {
            // Personal celebration from a completed goal takes priority.
            bool personalCelebration = false;
            if (const auto* g = registry.try_get<Goal>(entity))
                personalCelebration = (g->celebrateTimer > 0.f);

            // Skill milestone celebration (drain timer)
            bool skillCelebration = false;
            {
                float ghDt = dt * GAME_MINS_PER_REAL_SEC / 60.f;
                if (timer.skillCelebrateTimer > 0.f) {
                    timer.skillCelebrateTimer = std::max(0.f, timer.skillCelebrateTimer - ghDt);
                    skillCelebration = (timer.skillCelebrateTimer > 0.f);
                }
            }

            // Nearby friends join skill celebration (staggered: 1/4 of NPCs per frame)
            if (skillCelebration && (static_cast<uint32_t>(entity) % 4 == static_cast<uint32_t>(s_frameCounter) % 4)) {
                const auto* myRel = registry.try_get<Relations>(entity);
                if (myRel) {
                    registry.view<AgentState, Position, DeprivationTimer>(
                        entt::exclude<Hauler, PlayerTag, BanditTag>).each(
                        [&](auto other, AgentState& oState, const Position& oPos, DeprivationTimer& oTimer) {
                            if (other == entity) return;
                            if (oState.behavior != AgentBehavior::Idle) return;
                            if (oTimer.skillCelebrateTimer > 0.f) return; // already celebrating
                            float ddx = oPos.x - pos.x, ddy = oPos.y - pos.y;
                            if (ddx * ddx + ddy * ddy > 30.f * 30.f) return;
                            auto it = myRel->affinity.find(other);
                            if (it == myRel->affinity.end() || it->second < 0.2f) return;
                            // Recruit friend into celebration
                            oState.behavior = AgentBehavior::Celebrating;
                            oTimer.skillCelebrateTimer = 0.25f;
                            // Log
                            auto lv = registry.view<EventLog>();
                            if (lv.begin() != lv.end()) {
                                const auto* oName = registry.try_get<Name>(other);
                                const auto* myName = registry.try_get<Name>(entity);
                                std::string msg = (oName ? oName->value : "An NPC") +
                                    " joins " + (myName ? myName->value : "an NPC") +
                                    "'s celebration.";
                                lv.get<EventLog>(*lv.begin()).Push(
                                    tm.day, (int)tm.hourOfDay, msg);
                            }
                        });
                }
            }

            // Check if the festival is still active at home settlement
            bool festivalActive = false;
            if (home.settlement != entt::null && registry.valid(home.settlement)) {
                if (const auto* s = registry.try_get<Settlement>(home.settlement))
                    festivalActive = (s->modifierName == "Festival");
            }
            if (!festivalActive && !personalCelebration && !skillCelebration) {
                state.behavior = AgentBehavior::Idle;
                // Fall through to normal decision-making below
            } else {
                // Leave celebration if any need becomes critical (same check as WORKING)
                bool anyCritical = false;
                for (const auto& n : needs.list)
                    if (n.value < n.criticalThreshold) { anyCritical = true; break; }
                if (anyCritical) {
                    state.behavior = AgentBehavior::Idle;
                    // Fall through to normal seeking below; re-enters Celebrating next tick
                    // when no longer critical (festival still active)
                } else {
                    // Drift toward settlement centre at half speed
                    if (home.settlement != entt::null && registry.valid(home.settlement)) {
                        const auto& homePos = registry.get<Position>(home.settlement);
                        static constexpr float CELEBRATE_ARRIVE = 45.f;
                        float dx = homePos.x - pos.x, dy = homePos.y - pos.y;
                        if (dx*dx + dy*dy > CELEBRATE_ARRIVE * CELEBRATE_ARRIVE)
                            MoveToward(vel, pos, homePos.x, homePos.y, speed * 0.5f);
                        else
                            vel.vx = vel.vy = 0.f;
                    } else {
                        vel.vx = vel.vy = 0.f;
                    }
                    continue;
                }
            }
        }

        // ============================================================
        // WORKING: only interrupt if a need is critical
        // ============================================================
        if (state.behavior == AgentBehavior::Working) {
            bool anyCritical = false;
            for (const auto& n : needs.list)
                if (n.value < n.criticalThreshold) { anyCritical = true; break; }
            if (!anyCritical) continue;
            // Critical need — fall through to seeking logic below
            state.behavior = AgentBehavior::Idle;
        }

        // ============================================================
        // MIGRATING: move toward target settlement, settle on arrival
        // ============================================================
        if (state.behavior == AgentBehavior::Migrating) {
            if (state.target == entt::null || !registry.valid(state.target)) {
                state.behavior = AgentBehavior::Idle;
                vel.vx = vel.vy = 0.f;
                continue;
            }
            const auto& destPos = registry.get<Position>(state.target);
            if (InRange(pos.x, pos.y, destPos.x, destPos.y, SETTLE_RANGE)) {
                // Arrived — adopt new home
                home.settlement       = state.target;
                timer.stockpileEmpty  = 0.f;
                timer.homesickTimer   = 0.f;
                state.behavior        = AgentBehavior::Idle;
                state.target          = entt::null;
                vel.vx = vel.vy       = 0.f;

                // Record new settlement's prices in migration memory on arrival.
                if (auto* mem = registry.try_get<MigrationMemory>(entity)) {
                    if (home.settlement != entt::null && registry.valid(home.settlement)) {
                        if (const auto* mkt = registry.try_get<Market>(home.settlement))
                            if (const auto* stt = registry.try_get<Settlement>(home.settlement))
                                mem->Record(stt->name,
                                    mkt->GetPrice(ResourceType::Food),
                                    mkt->GetPrice(ResourceType::Water),
                                    mkt->GetPrice(ResourceType::Wood), tm.day);
                    }
                }

                // Adopt the profession of the destination settlement's primary facility.
                // Primary = highest baseRate among facilities belonging to this settlement.
                if (home.settlement != entt::null && registry.valid(home.settlement)) {
                    entt::entity bestFac = entt::null;
                    float bestRate = 0.f;
                    registry.view<ProductionFacility>().each(
                        [&](auto fe, const ProductionFacility& pf) {
                        if (pf.settlement == home.settlement && pf.baseRate > bestRate) {
                            bestRate = pf.baseRate;
                            bestFac  = fe;
                        }
                    });
                    if (bestFac != entt::null) {
                        const auto& pf = registry.get<ProductionFacility>(bestFac);
                        if (auto* prof = registry.try_get<Profession>(entity)) {
                            ProfessionType oldType = prof->type;
                            ProfessionType newType = ProfessionForResource(pf.output);
                            prof->type = newType;

                            // Skill adjustment on profession change: halve old, boost new by 10%
                            if (oldType != newType && oldType != ProfessionType::Idle
                                && newType != ProfessionType::Idle) {
                                if (auto* sk = registry.try_get<Skills>(entity)) {
                                    // Map profession → resource for skill lookup
                                    auto profToRes = [](ProfessionType p) -> ResourceType {
                                        switch (p) {
                                            case ProfessionType::Farmer:       return ResourceType::Food;
                                            case ProfessionType::WaterCarrier: return ResourceType::Water;
                                            case ProfessionType::Lumberjack:   return ResourceType::Wood;
                                            default:                           return ResourceType::Food;
                                        }
                                    };
                                    ResourceType oldRes = profToRes(oldType);
                                    ResourceType newRes = profToRes(newType);
                                    // Halve old skill
                                    float oldVal = sk->ForResource(oldRes);
                                    sk->Advance(oldRes, -(oldVal * 0.5f));
                                    // Boost new skill by 10% (capped at 1.0 by Advance)
                                    sk->Advance(newRes, 0.1f);
                                }
                            }
                        }
                    }
                }

                // Log arrival with profession
                {
                    auto lv = registry.view<EventLog>();
                    if (!lv.empty()) {
                        auto tmv2 = registry.view<TimeManager>();
                        if (!tmv2.empty()) {
                            auto& tm2 = tmv2.get<TimeManager>(*tmv2.begin());
                            std::string who = "Someone";
                            if (const auto* n = registry.try_get<Name>(entity))
                                who = n->value;
                            const auto* prof = registry.try_get<Profession>(entity);
                            if (prof && prof->type != ProfessionType::Idle) {
                                who += " (";
                                who += ProfessionLabel(prof->type);
                                who += ")";
                            }
                            std::string dest = "unknown";
                            if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                dest = s->name;
                            lv.get<EventLog>(*lv.begin()).Push(
                                tm2.day, (int)tm2.hourOfDay,
                                who + " moved to " + dest);

                            // Welcome log from destination's perspective
                            int newPop = 0;
                            registry.view<HomeSettlement>(entt::exclude<PlayerTag, Hauler>).each(
                                [&](const HomeSettlement& hs) {
                                    if (hs.settlement == home.settlement) ++newPop;
                                });
                            char wbuf[160];
                            std::snprintf(wbuf, sizeof(wbuf),
                                "%s welcomes %s — pop now %d",
                                dest.c_str(), who.c_str(), newPop);
                            lv.get<EventLog>(*lv.begin()).Push(
                                tm2.day, (int)tm2.hourOfDay, wbuf);
                        }
                    }
                }

                // Master homecoming: log when a master-level NPC settles at a new settlement
                if (const auto* dt = registry.try_get<DeprivationTimer>(entity)) {
                    if (dt->masterSettled) {
                        const auto* sk = registry.try_get<Skills>(entity);
                        if (sk) {
                            const char* masterSkill = (sk->farming >= 0.9f)       ? "farmer"       :
                                                      (sk->water_drawing >= 0.9f) ? "water-drawer" :
                                                      (sk->woodcutting >= 0.9f)   ? "lumberjack"   : nullptr;
                            if (masterSkill) {
                                auto lv2 = registry.view<EventLog>();
                                if (!lv2.empty()) {
                                    std::string who = "A master";
                                    if (const auto* n = registry.try_get<Name>(entity))
                                        who = n->value;
                                    std::string dest = "a settlement";
                                    if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                        dest = s->name;
                                    lv2.get<EventLog>(*lv2.begin()).Push(
                                        tm.day, (int)tm.hourOfDay,
                                        who + ", a master " + masterSkill + ", settles at " + dest + ".");
                                }
                            }
                        }
                    }
                }

                // Reunion affinity boost: check if any residents at the new
                // settlement are existing friends (affinity > 0.3).
                if (auto* myRel = registry.try_get<Relations>(entity)) {
                    static int s_reunionCounter = 0;
                    std::string myName;
                    if (const auto* n = registry.try_get<Name>(entity))
                        myName = n->value;
                    std::string settlName;
                    if (const auto* s = registry.try_get<Settlement>(home.settlement))
                        settlName = s->name;

                    for (auto& [other, aff] : myRel->affinity) {
                        if (aff <= 0.3f) continue;
                        if (!registry.valid(other)) continue;
                        auto* otherHome = registry.try_get<HomeSettlement>(other);
                        if (!otherHome || otherHome->settlement != home.settlement) continue;

                        // Boost both parties
                        aff = std::min(1.0f, aff + 0.1f);
                        if (auto* otherRel = registry.try_get<Relations>(other)) {
                            auto it = otherRel->affinity.find(entity);
                            if (it != otherRel->affinity.end())
                                it->second = std::min(1.0f, it->second + 0.1f);
                        }

                        // Log at 1-in-2 frequency
                        if (++s_reunionCounter % 2 == 1) {
                            auto lv3 = registry.view<EventLog>();
                            auto tmv3 = registry.view<TimeManager>();
                            if (!lv3.empty() && !tmv3.empty()) {
                                const auto& tm3 = tmv3.get<TimeManager>(*tmv3.begin());
                                std::string friendName = "a friend";
                                if (const auto* fn = registry.try_get<Name>(other))
                                    friendName = fn->value;
                                char rbuf[180];
                                std::snprintf(rbuf, sizeof(rbuf),
                                    "%s reunites with %s at %s.",
                                    myName.c_str(), friendName.c_str(), settlName.c_str());
                                lv3.get<EventLog>(*lv3.begin()).Push(
                                    tm3.day, (int)tm3.hourOfDay, rbuf);
                            }
                        }
                    }
                }

                // Migration letter home: partially recover farewell-strained bonds
                if (home.prevSettlement != entt::null && registry.valid(home.prevSettlement)) {
                    static std::mt19937 s_letterRng{ std::random_device{}() };
                    auto* myRel2 = registry.try_get<Relations>(entity);
                    if (myRel2) {
                        registry.view<Relations, HomeSettlement>(
                            entt::exclude<PlayerTag, BanditTag>).each(
                            [&](auto friend_e, Relations& friendRel, const HomeSettlement& friendHome) {
                                if (friend_e == entity) return;
                                if (friendHome.settlement != home.prevSettlement) return;
                                auto myIt = myRel2->affinity.find(friend_e);
                                if (myIt == myRel2->affinity.end()) return;
                                // Only recover bonds that are strained (below 0.5) but still present
                                if (myIt->second >= 0.5f || myIt->second < 0.05f) return;
                                auto friendIt = friendRel.affinity.find(entity);
                                if (friendIt == friendRel.affinity.end()) return;
                                // Recover +0.03 on both sides
                                myIt->second = std::min(1.f, myIt->second + 0.03f);
                                friendIt->second = std::min(1.f, friendIt->second + 0.03f);
                                // Log at 1-in-4 frequency
                                if (s_letterRng() % 4 == 0) {
                                    auto llv = registry.view<EventLog>();
                                    if (!llv.empty()) {
                                        std::string migrantName = "A migrant";
                                        if (const auto* n = registry.try_get<Name>(entity)) migrantName = n->value;
                                        std::string friendName = "a friend";
                                        if (const auto* n = registry.try_get<Name>(friend_e)) friendName = n->value;
                                        llv.get<EventLog>(*llv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                            migrantName + " sends word back to " + friendName);
                                    }
                                }
                            });
                    }
                }
            } else {
                MoveToward(vel, pos, destPos.x, destPos.y, speed);
            }
            continue;
        }

        // ============================================================
        // SATISFYING: refill need at the facility, check completion
        // ============================================================
        if (state.behavior == AgentBehavior::Satisfying) {
            if (state.target == entt::null || !registry.valid(state.target)) {
                state.behavior = AgentBehavior::Idle;
                vel.vx = vel.vy = 0.f;
                continue;
            }
            const auto& facPos = registry.get<Position>(state.target);
            const auto& fac    = registry.get<ProductionFacility>(state.target);

            if (!InRange(pos.x, pos.y, facPos.x, facPos.y, FACILITY_RANGE)) {
                state.behavior = AgentBehavior::Idle;
                state.target   = entt::null;
                vel.vx = vel.vy = 0.f;
                continue;
            }

            int idx = NeedIndexForResource(fac.output);
            if (idx >= 0) {
                needs.list[idx].value += needs.list[idx].refillRate * dt;
                if (needs.list[idx].value >= 1.f) {
                    needs.list[idx].value = 1.f;
                    state.behavior = AgentBehavior::Idle;
                    state.target   = entt::null;
                    vel.vx = vel.vy = 0.f;
                }
            }
            continue;
        }

        // ============================================================
        // GRATITUDE: briefly move toward the NPC who recently helped you
        // ============================================================
        if (timer.gratitudeTimer > 0.f) {
            timer.gratitudeTimer -= dt;
            if (timer.gratitudeTarget != entt::null && registry.valid(timer.gratitudeTarget)) {
                const auto& tgtPos = registry.get<Position>(timer.gratitudeTarget);
                float gdx = tgtPos.x - pos.x, gdy = tgtPos.y - pos.y;
                float gdist2 = gdx*gdx + gdy*gdy;
                static constexpr float POLITE_DIST = 25.f;
                if (gdist2 <= POLITE_DIST * POLITE_DIST) {
                    // Close enough — stand still for the rest of the gratitude window
                    vel.vx = vel.vy = 0.f;
                } else {
                    MoveToward(vel, pos, tgtPos.x, tgtPos.y, speed * 0.7f);
                }
                state.behavior = AgentBehavior::Idle;
            } else {
                // Helper gone — cancel gratitude
                timer.gratitudeTimer  = 0.f;
                timer.gratitudeTarget = entt::null;
            }
            continue;
        }
        // Clear stale target once timer expires
        if (timer.gratitudeTarget != entt::null && timer.gratitudeTimer <= 0.f)
            timer.gratitudeTarget = entt::null;

        // ============================================================
        // IDLE / SEEKING: decide what to do this tick
        // ============================================================

        // -- Homesickness: tick timer and possibly return to previous settlement --
        if (home.prevSettlement != entt::null && registry.valid(home.prevSettlement)
            && home.prevSettlement != home.settlement) {
            float ghDtHS = dt * GAME_MINS_PER_REAL_SEC / 60.f;
            timer.homesickTimer += ghDtHS;
            if (timer.homesickTimer > 72.f && timer.lastSatisfaction < 0.4f) {
                // Return to previous settlement
                entt::entity returnDest = home.prevSettlement;
                timer.homesickTimer  = 0.f;
                state.behavior       = AgentBehavior::Migrating;
                state.target         = returnDest;
                home.prevSettlement  = entt::null; // clear so it doesn't loop
                vel.vx = vel.vy      = 0.f;

                // Log at 1-in-3 frequency
                static std::mt19937 s_hsRng{ std::random_device{}() };
                static std::uniform_int_distribution<int> s_hsDist(0, 2);
                if (s_hsDist(s_hsRng) == 0) {
                    auto lv = registry.view<EventLog>();
                    if (!lv.empty()) {
                        std::string who = "Someone";
                        if (const auto* n = registry.try_get<Name>(entity))
                            who = n->value;
                        std::string oldHome = "their old home";
                        if (const auto* s = registry.try_get<Settlement>(returnDest))
                            oldHome = s->name;
                        lv.get<EventLog>(*lv.begin()).Push(
                            tm.day, (int)tm.hourOfDay,
                            who + " feels homesick and returns to " + oldHome);
                    }
                }
                continue;
            }
        }

        // -- Decision cooldown: skip expensive re-evaluation while committed --
        if (state.decisionCooldown > 0.f) {
            state.decisionCooldown -= realDt;
            // Emergency override: if any need is critical, force re-evaluation
            bool emergency = false;
            for (const auto& n : needs.list)
                if (n.value < 0.2f) { emergency = true; break; }
            if (!emergency) continue;
            state.decisionCooldown = 0.f;
        }
        // Set cooldown: NPC won't re-evaluate for ~0.5 real-seconds (~30 frames)
        state.decisionCooldown = 0.5f;

        // -- Check migration trigger first --
        float effectiveMigrateThreshold = timer.migrateThreshold;
        // Master retention: masters need 50% more scarcity to migrate.
        if (timer.masterSettled)
            effectiveMigrateThreshold *= 1.5f;
        // NPCs in a plague settlement are more fearful and migrate at half the normal threshold.
        if (const auto* hs = registry.try_get<Settlement>(home.settlement))
            if (hs->modifierName == "Plague")
                effectiveMigrateThreshold *= 0.50f;
        // Career changer restlessness: frequent changers migrate more easily.
        bool careerRestless = false;
        if (const auto* prof = registry.try_get<Profession>(entity)) {
            if (prof->careerChanges >= 3) {
                effectiveMigrateThreshold *= 0.8f;
                careerRestless = true;
            }
        }

        // Nostalgic elder homesickness resistance: elders with many bonds stay put
        if (const auto* elderAge = registry.try_get<Age>(entity)) {
            if (elderAge->days > 60.f) {
                if (const auto* elderRel = registry.try_get<Relations>(entity)) {
                    int localBonds = 0;
                    for (const auto& [other, aff] : elderRel->affinity) {
                        if (aff < 0.5f) continue;
                        if (!registry.valid(other)) continue;
                        const auto* otherHome = registry.try_get<HomeSettlement>(other);
                        if (otherHome && otherHome->settlement == home.settlement)
                            ++localBonds;
                    }
                    if (localBonds >= 3) {
                        effectiveMigrateThreshold *= 1.5f;
                        // Log at 1-in-8 when migration would have triggered
                        if (timer.stockpileEmpty >= effectiveMigrateThreshold / 1.5f
                            && timer.stockpileEmpty < effectiveMigrateThreshold) {
                            static std::mt19937 s_elderHomeRng{ std::random_device{}() };
                            if (s_elderHomeRng() % 8 == 0) {
                                auto lv2 = registry.view<EventLog>();
                                if (!lv2.empty()) {
                                    std::string who = "An elder";
                                    if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                                    std::string where = "settlement";
                                    if (const auto* st = registry.try_get<Settlement>(home.settlement))
                                        where = st->name;
                                    lv2.get<EventLog>(*lv2.begin()).Push(tm.day, (int)tm.hourOfDay,
                                        who + " has too many bonds to leave " + where);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (timer.stockpileEmpty >= effectiveMigrateThreshold) {
            const auto* skills     = registry.try_get<Skills>(entity);
            const auto* profession = registry.try_get<Profession>(entity);
            auto*       memory     = registry.try_get<MigrationMemory>(entity);

            // Record current home prices before departing so the NPC can compare later.
            if (memory && home.settlement != entt::null && registry.valid(home.settlement)) {
                if (const auto* mkt  = registry.try_get<Market>(home.settlement))
                    if (const auto* stt = registry.try_get<Settlement>(home.settlement))
                        memory->Record(stt->name,
                            mkt->GetPrice(ResourceType::Food),
                            mkt->GetPrice(ResourceType::Water),
                            mkt->GetPrice(ResourceType::Wood), tm.day);
            }

            // Check loneliness: no friends (affinity >= 0.3) at current settlement
            bool lonely = true;
            if (const auto* rel = registry.try_get<Relations>(entity)) {
                for (const auto& [fe, aff] : rel->affinity) {
                    if (aff < 0.3f) continue;
                    auto sit = s_entitySettlement.find(fe);
                    if (sit != s_entitySettlement.end() && sit->second == home.settlement) { lonely = false; break; }
                }
            }

            entt::entity dest = FindMigrationTarget(registry, home.settlement, skills, profession, memory, tm.day, timer.lastSatisfaction, lonely);
            if (dest != entt::null) {
                home.prevSettlement  = home.settlement;
                timer.homesickTimer  = 0.f;
                state.behavior       = AgentBehavior::Migrating;
                state.target         = dest;
                timer.stockpileEmpty = 0.f;

                // Log migration event
                auto lv = registry.view<EventLog>();
                if (lv.begin() != lv.end()) {
                    auto tv = registry.view<TimeManager>();
                    if (tv.begin() != tv.end()) {
                        const auto& tm2 = tv.get<TimeManager>(*tv.begin());
                        std::string from = "?", to = "?", who = "NPC";
                        if (auto* s = registry.try_get<Settlement>(home.settlement))
                            from = s->name;
                        if (auto* s = registry.try_get<Settlement>(dest))
                            to = s->name;
                        if (auto* n = registry.try_get<Name>(entity))
                            who = n->value;
                        lv.get<EventLog>(*lv.begin()).Push(
                            tm2.day, (int)tm2.hourOfDay,
                            who + " migrating " + from + " → " + to);

                        // Harmony-driven migration log (1-in-12)
                        {
                            auto hIt = s_harmonyCache.values.find(dest);
                            if (hIt != s_harmonyCache.values.end() && hIt->second > 0.2f) {
                                static std::mt19937 s_harmMigRng{ std::random_device{}() };
                                if (s_harmMigRng() % 12 == 0) {
                                    lv.get<EventLog>(*lv.begin()).Push(
                                        tm2.day, (int)tm2.hourOfDay,
                                        who + " is drawn to " + to + "'s friendly community");
                                }
                            }
                        }
                    }
                }

                // ---- Elder departure farewell feast ----
                if (const auto* elderAge = registry.try_get<Age>(entity)) {
                    if (elderAge->days > 60.f) {
                        // Count local friends (affinity >= 0.3) at the old settlement
                        std::vector<entt::entity> localFriends;
                        if (const auto* elderRel = registry.try_get<Relations>(entity)) {
                            for (const auto& [fe, aff] : elderRel->affinity) {
                                if (aff < 0.3f) continue;
                                auto sit = s_entitySettlement.find(fe);
                                if (sit != s_entitySettlement.end() && sit->second == home.settlement)
                                    localFriends.push_back(fe);
                            }
                        }
                        if (localFriends.size() >= 3) {
                            // Morale hit on settlement
                            if (auto* settl = registry.try_get<Settlement>(home.settlement))
                                settl->morale = std::max(0.f, settl->morale - 0.01f);
                            // Mutual affinity boost +0.03 among friends left behind
                            for (size_t i = 0; i < localFriends.size(); ++i) {
                                auto* relA = registry.try_get<Relations>(localFriends[i]);
                                if (!relA) continue;
                                for (size_t j = i + 1; j < localFriends.size(); ++j) {
                                    relA->affinity[localFriends[j]] = std::min(1.f, relA->affinity[localFriends[j]] + 0.03f);
                                    if (auto* relB = registry.try_get<Relations>(localFriends[j]))
                                        relB->affinity[localFriends[i]] = std::min(1.f, relB->affinity[localFriends[i]] + 0.03f);
                                }
                            }
                            // Log once
                            auto lvF = registry.view<EventLog>();
                            if (!lvF.empty()) {
                                std::string settlName = "settlement";
                                if (const auto* s = registry.try_get<Settlement>(home.settlement)) settlName = s->name;
                                std::string elderName = "an elder";
                                if (const auto* nm = registry.try_get<Name>(entity)) elderName = nm->value;
                                lvF.get<EventLog>(*lvF.begin()).Push(tm.day, (int)tm.hourOfDay,
                                    settlName + " holds a farewell feast for " + elderName + ".");
                            }
                        }
                    }
                }

                // ---- Career changer restlessness log ----
                static std::mt19937 s_restlessRng{std::random_device{}()};
                if (careerRestless && s_restlessRng() % 10 == 0) {
                    auto lv5 = registry.view<EventLog>();
                    if (!lv5.empty()) {
                        std::string who = "NPC";
                        if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                        std::string where = "settlement";
                        if (const auto* s = registry.try_get<Settlement>(home.settlement)) where = s->name;
                        lv5.get<EventLog>(*lv5.begin()).Push(
                            tm.day, (int)tm.hourOfDay,
                            who + " feels restless at " + where + ".");
                    }
                }

                // ---- Farewell log: departing NPC says goodbye to top friend ----
                if (const auto* rel = registry.try_get<Relations>(entity)) {
                    static std::mt19937 s_farewellRng{ std::random_device{}() };
                    static std::uniform_int_distribution<int> s_farewellDist(0, 2);
                    if (s_farewellDist(s_farewellRng) == 0) {  // 1-in-3 frequency
                        entt::entity topFriend = entt::null;
                        float topAff = 0.4f - 0.01f;
                        for (const auto& [fe, aff] : rel->affinity) {
                            if (aff < 0.4f) continue;
                            auto sit = s_entitySettlement.find(fe);
                            if (sit == s_entitySettlement.end() || sit->second != home.settlement) continue;
                            if (aff > topAff) { topAff = aff; topFriend = fe; }
                        }
                        if (topFriend != entt::null) {
                            auto lv2 = registry.view<EventLog>();
                            auto tv2 = registry.view<TimeManager>();
                            if (!lv2.empty() && !tv2.empty()) {
                                const auto& tm3 = tv2.get<TimeManager>(*tv2.begin());
                                std::string who = "NPC", friendName = "a friend", settlName = "?";
                                if (auto* n = registry.try_get<Name>(entity)) who = n->value;
                                if (auto* n = registry.try_get<Name>(topFriend)) friendName = n->value;
                                if (auto* s = registry.try_get<Settlement>(home.settlement)) settlName = s->name;
                                lv2.get<EventLog>(*lv2.begin()).Push(tm3.day, (int)tm3.hourOfDay,
                                    who + " says farewell to " + friendName + " before leaving " + settlName);
                            }
                        }
                    }
                }

                // ---- Friend farewell on migration: distance strain weakens bonds ----
                if (auto* myRelFw = registry.try_get<Relations>(entity)) {
                    static std::mt19937 s_farewellStrainRng{ std::random_device{}() };
                    for (auto& [friendEnt, aff] : myRelFw->affinity) {
                        if (aff < 0.5f) continue;
                        // Check friend is at the same (old) settlement
                        auto sit = s_entitySettlement.find(friendEnt);
                        if (sit == s_entitySettlement.end() || sit->second != home.settlement) continue;
                        // Decrease both sides by 0.1 (floor 0.0)
                        aff = std::max(0.f, aff - 0.1f);
                        if (auto* fRel = registry.try_get<Relations>(friendEnt))
                            fRel->affinity[entity] = std::max(0.f, fRel->affinity[entity] - 0.1f);
                        // Log at 1-in-3 frequency per friend
                        if (s_farewellStrainRng() % 3 == 0) {
                            auto lv5 = registry.view<EventLog>();
                            if (!lv5.empty()) {
                                std::string who = "NPC", fname = "a friend", where = "settlement";
                                if (auto* n = registry.try_get<Name>(entity)) who = n->value;
                                if (auto* n = registry.try_get<Name>(friendEnt)) fname = n->value;
                                if (auto* s = registry.try_get<Settlement>(home.settlement)) where = s->name;
                                char buf[180];
                                std::snprintf(buf, sizeof(buf), "%s says goodbye to %s at %s",
                                              who.c_str(), fname.c_str(), where.c_str());
                                lv5.get<EventLog>(*lv5.begin()).Push(tm.day, (int)tm.hourOfDay, buf);
                            }
                        }
                    }
                }

                // ---- Master exodus warning: losing a master hurts the settlement ----
                if (skills) {
                    const char* masterSkill = nullptr;
                    if (skills->farming >= 0.9f)       masterSkill = "farming";
                    else if (skills->water_drawing >= 0.9f) masterSkill = "water-drawing";
                    else if (skills->woodcutting >= 0.9f)   masterSkill = "woodcutting";
                    if (masterSkill) {
                        auto lv3 = registry.view<EventLog>();
                        if (!lv3.empty()) {
                            auto& tmE = registry.view<TimeManager>().get<TimeManager>(
                                *registry.view<TimeManager>().begin());
                            std::string who = "NPC";
                            if (auto* n = registry.try_get<Name>(entity)) who = n->value;
                            std::string from = "settlement";
                            if (auto* s = registry.try_get<Settlement>(home.settlement)) from = s->name;
                            lv3.get<EventLog>(*lv3.begin()).Push(tmE.day, (int)tmE.hourOfDay,
                                who + ", a master " + masterSkill + ", leaves " + from + ".");
                        }
                    }
                }

                // ---- Master loss morale penalty: settlement mourns departing master ----
                if (timer.masterSettled && home.settlement != entt::null && registry.valid(home.settlement)) {
                    if (auto* settl = registry.try_get<Settlement>(home.settlement)) {
                        settl->morale = std::max(0.f, settl->morale - 0.03f);
                        // Log at 1-in-2 frequency
                        static int s_masterLossCounter = 0;
                        if (++s_masterLossCounter % 2 == 0) {
                            auto lv4 = registry.view<EventLog>();
                            if (!lv4.empty()) {
                                lv4.get<EventLog>(*lv4.begin()).Push(
                                    tm.day, (int)tm.hourOfDay,
                                    settl->name + " mourns the loss of a master.");
                            }
                        }
                    }
                }

                // ---- Friend co-migration: best friend (highest affinity ≥ 0.5) follows if they also want to migrate ----
                if (const auto* rel = registry.try_get<Relations>(entity)) {
                    entt::entity bestFriend = entt::null;
                    float bestAffinity = FRIEND_THRESHOLD - 0.01f;
                    for (const auto& [friendEnt, affinity] : rel->affinity) {
                        if (affinity < FRIEND_THRESHOLD) continue;
                        auto sit = s_entitySettlement.find(friendEnt);
                        if (sit == s_entitySettlement.end() || sit->second != home.settlement) continue;
                        auto* fState = registry.try_get<AgentState>(friendEnt);
                        if (!fState) continue;
                        if (fState->behavior == AgentBehavior::Migrating) continue;
                        if (affinity > bestAffinity) {
                            bestAffinity = affinity;
                            bestFriend = friendEnt;
                        }
                    }
                    if (bestFriend != entt::null) {
                        // Check if friend also has a valid migration target (would want to leave)
                        auto* fSkills     = registry.try_get<Skills>(bestFriend);
                        auto* fProfession = registry.try_get<Profession>(bestFriend);
                        auto* fMemory     = registry.try_get<MigrationMemory>(bestFriend);
                        auto* fTimer      = registry.try_get<DeprivationTimer>(bestFriend);
                        float fSatisfaction = fTimer ? fTimer->lastSatisfaction : 0.5f;
                        auto* fHome  = registry.try_get<HomeSettlement>(bestFriend);
                        auto* fState = registry.try_get<AgentState>(bestFriend);
                        entt::entity friendDest = FindMigrationTarget(registry, fHome->settlement,
                            fSkills, fProfession, fMemory, tm.day, fSatisfaction);
                        // Friend follows if they have any valid migration target (score > 0)
                        if (friendDest != entt::null) {
                            // Send friend to same destination as the initiator
                            fState->behavior = AgentBehavior::Migrating;
                            fState->target   = dest;
                            if (fTimer)
                                fTimer->stockpileEmpty = 0.f;

                            // ---- Third NPC: scan best friend's friends for one additional companion ----
                            entt::entity thirdNpc = entt::null;
                            if (const auto* fRel = registry.try_get<Relations>(bestFriend)) {
                                float bestThirdAff = FRIEND_THRESHOLD - 0.01f;
                                for (const auto& [candidate, aff] : fRel->affinity) {
                                    if (aff < FRIEND_THRESHOLD) continue;
                                    if (candidate == entity) continue; // already the initiator
                                    auto sit = s_entitySettlement.find(candidate);
                                    if (sit == s_entitySettlement.end() || sit->second != home.settlement) continue;
                                    auto* cState = registry.try_get<AgentState>(candidate);
                                    if (!cState) continue;
                                    if (cState->behavior == AgentBehavior::Migrating) continue;
                                    if (aff > bestThirdAff) {
                                        // Check if candidate has a valid migration target
                                        auto* cSkills     = registry.try_get<Skills>(candidate);
                                        auto* cProfession = registry.try_get<Profession>(candidate);
                                        auto* cMemory     = registry.try_get<MigrationMemory>(candidate);
                                        auto* cTimerPtr   = registry.try_get<DeprivationTimer>(candidate);
                                        float cSat = cTimerPtr ? cTimerPtr->lastSatisfaction : 0.5f;
                                        entt::entity cDest = FindMigrationTarget(registry, home.settlement,
                                            cSkills, cProfession, cMemory, tm.day, cSat);
                                        if (cDest != entt::null) {
                                            bestThirdAff = aff;
                                            thirdNpc = candidate;
                                        }
                                    }
                                }
                                if (thirdNpc != entt::null) {
                                    auto* cState2 = registry.try_get<AgentState>(thirdNpc);
                                    auto* cTimer2 = registry.try_get<DeprivationTimer>(thirdNpc);
                                    if (cState2) {
                                        cState2->behavior = AgentBehavior::Migrating;
                                        cState2->target   = dest;
                                    }
                                    if (cTimer2)
                                        cTimer2->stockpileEmpty = 0.f;
                                }
                            }

                            // Log co-migration at 1-in-2 frequency
                            static std::mt19937 s_coMigRng{ std::random_device{}() };
                            static std::uniform_int_distribution<int> s_coMigDist(0, 1);
                            if (s_coMigDist(s_coMigRng) == 0) {
                                auto lv2 = registry.view<EventLog>();
                                if (lv2.begin() != lv2.end()) {
                                    const auto& tm3 = timeView.get<TimeManager>(*timeView.begin());
                                    std::string friendName = "A neighbour";
                                    if (const auto* fn = registry.try_get<Name>(bestFriend))
                                        friendName = fn->value;
                                    std::string entityName = "someone";
                                    if (const auto* en = registry.try_get<Name>(entity))
                                        entityName = en->value;
                                    std::string toName = "?";
                                    if (auto* s = registry.try_get<Settlement>(dest)) toName = s->name;
                                    if (thirdNpc != entt::null) {
                                        std::string thirdName = "another";
                                        if (const auto* tn = registry.try_get<Name>(thirdNpc))
                                            thirdName = tn->value;
                                        lv2.get<EventLog>(*lv2.begin()).Push(
                                            tm3.day, (int)tm3.hourOfDay,
                                            entityName + ", " + friendName + ", and " + thirdName + " leave together for " + toName);
                                    } else {
                                        lv2.get<EventLog>(*lv2.begin()).Push(
                                            tm3.day, (int)tm3.hourOfDay,
                                            entityName + " and " + friendName + " migrate together to " + toName);
                                    }
                                }
                            }
                        }
                    }
                }

                // ---- Work buddy co-migration: workBestFriend follows if close to migrating ----
                if (const auto* rel = registry.try_get<Relations>(entity)) {
                    entt::entity workBuddy = rel->workBestFriend;
                    if (workBuddy != entt::null && registry.valid(workBuddy)) {
                        auto* wbState = registry.try_get<AgentState>(workBuddy);
                        auto* wbTimer = registry.try_get<DeprivationTimer>(workBuddy);
                        auto* wbHome  = registry.try_get<HomeSettlement>(workBuddy);
                        if (wbState && wbTimer && wbHome
                            && wbHome->settlement == home.settlement
                            && wbState->behavior != AgentBehavior::Migrating
                            && wbTimer->stockpileEmpty >= wbTimer->migrateThreshold * 0.7f) {
                            static std::mt19937 s_wbCoMigRng{ std::random_device{}() };
                            if (s_wbCoMigRng() % 4 == 0) {
                                wbState->behavior = AgentBehavior::Migrating;
                                wbState->target   = dest;
                                wbTimer->stockpileEmpty = 0.f;
                                // Log at full frequency
                                auto lvWb = registry.view<EventLog>();
                                if (!lvWb.empty()) {
                                    std::string buddyName = "A worker";
                                    if (const auto* bn = registry.try_get<Name>(workBuddy)) buddyName = bn->value;
                                    std::string migrantName = "someone";
                                    if (const auto* mn = registry.try_get<Name>(entity)) migrantName = mn->value;
                                    std::string destName = "?";
                                    if (const auto* ds = registry.try_get<Settlement>(dest)) destName = ds->name;
                                    lvWb.get<EventLog>(*lvWb.begin()).Push(tm.day, (int)tm.hourOfDay,
                                        buddyName + " follows work partner " + migrantName + " to " + destName + ".");
                                }
                            }
                        }
                    }
                }

                continue;
            }
        }

        // -- Theft from stockpile --
        // NPCs with very little money and no steal cooldown can steal 1 unit of their
        // most-needed resource from the home settlement stockpile.
        static constexpr float STEAL_MONEY_THRESHOLD = 5.f;   // below this balance → can steal
        static constexpr float STEAL_COOLDOWN_HOURS  = 48.f;  // game-hours between steals
        const auto* moneyComp = registry.try_get<Money>(entity);
        float currentBalance = moneyComp ? moneyComp->balance : 0.f;
        if (currentBalance < STEAL_MONEY_THRESHOLD && timer.stealCooldown <= 0.f &&
            home.settlement != entt::null && registry.valid(home.settlement))
        {
            // Find most-needed non-Heat resource (lowest need value)
            int   stealIdx = -1;
            float stealLow = std::numeric_limits<float>::max();
            for (int i = 0; i < (int)needs.list.size(); ++i) {
                const auto& n = needs.list[i];
                if (n.type == NeedType::Heat) continue;
                if (n.value < stealLow) { stealLow = n.value; stealIdx = i; }
            }
            if (stealIdx != -1) {
                ResourceType stealRes = ResourceTypeForNeed(needs.list[stealIdx].type);
                auto* sp  = registry.try_get<Stockpile>(home.settlement);
                auto* mkt = registry.try_get<Market>(home.settlement);
                auto* stl = registry.try_get<Settlement>(home.settlement);
                if (sp && sp->quantities.count(stealRes) && sp->quantities[stealRes] >= 1.f) {
                    sp->quantities[stealRes] -= 1.f;
                    // Credit the stolen unit to the thief's inventory-equivalent: increase need value
                    // (same as ConsumptionSystem does when food is consumed)
                    needs.list[stealIdx].value = std::min(1.f, needs.list[stealIdx].value + 0.3f);
                    // Deduct market value from settlement treasury (the settlement loses the good)
                    if (stl && mkt) {
                        float cost = mkt->GetPrice(stealRes);
                        stl->treasury -= cost;
                    }
                    timer.stealCooldown = STEAL_COOLDOWN_HOURS;
                    timer.theftCount++;

                    // Theft erodes social trust — lower settlement morale
                    if (stl) stl->morale = std::max(0.f, stl->morale - 0.05f);

                    // Social shame penalty: reduce all skills by 0.02 (ostracism effect)
                    if (auto* sk = registry.try_get<Skills>(entity)) {
                        sk->farming       = std::max(0.f, sk->farming       - 0.02f);
                        sk->water_drawing = std::max(0.f, sk->water_drawing - 0.02f);
                        sk->woodcutting   = std::max(0.f, sk->woodcutting   - 0.02f);
                    }

                    // Log the theft
                    auto lv = registry.view<EventLog>();
                    if (lv.begin() != lv.end()) {
                        auto tv = registry.view<TimeManager>();
                        if (tv.begin() != tv.end()) {
                            const auto& tm2 = tv.get<TimeManager>(*tv.begin());
                            std::string who = "NPC", where = "?";
                            if (auto* n = registry.try_get<Name>(entity))   who   = n->value;
                            if (stl)                                          where = stl->name;
                            std::string resName =
                                (stealRes == ResourceType::Food)    ? "food"    :
                                (stealRes == ResourceType::Water)   ? "water"   :
                                (stealRes == ResourceType::Wood)    ? "wood"    : "goods";
                            lv.get<EventLog>(*lv.begin()).Push(
                                tm2.day, (int)tm2.hourOfDay,
                                who + " stole " + resName + " from " + where + ".");

                            // Exile after 3 thefts: clear home settlement
                            if (timer.theftCount >= 3) {
                                std::string exileName = where;
                                home.settlement = entt::null;
                                lv.get<EventLog>(*lv.begin()).Push(
                                    tm2.day, (int)tm2.hourOfDay,
                                    who + " exiled from " + exileName + " for repeated theft.");
                            }
                        }
                    }
                }
            }
        }

        // -- Find the most critical need --
        // Heat is handled passively by ConsumptionSystem (Wood stockpile → warmth),
        // so we skip it here — NPCs don't "seek" a heating facility.
        int   critIdx = -1;
        float lowest  = std::numeric_limits<float>::max();
        for (int i = 0; i < (int)needs.list.size(); ++i) {
            const auto& n = needs.list[i];
            if (n.type == NeedType::Heat) continue;   // passive need
            if (n.value < n.criticalThreshold && n.value < lowest) {
                lowest  = n.value;
                critIdx = i;
            }
        }

        if (critIdx == -1) {
            state.behavior = AgentBehavior::Idle;
            state.target   = entt::null;

            // ---- Chat timer: NPC is mid-conversation, stay still ----
            if (timer.chatTimer > 0.f) {
                timer.chatTimer -= dt;
                vel.vx = vel.vy = 0.f;
                continue;
            }

            // ---- Bandit intimidation: nearby bandits erode settlement morale ----
            if (timer.intimidationCooldown > 0.f)
                timer.intimidationCooldown = std::max(0.f, timer.intimidationCooldown - dt);
            if (home.settlement != entt::null && registry.valid(home.settlement)) {
                static constexpr float INTIM_RADIUS = 50.f;
                bool nearBandit = false;
                for (const auto& [bx, by] : s_banditPositions) {
                    float bdx = bx - pos.x, bdy = by - pos.y;
                    if (bdx*bdx + bdy*bdy < INTIM_RADIUS * INTIM_RADIUS) {
                        nearBandit = true;
                        break;
                    }
                }
                if (nearBandit) {
                    float intimGameHoursDt = dt * GAME_MINS_PER_REAL_SEC / 60.f;
                    if (auto* settl = registry.try_get<Settlement>(home.settlement))
                        settl->morale = std::max(0.f, settl->morale - 0.02f * intimGameHoursDt);
                    if (timer.intimidationCooldown <= 0.f) {
                        timer.intimidationCooldown = 60.f;  // 60 game-seconds cooldown
                        auto lv = registry.view<EventLog>();
                        if (lv.begin() != lv.end()) {
                            const auto* myName = registry.try_get<Name>(entity);
                            std::string msg = (myName ? myName->value : "NPC") +
                                              " feels uneasy near bandits.";
                            lv.get<EventLog>(*lv.begin()).Push(
                                tm.day, (int)tm.hourOfDay, msg);
                        }
                    }
                }
            }

            // ---- Grief: drain timer, skip social actions, drain settlement morale ----
            if (timer.griefTimer > 0.f) {
                float ghDtGrief = dt * GAME_MINS_PER_REAL_SEC / 60.f;
                timer.griefTimer = std::max(0.f, timer.griefTimer - ghDtGrief);
                // Drain home settlement morale while grieving
                if (home.settlement != entt::null && registry.valid(home.settlement)) {
                    if (auto* settl = registry.try_get<Settlement>(home.settlement))
                        settl->morale = std::max(0.f, settl->morale - 0.05f * ghDtGrief);
                }
            }
            // ---- Grief anniversary remembrance ----
            // Every 30 days after the last grief event, if the NPC has a close
            // friend at the same settlement, brief renewed grief triggers.
            if (timer.griefTimer <= 0.f && timer.lastGriefDay >= 0.f) {
                int daysSince = (int)tm.day - (int)timer.lastGriefDay;
                if (daysSince > 0 && daysSince % 30 == 0) {
                    // Check for at least one friend (affinity >= 0.4) at the same settlement
                    bool hasLocalFriend = false;
                    if (const auto* rel = registry.try_get<Relations>(entity)) {
                        for (const auto& [fe, aff] : rel->affinity) {
                            if (aff < 0.4f) continue;
                            auto sit = s_entitySettlement.find(fe);
                            if (sit != s_entitySettlement.end() && sit->second == home.settlement) {
                                hasLocalFriend = true;
                                break;
                            }
                        }
                    }
                    if (hasLocalFriend) {
                        timer.griefTimer = 1.f;  // 1 game-hour of renewed grief
                        // Log at 1-in-4 frequency
                        static std::mt19937 s_anniversaryRng{ std::random_device{}() };
                        if (s_anniversaryRng() % 4 == 0) {
                            auto logVA = registry.view<EventLog>();
                            if (!logVA.empty()) {
                                std::string who = "NPC";
                                if (const auto* nm = registry.try_get<Name>(entity)) who = nm->value;
                                std::string where = "settlement";
                                if (home.settlement != entt::null && registry.valid(home.settlement))
                                    if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                        where = s->name;
                                logVA.get<EventLog>(*logVA.begin()).Push(tm.day, (int)tm.hourOfDay,
                                    who + " reflects on those lost at " + where + ".");
                            }
                        }
                    }
                }
            }

            bool isGrieving = (timer.griefTimer > 0.f);

            // ---- Grief vigil gathering: 3+ grieving NPCs at same settlement ----
            if (isGrieving && home.settlement != entt::null) {
                static std::map<entt::entity, int> s_lastVigilDay;
                auto gIt = grievingBySettlement.find(home.settlement);
                if (gIt != grievingBySettlement.end() && (int)gIt->second.size() >= 3) {
                    int today = (int)tm.day;
                    if (s_lastVigilDay[home.settlement] != today) {
                        s_lastVigilDay[home.settlement] = today;
                        // Boost mutual affinity among all grieving NPCs at this settlement
                        auto& grievers = gIt->second;
                        for (size_t i = 0; i < grievers.size(); ++i) {
                            for (size_t j = i + 1; j < grievers.size(); ++j) {
                                auto a = grievers[i], b = grievers[j];
                                if (!registry.valid(a) || !registry.valid(b)) continue;
                                if (auto* rA = registry.try_get<Relations>(a))
                                    rA->affinity[b] = std::min(1.f, rA->affinity[b] + 0.02f);
                                if (auto* rB = registry.try_get<Relations>(b))
                                    rB->affinity[a] = std::min(1.f, rB->affinity[a] + 0.02f);
                            }
                        }
                        // Vigil morale recovery: collective healing
                        if (auto* settlV = registry.try_get<Settlement>(home.settlement))
                            settlV->morale = std::min(1.f, settlV->morale + 0.03f);
                        // Log once
                        auto vlv = registry.view<EventLog>();
                        if (!vlv.empty()) {
                            std::string where = "A settlement";
                            if (const auto* st = registry.try_get<Settlement>(home.settlement))
                                where = st->name;
                            vlv.get<EventLog>(*vlv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                where + " holds a vigil for the fallen");
                            // Vigil morale recovery log at 1-in-3 frequency
                            static std::mt19937 s_vigilMoraleRng{ std::random_device{}() };
                            if (s_vigilMoraleRng() % 3 == 0)
                                vlv.get<EventLog>(*vlv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                    where + "'s vigil brings comfort.");
                        }
                    }
                }
            }

            // ---- Gossip idle animation (hours 20–22) ----
            // Idle NPCs visually gravitate toward a nearby same-settlement NPC.
            if (timer.gossipNudgeTimer > 0.f)
                timer.gossipNudgeTimer = std::max(0.f, timer.gossipNudgeTimer - dt);
            if (currentHour >= 20 && currentHour < 22 && timer.gossipNudgeTimer <= 0.f
                && home.settlement != entt::null && registry.valid(home.settlement)) {
                static constexpr float GOSSIP_ANIM_RADIUS = 30.f;
                bool found = false;
                registry.view<AgentState, Position, HomeSettlement, DeprivationTimer>(
                    entt::exclude<Hauler, PlayerTag, BanditTag>)
                    .each([&](auto other, const AgentState& oState, const Position& oPos,
                              const HomeSettlement& oHome, DeprivationTimer& oTimer) {
                    if (found) return;
                    if (other == entity) return;
                    if (oHome.settlement != home.settlement) return;
                    if (oState.behavior != AgentBehavior::Idle) return;
                    float gdx = oPos.x - pos.x, gdy = oPos.y - pos.y;
                    float gd2 = gdx*gdx + gdy*gdy;
                    if (gd2 > GOSSIP_ANIM_RADIUS * GOSSIP_ANIM_RADIUS || gd2 < 1.f) return;
                    float gdist = std::sqrt(gd2);
                    vel.vx += gdx * 0.1f / gdist;
                    vel.vy += gdy * 0.1f / gdist;
                    timer.gossipNudgeTimer = 3.f;  // cooldown: 3 game-seconds
                    found = true;
                });
            }

            // ---- Greeting: idle NPCs occasionally greet a nearby idle neighbour ----
            if (timer.greetCooldown > 0.f)
                timer.greetCooldown = std::max(0.f, timer.greetCooldown - dt);
            if (!isGrieving && timer.greetCooldown <= 0.f &&
                home.settlement != entt::null && registry.valid(home.settlement)) {
                static constexpr float GREET_RADIUS = 40.f;
                bool greeted = false;
                registry.view<AgentState, Position, HomeSettlement, DeprivationTimer, Name>(
                    entt::exclude<Hauler, PlayerTag, BanditTag>)
                    .each([&](auto other, const AgentState& oState, const Position& oPos,
                              const HomeSettlement& oHome, DeprivationTimer& oTimer,
                              const Name& oName) {
                    if (greeted) return;
                    if (other == entity) return;
                    if (oHome.settlement != home.settlement) return;
                    if (oState.behavior != AgentBehavior::Idle) return;
                    float gdx = oPos.x - pos.x, gdy = oPos.y - pos.y;
                    if (gdx*gdx + gdy*gdy > GREET_RADIUS * GREET_RADIUS) return;
                    // Greet — 120 game-seconds cooldown = 2 real-seconds
                    timer.greetCooldown  = 2.f;
                    oTimer.greetCooldown = 2.f;
                    // Check if this is a gratitude greeting (greeter's lastHelper == other)
                    bool isGratitude = (timer.lastHelper != entt::null && timer.lastHelper == other);
                    // Check if this is a family reunion (both share FamilyTag::name)
                    bool isFamilyReunion = false;
                    if (const auto* myFt = registry.try_get<FamilyTag>(entity)) {
                        if (const auto* oFt = registry.try_get<FamilyTag>(other)) {
                            if (!myFt->name.empty() && myFt->name == oFt->name)
                                isFamilyReunion = true;
                        }
                    }
                    {
                        auto lv = registry.view<EventLog>();
                        if (lv.begin() != lv.end()) {
                            const auto* myName = registry.try_get<Name>(entity);
                            std::string msg;
                            if (isGratitude) {
                                msg = (myName ? myName->value : "NPC") +
                                      " thanks " + oName.value + " for past kindness";
                                timer.lastHelper = entt::null;  // gratitude expressed, clear
                            } else if (isFamilyReunion) {
                                msg = (myName ? myName->value : "NPC") +
                                      " embraces " + oName.value + " warmly.";
                            } else {
                                // 20% chance: discuss hauler routes if settlement has haulers
                                bool discussTrade = false;
                                if (home.settlement != entt::null && oHome.settlement == home.settlement) {
                                    static std::mt19937 s_tradeRng{ std::random_device{}() };
                                    static std::uniform_real_distribution<float> s_tradeDist(0.f, 1.f);
                                    if (s_tradeDist(s_tradeRng) < 0.2f) {
                                        // Check if settlement has any haulers
                                        bool hasHauler = false;
                                        registry.view<Hauler, HomeSettlement>().each(
                                            [&](auto, const Hauler&, const HomeSettlement& hhs) {
                                                if (hhs.settlement == home.settlement) hasHauler = true;
                                            });
                                        discussTrade = hasHauler;
                                    }
                                }
                                if (discussTrade) {
                                    msg = (myName ? myName->value : "NPC") +
                                          " and " + oName.value + " discuss trade routes.";
                                } else {
                                    msg = (myName ? myName->value : "NPC") +
                                          " greets " + oName.value;
                                    // Complain about low need
                                    for (int ni = 0; ni < 4; ++ni) {
                                        if (needs.list[ni].value < 0.3f) {
                                            const char* nn = (ni == 0) ? "hunger" :
                                                             (ni == 1) ? "thirst" :
                                                             (ni == 2) ? "fatigue" : "the cold";
                                            msg += " (complains about ";
                                            msg += nn;
                                            msg += ")";
                                            break;
                                        }
                                    }
                                }
                            }
                            lv.get<EventLog>(*lv.begin()).Push(
                                tm.day, (int)tm.hourOfDay, msg);
                        }
                    }
                    // Family reunion clears grief early
                    if (isFamilyReunion && (timer.griefTimer > 0.f || oTimer.griefTimer > 0.f)) {
                        timer.griefTimer  *= 0.5f;
                        oTimer.griefTimer *= 0.5f;
                        auto lv3 = registry.view<EventLog>();
                        if (lv3.begin() != lv3.end()) {
                            const auto* myName3 = registry.try_get<Name>(entity);
                            std::string comfortMsg = (myName3 ? myName3->value : "An NPC") +
                                " finds comfort in " + oName.value + "'s company.";
                            lv3.get<EventLog>(*lv3.begin()).Push(
                                tm.day, (int)tm.hourOfDay, comfortMsg);
                        }
                    }
                    // Gossip about player bravery: spread lastHelper to the other NPC
                    if (playerEntity != entt::null &&
                        timer.lastHelper == playerEntity &&
                        oTimer.lastHelper != playerEntity) {
                        oTimer.lastHelper = playerEntity;
                        auto lv2 = registry.view<EventLog>();
                        if (lv2.begin() != lv2.end()) {
                            const auto* myName2 = registry.try_get<Name>(entity);
                            std::string gossipMsg = (myName2 ? myName2->value : "An NPC") +
                                " tells " + oName.value + " about the player's bravery.";
                            lv2.get<EventLog>(*lv2.begin()).Push(
                                tm.day, (int)tm.hourOfDay, gossipMsg);
                        }
                    }
                    // Build affinity: casual greetings slowly build familiarity
                    // Gratitude = +0.05, family reunion = +0.08, normal = +0.01
                    float affinityGain = isFamilyReunion ? 0.08f : (isGratitude ? 0.05f : 0.01f);
                    if (auto* rel = registry.try_get<Relations>(entity))
                        rel->affinity[other] = std::min(1.f, rel->affinity[other] + affinityGain);
                    if (auto* oRel = registry.try_get<Relations>(other))
                        oRel->affinity[entity] = std::min(1.f, oRel->affinity[entity] + affinityGain);
                    greeted = true;
                });
            }

            // ---- Comfort grieving neighbour: close friends reduce grief (staggered: 1/4 per frame) ----
            if (timer.comfortCooldown > 0.f)
                timer.comfortCooldown = std::max(0.f, timer.comfortCooldown - realDt);
            if (timer.comfortCooldown <= 0.f && timer.griefTimer <= 0.f
                && static_cast<uint32_t>(entity) % 4 == static_cast<uint32_t>(s_frameCounter) % 4) {
                const auto* myRel = registry.try_get<Relations>(entity);
                if (myRel) {
                    static constexpr float COMFORT_RADIUS = 25.f;
                    bool comforted = false;
                    registry.view<AgentState, Position, DeprivationTimer, Name>(
                        entt::exclude<Hauler, PlayerTag, BanditTag>).each(
                        [&](auto other, const AgentState& oState, const Position& oPos,
                            DeprivationTimer& oTimer, const Name& oName) {
                            if (comforted) return;
                            if (other == entity) return;
                            if (oState.behavior != AgentBehavior::Idle) return;
                            if (oTimer.griefTimer <= 0.f) return;
                            float cdx = oPos.x - pos.x, cdy = oPos.y - pos.y;
                            if (cdx * cdx + cdy * cdy > COMFORT_RADIUS * COMFORT_RADIUS) return;
                            auto it = myRel->affinity.find(other);
                            if (it == myRel->affinity.end() || it->second < 0.3f) return;
                            // Grief support network: both experienced grief → double comfort
                            bool empathicComfort = (timer.lastGriefDay >= 0.f && oTimer.lastGriefDay >= 0.f);
                            // Work buddy grief support: work best friend → double comfort + affinity boost
                            bool isWorkBuddy = (myRel->workBestFriend == other);
                            // Post-procession comfort: comforter who was in a mourning procession → double comfort
                            bool processionComfort = false;
                            if (timer.skillCelebrateTimer > 0.f) {
                                const auto* comforterSk = registry.try_get<Skills>(entity);
                                if (comforterSk && comforterSk->wisdomGriefDays > 0.f)
                                    processionComfort = true;
                            }
                            float comfortAmount = (empathicComfort || isWorkBuddy || processionComfort) ? 1.0f : 0.5f;
                            oTimer.griefTimer = std::max(0.f, oTimer.griefTimer - comfortAmount);
                            timer.comfortCooldown = 180.f; // 180 real-seconds
                            // Work buddy mutual affinity boost
                            if (isWorkBuddy) {
                                auto* myRelMut = registry.try_get<Relations>(entity);
                                if (myRelMut)
                                    myRelMut->affinity[other] = std::min(1.f, myRelMut->affinity[other] + 0.03f);
                                if (auto* oRel = registry.try_get<Relations>(other))
                                    oRel->affinity[entity] = std::min(1.f, oRel->affinity[entity] + 0.03f);
                            }
                            // Log
                            auto lv = registry.view<EventLog>();
                            if (lv.begin() != lv.end()) {
                                const auto* myName = registry.try_get<Name>(entity);
                                if (isWorkBuddy) {
                                    static std::mt19937 s_buddyGriefRng{ std::random_device{}() };
                                    if (s_buddyGriefRng() % 5 == 0) {
                                        std::string where = "settlement";
                                        if (home.settlement != entt::null && registry.valid(home.settlement))
                                            if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                                where = s->name;
                                        lv.get<EventLog>(*lv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                            (myName ? myName->value : std::string("An NPC")) +
                                            " stays by work buddy " + oName.value + "'s side at " + where + ".");
                                    }
                                } else if (processionComfort) {
                                    static std::mt19937 s_procComfortRng{ std::random_device{}() };
                                    if (s_procComfortRng() % 6 == 0) {
                                        std::string where = "settlement";
                                        if (home.settlement != entt::null && registry.valid(home.settlement))
                                            if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                                where = s->name;
                                        lv.get<EventLog>(*lv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                            (myName ? myName->value : std::string("An NPC")) +
                                            " draws strength from the procession to comfort " + oName.value + ".");
                                    }
                                } else if (empathicComfort) {
                                    static std::mt19937 s_empathyRng{ std::random_device{}() };
                                    if (s_empathyRng() % 6 == 0) {
                                        std::string where = "settlement";
                                        if (home.settlement != entt::null && registry.valid(home.settlement))
                                            if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                                where = s->name;
                                        lv.get<EventLog>(*lv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                            (myName ? myName->value : std::string("An NPC")) +
                                            " understands " + oName.value + "'s pain at " + where + ".");
                                    }
                                } else {
                                    std::string msg = (myName ? myName->value : "An NPC") +
                                        " comforts " + oName.value + ".";
                                    lv.get<EventLog>(*lv.begin()).Push(
                                        tm.day, (int)tm.hourOfDay, msg);
                                }
                            }
                            comforted = true;
                        });
                }
            }

            // ---- Shared grief affinity boost: grieving NPCs at the same settlement bond (staggered: 1/4 per frame) ----
            if (isGrieving && static_cast<uint32_t>(entity) % 4 == static_cast<uint32_t>(s_frameCounter) % 4) {
                static std::mt19937 s_griefBondRng{ std::random_device{}() };
                static constexpr float GRIEF_BOND_RADIUS = 30.f;
                registry.view<DeprivationTimer, HomeSettlement, Relations>(
                    entt::exclude<Hauler, PlayerTag, BanditTag>).each(
                    [&](auto other, DeprivationTimer& oTimer, const HomeSettlement& oHome, Relations& oRel) {
                        if (other == entity) return;
                        if (oTimer.griefTimer <= 0.f) return;
                        if (oHome.settlement != home.settlement) return;
                        // Proximity check
                        if (const auto* oPos2 = registry.try_get<Position>(other)) {
                            float gdx = oPos2->x - pos.x, gdy = oPos2->y - pos.y;
                            if (gdx * gdx + gdy * gdy > GRIEF_BOND_RADIUS * GRIEF_BOND_RADIUS) return;
                        } else return;
                        // Boost mutual affinity by +0.05 (cap 1.0)
                        auto* myRel2 = registry.try_get<Relations>(entity);
                        if (myRel2)
                            myRel2->affinity[other] = std::min(1.f, myRel2->affinity[other] + 0.05f);
                        oRel.affinity[entity] = std::min(1.f, oRel.affinity[entity] + 0.05f);
                        // Log at 1-in-6 frequency
                        if (s_griefBondRng() % 6 == 0) {
                            auto lv2 = registry.view<EventLog>();
                            if (!lv2.empty()) {
                                std::string who = "An NPC";
                                if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                                std::string otherName = "another NPC";
                                if (const auto* n = registry.try_get<Name>(other)) otherName = n->value;
                                std::string where = "settlement";
                                if (home.settlement != entt::null)
                                    if (const auto* s = registry.try_get<Settlement>(home.settlement)) where = s->name;
                                char buf[180];
                                std::snprintf(buf, sizeof(buf), "%s and %s find comfort in shared loss at %s",
                                              who.c_str(), otherName.c_str(), where.c_str());
                                lv2.get<EventLog>(*lv2.begin()).Push(tm.day, (int)tm.hourOfDay, buf);
                            }
                        }
                    });
            }

            // ---- Thank player: NPCs with good rep nod respectfully near the player (staggered: 1/4 per frame) ----
            if (timer.thankCooldown > 0.f)
                timer.thankCooldown = std::max(0.f, timer.thankCooldown - realDt);
            if (playerEntity != entt::null && timer.thankCooldown <= 0.f
                && static_cast<uint32_t>(entity) % 4 == static_cast<uint32_t>(s_frameCounter) % 4) {
                static constexpr float THANK_RADIUS = 40.f;
                if (const auto* rep = registry.try_get<Reputation>(entity)) {
                    if (rep->score > 0.3f) {
                        float tdx = playerPos.x - pos.x, tdy = playerPos.y - pos.y;
                        if (tdx*tdx + tdy*tdy <= THANK_RADIUS * THANK_RADIUS) {
                            timer.thankCooldown = 60.f;  // 60 real-seconds cooldown
                            auto lv = registry.view<EventLog>();
                            if (!lv.empty()) {
                                const auto* myName = registry.try_get<Name>(entity);
                                std::string msg = (myName ? myName->value : "An NPC") +
                                    std::string(" nods respectfully at you.");
                                lv.get<EventLog>(*lv.begin()).Push(
                                    tm.day, (int)tm.hourOfDay, msg);
                            }
                        }
                    }
                }
            }

            // ---- Wave at player when happy: content NPCs create warm ambient feedback (staggered: 1/4 per frame) ----
            if (playerEntity != entt::null && timer.thankCooldown <= 0.f
                && static_cast<uint32_t>(entity) % 4 == static_cast<uint32_t>(s_frameCounter) % 4) {
                float avgN = 0.f;
                for (int i = 0; i < 4; ++i) avgN += needs.list[i].value;
                avgN *= 0.25f;
                if (avgN > 0.8f) {
                    static constexpr float WAVE_RADIUS = 50.f;
                    float wdx = playerPos.x - pos.x, wdy = playerPos.y - pos.y;
                    if (wdx * wdx + wdy * wdy <= WAVE_RADIUS * WAVE_RADIUS) {
                        // 1% chance per real-second
                        static std::mt19937 s_waveRng{ std::random_device{}() };
                        static std::uniform_real_distribution<float> s_waveDist(0.f, 1.f);
                        if (s_waveDist(s_waveRng) < 0.01f * realDt) {
                            timer.thankCooldown = 60.f;
                            auto lv = registry.view<EventLog>();
                            if (lv.begin() != lv.end()) {
                                const auto* myName = registry.try_get<Name>(entity);
                                std::string msg = (myName ? myName->value : "An NPC") +
                                    " waves at you cheerfully.";
                                lv.get<EventLog>(*lv.begin()).Push(
                                    tm.day, (int)tm.hourOfDay, msg);
                            }
                        }
                    }
                }
            }

            // ---- Avoid player with bad reputation: NPCs flee nervously ----
            if (playerEntity != entt::null) {
                if (const auto* rep = registry.try_get<Reputation>(entity)) {
                    if (rep->score < -0.5f) {
                        static constexpr float AVOID_RADIUS = 30.f;
                        float adx = playerPos.x - pos.x, ady = playerPos.y - pos.y;
                        float adSq = adx * adx + ady * ady;
                        if (adSq > 0.01f && adSq <= AVOID_RADIUS * AVOID_RADIUS) {
                            float dist = std::sqrt(adSq);
                            vel.vx = -(adx / dist) * speed * 0.8f;
                            vel.vy = -(ady / dist) * speed * 0.8f;
                            timer.panicTimer = 2.f;
                            auto lv = registry.view<EventLog>();
                            if (lv.begin() != lv.end()) {
                                const auto* myName = registry.try_get<Name>(entity);
                                std::string msg = (myName ? myName->value : "An NPC") +
                                    " hurries away from you nervously.";
                                lv.get<EventLog>(*lv.begin()).Push(
                                    tm.day, (int)tm.hourOfDay, msg);
                            }
                        }
                    }
                }
            }

            // ---- Skill training: skilled NPC teaches nearby unskilled NPC (staggered: 1/4 per frame) ----
            if (timer.teachCooldown > 0.f)
                timer.teachCooldown = std::max(0.f, timer.teachCooldown - realDt);
            if (timer.teachCooldown <= 0.f
                && static_cast<uint32_t>(entity) % 4 == static_cast<uint32_t>(s_frameCounter) % 4) {
                if (auto* mySkills = registry.try_get<Skills>(entity)) {
                    static constexpr float TEACH_RADIUS = 30.f;
                    static constexpr float TEACH_MIN = 0.6f;
                    static constexpr float LEARN_MAX = 0.3f;
                    static constexpr float SKILL_GAIN = 0.005f; // per game-hour
                    bool taught = false;
                    registry.view<AgentState, Position, HomeSettlement, DeprivationTimer, Skills, Name>(
                        entt::exclude<Hauler, PlayerTag, BanditTag>)
                        .each([&](auto other, const AgentState& oState, const Position& oPos,
                                  const HomeSettlement& oHome, DeprivationTimer& oTimer,
                                  Skills& oSkills, const Name& oName) {
                        if (taught) return;
                        if (other == entity) return;
                        if (oState.behavior != AgentBehavior::Idle) return;
                        if (oTimer.teachCooldown > 0.f) return;
                        float tdx = oPos.x - pos.x, tdy = oPos.y - pos.y;
                        if (tdx*tdx + tdy*tdy > TEACH_RADIUS * TEACH_RADIUS) return;
                        // Find a skill where teacher is ≥0.6 and learner is <0.3
                        struct { ResourceType rt; float tVal; float lVal; const char* name; } candidates[3] = {
                            { ResourceType::Food,  mySkills->farming,       oSkills.farming,       "farming" },
                            { ResourceType::Water, mySkills->water_drawing, oSkills.water_drawing, "water carrying" },
                            { ResourceType::Wood,  mySkills->woodcutting,   oSkills.woodcutting,   "woodcutting" },
                        };
                        for (auto& c : candidates) {
                            if (c.tVal >= TEACH_MIN && c.lVal < LEARN_MAX) {
                                float ghDt = dt * GAME_MINS_PER_REAL_SEC / 60.f;
                                oSkills.Advance(c.rt, SKILL_GAIN * ghDt);
                                // Mutual affinity boost
                                if (auto* rel = registry.try_get<Relations>(entity))
                                    rel->affinity[other] = std::min(1.f, rel->affinity[other] + 0.02f);
                                if (auto* oRel = registry.try_get<Relations>(other))
                                    oRel->affinity[entity] = std::min(1.f, oRel->affinity[entity] + 0.02f);
                                // Cooldowns
                                timer.teachCooldown  = 120.f;
                                oTimer.teachCooldown = 120.f;
                                // Mentor bond: learner remembers teacher for gratitude greeting
                                oTimer.lastHelper = entity;
                                // Log
                                auto lv = registry.view<EventLog>();
                                if (lv.begin() != lv.end()) {
                                    const auto* myName = registry.try_get<Name>(entity);
                                    std::string msg = (myName ? myName->value : "An NPC") +
                                        std::string(" teaches ") + oName.value +
                                        " about " + c.name + ".";
                                    lv.get<EventLog>(*lv.begin()).Push(
                                        tm.day, (int)tm.hourOfDay, msg);
                                }
                                taught = true;
                                return;
                            }
                        }
                    });
                }
            }

            // ---- Mood contagion: happy NPCs cheer up struggling neighbours (staggered: 1/4 per frame) ----
            if (timer.moodContagionCooldown > 0.f)
                timer.moodContagionCooldown = std::max(0.f, timer.moodContagionCooldown - realDt);
            {
                float avgNeeds = 0.f;
                for (int i = 0; i < 4; ++i) avgNeeds += needs.list[i].value;
                avgNeeds *= 0.25f;
                // Only struggling NPCs (avg < 0.4) can receive mood contagion
                if (avgNeeds < 0.4f && timer.moodContagionCooldown <= 0.f
                    && static_cast<uint32_t>(entity) % 4 == static_cast<uint32_t>(s_frameCounter) % 4) {
                    registry.view<Needs, Position, AgentState, DeprivationTimer, Name>(
                        entt::exclude<Hauler, PlayerTag, BanditTag>).each(
                        [&](auto other, const Needs& oNeeds, const Position& oPos,
                            const AgentState& oState, DeprivationTimer&, const Name& oName) {
                            if (other == entity) return;
                            if (oState.behavior != AgentBehavior::Idle) return;
                            float oAvg = 0.f;
                            for (int i = 0; i < 4; ++i) oAvg += oNeeds.list[i].value;
                            oAvg *= 0.25f;
                            if (oAvg <= 0.8f) return;
                            float ddx = oPos.x - pos.x, ddy = oPos.y - pos.y;
                            if (ddx * ddx + ddy * ddy > 25.f * 25.f) return;
                            // Boost home settlement morale
                            float ghDt = dt * GAME_MINS_PER_REAL_SEC / 60.f;
                            if (home.settlement != entt::null && registry.valid(home.settlement)) {
                                if (auto* settl = registry.try_get<Settlement>(home.settlement))
                                    settl->morale = std::min(1.f, settl->morale + 0.01f * ghDt);
                            }
                            timer.moodContagionCooldown = 120.f; // 120 game-seconds
                            // Log
                            auto lv = registry.view<EventLog>();
                            if (lv.begin() != lv.end()) {
                                const auto* myName = registry.try_get<Name>(entity);
                                std::string msg = oName.value + " cheers up " +
                                    (myName ? myName->value : "an NPC") + ".";
                                lv.get<EventLog>(*lv.begin()).Push(
                                    tm.day, (int)tm.hourOfDay, msg);
                            }
                        });
                }
            }

            // ---- Family visit: idle NPC may visit family at another settlement ----
            if (!isGrieving) if (auto* ft = registry.try_get<FamilyTag>(entity); ft) {
                static std::mt19937 s_visitRng{ std::random_device{}() };
                static std::uniform_real_distribution<float> s_visitChance(0.f, 1.f);
                // 5% chance per game-hour
                float visitGameHoursDt = dt * GAME_MINS_PER_REAL_SEC / 60.f;
                if (s_visitChance(s_visitRng) < 0.05f * visitGameHoursDt) {
                    // Find a family member at a different settlement
                    entt::entity visitSettl = entt::null;
                    std::string visitSettlName;
                    registry.view<FamilyTag, HomeSettlement, Position>(
                        entt::exclude<PlayerTag, BanditTag>)
                        .each([&](auto other, const FamilyTag& oFt,
                                  const HomeSettlement& oHome, const Position&) {
                        if (visitSettl != entt::null) return;
                        if (other == entity) return;
                        if (oFt.name != ft->name) return;
                        if (oHome.settlement == home.settlement) return;
                        if (oHome.settlement == entt::null || !registry.valid(oHome.settlement)) return;
                        visitSettl = oHome.settlement;
                        if (auto* sn = registry.try_get<Name>(oHome.settlement))
                            visitSettlName = sn->value;
                    });
                    if (visitSettl != entt::null) {
                        timer.visitTimer  = 30.f;  // 30 game-minutes
                        timer.visitTarget = visitSettl;
                        const auto& tgtPos = registry.get<Position>(visitSettl);
                        MoveToward(vel, pos, tgtPos.x, tgtPos.y, speed * 0.8f);
                        // Log the visit
                        auto lv = registry.view<EventLog>();
                        if (lv.begin() != lv.end()) {
                            const auto* myName = registry.try_get<Name>(entity);
                            std::string msg = (myName ? myName->value : "NPC") +
                                " is visiting family in " +
                                (visitSettlName.empty() ? "a settlement" : visitSettlName);
                            lv.get<EventLog>(*lv.begin()).Push(
                                tm.day, (int)tm.hourOfDay, msg);
                        }
                        continue;
                    }
                }
            }

            // ---- Evening gathering (hours 18–21) ----
            // Idle NPCs drift toward their home settlement centre at dusk,
            // making the world visually alive: people return home in the evening.
            if (currentHour >= 18 && currentHour < 21 &&
                home.settlement != entt::null && registry.valid(home.settlement)) {
                const auto& homePos = registry.get<Position>(home.settlement);
                static constexpr float GATHER_ARRIVE = 40.f;
                float dx = homePos.x - pos.x, dy = homePos.y - pos.y;
                float dist2 = dx*dx + dy*dy;
                if (dist2 > GATHER_ARRIVE * GATHER_ARRIVE) {
                    MoveToward(vel, pos, homePos.x, homePos.y, speed * 0.6f);
                } else {
                    vel.vx = vel.vy = 0.f;

                    // ---- Lonely migrant morale drain ----
                    // NPCs with relations but no local friends (none >= 0.3 at settlement) drain morale.
                    {
                        static std::map<int, std::set<entt::entity>> s_lonelyChecked;
                        static int s_lonelyLastDay = -1;
                        int today = (int)tm.day;
                        if (today != s_lonelyLastDay) {
                            s_lonelyChecked.clear();
                            s_lonelyLastDay = today;
                        }
                        if (s_lonelyChecked[today].find(entity) == s_lonelyChecked[today].end()) {
                            const auto* rel = registry.try_get<Relations>(entity);
                            if (rel && !rel->affinity.empty()) {
                                bool hasLocalFriend = false;
                                for (const auto& [other, aff] : rel->affinity) {
                                    if (aff < 0.3f) continue;
                                    if (!registry.valid(other)) continue;
                                    const auto* oHome = registry.try_get<HomeSettlement>(other);
                                    if (oHome && oHome->settlement == home.settlement) {
                                        hasLocalFriend = true;
                                        break;
                                    }
                                }
                                if (!hasLocalFriend) {
                                    if (auto* settl = registry.try_get<Settlement>(home.settlement))
                                        settl->morale = std::max(0.f, settl->morale - 0.005f);
                                    // Log at 1-in-10 frequency
                                    static std::mt19937 s_lonelyRng{ std::random_device{}() };
                                    if (s_lonelyRng() % 10 == 0) {
                                        auto llv = registry.view<EventLog>();
                                        if (!llv.empty()) {
                                            std::string who = "An NPC";
                                            if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                                            std::string where = "settlement";
                                            if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                                where = s->name;
                                            llv.get<EventLog>(*llv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                                who + " feels lonely at " + where);
                                        }
                                    }
                                }
                                s_lonelyChecked[today].insert(entity);
                            }
                        }
                    }

                    // ---- Idle chat: pair up with a nearby Idle neighbour (staggered: 1/4 per frame) ----
                    // When gathered at home and not chatting, scan for another Idle NPC
                    // from the same settlement within 25 units. Stop both for 30–60 game-seconds.
                    static constexpr float CHAT_RADIUS   = 25.f;
                    static std::uniform_real_distribution<float> s_chatDist(30.f, 60.f);
                    static std::mt19937 s_chatRng{ std::random_device{}() };

                    if (static_cast<uint32_t>(entity) % 4 == static_cast<uint32_t>(s_frameCounter) % 4)
                    registry.view<AgentState, Position, HomeSettlement, DeprivationTimer>(
                        entt::exclude<Hauler, PlayerTag, BanditTag>)
                        .each([&](auto other, AgentState& oState, const Position& oPos,
                                  const HomeSettlement& oHome, DeprivationTimer& oTimer) {
                        if (other == entity) return;
                        if (oHome.settlement != home.settlement) return;
                        if (oState.behavior != AgentBehavior::Idle) return;
                        if (oTimer.chatTimer > 0.f) return;  // already chatting
                        float cdx = oPos.x - pos.x, cdy = oPos.y - pos.y;
                        if (cdx*cdx + cdy*cdy > CHAT_RADIUS * CHAT_RADIUS) return;
                        // Found a chat partner — stop both for a random duration
                        float dur = s_chatDist(s_chatRng);
                        timer.chatTimer  = dur;
                        oTimer.chatTimer = dur;
                        // Build affinity: proximity → friendship over time
                        // During a Harvest Festival, double the affinity gain
                        static constexpr float AFFINITY_GAIN = 0.02f;
                        bool festivalActive = false;
                        if (home.settlement != entt::null && registry.valid(home.settlement)) {
                            const auto* settl = registry.try_get<Settlement>(home.settlement);
                            if (settl && settl->modifierName == "Harvest Festival")
                                festivalActive = true;
                        }
                        float affinityGain = festivalActive ? 0.04f : AFFINITY_GAIN;
                        // Work best friend bonus: chatting with your work buddy deepens bonds
                        bool isWorkBuddy = false;
                        if (const auto* myRel = registry.try_get<Relations>(entity)) {
                            if (myRel->workBestFriend == other) {
                                affinityGain = std::max(affinityGain, 0.03f);
                                isWorkBuddy = true;
                            }
                        }
                        if (!isWorkBuddy) {
                            if (const auto* oRel = registry.try_get<Relations>(other)) {
                                if (oRel->workBestFriend == entity) {
                                    affinityGain = std::max(affinityGain, 0.03f);
                                    isWorkBuddy = true;
                                }
                            }
                        }
                        // Grief-born friendship persistence: shared grief deepens bonds
                        bool griefBond = false;
                        if (timer.lastGriefDay >= 0.f && oTimer.lastGriefDay >= 0.f
                            && (tm.day - timer.lastGriefDay) <= 5.f
                            && (tm.day - oTimer.lastGriefDay) <= 5.f) {
                            const auto* relA = registry.try_get<Relations>(entity);
                            const auto* relB = registry.try_get<Relations>(other);
                            float affAB = 0.f, affBA = 0.f;
                            if (relA) { auto it = relA->affinity.find(other);  if (it != relA->affinity.end()) affAB = it->second; }
                            if (relB) { auto it = relB->affinity.find(entity); if (it != relB->affinity.end()) affBA = it->second; }
                            if (affAB >= 0.6f && affBA >= 0.6f) {
                                affinityGain = std::max(affinityGain, 0.03f);
                                griefBond = true;
                            }
                        }
                        if (auto* rel = registry.try_get<Relations>(entity))
                            rel->affinity[other] = std::min(1.f, rel->affinity[other] + affinityGain);
                        if (auto* oRel = registry.try_get<Relations>(other))
                            oRel->affinity[entity] = std::min(1.f, oRel->affinity[entity] + affinityGain);
                        // Expert gratitude: novice gains extra affinity toward expert of same profession
                        {
                            const auto* profA = registry.try_get<Profession>(entity);
                            const auto* profB = registry.try_get<Profession>(other);
                            const auto* skA   = registry.try_get<Skills>(entity);
                            const auto* skB   = registry.try_get<Skills>(other);
                            if (profA && profB && skA && skB && profA->type == profB->type
                                && profA->type != ProfessionType::Idle && profA->type != ProfessionType::Hauler) {
                                auto getSkill = [](const Skills& sk, ProfessionType t) -> float {
                                    switch (t) {
                                        case ProfessionType::Farmer:      return sk.farming;
                                        case ProfessionType::WaterCarrier: return sk.water_drawing;
                                        case ProfessionType::Lumberjack:   return sk.woodcutting;
                                        default: return 0.f;
                                    }
                                };
                                float sA = getSkill(*skA, profA->type);
                                float sB = getSkill(*skB, profB->type);
                                entt::entity novice = entt::null, expert = entt::null;
                                if (sA < 0.5f && sB >= 0.8f) { novice = entity; expert = other; }
                                else if (sB < 0.5f && sA >= 0.8f) { novice = other; expert = entity; }
                                if (novice != entt::null) {
                                    if (auto* nRel = registry.try_get<Relations>(novice))
                                        nRel->affinity[expert] = std::min(1.f, nRel->affinity[expert] + 0.01f); // extra +0.01 on top of normal
                                    if (s_chatRng() % 8 == 0) {
                                        auto egLv = registry.view<EventLog>();
                                        if (!egLv.empty()) {
                                            std::string nN = "A novice", eN = "an expert";
                                            if (const auto* nm = registry.try_get<Name>(novice)) nN = nm->value;
                                            if (const auto* nm2 = registry.try_get<Name>(expert)) eN = nm2->value;
                                            std::string where = "settlement";
                                            if (const auto* stt = registry.try_get<Settlement>(home.settlement))
                                                where = stt->name;
                                            egLv.get<EventLog>(*egLv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                                nN + " thanks " + eN + " for the guidance at " + where + ".");
                                        }
                                    }
                                }
                            }
                        }
                        // Grief-born friendship log at 1-in-8
                        if (griefBond && s_chatRng() % 8 == 0) {
                            auto glv = registry.view<EventLog>();
                            if (!glv.empty()) {
                                std::string nA = "An NPC", nB = "another NPC";
                                if (const auto* nmA = registry.try_get<Name>(entity)) nA = nmA->value;
                                if (const auto* nmB = registry.try_get<Name>(other)) nB = nmB->value;
                                std::string where = "settlement";
                                if (const auto* stt = registry.try_get<Settlement>(home.settlement))
                                    where = stt->name;
                                glv.get<EventLog>(*glv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                    nA + " and " + nB + " share a knowing look at " + where);
                            }
                        }
                        // Work best friend log at 1-in-8
                        if (isWorkBuddy && s_chatRng() % 8 == 0) {
                            auto wblv = registry.view<EventLog>();
                            if (!wblv.empty()) {
                                std::string nA = "An NPC", nB = "another NPC";
                                if (const auto* nmA = registry.try_get<Name>(entity)) nA = nmA->value;
                                if (const auto* nmB = registry.try_get<Name>(other)) nB = nmB->value;
                                std::string where = "settlement";
                                if (const auto* stt = registry.try_get<Settlement>(home.settlement))
                                    where = stt->name;
                                wblv.get<EventLog>(*wblv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                    nA + " catches up with work buddy " + nB + " at " + where);
                            }
                        }
                        // Festival bonding log at 1-in-6 frequency
                        if (festivalActive && s_chatRng() % 6 == 0) {
                            auto flv = registry.view<EventLog>();
                            if (!flv.empty()) {
                                std::string nA = "An NPC", nB = "another NPC";
                                if (const auto* nmA = registry.try_get<Name>(entity)) nA = nmA->value;
                                if (const auto* nmB = registry.try_get<Name>(other)) nB = nmB->value;
                                std::string where = "settlement";
                                if (const auto* stt = registry.try_get<Settlement>(home.settlement))
                                    where = stt->name;
                                flv.get<EventLog>(*flv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                    nA + " and " + nB + " bond over the festival at " + where + ".");
                            }
                        }
                        // Bankruptcy survivor inspiration: survivor inspires non-survivor
                        if (timer.bankruptSurvivor != oTimer.bankruptSurvivor && s_chatRng() % 10 == 0) {
                            entt::entity survivor = timer.bankruptSurvivor ? entity : other;
                            entt::entity listener2 = timer.bankruptSurvivor ? other : entity;
                            if (auto* lRel2 = registry.try_get<Relations>(listener2))
                                lRel2->affinity[survivor] = std::min(1.f, lRel2->affinity[survivor] + 0.02f);
                            auto blv = registry.view<EventLog>();
                            if (!blv.empty()) {
                                std::string sName = "An NPC", lName = "another NPC";
                                if (const auto* ns = registry.try_get<Name>(survivor)) sName = ns->value;
                                if (const auto* nl = registry.try_get<Name>(listener2)) lName = nl->value;
                                std::string where = "settlement";
                                if (const auto* stt = registry.try_get<Settlement>(home.settlement))
                                    where = stt->name;
                                blv.get<EventLog>(*blv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                    sName + " inspires " + lName + " with their comeback story at " + where + ".");
                            }
                        }
                        // Log ~10% of chats for social flavour
                        static std::uniform_real_distribution<float> s_chatLogDist(0.f, 1.f);
                        if (s_chatLogDist(s_chatRng) < 0.1f) {
                            auto clv = registry.view<EventLog>();
                            if (clv.begin() != clv.end()) {
                                std::string nameA = "An NPC", nameB = "another NPC";
                                if (const auto* nA = registry.try_get<Name>(entity)) nameA = nA->value;
                                if (const auto* nB = registry.try_get<Name>(other))  nameB = nB->value;
                                std::string settName = "";
                                if (const auto* stt = registry.try_get<Settlement>(home.settlement))
                                    settName = " near " + stt->name;
                                char buf[160];
                                std::snprintf(buf, sizeof(buf), "%s and %s chat%s.",
                                              nameA.c_str(), nameB.c_str(), settName.c_str());
                                clv.get<EventLog>(*clv.begin()).Push(tm.day, (int)tm.hourOfDay, buf);
                            }
                        }
                        // ---- Elder storytelling: elder + non-elder chat pair ----
                        if (s_chatRng() % 12 == 0) {
                            const auto* ageA = registry.try_get<Age>(entity);
                            const auto* ageB = registry.try_get<Age>(other);
                            entt::entity elder = entt::null, listener = entt::null;
                            if (ageA && ageA->days > 60.f && (!ageB || ageB->days <= 60.f))
                                { elder = entity; listener = other; }
                            else if (ageB && ageB->days > 60.f && (!ageA || ageA->days <= 60.f))
                                { elder = other; listener = entity; }
                            if (elder != entt::null) {
                                // Boost listener's affinity toward elder
                                if (auto* lRel = registry.try_get<Relations>(listener))
                                    lRel->affinity[elder] = std::min(1.f, lRel->affinity[elder] + 0.03f);
                                auto elv = registry.view<EventLog>();
                                if (!elv.empty()) {
                                    std::string elderName = "An elder";
                                    if (const auto* ne = registry.try_get<Name>(elder)) elderName = ne->value;
                                    std::string listenerName = "a listener";
                                    if (const auto* nl = registry.try_get<Name>(listener)) listenerName = nl->value;
                                    std::string settlName = "settlement";
                                    if (home.settlement != entt::null && registry.valid(home.settlement))
                                        if (const auto* s = registry.try_get<Settlement>(home.settlement))
                                            settlName = s->name;
                                    elv.get<EventLog>(*elv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                        elderName + " tells " + listenerName +
                                        " tales of the old days at " + settlName + ".");
                                }
                            }
                        }
                        // ---- Gossip about career changers ----
                        if (s_chatRng() % 8 == 0) {
                            const auto* profA = registry.try_get<Profession>(entity);
                            const auto* profB = registry.try_get<Profession>(other);
                            entt::entity changer = entt::null, listener = entt::null;
                            if (profA && profA->careerChanges >= 2) { changer = entity; listener = other; }
                            else if (profB && profB->careerChanges >= 2) { changer = other; listener = entity; }
                            if (changer != entt::null) {
                                auto glv = registry.view<EventLog>();
                                if (!glv.empty()) {
                                    std::string lName = "An NPC", cName = "someone";
                                    if (const auto* nl = registry.try_get<Name>(listener)) lName = nl->value;
                                    if (const auto* nc = registry.try_get<Name>(changer)) cName = nc->value;
                                    glv.get<EventLog>(*glv.begin()).Push(tm.day, (int)tm.hourOfDay,
                                        lName + " hears about " + cName + "'s varied career.");
                                }
                            }
                        }
                    });
                }
            } else {
                vel.vx = vel.vy = 0.f;
            }
            continue;
        }

        ResourceType resType = ResourceTypeForNeed(needs.list[critIdx].type);
        state.behavior       = BehaviorForNeed(needs.list[critIdx].type);

        entt::entity fac = FindNearestFacility(registry, resType,
                                                home.settlement, pos.x, pos.y);
        if (fac == entt::null) {
            state.behavior = AgentBehavior::Idle;
            vel.vx = vel.vy = 0.f;
            continue;
        }

        state.target = fac;
        const auto& facPos = registry.get<Position>(fac);

        if (InRange(pos.x, pos.y, facPos.x, facPos.y, FACILITY_RANGE)) {
            state.behavior = AgentBehavior::Satisfying;
            vel.vx = vel.vy = 0.f;
        } else {
            MoveToward(vel, pos, facPos.x, facPos.y, speed);
        }
    }
    spFlush(1); // AD:MainLoop (all per-NPC decision logic)

    // ============================================================
    // WANDERING ORPHAN RE-SETTLEMENT
    // Children (ChildTag) with no valid HomeSettlement wander toward
    // the nearest settlement with available pop capacity (within 200 units).
    // ============================================================
    {
        // Pre-build per-settlement current pop count
        std::map<entt::entity, int> orphanPopCount;
        registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
            [&](const HomeSettlement& hs) {
            if (hs.settlement != entt::null) ++orphanPopCount[hs.settlement];
        });

        registry.view<ChildTag, Position, Velocity, MoveSpeed, HomeSettlement, AgentState>().each(
            [&](auto orphan, const Position& pos, Velocity& vel,
                const MoveSpeed& spd, HomeSettlement& home, AgentState& state) {
            // Only process true orphans — no valid home settlement
            if (home.settlement != entt::null && registry.valid(home.settlement)) return;

            // Find nearest settlement with available capacity within 200 units
            entt::entity best   = entt::null;
            float        bestD2 = 200.f * 200.f;
            registry.view<Position, Settlement>().each(
                [&](auto se, const Position& sp, const Settlement& s) {
                int curPop = orphanPopCount.count(se) ? orphanPopCount.at(se) : 0;
                if (curPop >= s.popCap) return;
                float dx = sp.x - pos.x, dy = sp.y - pos.y;
                float d2 = dx*dx + dy*dy;
                if (d2 < bestD2) { bestD2 = d2; best = se; }
            });

            if (best == entt::null) return;

            const auto& destPos = registry.get<Position>(best);
            float dist = std::sqrt(bestD2);

            if (dist <= SETTLE_RANGE) {
                // Arrived — assign home
                home.settlement = best;
                ++orphanPopCount[best];  // update count so two orphans don't pick the same slot
                state.behavior  = AgentBehavior::Idle;
                vel.vx = vel.vy = 0.f;

                // Log
                auto lv = registry.view<EventLog>();
                auto tv = registry.view<TimeManager>();
                if (lv.begin() != lv.end() && tv.begin() != tv.end()) {
                    const auto& tm = tv.get<TimeManager>(*tv.begin());
                    const auto& sn = registry.get<Settlement>(best);
                    char buf[128];
                    const char* who = "Orphan";
                    if (const auto* nm = registry.try_get<Name>(orphan)) who = nm->value.c_str();
                    std::snprintf(buf, sizeof(buf), "%s found a new home at %s.",
                                  who, sn.name.c_str());
                    lv.get<EventLog>(*lv.begin()).Push(tm.day, (int)tm.hourOfDay, buf);
                }
            } else {
                // Move toward destination
                MoveToward(vel, pos, destPos.x, destPos.y, spd.value);
                state.behavior = AgentBehavior::Migrating;
            }
        });
    }
    spFlush(2); // AD:Orphan

    // ============================================================
    // GOSSIP / PRICE SHARING
    // When two NPCs from different settlements are within 30 units,
    // the "visitor's" home Market prices nudge 5% toward the local
    // settlement's prices. Runs at most once per 6 game-hours per NPC.
    // ============================================================
    static constexpr float GOSSIP_RADIUS    = 30.f;
    static constexpr float GOSSIP_NUDGE     = 0.05f;   // 5% nudge toward other's prices
    static constexpr float GOSSIP_COOLDOWN  = 6.f;     // game-hours between gossip events
    float gameHoursDt = dt * GAME_MINS_PER_REAL_SEC / 60.f;

    // ---- Stale rumour removal ----
    // Remove Rumour components whose hops have reached 0 (fully propagated).
    // Also drain/prune rumour immunity timers (48 game-hour cooldown per origin+type+settlement).
    static std::map<std::tuple<entt::entity, RumourType, entt::entity>, float> s_rumourImmunity;
    {
        std::vector<entt::entity> staleRumours;
        registry.view<Rumour>().each([&](auto e, const Rumour& r) {
            if (r.hops <= 0) staleRumours.push_back(e);
        });
        for (auto e : staleRumours) registry.remove<Rumour>(e);

        for (auto it = s_rumourImmunity.begin(); it != s_rumourImmunity.end(); ) {
            if (!registry.valid(std::get<0>(it->first)) ||
                !registry.valid(std::get<2>(it->first)) ||
                (it->second -= gameHoursDt) <= 0.f)
                it = s_rumourImmunity.erase(it);
            else ++it;
        }
    }

    // ---- Affinity decay ----
    // Relations not reinforced by chat drift back toward 0 at 0.001/game-hour.
    // Also prune entries for destroyed entities to prevent map bloat.
    static constexpr float AFFINITY_DECAY = 0.001f;
    registry.view<Relations>().each([&](auto e, Relations& rel) {
        for (auto it = rel.affinity.begin(); it != rel.affinity.end(); ) {
            if (!registry.valid(it->first)) {
                it = rel.affinity.erase(it);
                continue;
            }
            it->second = std::max(0.f, it->second - AFFINITY_DECAY * gameHoursDt);
            if (it->second <= 0.f)
                it = rel.affinity.erase(it);
            else
                ++it;
        }
    });

    // Collect snapshot of NPC positions/home-settlements for the O(N²) check.
    // We use a simple vector to avoid re-querying inside nested loops.
    struct GossipEntry {
        entt::entity entity;
        float        x, y;
        entt::entity homeSettl;
    };
    std::vector<GossipEntry> gossipAgents;
    registry.view<Position, HomeSettlement, DeprivationTimer>(
        entt::exclude<Hauler, PlayerTag>).each(
        [&](auto e, const Position& p, const HomeSettlement& hs, DeprivationTimer& tmr) {
            // Drain cooldown every frame
            if (tmr.gossipCooldown > 0.f)
                tmr.gossipCooldown = std::max(0.f, tmr.gossipCooldown - gameHoursDt);
            if (hs.settlement != entt::null && registry.valid(hs.settlement))
                gossipAgents.push_back({ e, p.x, p.y, hs.settlement });
        });

    for (std::size_t i = 0; i < gossipAgents.size(); ++i) {
        auto& A = gossipAgents[i];
        auto* tmrA = registry.try_get<DeprivationTimer>(A.entity);
        if (!tmrA || tmrA->gossipCooldown > 0.f) continue;

        // A needs a Market at their home settlement to update
        auto* mktA = registry.try_get<Market>(A.homeSettl);
        if (!mktA) continue;

        for (std::size_t j = i + 1; j < gossipAgents.size(); ++j) {
            auto& B = gossipAgents[j];
            if (B.homeSettl == A.homeSettl) continue;   // same settlement — no gossip

            float dx = B.x - A.x, dy = B.y - A.y;
            if (dx*dx + dy*dy > GOSSIP_RADIUS * GOSSIP_RADIUS) continue;

            auto* tmrB = registry.try_get<DeprivationTimer>(B.entity);
            auto* mktB = registry.try_get<Market>(B.homeSettl);
            if (!mktB) continue;

            bool bWasReady = (tmrB && tmrB->gossipCooldown <= 0.f);

            // Nudge A's home prices toward B's, and B's home prices toward A's.
            for (auto& [res, priceA] : mktA->price) {
                float priceB = mktB->GetPrice(res);
                priceA += (priceB - priceA) * GOSSIP_NUDGE;
            }
            // A learns B's settlement prices (for migration decision-making)
            if (auto* memA = registry.try_get<MigrationMemory>(A.entity)) {
                if (const auto* sttB = registry.try_get<Settlement>(B.homeSettl))
                    memA->Record(sttB->name,
                        mktB->GetPrice(ResourceType::Food),
                        mktB->GetPrice(ResourceType::Water),
                        mktB->GetPrice(ResourceType::Wood), tm.day);
            }
            if (bWasReady) {
                for (auto& [res, priceB] : mktB->price) {
                    float priceA = mktA->GetPrice(res);
                    priceB += (priceA - priceB) * GOSSIP_NUDGE;
                }
                // B learns A's settlement prices
                if (auto* memB = registry.try_get<MigrationMemory>(B.entity)) {
                    if (const auto* sttA = registry.try_get<Settlement>(A.homeSettl))
                        memB->Record(sttA->name,
                            mktA->GetPrice(ResourceType::Food),
                            mktA->GetPrice(ResourceType::Water),
                            mktA->GetPrice(ResourceType::Wood), tm.day);
                }
                tmrB->gossipCooldown = GOSSIP_COOLDOWN;
            }
            tmrA->gossipCooldown = GOSSIP_COOLDOWN;

            // ---- Rumour spreading ----
            // If one NPC carries a rumour and the other doesn't, pass it along (hops-1).
            // When the rumour first arrives at a new settlement, apply a market fear effect.
            auto spreadRumour = [&](entt::entity carrier, entt::entity recipient,
                                    entt::entity recipientSettl) {
                auto* rum = registry.try_get<Rumour>(carrier);
                if (!rum || rum->hops <= 0) return;
                if (registry.any_of<Rumour>(recipient)) return;  // already has a rumour

                int newHops = rum->hops - 1;
                registry.emplace<Rumour>(recipient, Rumour{rum->type, rum->origin, newHops});

                // Apply market effect only if this settlement isn't immune to this rumour.
                if (recipientSettl == rum->origin) return;  // same settlement — no fear effect
                auto key = std::make_tuple(rum->origin, rum->type, recipientSettl);
                if (s_rumourImmunity.count(key)) return;   // still immune from previous delivery
                s_rumourImmunity[key] = 48.f;              // 48 game-hour immunity

                auto* mkt = registry.try_get<Market>(recipientSettl);
                auto* stt = registry.try_get<Settlement>(recipientSettl);
                if (!mkt || !stt) return;

                const char* rumourLabel = nullptr;
                if (rum->type == RumourType::PlagueNearby) {
                    mkt->price[ResourceType::Food] =
                        std::min(mkt->price[ResourceType::Food] * 1.10f, 20.f);
                    rumourLabel = "plague";
                } else if (rum->type == RumourType::DroughtNearby) {
                    mkt->price[ResourceType::Water] =
                        std::min(mkt->price[ResourceType::Water] * 1.15f, 20.f);
                    rumourLabel = "drought";
                } else if (rum->type == RumourType::BanditRoads) {
                    rumourLabel = "bandits";
                } else if (rum->type == RumourType::GoodHarvest) {
                    mkt->price[ResourceType::Food] =
                        std::max(mkt->price[ResourceType::Food] * 0.95f, 0.5f);
                    rumourLabel = "good harvest";
                }
                if (rumourLabel) {
                    auto lv = registry.view<EventLog>();
                    if (lv.begin() != lv.end())
                        lv.get<EventLog>(*lv.begin()).Push(
                            tm.day, (int)tm.hourOfDay,
                            std::string("Rumour of ") + rumourLabel + " reached " + stt->name + ".");
                }
            };

            spreadRumour(A.entity, B.entity, B.homeSettl);
            if (bWasReady)
                spreadRumour(B.entity, A.entity, A.homeSettl);

            // ---- Illness contagion ----
            // 10% chance per gossip encounter to spread illness from sick to healthy NPC.
            {
                static std::mt19937 s_illRng{ std::random_device{}() };
                static std::uniform_real_distribution<float> s_illDist(0.f, 1.f);
                static constexpr float CONTAGION_CHANCE = 0.10f;
                static constexpr float CONTAGION_ILLNESS_DUR = 6.f; // game-hours

                auto trySpread = [&](entt::entity sick, entt::entity healthy) {
                    auto* dtSick    = registry.try_get<DeprivationTimer>(sick);
                    auto* dtHealthy = registry.try_get<DeprivationTimer>(healthy);
                    if (!dtSick || !dtHealthy) return;
                    if (dtSick->illnessTimer <= 0.f || dtHealthy->illnessTimer > 0.f) return;
                    if (s_illDist(s_illRng) > CONTAGION_CHANCE) return;
                    dtHealthy->illnessTimer   = CONTAGION_ILLNESS_DUR;
                    dtHealthy->illnessNeedIdx = dtSick->illnessNeedIdx;
                    auto lv = registry.view<EventLog>();
                    if (lv.begin() != lv.end()) {
                        std::string sickName = "An NPC", healthyName = "An NPC";
                        if (const auto* n = registry.try_get<Name>(sick))    sickName = n->value;
                        if (const auto* n = registry.try_get<Name>(healthy)) healthyName = n->value;
                        char buf[128];
                        std::snprintf(buf, sizeof(buf), "%s caught illness from %s",
                            healthyName.c_str(), sickName.c_str());
                        lv.get<EventLog>(*lv.begin()).Push(tm.day, (int)tm.hourOfDay, buf);
                    }
                };
                trySpread(A.entity, B.entity);
                trySpread(B.entity, A.entity);
            }

            // ---- Low-morale grumbling ----
            // When either NPC lives at a low-morale settlement, 20% chance to log grumbling.
            // Rate-limited per settlement to once per 12 game-hours.
            {
                static std::map<entt::entity, float> s_grumbleCooldown;
                // Drain cooldowns
                for (auto it = s_grumbleCooldown.begin(); it != s_grumbleCooldown.end(); ) {
                    it->second -= gameHoursDt;
                    if (it->second <= 0.f) it = s_grumbleCooldown.erase(it);
                    else ++it;
                }
                auto checkGrumble = [&](entt::entity npcA, entt::entity npcB,
                                        entt::entity settl) {
                    if (s_grumbleCooldown.count(settl)) return;
                    auto* stt = registry.try_get<Settlement>(settl);
                    if (!stt || stt->morale >= 0.3f) return;
                    static std::mt19937 s_grumbleRng{ std::random_device{}() };
                    static std::uniform_real_distribution<float> s_grumbleDist(0.f, 1.f);
                    if (s_grumbleDist(s_grumbleRng) > 0.20f) return;
                    s_grumbleCooldown[settl] = 12.f;
                    std::string nameA = "An NPC", nameB = "An NPC";
                    if (const auto* n = registry.try_get<Name>(npcA)) nameA = n->value;
                    if (const auto* n = registry.try_get<Name>(npcB)) nameB = n->value;
                    auto lv = registry.view<EventLog>();
                    if (lv.begin() != lv.end())
                        lv.get<EventLog>(*lv.begin()).Push(
                            tm.day, (int)tm.hourOfDay,
                            nameA + " and " + nameB + " grumble about conditions at " + stt->name + ".");
                };
                checkGrumble(A.entity, B.entity, A.homeSettl);
                checkGrumble(A.entity, B.entity, B.homeSettl);
            }

            break;  // A gossips with at most one NPC per cooldown window
        }
    }
    spFlush(3); // AD:Gossip

    // ============================================================
    // FAMILY PAIRING
    // Every 12 game-hours, find pairs of unpaired adults (age ≥ 18,
    // same settlement, no FamilyTag) and give them a shared FamilyTag.
    // The family name is the most common surname at that settlement;
    // on a tie the first NPC's surname is used.
    // ============================================================
    static constexpr float FAMILY_CHECK_INTERVAL = 12.f;   // game-hours
    static float s_familyAccum = 0.f;
    s_familyAccum += gameHoursDt;
    if (s_familyAccum >= FAMILY_CHECK_INTERVAL) {
        s_familyAccum -= FAMILY_CHECK_INTERVAL;

        // Collect unpaired adults grouped by settlement
        struct UnpairedAdult {
            entt::entity entity;
            std::string  surname;
        };
        std::map<entt::entity, std::vector<UnpairedAdult>> bySettlement;

        registry.view<Age, HomeSettlement, Name>(
            entt::exclude<FamilyTag, ChildTag, Hauler, PlayerTag>).each(
            [&](auto e, const Age& age, const HomeSettlement& hs, const Name& n) {
                if (age.days < 18.f) return;
                if (hs.settlement == entt::null || !registry.valid(hs.settlement)) return;
                std::string surname;
                auto sp = n.value.rfind(' ');
                if (sp != std::string::npos) surname = n.value.substr(sp + 1);
                bySettlement[hs.settlement].push_back({ e, surname });
            });

        for (auto& [settl, adults] : bySettlement) {
            if (adults.size() < 2) continue;

            // Build surname frequency map from ALL residents (adults + paired) for tie-breaking.
            std::map<std::string, int> surnameFreq;
            registry.view<HomeSettlement, Name>(entt::exclude<ChildTag, PlayerTag>).each(
                [&](const HomeSettlement& hs2, const Name& n2) {
                    if (hs2.settlement != settl) return;
                    auto sp2 = n2.value.rfind(' ');
                    if (sp2 != std::string::npos)
                        ++surnameFreq[n2.value.substr(sp2 + 1)];
                });

            // Pair adults two-by-two
            for (std::size_t i = 0; i + 1 < adults.size(); i += 2) {
                auto& A = adults[i];
                auto& B = adults[i + 1];

                // Determine family name: most common surname at this settlement
                std::string familyName = A.surname;
                if (!surnameFreq.empty()) {
                    auto best = std::max_element(surnameFreq.begin(), surnameFreq.end(),
                        [](const auto& x, const auto& y){ return x.second < y.second; });
                    if (!best->first.empty()) familyName = best->first;
                }
                if (familyName.empty()) familyName = B.surname;
                if (familyName.empty()) continue;

                registry.emplace_or_replace<FamilyTag>(A.entity, FamilyTag{ familyName });
                registry.emplace_or_replace<FamilyTag>(B.entity, FamilyTag{ familyName });
            }
        }
    }

    // ============================================================
    // CHARITY: NPC helps starving neighbour
    // A well-fed wealthy NPC (Hunger > 0.8, Money > 20g) within
    // CHARITY_RADIUS units of a starving NPC (Hunger < 0.2) gifts 5g.
    // The starving NPC uses it to buy food immediately (market purchase).
    // Happens at most once per 24 game-hours per helper.
    // ============================================================
    static constexpr float CHARITY_RADIUS   = 80.f;
    static constexpr float CHARITY_GIFT     = 5.f;
    static constexpr float CHARITY_COOLDOWN = 24.f;   // game-hours
    static constexpr float HUNGER_HELPER    = 0.8f;   // well-fed threshold
    static constexpr float HUNGER_STARVING  = 0.2f;   // starving threshold
    static constexpr float MONEY_HELPER_MIN = 20.f;   // must have at least this to donate

    // Drain charity timers and build candidate list
    struct CharityEntry {
        entt::entity entity;
        float        x, y;
        float        hunger;
        float        balance;
        entt::entity homeSettl;
        bool         canHelp;     // well-fed + rich + cooldown done
        bool         isStarving;
    };
    std::vector<CharityEntry> charityAgents;
    registry.view<Position, Needs, Money, HomeSettlement, DeprivationTimer>(
        entt::exclude<Hauler, PlayerTag, ChildTag>).each(
        [&](auto e, const Position& p, const Needs& n, const Money& m,
            const HomeSettlement& hs, DeprivationTimer& tmr) {
            tmr.charityTimer = std::max(0.f, tmr.charityTimer - gameHoursDt);
            tmr.helpedTimer  = std::max(0.f, tmr.helpedTimer  - gameHoursDt);
            if (hs.settlement == entt::null || !registry.valid(hs.settlement)) return;
            float hunger = n.list[(int)NeedType::Hunger].value;
            charityAgents.push_back({
                e, p.x, p.y, hunger, m.balance, hs.settlement,
                /*canHelp=*/  (hunger >= HUNGER_HELPER && m.balance >= MONEY_HELPER_MIN
                               && tmr.charityTimer <= 0.f),
                /*isStarving*/(hunger < HUNGER_STARVING)
            });
        });

    // Get EventLog for charity messages
    auto elv2 = registry.view<EventLog>();
    EventLog* charityLog = (elv2.begin() == elv2.end())
                           ? nullptr : &elv2.get<EventLog>(*elv2.begin());
    auto tmv2 = registry.view<TimeManager>();
    int  charityDay  = 1;
    int  charityHour = 0;
    if (tmv2.begin() != tmv2.end()) {
        const auto& ctm = tmv2.get<TimeManager>(*tmv2.begin());
        charityDay  = ctm.day;
        charityHour = (int)ctm.hourOfDay;
    }

    static constexpr float FRIEND_CHARITY_MIN = 1.f;  // friends help even with little gold

    for (auto& helper : charityAgents) {
      // Two passes: family first, then non-family
      bool helped = false;
      for (int pass = 0; pass < 2 && !helped; ++pass) {
        for (auto& starving : charityAgents) {
            if (starving.entity == helper.entity) continue;
            if (!starving.isStarving) continue;

            // Family check — family members help unconditionally (just need ≥1g)
            bool isFamily = false;
            if (const auto* hFam = registry.try_get<FamilyTag>(helper.entity)) {
                if (const auto* sFam = registry.try_get<FamilyTag>(starving.entity)) {
                    if (!hFam->name.empty() && hFam->name == sFam->name)
                        isFamily = true;
                }
            }
            // Pass 0: only family. Pass 1: only non-family.
            if (pass == 0 && !isFamily) continue;
            if (pass == 1 && isFamily)  continue;
            bool familyCanHelp = isFamily && helper.balance >= FRIEND_CHARITY_MIN;

            // Check if helper qualifies: normal canHelp OR friend with ≥1g and well-fed OR family
            bool isFriend = false;
            if (const auto* rel = registry.try_get<Relations>(helper.entity)) {
                auto it = rel->affinity.find(starving.entity);
                if (it != rel->affinity.end() && it->second >= FRIEND_THRESHOLD)
                    isFriend = true;
            }
            bool friendCanHelp = isFriend && helper.hunger >= HUNGER_HELPER
                                 && helper.balance >= FRIEND_CHARITY_MIN;
            if (!helper.canHelp && !friendCanHelp && !familyCanHelp) continue;

            // Check helper cooldown (both normal and friend paths respect it)
            if (auto* helperTmrCheck = registry.try_get<DeprivationTimer>(helper.entity))
                if (helperTmrCheck->charityTimer > 0.f) continue;

            float dx = starving.x - helper.x, dy = starving.y - helper.y;
            if (dx*dx + dy*dy > CHARITY_RADIUS * CHARITY_RADIUS) continue;

            // Skip NPCs with bad reputation — community cold-shoulders antisocial individuals
            // (family members bypass this check)
            static constexpr float CHARITY_REP_THRESHOLD = -0.5f;
            if (!isFamily) {
                if (const auto* starvingRep = registry.try_get<Reputation>(starving.entity)) {
                    if (starvingRep->score < CHARITY_REP_THRESHOLD) {
                        if (charityLog) {
                            std::string helperName = "Someone";
                            std::string starvName  = "a neighbour";
                            if (const auto* hn = registry.try_get<Name>(helper.entity))   helperName = hn->value;
                            if (const auto* sn = registry.try_get<Name>(starving.entity)) starvName  = sn->value;
                            charityLog->Push(charityDay, charityHour,
                                helperName + " refused to help " + starvName + " (bad reputation).");
                        }
                        continue;
                    }
                }
            }

            // Transfer gold: helper → starving NPC (peer transfer, gold flow rule satisfied)
            auto* helperMoney   = registry.try_get<Money>(helper.entity);
            auto* starvingMoney = registry.try_get<Money>(starving.entity);
            auto* starvingTmr   = registry.try_get<DeprivationTimer>(starving.entity);
            if (!helperMoney || !starvingMoney) continue;

            helperMoney->balance  -= CHARITY_GIFT;
            starvingMoney->balance += CHARITY_GIFT;

            // Immediately buy food for the starving NPC at home market price
            auto* mkt = registry.try_get<Market>(starving.homeSettl);
            auto* sp  = registry.try_get<Stockpile>(starving.homeSettl);
            auto* sett = registry.try_get<Settlement>(starving.homeSettl);
            if (mkt && sp && sett) {
                float price = mkt->GetPrice(ResourceType::Food);
                if (starvingMoney->balance >= price) {
                    starvingMoney->balance -= price;
                    sett->treasury         += price;
                    sp->quantities[ResourceType::Food] += 1.f;
                }
            }

            // Reset the starving NPC's purchaseTimer so ConsumptionSystem acts promptly.
            // Mark them as recently helped (shown in HUD tooltip for 1 game-hour).
            // Set gratitude walk: move toward helper for 30–60 real-seconds.
            if (starvingTmr) {
                starvingTmr->purchaseTimer   = 0.f;
                starvingTmr->helpedTimer     = 1.f;   // 1 game-hour display window
                starvingTmr->gratitudeTarget = helper.entity;
                starvingTmr->lastHelper      = helper.entity;
                static std::uniform_real_distribution<float> s_gratDist(30.f, 60.f);
                static std::mt19937 s_gratRng{ std::random_device{}() };
                starvingTmr->gratitudeTimer  = s_gratDist(s_gratRng);
            }

            // Set helper cooldown
            auto* helperTmr = registry.try_get<DeprivationTimer>(helper.entity);
            if (helperTmr) helperTmr->charityTimer = CHARITY_COOLDOWN;
            // Charity boosts reputation (+0.2 per act of generosity)
            {
                auto& rep = registry.get_or_emplace<Reputation>(helper.entity);
                rep.score += 0.2f;
            }
            helper.canHelp = false;   // don't help a second NPC this frame

            // Warmth "warm glow" buff: giving charity raises the helper's Heat need slightly.
            if (auto* helperNeeds = registry.try_get<Needs>(helper.entity)) {
                auto& heat = helperNeeds->list[(int)NeedType::Heat].value;
                heat = std::min(1.f, heat + 0.15f);
            }

            // Increment charity counter for this helper
            int charityN = ++s_charityCount[helper.entity];

            // Log — name both helper and recipient, settlement, and frequency
            if (charityLog) {
                std::string who      = "An NPC";
                std::string whom     = "a neighbour";
                std::string at       = "";
                if (const auto* n = registry.try_get<Name>(helper.entity))   who  = n->value;
                if (const auto* n = registry.try_get<Name>(starving.entity)) whom = n->value;
                if (sett) at = " at " + sett->name;
                std::string suffix = (charityN > 1)
                    ? " (x" + std::to_string(charityN) + ")" : "";
                if (isFamily) {
                    charityLog->Push(charityDay, charityHour,
                        who + " feeds family member " + whom + at + "." + suffix);
                } else {
                    charityLog->Push(charityDay, charityHour,
                        who + " helped " + whom + at + "." + suffix);
                }
            }
            // Charity chain reaction: high-reputation giver triggers pay-it-forward
            {
                static std::mt19937 s_chainRng{std::random_device{}()};
                const auto* giverRep = registry.try_get<Reputation>(helper.entity);
                if (giverRep && giverRep->score >= 0.5f && s_chainRng() % 6 == 0) {
                    static constexpr float CHAIN_GIFT = 3.f;
                    // Recipient looks for a poor NPC at the same settlement
                    if (starvingMoney->balance >= CHAIN_GIFT) {
                        entt::entity chainTarget = entt::null;
                        registry.view<Money, HomeSettlement, Position>(
                            entt::exclude<Hauler, PlayerTag, ChildTag>).each(
                            [&](auto cand, const Money& candMoney, const HomeSettlement& candHs, const Position& candPos) {
                                if (chainTarget != entt::null) return;
                                if (cand == starving.entity || cand == helper.entity) return;
                                if (candHs.settlement != starving.homeSettl) return;
                                if (candMoney.balance >= 10.f) return;  // not poor enough
                                float cdx = candPos.x - starving.x, cdy = candPos.y - starving.y;
                                if (cdx*cdx + cdy*cdy > CHARITY_RADIUS * CHARITY_RADIUS) return;
                                chainTarget = cand;
                            });
                        if (chainTarget != entt::null) {
                            auto* chainMoney = registry.try_get<Money>(chainTarget);
                            if (chainMoney) {
                                // Gold Flow Rule: balance-to-balance
                                starvingMoney->balance -= CHAIN_GIFT;
                                chainMoney->balance    += CHAIN_GIFT;
                                if (charityLog) {
                                    std::string rName = "An NPC", gName = "someone";
                                    if (const auto* rn = registry.try_get<Name>(starving.entity)) rName = rn->value;
                                    if (const auto* gn = registry.try_get<Name>(helper.entity)) gName = gn->value;
                                    std::string where = "settlement";
                                    if (sett) where = sett->name;
                                    charityLog->Push(charityDay, charityHour,
                                        rName + " passes on " + gName + "'s generosity at " + where + ".");
                                }
                            }
                        }
                    }
                }
            }
            helped = true;
            break;   // helper gives to at most one starving NPC per cooldown window
        }
      } // end two-pass loop
    }

    // ============================================================
    // ORPHAN ADOPTION
    // Adults with charityTimer == 0 at a settlement with pop < popCap-1
    // adopt nearby orphans (ChildTag, no valid home, within 60 units).
    // Gives the orphan the adopter's family name and home settlement.
    // ============================================================
    {
        static constexpr float ADOPT_RANGE   = 60.f;
        static constexpr float ADOPT_RANGE2  = ADOPT_RANGE * ADOPT_RANGE;
        static constexpr float ADOPT_COOLDOWN = 120.f; // reuse charity cooldown (game-hours)

        // Build per-settlement pop counts
        std::map<entt::entity, int> adoptPopCount;
        registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
            [&](const HomeSettlement& hs) {
            if (hs.settlement != entt::null) ++adoptPopCount[hs.settlement];
        });

        // Collect orphans: children with no valid home
        struct OrphanInfo { entt::entity e; float x, y; bool adopted = false; };
        std::vector<OrphanInfo> orphans;
        registry.view<ChildTag, Position, HomeSettlement>().each(
            [&](auto oe, const Position& op, const HomeSettlement& oh) {
            if (oh.settlement == entt::null || !registry.valid(oh.settlement))
                orphans.push_back({oe, op.x, op.y});
        });

        if (!orphans.empty()) {
            // Iterate adults who could adopt
            registry.view<Position, HomeSettlement, DeprivationTimer>(
                entt::exclude<ChildTag, Hauler, PlayerTag, BanditTag>).each(
                [&](auto ae, const Position& ap, const HomeSettlement& ahs,
                    DeprivationTimer& atm) {
                if (ahs.settlement == entt::null || !registry.valid(ahs.settlement)) return;
                if (atm.charityTimer > 0.f) return;
                // Check settlement has room
                const auto* sett = registry.try_get<Settlement>(ahs.settlement);
                if (!sett) return;
                int curPop = adoptPopCount.count(ahs.settlement) ? adoptPopCount.at(ahs.settlement) : 0;
                if (curPop >= sett->popCap - 1) return;

                // Find nearest unadopted orphan within range
                float bestD2 = ADOPT_RANGE2;
                OrphanInfo* bestOrphan = nullptr;
                for (auto& o : orphans) {
                    if (o.adopted) continue;
                    float dx = o.x - ap.x, dy = o.y - ap.y;
                    float d2 = dx*dx + dy*dy;
                    if (d2 < bestD2) { bestD2 = d2; bestOrphan = &o; }
                }
                if (!bestOrphan) return;

                // Adopt: assign home and family
                bestOrphan->adopted = true;
                auto& orphanHome = registry.get<HomeSettlement>(bestOrphan->e);
                orphanHome.settlement = ahs.settlement;
                ++adoptPopCount[ahs.settlement];

                // Give adopter's family name (or emplace new FamilyTag)
                const auto* adopterFam = registry.try_get<FamilyTag>(ae);
                if (adopterFam && !adopterFam->name.empty()) {
                    registry.emplace_or_replace<FamilyTag>(bestOrphan->e, FamilyTag{adopterFam->name});
                }

                // Set charity cooldown on adopter
                atm.charityTimer = ADOPT_COOLDOWN;

                // Log
                if (charityLog) {
                    std::string adopterName = "An NPC";
                    std::string orphanName  = "an orphan";
                    if (const auto* an = registry.try_get<Name>(ae))             adopterName = an->value;
                    if (const auto* on = registry.try_get<Name>(bestOrphan->e))  orphanName  = on->value;
                    charityLog->Push(charityDay, charityHour,
                        adopterName + " took in orphan " + orphanName + " at " + sett->name + ".");
                }
            });
        }
    }

    // ============================================================
    // TRADE GIFT BETWEEN FRIENDS
    // NPCs with a close friend (affinity ≥ 0.6) at the same settlement
    // gift 5g once per 48 game-hours. Gold flows balance-to-balance.
    // ============================================================
    {
        static constexpr float GIFT_COOLDOWN   = 48.f;   // game-hours
        static constexpr float GIFT_AFFINITY   = 0.6f;
        static constexpr float GIFT_MIN_BAL    = 50.f;   // sender must have > 50g
        static constexpr float GIFT_AMOUNT     = 5.f;

        registry.view<Relations, Money, HomeSettlement, DeprivationTimer, Name>(
            entt::exclude<Hauler, PlayerTag, ChildTag, BanditTag>).each(
            [&](auto giver, const Relations& rel, Money& giverMoney,
                const HomeSettlement& giverHome, DeprivationTimer& giverTmr,
                const Name& giverName) {
            if (giverTmr.charityTimer > 0.f) return;
            if (giverMoney.balance <= GIFT_MIN_BAL) return;
            if (giverHome.settlement == entt::null) return;

            // Find best friend at same settlement with affinity ≥ 0.6
            entt::entity bestFriend = entt::null;
            float bestAff = 0.f;
            for (const auto& [other, aff] : rel.affinity) {
                if (aff < GIFT_AFFINITY) continue;
                auto sit = s_entitySettlement.find(other);
                if (sit == s_entitySettlement.end() || sit->second != giverHome.settlement) continue;
                auto* otherMoney = registry.try_get<Money>(other);
                if (!otherMoney) continue;
                if (aff > bestAff) { bestAff = aff; bestFriend = other; }
            }
            if (bestFriend == entt::null) return;

            // Close friends (recipient affinity ≥ 0.8 toward giver) give larger gifts
            float giftAmt = GIFT_AMOUNT;
            if (const auto* recipRel2 = registry.try_get<Relations>(bestFriend)) {
                auto it = recipRel2->affinity.find(giver);
                if (it != recipRel2->affinity.end() && it->second >= 0.8f)
                    giftAmt = 8.f;
            }

            // Transfer gold: balance-to-balance (no treasury)
            giverMoney.balance -= giftAmt;
            registry.get<Money>(bestFriend).balance += giftAmt;
            giverTmr.charityTimer = GIFT_COOLDOWN;

            // Reciprocity: boost recipient's affinity toward giver
            if (auto* recipRel = registry.try_get<Relations>(bestFriend)) {
                float& recAff = recipRel->affinity[giver];
                recAff = std::min(1.0f, recAff + 0.05f);
            }

            // Log
            if (charityLog) {
                std::string friendName = "a friend";
                if (const auto* fn = registry.try_get<Name>(bestFriend))
                    friendName = fn->value;
                charityLog->Push(charityDay, charityHour,
                    giverName.value + " gifts gold to " + friendName + ".");

                // Thank-you log: recipient acknowledges the gift (1-in-3)
                static std::mt19937 s_thankRng{ std::random_device{}() };
                if (s_thankRng() % 3 == 0) {
                    std::string settlName = "settlement";
                    if (giverHome.settlement != entt::null && registry.valid(giverHome.settlement))
                        if (const auto* s = registry.try_get<Settlement>(giverHome.settlement))
                            settlName = s->name;
                    charityLog->Push(charityDay, charityHour,
                        friendName + " thanks " + giverName.value +
                        " for the gift at " + settlName + ".");
                }
            }
        });
    }
    spFlush(4); // AD:Social (family pairing, charity, adoption, trade gifts)

    // ============================================================
    // BANDIT PROMOTION & BEHAVIOUR
    // Exiles (home.settlement == entt::null) with balance < 2g for
    // 48+ game-hours become bandits (BanditTag). Bandits lurk near
    // the nearest Road midpoint and intercept haulers within 40 units,
    // stealing 30% of cargo (converted to gold at 3g/unit). Removed
    // when balance recovers above 20g.
    // ============================================================
    static constexpr float BANDIT_POVERTY_THRESH  = 2.f;    // gold below this → accrue poverty
    static constexpr float BANDIT_PROMOTE_HOURS   = 48.f;   // poverty hours before turning bandit
    static constexpr float BANDIT_RECOVER_BALANCE = 20.f;   // gold to go straight again
    static constexpr float BANDIT_INTERCEPT_RANGE = 40.f;   // units to intercept a hauler
    static constexpr float BANDIT_STEAL_FRACTION  = 0.30f;  // fraction of cargo to steal
    static constexpr float BANDIT_CARGO_GOLD_RATE = 3.f;    // gold per stolen cargo unit

    auto banditELV = registry.view<EventLog>();
    EventLog* banditLog = (banditELV.begin() == banditELV.end())
                          ? nullptr : &banditELV.get<EventLog>(*banditELV.begin());

    // Pre-compute road midpoints once — reused for banditsPerRoad, lurk target, and gang checks.
    struct RoadMid { entt::entity entity; float mx; float my; bool blocked; };
    static std::vector<RoadMid> s_roadMids;
    s_roadMids.clear();
    registry.view<Road>().each([&](auto re, const Road& road) {
        const auto* pa = registry.try_get<Position>(road.from);
        const auto* pb = registry.try_get<Position>(road.to);
        if (pa && pb)
            s_roadMids.push_back({re, (pa->x + pb->x) * 0.5f, (pa->y + pb->y) * 0.5f, road.blocked});
    });

    // Helper: find nearest non-blocked road midpoint to a position.
    auto findNearestRoad = [](float px, float py) -> std::pair<entt::entity, std::pair<float,float>> {
        entt::entity nearest = entt::null;
        float bestD2 = std::numeric_limits<float>::max();
        float bestMx = 0.f, bestMy = 0.f;
        for (const auto& rm : s_roadMids) {
            if (rm.blocked) continue;
            float dx = rm.mx - px, dy = rm.my - py;
            float d2 = dx*dx + dy*dy;
            if (d2 < bestD2) { bestD2 = d2; nearest = rm.entity; bestMx = rm.mx; bestMy = rm.my; }
        }
        return {nearest, {bestMx, bestMy}};
    };

    // Pre-count bandits per road for density cap (max 3 per road midpoint).
    std::map<entt::entity, int> banditsPerRoad;
    for (const auto& [bx, by] : s_banditPositions) {
        auto [roadEnt, mid] = findNearestRoad(bx, by);
        if (roadEnt != entt::null) banditsPerRoad[roadEnt]++;
    }

    // Iterate all NPCs that could be bandits (includes current BanditTag entities).
    registry.view<Position, Velocity, MoveSpeed, Needs, AgentState,
                  HomeSettlement, DeprivationTimer, Money>(
        entt::exclude<Hauler, PlayerTag, ChildTag>).each(
        [&](auto e, Position& pos, Velocity& vel, const MoveSpeed& spd,
            Needs& /*needs*/, AgentState& state, HomeSettlement& home,
            DeprivationTimer& timer, Money& money)
        {
            bool isBanditNow = registry.all_of<BanditTag>(e);

            // NPCs with a home cannot be bandits
            if (home.settlement != entt::null && registry.valid(home.settlement)) {
                if (isBanditNow) {
                    registry.remove<BanditTag>(e);
                    isBanditNow = false;
                }
                timer.banditPovertyTimer = 0.f;
                return;
            }

            // Recover from banditry if they scraped together enough money
            if (isBanditNow && money.balance >= BANDIT_RECOVER_BALANCE) {
                registry.remove<BanditTag>(e);
                isBanditNow = false;
                timer.banditPovertyTimer = 0.f;
            }

            // Wanderer re-settlement: exile with enough gold can buy a fresh start
            static constexpr float RESETTLE_COST = 30.f;
            if (!isBanditNow && money.balance >= RESETTLE_COST) {
                entt::entity bestSettl = entt::null;
                float bestScore = -std::numeric_limits<float>::max();
                // Check if wanderer has friends for settlement preference
                const auto* wandRel = registry.try_get<Relations>(e);
                // Build set of settlements where this NPC has a friend (affinity >= 0.3)
                std::unordered_set<entt::entity> friendSettlements;
                if (wandRel) {
                    for (const auto& [other, aff] : wandRel->affinity) {
                        if (aff < 0.3f) continue;
                        auto sit = s_entitySettlement.find(other);
                        if (sit != s_entitySettlement.end())
                            friendSettlements.insert(sit->second);
                    }
                }
                registry.view<Position, Settlement>().each(
                    [&](auto se, const Position& sp, const Settlement& ss) {
                    if (ss.ruinTimer > 0.f) return;
                    // Count pop at this settlement
                    int sPop = 0;
                    registry.view<HomeSettlement>(entt::exclude<Hauler>).each(
                        [&](const HomeSettlement& hs) { if (hs.settlement == se) ++sPop; });
                    if (sPop >= ss.popCap - 2) return;
                    float dx = sp.x - pos.x, dy = sp.y - pos.y;
                    float d2 = dx*dx + dy*dy;
                    // Score: closer = better (invert distance), friend bonus +20%
                    float score = -d2;
                    if (friendSettlements.count(se))
                        score *= 0.8f;  // reduce penalty (= +20% effective preference)
                    if (score > bestScore) { bestScore = score; bestSettl = se; }
                });
                if (bestSettl != entt::null) {
                    home.settlement = bestSettl;
                    money.balance -= RESETTLE_COST;
                    if (auto* ts = registry.try_get<Settlement>(bestSettl))
                        ts->treasury += RESETTLE_COST;
                    timer.theftCount = 0;
                    timer.banditPovertyTimer = 0.f;
                    if (banditLog) {
                        std::string who = "An exile";
                        if (const auto* n = registry.try_get<Name>(e)) who = n->value;
                        std::string where = "?";
                        if (const auto* ts = registry.try_get<Settlement>(bestSettl))
                            where = ts->name;
                        banditLog->Push(charityDay, charityHour,
                            who + " settled at " + where + " (fresh start).");
                    }
                    return;
                }
            }

            // Poverty accumulation → promotion
            if (!isBanditNow) {
                if (money.balance < BANDIT_POVERTY_THRESH) {
                    timer.banditPovertyTimer += gameHoursDt;
                    if (timer.banditPovertyTimer >= BANDIT_PROMOTE_HOURS) {
                        registry.emplace_or_replace<BanditTag>(e);
                        isBanditNow = true;
                        if (banditLog) {
                            std::string name = "An exile";
                            if (const auto* n = registry.try_get<Name>(e)) name = n->value;
                            banditLog->Push(charityDay, charityHour,
                                name + " has turned bandit.");
                        }
                    }
                } else {
                    timer.banditPovertyTimer = 0.f;
                }
            }

            if (!isBanditNow) return;

            // ---- Bandit flee: skip normal behavior while fleeing ----
            if (timer.fleeTimer > 0.f) {
                timer.fleeTimer -= realDt;
                return;   // velocity was set on confrontation; just let it play out
            }

            // ---- Bandit movement: lurk near nearest road midpoint (max 3 per road) ----
            static constexpr int BANDIT_CAP_PER_ROAD = 3;
            float bestRoadD2 = std::numeric_limits<float>::max();
            float lurk_x = pos.x, lurk_y = pos.y;
            entt::entity lurkRoad = entt::null;
            for (const auto& rm : s_roadMids) {
                if (rm.blocked) continue;
                if (banditsPerRoad[rm.entity] >= BANDIT_CAP_PER_ROAD) continue;
                float dx2 = rm.mx - pos.x, dy2 = rm.my - pos.y;
                float d2  = dx2*dx2 + dy2*dy2;
                if (d2 < bestRoadD2) { bestRoadD2 = d2; lurk_x = rm.mx; lurk_y = rm.my; lurkRoad = rm.entity; }
            }
            if (lurkRoad != entt::null) {
                banditsPerRoad[lurkRoad]++;
                // Assign gang name when 2+ bandits share a road
                std::string oldGangName = timer.gangName;
                if (banditsPerRoad[lurkRoad] >= 2) {
                    // Try to copy an existing gang name from another bandit at this road
                    std::string existingGang;
                    registry.view<Position, BanditTag, DeprivationTimer>(
                        entt::exclude<Hauler, PlayerTag>).each(
                        [&](auto other, const Position& op, const DeprivationTimer& odt) {
                            if (other == e || !existingGang.empty()) return;
                            if (odt.gangName.empty()) return;
                            // Check if this bandit is also targeting the same road
                            auto [otherRoad, otherMid] = findNearestRoad(op.x, op.y);
                            if (otherRoad == lurkRoad) existingGang = odt.gangName;
                        });
                    if (!existingGang.empty()) {
                        timer.gangName = existingGang;
                    } else {
                        // Generate gang name from road endpoint settlements
                        std::string nA, nB;
                        if (const auto* rd = registry.try_get<Road>(lurkRoad)) {
                            if (const auto* sa = registry.try_get<Settlement>(rd->from)) nA = sa->name;
                            if (const auto* sb = registry.try_get<Settlement>(rd->to))   nB = sb->name;
                        }
                        if (!nA.empty() && !nB.empty())
                            timer.gangName = "The " + nA + "-" + nB + " Wolves";
                        else
                            timer.gangName = "Road Wolves";
                    }
                    // Log when a bandit first joins a gang
                    if (oldGangName.empty() && !timer.gangName.empty() && banditLog) {
                        std::string who = "A bandit";
                        if (const auto* n = registry.try_get<Name>(e)) who = n->value;
                        std::string roadNames;
                        if (const auto* rd = registry.try_get<Road>(lurkRoad)) {
                            std::string nA2, nB2;
                            if (const auto* sa = registry.try_get<Settlement>(rd->from)) nA2 = sa->name;
                            if (const auto* sb = registry.try_get<Settlement>(rd->to))   nB2 = sb->name;
                            if (!nA2.empty() && !nB2.empty())
                                roadNames = " on the " + nA2 + "-" + nB2 + " road";
                        }
                        banditLog->Push(charityDay, charityHour,
                            who + " joined " + timer.gangName + roadNames + ".");
                    }
                } else {
                    timer.gangName.clear();
                }
            } else {
                timer.gangName.clear();
            }

            // ---- Try to intercept a nearby hauler ----
            bool intercepted = false;
            registry.view<Position, Hauler, Inventory, Money>(
                entt::exclude<PlayerTag>).each(
                [&](auto haulerE, const Position& hpos, const Hauler& h,
                    Inventory& haulerInv, Money& /*haulerMoney*/)
                {
                    if (intercepted) return;
                    if (h.inConvoy) return;  // bandits won't attack a convoy
                    float dx3 = hpos.x - pos.x, dy3 = hpos.y - pos.y;
                    if (dx3*dx3 + dy3*dy3 > BANDIT_INTERCEPT_RANGE * BANDIT_INTERCEPT_RANGE)
                        return;

                    // Steal a fraction of each cargo type
                    float stolenGold = 0.f;
                    for (auto& [res, qty] : haulerInv.contents) {
                        if (qty <= 0) continue;
                        int stealQty = std::max(1, (int)(qty * BANDIT_STEAL_FRACTION));
                        stealQty = std::min(stealQty, qty);
                        haulerInv.contents[res] -= stealQty;
                        stolenGold += stealQty * BANDIT_CARGO_GOLD_RATE;
                    }
                    if (stolenGold > 0.f) {
                        money.balance += stolenGold;
                        intercepted    = true;
                        if (banditLog) {
                            std::string bandName = "A bandit";
                            if (const auto* n = registry.try_get<Name>(e)) bandName = n->value;
                            banditLog->Push(charityDay, charityHour,
                                bandName + " ambushed a hauler on the road.");
                        }
                        // Flee away from the hauler
                        float dist = std::sqrt(dx3*dx3 + dy3*dy3);
                        if (dist > 0.1f) {
                            vel.vx = -(dx3 / dist) * spd.value;
                            vel.vy = -(dy3 / dist) * spd.value;
                        }
                    }
                });

            // Nearby non-bandit NPCs panic-flee from the bandit
            if (intercepted) {
                static constexpr float PANIC_RANGE = 60.f;
                registry.view<Position, Velocity, MoveSpeed, DeprivationTimer>(
                    entt::exclude<BanditTag, PlayerTag, Hauler>).each(
                    [&](auto npcE, const Position& npos, Velocity& nvel,
                        const MoveSpeed& nspd, DeprivationTimer& ntmr) {
                        float pdx = npos.x - pos.x, pdy = npos.y - pos.y;
                        float d2 = pdx * pdx + pdy * pdy;
                        if (d2 > PANIC_RANGE * PANIC_RANGE || d2 < 0.01f) return;
                        float dist = std::sqrt(d2);
                        nvel.vx = (pdx / dist) * nspd.value * 1.5f;
                        nvel.vy = (pdy / dist) * nspd.value * 1.5f;
                        ntmr.panicTimer = 2.f;
                    });
            }

            if (!intercepted) {
                // Drift toward road lurk point
                static constexpr float LURK_ARRIVE = 20.f;
                float dx4 = lurk_x - pos.x, dy4 = lurk_y - pos.y;
                if (dx4*dx4 + dy4*dy4 > LURK_ARRIVE * LURK_ARRIVE)
                    MoveToward(vel, pos, lurk_x, lurk_y, spd.value * 0.8f);
                else
                    vel.vx = vel.vy = 0.f;
            }

            state.behavior = AgentBehavior::Idle;
        });
    spFlush(5); // AD:Bandits

    // ============================================================
    // PERSONAL GOAL SYSTEM
    // Every frame: update progress for all NPCs with a Goal component.
    // When progress >= target: log a celebration, set Celebrating state
    // for 2 game-hours, then assign a fresh random goal.
    // ============================================================
    static std::mt19937       s_goalRng{ std::random_device{}() };
    static std::uniform_int_distribution<int> s_goalTypeDist(0, 3);

    registry.view<Goal>(entt::exclude<PlayerTag>).each(
        [&](auto e, Goal& goal)
        {
            // Drain personal celebration timer
            if (goal.celebrateTimer > 0.f) {
                goal.celebrateTimer = std::max(0.f, goal.celebrateTimer - gameHoursDt);
                return;  // still celebrating — skip progress check this tick
            }

            // Update progress for the active goal type
            switch (goal.type) {
                case GoalType::SaveGold:
                    if (const auto* m = registry.try_get<Money>(e))
                        goal.progress = m->balance;
                    break;
                case GoalType::ReachAge:
                    if (const auto* a = registry.try_get<Age>(e))
                        goal.progress = a->days;
                    break;
                case GoalType::FindFamily:
                    goal.progress = registry.all_of<FamilyTag>(e) ? 1.f : 0.f;
                    break;
                case GoalType::BecomeHauler:
                    goal.progress = registry.all_of<Hauler>(e) ? 1.f : 0.f;
                    break;
            }

            // ---- Halfway milestone log ----
            if (!goal.halfwayLogged && goal.target > 0.f &&
                goal.progress >= goal.target * 0.5f) {
                goal.halfwayLogged = true;
                auto hmlv = registry.view<EventLog>();
                if (hmlv.begin() != hmlv.end()) {
                    std::string who = "An NPC";
                    if (const auto* n = registry.try_get<Name>(e)) who = n->value;
                    const char* unit = (goal.type == GoalType::SaveGold) ? "g" :
                                       (goal.type == GoalType::ReachAge) ? "d" : "";
                    char buf[160];
                    std::snprintf(buf, sizeof(buf),
                        "%s is halfway to their %s goal (%.0f/%.0f%s).",
                        who.c_str(), GoalLabel(goal.type),
                        goal.progress, goal.target, unit);
                    hmlv.get<EventLog>(*hmlv.begin()).Push(charityDay, charityHour, buf);
                }
            }

            if (goal.progress < goal.target) return;  // not yet met

            // ---- Goal completed! ----
            // Log the event
            auto gelv = registry.view<EventLog>();
            if (gelv.begin() != gelv.end()) {
                std::string who = "An NPC";
                if (const auto* n = registry.try_get<Name>(e)) who = n->value;
                std::string msg;
                switch (goal.type) {
                    case GoalType::SaveGold:
                        msg = who + " reached their savings goal!";    break;
                    case GoalType::ReachAge:
                        msg = who + " celebrated a life milestone!";   break;
                    case GoalType::FindFamily:
                        msg = who + " found a family!";                break;
                    case GoalType::BecomeHauler:
                        msg = who + " achieved their dream of becoming a merchant!"; break;
                }
                gelv.get<EventLog>(*gelv.begin()).Push(charityDay, charityHour, msg);
            }

            // Start personal celebration: 2 game-hours for regular NPCs
            goal.celebrateTimer = 2.f;
            if (!registry.all_of<Hauler>(e)) {
                if (auto* st = registry.try_get<AgentState>(e))
                    st->behavior = AgentBehavior::Celebrating;
            }

            // Assign a new goal (avoid immediately re-assigning the same type)
            GoalType newType = static_cast<GoalType>(s_goalTypeDist(s_goalRng));
            if (newType == goal.type)
                newType = static_cast<GoalType>((static_cast<int>(newType) + 1) % 4);

            goal.type     = newType;
            goal.progress = 0.f;
            goal.halfwayLogged = false;
            switch (newType) {
                case GoalType::SaveGold: {
                    float bal = registry.try_get<Money>(e)
                                ? registry.get<Money>(e).balance : 0.f;
                    goal.target = std::max(50.f, bal + 75.f);   // save 75g more
                    break;
                }
                case GoalType::ReachAge: {
                    float days = registry.try_get<Age>(e)
                                 ? registry.get<Age>(e).days : 0.f;
                    goal.target = days + 20.f;
                    break;
                }
                case GoalType::FindFamily:
                case GoalType::BecomeHauler:
                    goal.target = 1.f;
                    break;
            }
        });

    spFlush(6); // AD:Goals

    // Sub-profile window management: flush averages every ~1 second
    ++m_subProfileSteps;
    m_subProfileAccum += realDt;
    if (m_subProfileAccum >= 1.f) {
        SubProfileFlush();
        m_subProfileAccum = 0.f;
    }
}

void AgentDecisionSystem::SubProfileFlush() {
    if (m_subProfileSteps > 0) {
        float inv = 1.f / m_subProfileSteps;
        for (int i = 0; i < SUB_PROFILE_COUNT; ++i) {
            m_subProfile[i].avgUs = m_subProfile[i].accumUs * inv;
            m_subProfile[i].accumUs = 0.f;
        }
    }
    m_subProfileSteps = 0;
}
