#include "TransportSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <limits>

static constexpr float ARRIVE_RADIUS         = 130.f;
static constexpr float MIN_TRIP_PROFIT       = 5.f;   // gold; ideal minimum to haul for
static constexpr float MIN_TRIP_PROFIT_FLOOR = 0.5f;  // hauler will accept this after patience runs out
static constexpr float WAIT_INTERVAL         = 1.f;   // game-hours to wait if no profitable route
static constexpr int   MAX_WAIT_CYCLES       = 5;     // after this many waits, accept any positive margin

static void MoveToward(Velocity& vel, const Position& from,
                        float tx, float ty, float speed) {
    float dx = tx - from.x, dy = ty - from.y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 1.f) { vel.vx = vel.vy = 0.f; return; }
    vel.vx = (dx / dist) * speed;
    vel.vy = (dy / dist) * speed;
}

static bool InRange(const Position& a, const Position& b, float r) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return (dx * dx + dy * dy) <= r * r;
}

// Describes the best available trade opportunity for a hauler.
struct TradeRoute {
    entt::entity dest     = entt::null;
    ResourceType resType  = ResourceType::Food;
    int          qty      = 0;
    float        profit   = 0.f;
    float        buyPrice = 0.f;
};

// Scan all reachable settlements for the most profitable trade, weighted by
// proximity: profit-per-distance so close routes beat distant ones.
static TradeRoute FindBestRoute(entt::registry& registry,
                                 entt::entity homeSettlement,
                                 int maxCapacity) {
    TradeRoute best;
    float bestScore = 0.f;  // profit / max(100, distance) — avoids zero division

    auto* homeSp  = registry.try_get<Stockpile>(homeSettlement);
    auto* homeMkt = registry.try_get<Market>(homeSettlement);
    const auto* homePos = registry.try_get<Position>(homeSettlement);
    if (!homeSp || !homeMkt || !homePos) return best;

    // Build reachable-destination list from open roads, carrying road condition.
    // Poor-condition roads get a penalty applied to the route score so haulers
    // prefer better-maintained routes when multiple options are available.
    struct DestInfo { entt::entity e; float conditionPenalty; };
    std::vector<DestInfo> dests;
    registry.view<Road>().each([&](const Road& road) {
        if (road.blocked) return;
        // Condition penalty: 0 = perfect road (no penalty), 1 = near-collapse (50% penalty)
        float penalty = 1.f - std::max(0.15f, road.condition);  // range [0, 0.85]
        float condPenalty = penalty * 0.6f;  // scale to [0, 0.51] — max ~50% score reduction
        if (road.from == homeSettlement) dests.push_back({ road.to,   condPenalty });
        else if (road.to == homeSettlement) dests.push_back({ road.from, condPenalty });
    });

    for (const auto& di : dests) {
        entt::entity destEnt = di.e;
        if (!registry.valid(destEnt)) continue;
        auto* destMkt = registry.try_get<Market>(destEnt);
        auto* destPos = registry.try_get<Position>(destEnt);
        if (!destMkt || !destPos) continue;

        float dx = destPos->x - homePos->x, dy = destPos->y - homePos->y;
        float dist = std::sqrt(dx*dx + dy*dy);

        for (const auto& [res, homePrice] : homeMkt->price) {
            float destPrice = destMkt->GetPrice(res);
            if (destPrice <= homePrice) continue;

            auto stockIt = homeSp->quantities.find(res);
            float stock  = (stockIt != homeSp->quantities.end()) ? stockIt->second : 0.f;
            int qty = std::min(maxCapacity, (int)(stock * 0.5f));
            if (qty <= 0) continue;

            // Net profit = sell revenue - buy cost - tax (20% of sell)
            static constexpr float TAX_RATE = 0.20f;
            float gross  = destPrice * qty;
            float profit = gross - homePrice * qty - gross * TAX_RATE;
            float score  = profit / std::max(100.f, dist);  // profit-per-distance
            score *= (1.f - di.conditionPenalty);            // road condition discount
            if (score > bestScore) {
                bestScore     = score;
                best.dest     = destEnt;
                best.resType  = res;
                best.qty      = qty;
                best.profit   = profit;
                best.buyPrice = homePrice;
            }
        }
    }
    return best;
}

