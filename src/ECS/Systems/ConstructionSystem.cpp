#include "ConstructionSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <map>
#include <set>
#include <vector>
#include <limits>
#include <random>

// How often to evaluate construction opportunities (game-hours).
static constexpr float CHECK_INTERVAL    = 24.f;

// Treasury must exceed this before the settlement will invest in construction.
static constexpr float CONSTRUCTION_COST = 200.f;

// A resource is considered "critically scarce" if its market price exceeds this.
// At baseline prices of 1-2g, 8g indicates severe shortage.
static constexpr float PRICE_THRESHOLD   = 7.f;

// Stock must also be below this threshold (units) to trigger construction.
static constexpr float STOCK_THRESHOLD   = 20.f;

// Max productive facilities per resource type per settlement (prevents unbounded growth).
static constexpr int   MAX_FACILITIES_PER_TYPE = 4;

// New facility production rate (units/game-hour at 1 worker).
// Slightly less than the starting 4.0 — expansions are smaller than the founding facility.
static constexpr float NEW_FACILITY_RATE = 3.f;

// Offset from the settlement center to place the new facility.
static constexpr float PLACEMENT_RADIUS  = 60.f;

// Housing expansion — builds when settlement population is near cap.
static constexpr float HOUSING_COST         = 300.f;  // more expensive than resource facilities
static constexpr float HOUSING_POP_FRACTION = 0.80f;  // expand when pop >= 80% of cap
static constexpr int   HOUSING_CAP_GAIN     = 5;      // pop cap increase per housing built
static constexpr int   HOUSING_MAX_CAP      = 70;     // absolute maximum population cap

// Facility degradation — facilities decay unless the settlement pays maintenance.
// Without maintenance (treasury empty): DEGRADE_RATE_UNMAINTAINED per day.
// With maintenance: DEGRADE_RATE_MAINTAINED per day (normal upkeep slows decay).
// Facilities below FACILITY_MIN_RATE collapse and are removed.
static constexpr float DEGRADE_RATE_UNMAINTAINED = 0.05f;   // 5%/day without upkeep
static constexpr float DEGRADE_RATE_MAINTAINED   = 0.008f;  // 0.8%/day with upkeep
static constexpr float MAINTENANCE_COST_PER_FAC  = 3.f;     // gold per facility per game-day
static constexpr float FACILITY_MIN_RATE         = 0.4f;    // below this → collapse

// Autonomous road building — isolated wealthy settlements fund new connections.
// Checks every ROAD_BUILD_INTERVAL game-hours. Costs ROAD_AUTO_COST from the
// building settlement's treasury. Only builds if no existing direct road to target.
static constexpr float ROAD_BUILD_INTERVAL = 72.f;    // game-hours between road checks
static constexpr float ROAD_AUTO_COST      = 400.f;   // treasury cost for auto-built road
static constexpr float ROAD_BUILD_MIN_TRES = 800.f;   // min treasury to fund a new road

// Road degradation — roads lose condition each CHECK_INTERVAL unless maintained.
// Each settlement pays half the maintenance; if neither can pay, road decays faster.
// A road below ROAD_COLLAPSE_THRESHOLD auto-blocks until repaired (R key).
static constexpr float ROAD_MAINT_COST_EACH      = 5.f;     // gold per road per day, per endpoint
static constexpr float ROAD_DECAY_MAINTAINED     = 0.010f;  // 1%/day with full upkeep
static constexpr float ROAD_DECAY_PARTIAL        = 0.025f;  // 2.5%/day with one endpoint paying
static constexpr float ROAD_DECAY_UNMAINTAINED   = 0.045f;  // 4.5%/day with neither paying
static constexpr float ROAD_COLLAPSE_THRESHOLD   = 0.15f;   // below this → auto-block

