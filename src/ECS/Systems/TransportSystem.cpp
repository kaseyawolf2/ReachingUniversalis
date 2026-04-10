#include "TransportSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <algorithm>

static constexpr float ARRIVE_RADIUS  = 130.f;  // same as settlement interaction radius
static constexpr float PICKUP_MIN     = 2.f;    // minimum surplus before hauler picks up
static constexpr float PICKUP_RATIO   = 0.5f;   // take at most half the surplus

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

        // ---- Idle: wait at home, then pick up surplus ----
        case HaulerState::Idle: {
            if (!InRange(pos, homePos, ARRIVE_RADIUS)) {
                MoveToward(vel, pos, homePos.x, homePos.y, speed);
                break;
            }
            vel.vx = vel.vy = 0.f;

            // Road must be open before committing to a trip
            entt::entity dest = FindRoadPartner(registry, home.settlement);
            if (dest == entt::null) break;

            auto* stockpile = registry.try_get<Stockpile>(home.settlement);
            if (!stockpile) break;

            // Find resource type with most surplus
            ResourceType bestType  = ResourceType::Food;
            float        bestQty   = 0.f;
            for (auto& [type, qty] : stockpile->quantities) {
                if (qty > bestQty) { bestQty = qty; bestType = type; }
            }

            if (bestQty < PICKUP_MIN) break;

            int pickup = std::max(1, std::min(inv.maxCapacity,
                                              (int)(bestQty * PICKUP_RATIO)));
            stockpile->quantities[bestType] -= pickup;
            inv.contents[bestType]           = pickup;

            hauler.targetSettlement = dest;
            hauler.state            = HaulerState::GoingToDeposit;
            break;
        }

        // ---- GoingToDeposit: walk to target, then deposit ----
        case HaulerState::GoingToDeposit: {
            if (hauler.targetSettlement == entt::null ||
                !registry.valid(hauler.targetSettlement)) {
                // Road blocked or target gone — return home empty
                hauler.state = HaulerState::GoingHome;
                inv.contents.clear();
                break;
            }

            // Check if road got blocked mid-trip
            bool roadOpen = (FindRoadPartner(registry, home.settlement)
                             == hauler.targetSettlement);
            if (!roadOpen) {
                hauler.state = HaulerState::GoingHome;
                inv.contents.clear();
                break;
            }

            const auto& destPos = registry.get<Position>(hauler.targetSettlement);
            if (!InRange(pos, destPos, ARRIVE_RADIUS)) {
                MoveToward(vel, pos, destPos.x, destPos.y, speed);
            } else {
                vel.vx = vel.vy = 0.f;
                auto* destStock = registry.try_get<Stockpile>(hauler.targetSettlement);
                if (destStock) {
                    for (auto& [type, qty] : inv.contents)
                        destStock->quantities[type] += static_cast<float>(qty);
                }
                inv.contents.clear();
                hauler.state = HaulerState::GoingHome;
            }
            break;
        }

        // ---- GoingHome: walk back, reset to Idle on arrival ----
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