// When a hauler arrives at destination, check if there's a profitable return
// trip (destination → home) — load cargo and carry it back, saving an empty trip.
static TradeRoute FindReturnTrip(entt::registry& registry,
                                  entt::entity currentSettl,
                                  entt::entity homeSettl,
                                  int maxCapacity) {
    TradeRoute best;
    auto* curSp  = registry.try_get<Stockpile>(currentSettl);
    auto* curMkt = registry.try_get<Market>(currentSettl);
    auto* homeMkt = registry.try_get<Market>(homeSettl);
    if (!curSp || !curMkt || !homeMkt) return best;

    // Check that road is still open in the return direction
    bool open = false;
    registry.view<Road>().each([&](const Road& r) {
        if (!r.blocked &&
            ((r.from == currentSettl && r.to == homeSettl) ||
             (r.to == currentSettl && r.from == homeSettl)))
            open = true;
    });
    if (!open) return best;

    for (const auto& [res, curPrice] : curMkt->price) {
        float homePrice = homeMkt->GetPrice(res);
        if (homePrice <= curPrice) continue;

        auto stockIt = curSp->quantities.find(res);
        float stock  = (stockIt != curSp->quantities.end()) ? stockIt->second : 0.f;
        int qty = std::min(maxCapacity, (int)(stock * 0.5f));
        if (qty <= 0) continue;

        // Net profit after tax (20% of sell at home)
        static constexpr float TAX_RATE = 0.20f;
        float gross  = homePrice * qty;
        float profit = gross - curPrice * qty - gross * TAX_RATE;
        if (profit > best.profit) {
            best.dest     = homeSettl;
            best.resType  = res;
            best.qty      = qty;
            best.profit   = profit;
            best.buyPrice = curPrice;
        }
    }
    return best;
}

// Check whether the road from home to dest is still open.
static bool RouteOpen(entt::registry& registry,
                       entt::entity home, entt::entity dest) {
    bool open = false;
    registry.view<Road>().each([&](const Road& road) {
        if (!road.blocked &&
            ((road.from == home && road.to == dest) ||
             (road.to == home && road.from == dest)))
            open = true;
    });
    return open;
}

entt::entity TransportSystem::FindRoadPartner(entt::registry& registry,
                                               entt::entity settlement) {
    auto view = registry.view<Road>();
    for (auto e : view) {
        const auto& road = view.get<Road>(e);
        if (road.blocked) continue;
        if (road.from == settlement) return road.to;
        if (road.to   == settlement) return road.from;
    }
    return entt::null;
}

