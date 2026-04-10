#include "TransportSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <limits>

static constexpr float ARRIVE_RADIUS   = 130.f;
static constexpr float MIN_TRIP_PROFIT = 5.f;    // gold; minimum worth hauling for
static constexpr float WAIT_INTERVAL   = 1.f;    // game-hours to wait if no profitable route

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

// Scan all reachable settlements for the most profitable trade.
static TradeRoute FindBestRoute(entt::registry& registry,
                                 entt::entity homeSettlement,
                                 int maxCapacity) {
    TradeRoute best;

    auto* homeSp  = registry.try_get<Stockpile>(homeSettlement);
    auto* homeMkt = registry.try_get<Market>(homeSettlement);
    if (!homeSp || !homeMkt) return best;

    // Build reachable-destination list from open roads
    std::vector<entt::entity> dests;
    registry.view<Road>().each([&](const Road& road) {
        if (road.blocked) return;
        if (road.from == homeSettlement) dests.push_back(road.to);
        else if (road.to == homeSettlement) dests.push_back(road.from);
    });

    for (entt::entity destEnt : dests) {
        if (!registry.valid(destEnt)) continue;
        auto* destMkt = registry.try_get<Market>(destEnt);
        if (!destMkt) continue;

        for (const auto& [res, homePrice] : homeMkt->price) {
            float destPrice = destMkt->GetPrice(res);
            if (destPrice <= homePrice) continue;   // no margin this direction

            auto stockIt = homeSp->quantities.find(res);
            float stock  = (stockIt != homeSp->quantities.end()) ? stockIt->second : 0.f;
            // Haul at most half the stock, capped to carry capacity
            int qty = std::min(maxCapacity, (int)(stock * 0.5f));
            if (qty <= 0) continue;

            float profit = (destPrice - homePrice) * qty;
            if (profit > best.profit) {
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

            // Evaluate all reachable trade routes by expected profit
            TradeRoute best = FindBestRoute(registry, home.settlement, inv.maxCapacity);

            if (best.dest != entt::null && best.profit >= MIN_TRIP_PROFIT) {
                // Load goods from home stockpile
                auto* sp = registry.try_get<Stockpile>(home.settlement);
                if (!sp) break;
                sp->quantities[best.resType] -= best.qty;
                inv.contents[best.resType]    = best.qty;
                hauler.buyPrice         = best.buyPrice;
                hauler.targetSettlement = best.dest;
                hauler.state            = HaulerState::GoingToDeposit;
            } else {
                // No good route today — wait before re-evaluating
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

                float earned = 0.f;
                if (destSp) {
                    for (auto& [type, qty] : inv.contents) {
                        destSp->quantities[type] += qty;
                        // Earn (sell price - buy price) * quantity
                        float sellPrice = destMkt ? destMkt->GetPrice(type) : hauler.buyPrice;
                        earned += (sellPrice - hauler.buyPrice) * qty;
                    }
                }
                // Credit hauler's wallet with net profit
                if (auto* money = registry.try_get<Money>(entity))
                    if (earned > 0.f) money->balance += earned;

                inv.contents.clear();
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
            }
            break;
        }
        }
    }
}
