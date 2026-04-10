#include "ConstructionSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <map>
#include <vector>
#include <limits>

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

void ConstructionSystem::Update(entt::registry& registry, float realDt) {
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
    std::map<entt::entity, std::map<ResourceType, int>> facCount;
    // Also track average position of existing facilities by type for placement
    std::map<entt::entity, std::map<ResourceType, std::pair<float,float>>> facPos;
    std::map<entt::entity, std::map<ResourceType, int>> facPosCount;

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

    // Evaluate each settlement for construction opportunity
    registry.view<Position, Settlement, Stockpile, Market>().each(
        [&](auto e, const Position& sPos, Settlement& s, const Stockpile& sp, const Market& mkt) {
        // Must have sufficient treasury
        if (s.treasury < CONSTRUCTION_COST) return;

        // Find the most critically scarce resource (highest price, below stock threshold)
        ResourceType buildType  = ResourceType::Food;
        float        bestPrice  = 0.f;
        bool         shouldBuild = false;

        for (auto resType : { ResourceType::Food, ResourceType::Water, ResourceType::Wood }) {
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

        // Charge the treasury
        s.treasury -= CONSTRUCTION_COST;

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
            float angle = (buildType == ResourceType::Food)  ? 4.71f :   // south (~270°)
                          (buildType == ResourceType::Water) ? 1.57f :   // north (~90°)
                                                               0.f;      // east (0°) for wood
            px = sPos.x + std::cos(angle) * PLACEMENT_RADIUS;
            py = sPos.y + std::sin(angle) * PLACEMENT_RADIUS;
        }

        // Create the new facility
        auto newFac = registry.create();
        registry.emplace<Position>(newFac, px, py);
        registry.emplace<ProductionFacility>(newFac,
            ProductionFacility{ buildType, NEW_FACILITY_RATE, e, {} });

        const char* resName = (buildType == ResourceType::Food)  ? "farm"    :
                              (buildType == ResourceType::Water) ? "well"    : "lumber mill";

        if (log) {
            std::string where = "?";
            if (const auto* st = registry.try_get<Settlement>(e)) where = st->name;
            char buf[120];
            std::snprintf(buf, sizeof(buf),
                "%s built a new %s (%.0fg) — %s price was %.1fg",
                where.c_str(), resName, CONSTRUCTION_COST,
                (buildType == ResourceType::Food)  ? "food"  :
                (buildType == ResourceType::Water) ? "water" : "wood",
                bestPrice);
            log->Push(tm.day, (int)tm.hourOfDay, buf);
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
            log->Push(tm.day, (int)tm.hourOfDay, buf);
        }
    });
}