void TransportSystem::Update(entt::registry& registry, float realDt) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    float gameDt = timeView.get<TimeManager>(*timeView.begin()).GameDt(realDt);
    if (gameDt <= 0.f) return;
    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;

    auto view = registry.view<Hauler, Inventory, Position, Velocity,
                               MoveSpeed, HomeSettlement>();

    for (auto entity : view) {
        auto& hauler = view.get<Hauler>(entity);
        auto& inv    = view.get<Inventory>(entity);
        auto& pos    = view.get<Position>(entity);
        auto& vel    = view.get<Velocity>(entity);
        float speed  = view.get<MoveSpeed>(entity).value;
        auto& home   = view.get<HomeSettlement>(entity);

        if (home.settlement == entt::null || !registry.valid(home.settlement)) continue;
        const auto& homePos = registry.get<Position>(home.settlement);

        switch (hauler.state) {

        // ---- Idle: return home, then evaluate trades ----
        case HaulerState::Idle: {
            if (!InRange(pos, homePos, ARRIVE_RADIUS)) {
                MoveToward(vel, pos, homePos.x, homePos.y, speed);
                break;
            }
            vel.vx = vel.vy = 0.f;

            // Waiting for better margins — tick the timer
            if (hauler.waitTimer > 0.f) {
                hauler.waitTimer -= gameHoursDt;
                break;
            }

            // Evaluate all reachable trade routes by expected profit.
            // Patience: after MAX_WAIT_CYCLES idle evaluations, accept any
            // positive margin rather than insisting on MIN_TRIP_PROFIT.
            TradeRoute best = FindBestRoute(registry, home.settlement, inv.maxCapacity);
            float effectiveMin = (hauler.waitCycles >= MAX_WAIT_CYCLES)
                                 ? MIN_TRIP_PROFIT_FLOOR
                                 : MIN_TRIP_PROFIT;

            if (best.dest != entt::null && best.profit >= effectiveMin) {
                auto* sp    = registry.try_get<Stockpile>(home.settlement);
                auto* money = registry.try_get<Money>(entity);
                auto* settl = registry.try_get<Settlement>(home.settlement);
                if (!sp) break;

                // Hauler buys goods from home settlement at current market price.
                // If they can't afford the full lot, reduce quantity to their budget.
                float totalCost = best.buyPrice * best.qty;
                if (money && money->balance < totalCost && best.buyPrice > 0.f) {
                    best.qty  = (int)(money->balance / best.buyPrice);
                    totalCost = best.buyPrice * best.qty;
                }
                if (best.qty <= 0) {
                    ++hauler.waitCycles;
                    hauler.waitTimer = WAIT_INTERVAL;
                    break;
                }

                sp->quantities[best.resType] -= best.qty;
                inv.contents[best.resType]    = best.qty;
                hauler.buyPrice               = best.buyPrice;
                // Purchase: gold leaves hauler, enters home settlement treasury
                if (money) money->balance   -= totalCost;
                if (settl) settl->treasury  += totalCost;
                hauler.targetSettlement = best.dest;
                hauler.state            = HaulerState::GoingToDeposit;
                hauler.waitCycles       = 0;
            } else {
                // No good route yet — wait before re-evaluating (patience increases)
                ++hauler.waitCycles;
                hauler.waitTimer = WAIT_INTERVAL;
            }
            break;
        }

        // ---- GoingToDeposit: walk to target and sell ----
        case HaulerState::GoingToDeposit: {
            if (hauler.targetSettlement == entt::null ||
                !registry.valid(hauler.targetSettlement)) {
                hauler.state = HaulerState::GoingHome;
                inv.contents.clear();
                break;
            }

            // Abort if road blocked mid-trip — return goods to home stockpile
            if (!RouteOpen(registry, home.settlement, hauler.targetSettlement)) {
                auto* sp = registry.try_get<Stockpile>(home.settlement);
                if (sp) {
                    for (auto& [type, qty] : inv.contents)
                        sp->quantities[type] += qty;
                }
                inv.contents.clear();
                hauler.state = HaulerState::GoingHome;
                break;
            }

            const auto& destPos = registry.get<Position>(hauler.targetSettlement);
            if (!InRange(pos, destPos, ARRIVE_RADIUS)) {
                MoveToward(vel, pos, destPos.x, destPos.y, speed);
            } else {
                vel.vx = vel.vy = 0.f;
                auto* destSp  = registry.try_get<Stockpile>(hauler.targetSettlement);
                auto* destMkt = registry.try_get<Market>(hauler.targetSettlement);

                static constexpr float TRADE_TAX = 0.20f;  // 20% of gross sale to settlement

                float earned = 0.f;
                float taxCollected = 0.f;
                if (destSp) {
                    for (auto& [type, qty] : inv.contents) {
                        destSp->quantities[type] += qty;
                        float sellPrice = destMkt ? destMkt->GetPrice(type) : hauler.buyPrice;
                        float gross = sellPrice * qty;
                        float tax   = gross * TRADE_TAX;
                        // Buying cost was paid at source — earn full revenue minus tax
                        earned       += gross - tax;
                        taxCollected += tax;
                    }
                }
                // Credit hauler's wallet with net profit
                if (auto* money = registry.try_get<Money>(entity))
                    if (earned > 0.f) money->balance += earned;
                // Tax goes to destination settlement treasury
                if (taxCollected > 0.f) {
                    if (auto* destSettl = registry.try_get<Settlement>(hauler.targetSettlement))
                        destSettl->treasury += taxCollected;
                }

                inv.contents.clear();

                // Return-trip opportunism: check if destination has profitable goods to
                // carry home rather than returning empty-handed.
                static constexpr float RETURN_MIN_PROFIT = 2.f;
                TradeRoute returnTrip = FindReturnTrip(registry,
                    hauler.targetSettlement, home.settlement, inv.maxCapacity);
                if (returnTrip.dest != entt::null && returnTrip.profit >= RETURN_MIN_PROFIT) {
                    entt::entity curSettlEnt = hauler.targetSettlement;  // save before overwrite
                    auto* srcSp    = registry.try_get<Stockpile>(curSettlEnt);
                    auto* retMoney = registry.try_get<Money>(entity);
                    auto* curSettl = registry.try_get<Settlement>(curSettlEnt);
                    if (srcSp) {
                        // Cap qty by affordability
                        float retCost = returnTrip.buyPrice * returnTrip.qty;
                        if (retMoney && retMoney->balance < retCost && returnTrip.buyPrice > 0.f) {
                            returnTrip.qty  = (int)(retMoney->balance / returnTrip.buyPrice);
                            retCost         = returnTrip.buyPrice * returnTrip.qty;
                        }
                        if (returnTrip.qty > 0) {
                            srcSp->quantities[returnTrip.resType] -= returnTrip.qty;
                            inv.contents[returnTrip.resType] = returnTrip.qty;
                            hauler.buyPrice         = returnTrip.buyPrice;
                            // Purchase return goods from current settlement
                            if (retMoney) retMoney->balance    -= retCost;
                            if (curSettl) curSettl->treasury   += retCost;
                            hauler.targetSettlement = returnTrip.dest;  // = home
                            // Stay in GoingToDeposit state — home is the new destination
                            break;
                        }
                    }
                }

                hauler.state = HaulerState::GoingHome;
            }
            break;
        }

        // ---- GoingHome: walk back to home settlement ----
        case HaulerState::GoingHome: {
            if (!InRange(pos, homePos, ARRIVE_RADIUS)) {
                MoveToward(vel, pos, homePos.x, homePos.y, speed);
            } else {
                vel.vx = vel.vy = 0.f;
                hauler.state            = HaulerState::Idle;
                hauler.targetSettlement = entt::null;
                hauler.waitCycles       = 0;   // arrived home — fresh evaluation next cycle
            }
            break;
        }
        }
    }
}
