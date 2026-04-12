#include "TransportSystem.h"
#include "ECS/Components.h"
#include <cmath>
#include <algorithm>
#include <set>
#include <vector>
#include <limits>
#include <random>

static constexpr float ARRIVE_RADIUS         = 130.f;
static constexpr float MIN_TRIP_PROFIT       = 5.f;   // gold; ideal minimum to haul for
static constexpr float MIN_TRIP_PROFIT_FLOOR = 0.5f;  // hauler will accept this after patience runs out
static constexpr float WAIT_INTERVAL         = 1.f;   // game-hours to wait if no profitable route
static constexpr int   MAX_WAIT_CYCLES       = 5;     // after this many waits, accept any positive margin

// Inter-settlement rivalry/alliance
static constexpr float RIVAL_THRESHOLD   = -0.5f;   // below this → importer sees exporter as rival
static constexpr float ALLY_THRESHOLD    =  0.5f;   // above this → importer sees exporter as ally
static constexpr float TRADE_DELTA       =  0.04f;  // per delivery: exporter gains, importer loses
static constexpr float RIVAL_SURCHARGE   =  0.10f;  // extra tax fraction when rival (20%→30%)
static constexpr float ALLY_DISCOUNT     =  0.05f;  // tax reduction when allied (20%→15%)
static std::set<entt::entity> s_loggedIdle;  // tracks haulers that have triggered the idle warning log

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
    // Bandit-heavy roads also penalised: each bandit adds ~15% score reduction.
    struct DestInfo { entt::entity e; float conditionPenalty; int bandits; };
    std::vector<DestInfo> dests;
    registry.view<Road>().each([&](auto roadEnt, const Road& road) {
        if (road.blocked) return;
        // Condition penalty: 0 = perfect road (no penalty), 1 = near-collapse (50% penalty)
        float penalty = 1.f - std::max(0.15f, road.condition);  // range [0, 0.85]
        float condPenalty = penalty * 0.6f;  // scale to [0, 0.51] — max ~50% score reduction
        // Count bandits near this road's midpoint
        int banditCount = 0;
        const auto* pa = registry.try_get<Position>(road.from);
        const auto* pb = registry.try_get<Position>(road.to);
        if (pa && pb) {
            float mx = (pa->x + pb->x) * 0.5f;
            float my = (pa->y + pb->y) * 0.5f;
            registry.view<Position, BanditTag>().each(
                [&](const Position& bp) {
                    float dx = bp.x - mx, dy = bp.y - my;
                    if (dx*dx + dy*dy < 100.f * 100.f) ++banditCount;
                });
        }
        if (road.from == homeSettlement) dests.push_back({ road.to,   condPenalty, banditCount });
        else if (road.to == homeSettlement) dests.push_back({ road.from, condPenalty, banditCount });
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
            float banditPen = std::min(0.6f, di.bandits * 0.15f); // each bandit ≈ 15%, max 60%
            score *= (1.f - banditPen);
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

        // Reset convoy flag; it's recalculated in GoingToDeposit
        if (hauler.state != HaulerState::GoingToDeposit)
            hauler.inConvoy = false;

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
                hauler.cargoSource      = home.settlement;   // track where goods came from
                if (settl) settl->exportCount += best.qty;
                hauler.state            = HaulerState::GoingToDeposit;
                hauler.waitCycles       = 0;
                s_loggedIdle.erase(entity);

                // Log nervous warning if route has bandits
                {
                    int routeBandits = 0;
                    std::string roadNameA, roadNameB;
                    registry.view<Road>().each([&](const Road& rd) {
                        if (rd.blocked) return;
                        bool match = (rd.from == home.settlement && rd.to == best.dest)
                                  || (rd.to == home.settlement && rd.from == best.dest);
                        if (!match) return;
                        const auto* pa2 = registry.try_get<Position>(rd.from);
                        const auto* pb2 = registry.try_get<Position>(rd.to);
                        if (!pa2 || !pb2) return;
                        float mx = (pa2->x + pb2->x) * 0.5f;
                        float my = (pa2->y + pb2->y) * 0.5f;
                        registry.view<Position, BanditTag>().each(
                            [&](const Position& bp) {
                                float dx2 = bp.x - mx, dy2 = bp.y - my;
                                if (dx2*dx2 + dy2*dy2 < 100.f * 100.f) ++routeBandits;
                            });
                        if (const auto* sa = registry.try_get<Settlement>(rd.from)) roadNameA = sa->name;
                        if (const auto* sb = registry.try_get<Settlement>(rd.to))   roadNameB = sb->name;
                    });
                    if (routeBandits > 0) {
                        auto logV = registry.view<EventLog>();
                        auto tmV  = registry.view<TimeManager>();
                        if (logV.begin() != logV.end() && tmV.begin() != tmV.end()) {
                            auto& evLog = logV.get<EventLog>(*logV.begin());
                            const auto& tmRef = tmV.get<TimeManager>(*tmV.begin());
                            std::string who = "A hauler";
                            if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                            char buf[160];
                            std::snprintf(buf, sizeof(buf), "%s nervously travels the %s-%s road (%d bandit%s spotted)",
                                who.c_str(), roadNameA.c_str(), roadNameB.c_str(),
                                routeBandits, routeBandits > 1 ? "s" : "");
                            evLog.Push(tmRef.day, (int)tmRef.hourOfDay, buf);
                        }
                    }
                }
            } else {
                // No good route yet — wait before re-evaluating (patience increases)
                ++hauler.waitCycles;
                hauler.waitTimer = WAIT_INTERVAL;
                // Log once when a hauler has been idle for 12+ hours
                if (hauler.waitCycles >= 12 && s_loggedIdle.insert(entity).second) {
                    auto logV = registry.view<EventLog>();
                    auto tmV  = registry.view<TimeManager>();
                    if (logV.begin() != logV.end() && tmV.begin() != tmV.end()) {
                        auto& evLog = logV.get<EventLog>(*logV.begin());
                        const auto& tmRef = tmV.get<TimeManager>(*tmV.begin());
                        std::string who = "Hauler";
                        if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                        std::string where = "?";
                        if (const auto* s = registry.try_get<Settlement>(home.settlement)) where = s->name;
                        evLog.Push(tmRef.day, (int)tmRef.hourOfDay,
                            who + " idle for 12h at " + where + " — no profitable routes.");
                    }
                }
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

            // Abort if road blocked mid-trip — return goods and refund purchase price
            if (!RouteOpen(registry, home.settlement, hauler.targetSettlement)) {
                auto* sp     = registry.try_get<Stockpile>(home.settlement);
                auto* settl  = registry.try_get<Settlement>(home.settlement);
                auto* money  = registry.try_get<Money>(entity);
                if (sp) {
                    for (auto& [type, qty] : inv.contents) {
                        sp->quantities[type] += qty;
                        // Refund purchase cost: deduct from settlement treasury back to hauler
                        float refund = hauler.buyPrice * qty;
                        if (money) money->balance    += refund;
                        if (settl && settl->treasury >= refund) settl->treasury -= refund;
                    }
                }
                inv.contents.clear();
                hauler.state = HaulerState::GoingHome;
                break;
            }

            const auto& destPos = registry.get<Position>(hauler.targetSettlement);
            if (!InRange(pos, destPos, ARRIVE_RADIUS)) {
                // Convoy check: nearby hauler headed to same destination → 25% speed bonus
                static constexpr float CONVOY_RANGE = 60.f;
                hauler.inConvoy = false;
                registry.view<Hauler, Position>(entt::exclude<PlayerTag>).each(
                    [&](auto other, const Hauler& oh, const Position& op) {
                        if (other == entity || hauler.inConvoy) return;
                        if (oh.state != HaulerState::GoingToDeposit) return;
                        if (oh.targetSettlement != hauler.targetSettlement) return;
                        float dx = op.x - pos.x, dy = op.y - pos.y;
                        if (dx*dx + dy*dy < CONVOY_RANGE * CONVOY_RANGE)
                            hauler.inConvoy = true;
                    });
                float convoySpeed = hauler.inConvoy ? speed * 1.25f : speed;
                MoveToward(vel, pos, destPos.x, destPos.y, convoySpeed);
            } else {
                vel.vx = vel.vy = 0.f;
                auto* destSp   = registry.try_get<Stockpile>(hauler.targetSettlement);
                auto* destMkt  = registry.try_get<Market>(hauler.targetSettlement);
                auto* destSettl = registry.try_get<Settlement>(hauler.targetSettlement);

                static constexpr float TRADE_TAX = 0.20f;  // base 20% of gross sale to settlement

                // Determine effective tax based on destination's view of the cargo source.
                // Rival surcharge: destination imposes a tariff on an undercutting exporter (+10%).
                // Ally discount: destination rewards trusted trading partner (-5%).
                float effectiveTax = TRADE_TAX;
                bool allyTrade = false;
                if (hauler.cargoSource != entt::null && registry.valid(hauler.cargoSource) && destSettl) {
                    auto it = destSettl->relations.find(hauler.cargoSource);
                    if (it != destSettl->relations.end()) {
                        if (it->second < RIVAL_THRESHOLD) effectiveTax = TRADE_TAX + RIVAL_SURCHARGE;
                        if (it->second > ALLY_THRESHOLD)  { effectiveTax = TRADE_TAX - ALLY_DISCOUNT; allyTrade = true; }
                    }
                }

                float earned = 0.f;
                float taxCollected = 0.f;
                if (destSp) {
                    for (auto& [type, qty] : inv.contents) {
                        destSp->quantities[type] += qty;
                        float sellPrice = destMkt ? destMkt->GetPrice(type) : hauler.buyPrice;
                        float gross = sellPrice * qty;
                        float tax   = gross * effectiveTax;
                        // Buying cost was paid at source — earn full revenue minus tax
                        earned       += gross - tax;
                        taxCollected += tax;
                    }
                }
                // Rival hauler harassment: 20% chance of extra gate tax when
                // destination is a rival of the cargo source settlement.
                float gateTax = 0.f;
                if (earned > 0.f && hauler.cargoSource != entt::null
                    && registry.valid(hauler.cargoSource) && destSettl) {
                    auto rit = destSettl->relations.find(hauler.cargoSource);
                    if (rit != destSettl->relations.end() && rit->second < RIVAL_THRESHOLD) {
                        static std::mt19937 s_gateRng{std::random_device{}()};
                        std::uniform_int_distribution<int> chance(0, 4);  // 1/5 = 20%
                        if (chance(s_gateRng) == 0) {
                            gateTax = earned * 0.10f;  // extra 10% of earned
                            earned -= gateTax;
                            // Log at 1-in-5 frequency (every harassment is logged since
                            // the 20% chance already limits frequency)
                            auto logView = registry.view<EventLog>();
                            EventLog* log = logView.empty() ? nullptr
                                          : &logView.get<EventLog>(*logView.begin());
                            auto tmView = registry.view<TimeManager>();
                            if (log && !tmView.empty()) {
                                const auto& tm = tmView.get<TimeManager>(*tmView.begin());
                                auto* srcSettl = registry.try_get<Settlement>(hauler.cargoSource);
                                std::string haulerName = "Hauler";
                                if (auto* nm = registry.try_get<Name>(entity))
                                    haulerName = nm->value;
                                log->Push(tm.day, (int)tm.hourOfDay,
                                    haulerName + " from " +
                                    (srcSettl ? srcSettl->name : "???") +
                                    " taxed at gate in " + destSettl->name +
                                    " (rivalry tariff)");
                            }
                        }
                    }
                }

                // Loyalty bonus: 5% extra when delivering to home settlement
                bool loyaltyApplied = false;
                if (earned > 0.f) {
                    auto* hs = registry.try_get<HomeSettlement>(entity);
                    if (hs && hs->settlement == hauler.targetSettlement) {
                        float bonus = earned * 0.05f;
                        earned += bonus;
                        loyaltyApplied = true;
                    }
                }

                // Credit hauler's wallet with net profit
                if (auto* money = registry.try_get<Money>(entity))
                    if (earned > 0.f) money->balance += earned;
                // Tax goes to destination settlement treasury (includes gate tax)
                if ((taxCollected + gateTax) > 0.f && destSettl)
                    destSettl->treasury += taxCollected + gateTax;

                // Update inter-settlement relations: exporter gains (+), importer loses (-)
                if (hauler.cargoSource != entt::null && registry.valid(hauler.cargoSource)
                    && hauler.cargoSource != hauler.targetSettlement) {
                    auto* exporterSettl = registry.try_get<Settlement>(hauler.cargoSource);
                    if (exporterSettl && destSettl) {
                        auto& relExp = exporterSettl->relations[hauler.targetSettlement];
                        relExp = std::min(1.f, relExp + TRADE_DELTA);
                        auto& relImp = destSettl->relations[hauler.cargoSource];
                        relImp = std::max(-1.f, relImp - TRADE_DELTA);
                    }
                }
                // Save source settlement name before clearing (needed for alliance log)
                std::string cargoSourceName;
                if (hauler.cargoSource != entt::null && registry.valid(hauler.cargoSource))
                    if (const auto* css = registry.try_get<Settlement>(hauler.cargoSource))
                        cargoSourceName = css->name;
                hauler.cargoSource = entt::null;

                // Hauler gains reputation for each successful delivery
                if (auto* rep = registry.try_get<Reputation>(entity))
                    rep->score += 0.05f;

                // Successful trade delivery lifts destination settlement morale slightly.
                if (destSettl) {
                    destSettl->morale = std::min(1.f, destSettl->morale + 0.01f);
                    destSettl->tradeVolume++;
                    for (const auto& [type, qty] : inv.contents)
                        destSettl->importCount += qty;
                }

                // Log the delivery (cargo summary + morale bump)
                if (destSettl && !inv.contents.empty()) {
                    auto logV = registry.view<EventLog>();
                    auto tmV  = registry.view<TimeManager>();
                    if (!logV.empty() && !tmV.empty()) {
                        const auto& tm2 = tmV.get<TimeManager>(*tmV.begin());
                        std::string cargo;
                        for (const auto& [type, qty] : inv.contents) {
                            if (!cargo.empty()) cargo += "+";
                            const char* rn = (type == ResourceType::Food)  ? "food"  :
                                             (type == ResourceType::Water) ? "water" : "wood";
                            cargo += std::to_string(qty) + " " + rn;
                        }
                        char buf[160];
                        std::snprintf(buf, sizeof(buf),
                            "Hauler delivered %s to %s (morale +1%%)",
                            cargo.c_str(), destSettl->name.c_str());
                        logV.get<EventLog>(*logV.begin()).Push(tm2.day, (int)tm2.hourOfDay, buf);
                        if (loyaltyApplied) {
                            std::string haulerName = "Hauler";
                            if (auto* nm = registry.try_get<Name>(entity))
                                haulerName = nm->value;
                            logV.get<EventLog>(*logV.begin()).Push(tm2.day, (int)tm2.hourOfDay,
                                haulerName + " received local loyalty bonus at " + destSettl->name + ".");
                        }
                        if (allyTrade && !cargoSourceName.empty()) {
                            // Calculate the bonus saved: ALLY_DISCOUNT fraction of gross
                            float gross = 0.f;
                            for (const auto& [type, qty] : inv.contents) {
                                float sp = destMkt ? destMkt->GetPrice(type) : hauler.buyPrice;
                                gross += sp * qty;
                            }
                            float bonus = gross * ALLY_DISCOUNT;
                            char abuf[120];
                            std::snprintf(abuf, sizeof(abuf),
                                "Ally trade: %s → %s (+%.1fg bonus)",
                                cargoSourceName.c_str(), destSettl->name.c_str(), bonus);
                            logV.get<EventLog>(*logV.begin()).Push(tm2.day, (int)tm2.hourOfDay, abuf);
                        }
                    }
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
                            hauler.cargoSource      = curSettlEnt;  // return trip: goods came from here
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