void ConstructionSystem::Update(entt::registry& registry, float realDt, const WorldSchema& /*schema*/) {
    auto tv = registry.view<TimeManager>();
    if (tv.begin() == tv.end()) return;
    const auto& tm = tv.get<TimeManager>(*tv.begin());
    float gameDt = tm.GameDt(realDt);
    if (gameDt <= 0.f) return;

    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;
    m_checkAccum += gameHoursDt;
    if (m_checkAccum < CHECK_INTERVAL) return;
    m_checkAccum -= CHECK_INTERVAL;

    auto lv  = registry.view<EventLog>();
    EventLog* log = (lv.begin() == lv.end()) ? nullptr : &lv.get<EventLog>(*lv.begin());

    // Count existing productive facilities per settlement per resource type
    std::map<entt::entity, std::map<int, int>> facCount;
    // Also track average position of existing facilities by type for placement
    std::map<entt::entity, std::map<int, std::pair<float,float>>> facPos;
    std::map<entt::entity, std::map<int, int>> facPosCount;

    registry.view<Position, ProductionFacility>().each(
        [&](auto fe, const Position& pos, const ProductionFacility& fac) {
        if (fac.baseRate <= 0.f) return;   // skip shelter nodes
        ++facCount[fac.settlement][fac.output];
        facPos[fac.settlement][fac.output].first  += pos.x;
        facPos[fac.settlement][fac.output].second += pos.y;
        ++facPosCount[fac.settlement][fac.output];
    });

    // Count population per settlement (for housing decisions)
    std::map<entt::entity, int> popCount;
    registry.view<HomeSettlement>(entt::exclude<Hauler, PlayerTag>).each(
        [&](const HomeSettlement& hs) { ++popCount[hs.settlement]; });

    // Count skilled elders per settlement (for construction discount)
    std::map<entt::entity, int> skilledElderCount;
    registry.view<Age, Skills, HomeSettlement>(entt::exclude<PlayerTag>).each(
        [&](const Age& age, const Skills& sk, const HomeSettlement& hs) {
        if (age.days > 60.f && sk.AnyAbove(0.7f))
            ++skilledElderCount[hs.settlement];
    });

    // Evaluate each settlement for construction opportunity
    registry.view<Position, Settlement, Stockpile, Market>().each(
        [&](auto e, const Position& sPos, Settlement& s, const Stockpile& sp, const Market& mkt) {
        // Must have sufficient treasury
        if (s.treasury < CONSTRUCTION_COST) return;

        // Find the most critically scarce resource (highest price, below stock threshold)
        int buildType  = RES_FOOD;
        float        bestPrice  = 0.f;
        bool         shouldBuild = false;

        for (auto resType : { RES_FOOD, RES_WATER, RES_WOOD }) {
            float price = mkt.GetPrice(resType);
            if (price <= PRICE_THRESHOLD) continue;

            auto stockIt = sp.quantities.find(resType);
            float stock = (stockIt != sp.quantities.end()) ? stockIt->second : 0.f;
            if (stock >= STOCK_THRESHOLD) continue;

            // Check facility cap
            int existing = 0;
            auto it = facCount.find(e);
            if (it != facCount.end()) {
                auto it2 = it->second.find(resType);
                if (it2 != it->second.end()) existing = it2->second;
            }
            if (existing >= MAX_FACILITIES_PER_TYPE) continue;

            if (price > bestPrice) {
                bestPrice  = price;
                buildType  = resType;
                shouldBuild = true;
            }
        }

        if (!shouldBuild) return;

        // Elder council discount: 2+ skilled elders reduce cost by 10%
        float actualCost = CONSTRUCTION_COST;
        bool elderDiscount = (skilledElderCount[e] >= 2);
        if (elderDiscount)
            actualCost = std::floor(CONSTRUCTION_COST * 0.9f);

        // Charge the treasury
        s.treasury -= actualCost;

        // Determine placement position:
        // If existing facilities of this type exist, place near their average center.
        // Otherwise, place at an angle from the settlement center based on resource type.
        // Determine placement position using a spiral outward from existing facilities.
        // For each existing facility of this type, the new one is placed 30 units
        // further out at a slight clockwise angle — avoids stacking.
        float px, py;
        auto& pfMap = facPos[e];
        auto& pfCnt = facPosCount[e];
        if (pfMap.count(buildType) && pfCnt[buildType] > 0) {
            int n = pfCnt[buildType];
            float avgX = pfMap[buildType].first  / n;
            float avgY = pfMap[buildType].second / n;
            // Each new facility spirals 30px further from settlement center at ~60° steps
            float baseAngle = std::atan2(avgY - sPos.y, avgX - sPos.x);
            float angle = baseAngle + (float)n * 1.05f;  // ~60° per additional facility
            float radius = std::sqrt((avgX-sPos.x)*(avgX-sPos.x) + (avgY-sPos.y)*(avgY-sPos.y));
            radius = std::max(PLACEMENT_RADIUS * 0.6f, radius) + 30.f;
            px = sPos.x + std::cos(angle) * radius;
            py = sPos.y + std::sin(angle) * radius;
        } else {
            // No existing facility of this type — place at a type-specific direction offset
            float angle = (buildType == RES_FOOD)  ? 4.71f :   // south (~270°)
                          (buildType == RES_WATER) ? 1.57f :   // north (~90°)
                                                               0.f;      // east (0°) for wood
            px = sPos.x + std::cos(angle) * PLACEMENT_RADIUS;
            py = sPos.y + std::sin(angle) * PLACEMENT_RADIUS;
        }

        // Create the new facility
        auto newFac = registry.create();
        registry.emplace<Position>(newFac, px, py);
        registry.emplace<ProductionFacility>(newFac,
            ProductionFacility{ buildType, NEW_FACILITY_RATE, e, {} });

        const char* resName = (buildType == RES_FOOD)  ? "farm"    :
                              (buildType == RES_WATER) ? "well"    : "lumber mill";

        if (log) {
            std::string where = "?";
            if (const auto* st = registry.try_get<Settlement>(e)) where = st->name;
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                "%s built a new %s (%.0fg) — %s price was %.1fg",
                where.c_str(), resName, actualCost,
                (buildType == RES_FOOD)  ? "food"  :
                (buildType == RES_WATER) ? "water" : "wood",
                bestPrice);
            log->Push(tm.day, (int)tm.hourOfDay, buf, "Build");

            // Elder council guidance log at 1-in-5 frequency
            if (elderDiscount) {
                static std::mt19937 s_elderCouncilRng{ std::random_device{}() };
                if (s_elderCouncilRng() % 5 == 0) {
                    char ebuf[120];
                    std::snprintf(ebuf, sizeof(ebuf),
                        "%s's elders guide the construction effort", where.c_str());
                    log->Push(tm.day, (int)tm.hourOfDay, ebuf, "Build");
                }
            }
        }
    });

    // ---- Housing expansion ----
    // Separate pass: when a settlement is crowded and flush with gold, expand its pop cap.
    // This lets thriving settlements grow beyond the default 35-NPC limit (up to HOUSING_MAX_CAP).
    registry.view<Position, Settlement>().each(
        [&](auto e, const Position&, Settlement& s) {
        if (s.treasury < HOUSING_COST) return;
        if (s.popCap >= HOUSING_MAX_CAP) return;

        int pop = popCount.count(e) ? popCount.at(e) : 0;
        if (pop < (int)(s.popCap * HOUSING_POP_FRACTION)) return;

        // Build housing: increase pop cap, charge treasury
        s.treasury -= HOUSING_COST;
        s.popCap   += HOUSING_CAP_GAIN;

        if (log) {
            char buf[120];
            std::snprintf(buf, sizeof(buf),
                "%s built housing (%.0fg) — pop cap now %d",
                s.name.c_str(), HOUSING_COST, s.popCap);
            log->Push(tm.day, (int)tm.hourOfDay, buf, "Build");
        }
    });

    // ---- Facility degradation ----
    // Each productive facility slowly loses efficiency (baseRate decay per game-day).
    // Settlements pay maintenance from treasury each check interval to slow the decay.
    // If treasury is too low to maintain all facilities, decay accelerates.
    // Facilities that fall below FACILITY_MIN_RATE collapse (entity destroyed + logged).
    {
        // Count facilities per settlement to compute maintenance cost
        std::map<entt::entity, int> facsBySettl;
        registry.view<ProductionFacility>().each([&](auto, const ProductionFacility& fac) {
            if (fac.baseRate > 0.f) ++facsBySettl[fac.settlement];
        });

        // Determine maintenance payment per settlement
        std::map<entt::entity, bool> maintained;
        registry.view<Settlement>().each([&](auto e, Settlement& s) {
            int facs = facsBySettl.count(e) ? facsBySettl.at(e) : 0;
            float mainCost = MAINTENANCE_COST_PER_FAC * facs;
            if (s.treasury >= mainCost) {
                s.treasury  -= mainCost;
                maintained[e] = true;
            } else {
                maintained[e] = false;
                // Partial deduction: pay what we can
                s.treasury = std::max(0.f, s.treasury - mainCost);
            }
        });

        // Degrade facility baseRates, collect collapses
        std::vector<entt::entity> toCollapse;
        registry.view<ProductionFacility>().each(
            [&](auto fe, ProductionFacility& fac) {
            if (fac.baseRate <= 0.f) return;  // shelter nodes exempt
            bool isMaintained = maintained.count(fac.settlement)
                                ? maintained.at(fac.settlement) : false;
            float decayFrac = isMaintained ? DEGRADE_RATE_MAINTAINED
                                           : DEGRADE_RATE_UNMAINTAINED;
            // Per CHECK_INTERVAL hours: decay fraction is decayFrac * (CHECK_INTERVAL/24)
            fac.baseRate -= fac.baseRate * decayFrac * (CHECK_INTERVAL / 24.f);
            if (fac.baseRate < FACILITY_MIN_RATE)
                toCollapse.push_back(fe);
        });

        for (auto fe : toCollapse) {
            if (!registry.valid(fe)) continue;
            const auto& fac = registry.get<ProductionFacility>(fe);
            std::string where = "?";
            const char* facName = "facility";
            if (fac.settlement != entt::null && registry.valid(fac.settlement))
                if (const auto* s = registry.try_get<Settlement>(fac.settlement))
                    where = s->name;
            if (fac.output == RES_FOOD)  facName = "farm";
            else if (fac.output == RES_WATER) facName = "well";
            else if (fac.output == RES_WOOD)  facName = "lumber mill";

            if (log) {
                char buf[120];
                std::snprintf(buf, sizeof(buf),
                    "%s's %s COLLAPSED from disrepair (no maintenance gold)",
                    where.c_str(), facName);
                log->Push(tm.day, (int)tm.hourOfDay, buf, "Build");
            }
            registry.destroy(fe);
        }
    }

    // ---- Road degradation ----
    // Roads decay each CHECK_INTERVAL. Both endpoint settlements contribute to maintenance.
    // If both pay → slow decay; one pays → medium decay; neither → fast decay.
    // Roads below ROAD_COLLAPSE_THRESHOLD are auto-blocked until the player repairs them.
    {
        static std::mt19937 s_elderRoadRng{ std::random_device{}() };
        std::vector<entt::entity> toBlock;
        registry.view<Road>().each([&](auto re, Road& road) {
            if (!registry.valid(road.from) || !registry.valid(road.to)) return;

            // Elder council road maintenance discount: 20% off when both endpoints have 2+ skilled elders
            bool elderRoadDiscount = (skilledElderCount[road.from] >= 2 && skilledElderCount[road.to] >= 2);
            float maintCost = elderRoadDiscount ? std::floor(ROAD_MAINT_COST_EACH * 0.8f) : ROAD_MAINT_COST_EACH;

            // Collect maintenance contribution from each endpoint settlement
            int paidCount = 0;
            auto tryPay = [&](entt::entity se) {
                if (se == entt::null || !registry.valid(se)) return;
                auto* s = registry.try_get<Settlement>(se);
                if (!s) return;
                if (s->treasury >= maintCost) {
                    s->treasury -= maintCost;
                    ++paidCount;
                }
            };
            tryPay(road.from);
            tryPay(road.to);

            // Log elder road discount at 1-in-8 frequency
            if (elderRoadDiscount && paidCount == 2 && log && s_elderRoadRng() % 8 == 0) {
                std::string nameA = "?", nameB = "?";
                if (const auto* sa = registry.try_get<Settlement>(road.from)) nameA = sa->name;
                if (const auto* sb = registry.try_get<Settlement>(road.to))   nameB = sb->name;
                char buf[140];
                std::snprintf(buf, sizeof(buf), "%s–%s road's upkeep eased by elder oversight",
                              nameA.c_str(), nameB.c_str());
                log->Push(tm.day, (int)tm.hourOfDay, buf, "Build");
            }

            float decayFrac = (paidCount == 2) ? ROAD_DECAY_MAINTAINED  :
                              (paidCount == 1) ? ROAD_DECAY_PARTIAL      :
                                                 ROAD_DECAY_UNMAINTAINED;
            // Per CHECK_INTERVAL hours: scale by proportion of a day
            road.condition -= road.condition * decayFrac * (CHECK_INTERVAL / 24.f);
            road.condition  = std::max(0.f, road.condition);

            if (!road.blocked && road.condition < ROAD_COLLAPSE_THRESHOLD)
                toBlock.push_back(re);
        });

        for (auto re : toBlock) {
            if (!registry.valid(re)) continue;
            auto& road = registry.get<Road>(re);
            road.blocked = true;
            if (log) {
                std::string nameA = "?", nameB = "?";
                if (const auto* sa = registry.try_get<Settlement>(road.from)) nameA = sa->name;
                if (const auto* sb = registry.try_get<Settlement>(road.to))   nameB = sb->name;
                char buf[120];
                std::snprintf(buf, sizeof(buf),
                    "Road %s–%s COLLAPSED from disrepair (condition %.0f%%)",
                    nameA.c_str(), nameB.c_str(), road.condition * 100.f);
                log->Push(tm.day, (int)tm.hourOfDay, buf, "Build");
            }
        }
    }

    // ---- Autonomous road repair ----
    // When a road's condition drops below ROAD_REPAIR_THRESHOLD, wealthy endpoint
    // settlements invest extra gold to restore it. Simulates coordinated maintenance
    // spending when both parties have enough reserves to care about the route.
    static constexpr float ROAD_REPAIR_THRESHOLD = 0.70f;  // trigger repairs below 70%
    static constexpr float ROAD_REPAIR_COST       = 30.f;  // gold per endpoint per check
    static constexpr float ROAD_REPAIR_AMOUNT     = 0.20f; // condition restored per paying endpoint
    static constexpr float ROAD_REPAIR_MIN_TRES   = 200.f; // min treasury to afford repairs
    {
        registry.view<Road>().each([&](Road& road) {
            if (road.condition >= ROAD_REPAIR_THRESHOLD) return;
            if (!registry.valid(road.from) || !registry.valid(road.to)) return;

            float repairGain = 0.f;
            auto tryRepair = [&](entt::entity se) {
                if (se == entt::null || !registry.valid(se)) return;
                auto* s = registry.try_get<Settlement>(se);
                if (!s || s->treasury < ROAD_REPAIR_MIN_TRES) return;
                if (s->treasury >= ROAD_REPAIR_COST) {
                    s->treasury -= ROAD_REPAIR_COST;
                    repairGain  += ROAD_REPAIR_AMOUNT;
                }
            };
            tryRepair(road.from);
            tryRepair(road.to);

            if (repairGain > 0.f) {
                road.condition = std::min(1.f, road.condition + repairGain);
                // If repair brought a blocked road above collapse threshold, unblock it
                if (road.blocked && road.condition > ROAD_COLLAPSE_THRESHOLD * 1.5f) {
                    road.blocked = false;
                    if (log) {
                        std::string nameA = "?", nameB = "?";
                        if (const auto* sa = registry.try_get<Settlement>(road.from)) nameA = sa->name;
                        if (const auto* sb = registry.try_get<Settlement>(road.to))   nameB = sb->name;
                        char buf[120];
                        std::snprintf(buf, sizeof(buf),
                            "Road %s–%s repaired by settlements — traffic resumes",
                            nameA.c_str(), nameB.c_str());
                        log->Push(tm.day, (int)tm.hourOfDay, buf, "Build");
                    }
                }
            }
        });
    }

    // ---- Autonomous road building ----
    // Wealthy settlements that lack a direct road to a neighbour fund construction
    // every ROAD_BUILD_INTERVAL game-hours. Only one road per check per settlement.
    m_roadBuildAccum += gameHoursDt;
    if (m_roadBuildAccum >= ROAD_BUILD_INTERVAL) {
        m_roadBuildAccum -= ROAD_BUILD_INTERVAL;

        // Build set of existing direct connections
        std::set<std::pair<uint32_t,uint32_t>> existingRoads;
        registry.view<Road>().each([&](const Road& road) {
            uint32_t a = static_cast<uint32_t>(entt::to_integral(road.from));
            uint32_t b = static_cast<uint32_t>(entt::to_integral(road.to));
            if (a > b) std::swap(a, b);
            existingRoads.insert({a, b});
        });

        // Collect all settlement positions
        struct SettlInfo { entt::entity e; float x, y; };
        std::vector<SettlInfo> allSettls;
        registry.view<Position, Settlement>().each(
            [&](auto e, const Position& pos, const Settlement&) {
            allSettls.push_back({ e, pos.x, pos.y });
        });

        registry.view<Position, Settlement>().each(
            [&](auto e, const Position& pos, Settlement& s) {
            if (s.treasury < ROAD_BUILD_MIN_TRES) return;

            // Find nearest settlement without an existing direct road
            entt::entity target = entt::null;
            float bestDist2     = std::numeric_limits<float>::max();

            for (const auto& si : allSettls) {
                if (si.e == e) continue;
                uint32_t a = static_cast<uint32_t>(entt::to_integral(e));
                uint32_t b = static_cast<uint32_t>(entt::to_integral(si.e));
                if (a > b) std::swap(a, b);
                if (existingRoads.count({a, b})) continue;   // already connected

                float dx = si.x - pos.x, dy = si.y - pos.y;
                float d2 = dx*dx + dy*dy;
                if (d2 < bestDist2) { bestDist2 = d2; target = si.e; }
            }

            if (target == entt::null) return;
            if (s.treasury < ROAD_AUTO_COST) return;

            s.treasury -= ROAD_AUTO_COST;
            auto newRoad = registry.create();
            registry.emplace<Road>(newRoad, Road{ e, target, false, 0.f });

            // Record so we don't double-build this tick
            uint32_t a = static_cast<uint32_t>(entt::to_integral(e));
            uint32_t b = static_cast<uint32_t>(entt::to_integral(target));
            if (a > b) std::swap(a, b);
            existingRoads.insert({a, b});

            std::string nameB = "?";
            if (const auto* ts = registry.try_get<Settlement>(target)) nameB = ts->name;
            if (log) {
                char buf[120];
                std::snprintf(buf, sizeof(buf),
                    "%s funded road to %s (%.0fg) — new trade route opened",
                    s.name.c_str(), nameB.c_str(), ROAD_AUTO_COST);
                log->Push(tm.day, (int)tm.hourOfDay, buf, "Build");
            }
        });
    }
}
