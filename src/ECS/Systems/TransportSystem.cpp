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

// Best rival-rejected route per home settlement (for complaint logging).
struct RivalRejected { entt::entity dest = entt::null; float profit = 0.f; };
static std::map<entt::entity, RivalRejected> s_rivalRejected;

// Scan all reachable settlements for the most profitable trade, weighted by
// proximity: profit-per-distance so close routes beat distant ones.
static TradeRoute FindBestRoute(entt::registry& registry,
                                 entt::entity homeSettlement,
                                 int maxCapacity,
                                 const std::string& preferredRoute = "",
                                 const std::string& avoidRoute = "",
                                 const std::string& loyalRoute = "") {
    TradeRoute best;
    float bestScore = 0.f;  // profit / max(100, distance) — avoids zero division
    s_rivalRejected.erase(homeSettlement);  // reset for this evaluation

    auto* homeSp  = registry.try_get<Stockpile>(homeSettlement);
    auto* homeMkt = registry.try_get<Market>(homeSettlement);
    const auto* homePos = registry.try_get<Position>(homeSettlement);
    if (!homeSp || !homeMkt || !homePos) return best;

    // Home settlement name for preferred-route matching
    std::string homeName;
    if (!preferredRoute.empty() || !loyalRoute.empty()) {
        if (auto* hs = registry.try_get<Settlement>(homeSettlement))
            homeName = hs->name;
    }

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
            // Rivalry penalty: 20% score reduction when home and dest are rivals
            auto* homeSettl = registry.try_get<Settlement>(homeSettlement);
            if (homeSettl && homeSettl->rivalryTimer > 0.f && homeSettl->rivalEntity == destEnt)
                score *= 0.8f;
            // Relations-based rivalry avoidance: -40% when relations < -0.5
            bool rivalPenalised = false;
            if (homeSettl) {
                auto rit = homeSettl->relations.find(destEnt);
                if (rit != homeSettl->relations.end() && rit->second < -0.5f) {
                    score *= 0.6f;
                    rivalPenalised = true;
                }
            }
            // Track best rival-rejected route for complaint log
            if (rivalPenalised && profit > MIN_TRIP_PROFIT) {
                auto& rr = s_rivalRejected[homeSettlement];
                if (profit > rr.profit) {
                    rr.dest   = destEnt;
                    rr.profit = profit;
                }
            }
            // Preferred route bonus: +10% when this route matches the hauler's best route
            // Worst route penalty: -20% when this route matches recent worst loss
            // Loyal route bonus: +15% when hauler has run the same route 5+ times
            if (!homeName.empty()) {
                auto* destSettl2 = registry.try_get<Settlement>(destEnt);
                if (destSettl2) {
                    std::string candidate = homeName + "\xe2\x86\x92" + destSettl2->name;
                    if (!preferredRoute.empty() && candidate == preferredRoute)
                        score *= 1.1f;
                    if (!loyalRoute.empty() && candidate == loyalRoute)
                        score *= 1.15f;
                    if (!avoidRoute.empty() && candidate == avoidRoute)
                        score *= 0.8f;
                }
            }
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

    std::vector<entt::entity> retireList;  // deferred hauler retirement

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

        // Tick down worst-route avoidance timer
        if (hauler.worstRouteTimer > 0.f) {
            hauler.worstRouteTimer -= gameHoursDt;
            if (hauler.worstRouteTimer <= 0.f) {
                hauler.worstRouteTimer = 0.f;
                hauler.worstRoute.clear();
                hauler.worstLoss = 0.f;
            }
        }

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
            TradeRoute best = FindBestRoute(registry, home.settlement, inv.maxCapacity,
                                            hauler.bestRoute,
                                            hauler.worstRouteTimer > 0.f ? hauler.worstRoute : "",
                                            hauler.consecutiveRouteCount >= 5 ? hauler.lastRoute : "");
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
                // Rivalry complaint: log when a hauler misses a profitable rival route
                auto rrIt = s_rivalRejected.find(home.settlement);
                if (rrIt != s_rivalRejected.end() && rrIt->second.dest != entt::null) {
                    static std::map<entt::entity, int> s_rivalComplaintCount;
                    int& cnt = s_rivalComplaintCount[entity];
                    if (++cnt % 3 == 1) {
                        auto logV2 = registry.view<EventLog>();
                        auto tmV2  = registry.view<TimeManager>();
                        if (!logV2.empty() && !tmV2.empty()) {
                            auto& evLog2 = logV2.get<EventLog>(*logV2.begin());
                            const auto& tm2 = tmV2.get<TimeManager>(*tmV2.begin());
                            std::string who = "A hauler";
                            if (auto* n = registry.try_get<Name>(entity)) who = n->value;
                            std::string destName = "a rival";
                            if (auto* ds = registry.try_get<Settlement>(rrIt->second.dest))
                                destName = ds->name;
                            char buf[180];
                            std::snprintf(buf, sizeof(buf),
                                "%s avoids %s due to rivalry — potential profit lost.",
                                who.c_str(), destName.c_str());
                            evLog2.Push(tm2.day, (int)tm2.hourOfDay, buf);
                        }
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
                bool wasInConvoy = hauler.inConvoy;
                hauler.inConvoy = false;
                entt::entity convoyPartner = entt::null;
                registry.view<Hauler, Position>(entt::exclude<PlayerTag>).each(
                    [&](auto other, const Hauler& oh, const Position& op) {
                        if (other == entity || hauler.inConvoy) return;
                        if (oh.state != HaulerState::GoingToDeposit) return;
                        if (oh.targetSettlement != hauler.targetSettlement) return;
                        float dx = op.x - pos.x, dy = op.y - pos.y;
                        if (dx*dx + dy*dy < CONVOY_RANGE * CONVOY_RANGE) {
                            hauler.inConvoy = true;
                            convoyPartner = other;
                        }
                    });
                // Log convoy formation on transition false → true
                if (hauler.inConvoy && !wasInConvoy) {
                    auto logV = registry.view<EventLog>();
                    auto tmV  = registry.view<TimeManager>();
                    if (!logV.empty() && !tmV.empty()) {
                        const auto& tmRef = tmV.get<TimeManager>(*tmV.begin());
                        std::string who = "A hauler";
                        if (const auto* n = registry.try_get<Name>(entity)) who = n->value;
                        std::string partner = "another hauler";
                        if (convoyPartner != entt::null) {
                            if (const auto* pn = registry.try_get<Name>(convoyPartner))
                                partner = pn->value;
                        }
                        std::string dest = "?";
                        if (const auto* ds = registry.try_get<Settlement>(hauler.targetSettlement))
                            dest = ds->name;
                        char buf[200];
                        std::snprintf(buf, sizeof(buf), "%s formed convoy with %s on the way to %s.",
                                      who.c_str(), partner.c_str(), dest.c_str());
                        logV.get<EventLog>(*logV.begin()).Push(tmRef.day, (int)tmRef.hourOfDay, buf);
                    }
                }
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

                // Mentor bonus: consume per-trip efficiency boost from veteran mentorship
                if (earned > 0.f && hauler.mentorBonus > 0.f) {
                    earned += earned * hauler.mentorBonus;
                    hauler.mentorBonus = 0.f;
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
                            // 1-in-3 frequency to avoid log spam
                            static std::map<entt::entity, int> s_allyLogCounter;
                            int& cnt = s_allyLogCounter[entity];
                            if (++cnt % 3 == 1) {
                                std::string hName = "Hauler";
                                if (auto* nm = registry.try_get<Name>(entity))
                                    hName = nm->value;
                                int totalUnits = 0;
                                std::string resName;
                                for (const auto& [type, qty] : inv.contents) {
                                    totalUnits += (int)qty;
                                    if (resName.empty())
                                        resName = (type == ResourceType::Food) ? "food" :
                                                  (type == ResourceType::Water) ? "water" : "wood";
                                }
                                if (inv.contents.size() > 1) resName = "mixed goods";
                                char abuf[180];
                                std::snprintf(abuf, sizeof(abuf),
                                    "Allied trade: %s delivers %d %s from %s to %s (boosted)",
                                    hName.c_str(), totalUnits, resName.c_str(),
                                    cargoSourceName.c_str(), destSettl->name.c_str());
                                logV.get<EventLog>(*logV.begin()).Push(tm2.day, (int)tm2.hourOfDay, abuf);
                            }
                        }
                    }
                }

                // Trip history + best-profit record
                {
                    float totalQty = 0.f;
                    for (const auto& [type, qty] : inv.contents) totalQty += qty;
                    float tripCost   = hauler.buyPrice * totalQty;
                    float tripProfit = earned - tripCost;
                    hauler.lifetimeTrips++;
                    hauler.lifetimeProfit += tripProfit;

                    // Route loyalty tracking
                    {
                        std::string rsrc = cargoSourceName.empty() ? "???" : cargoSourceName;
                        std::string rdst = destSettl ? destSettl->name : "???";
                        std::string currentRoute = rsrc + "\xe2\x86\x92" + rdst;  // UTF-8 →
                        if (currentRoute == hauler.lastRoute) {
                            hauler.consecutiveRouteCount++;
                        } else {
                            hauler.lastRoute = currentRoute;
                            hauler.consecutiveRouteCount = 1;
                        }
                        if (hauler.consecutiveRouteCount == 5) {
                            // Log once per hauler via static set
                            static std::set<entt::entity> s_routeLoyaltyLogged;
                            if (s_routeLoyaltyLogged.find(entity) == s_routeLoyaltyLogged.end()) {
                                s_routeLoyaltyLogged.insert(entity);
                                auto logVR = registry.view<EventLog>();
                                auto tmVR  = registry.view<TimeManager>();
                                if (!logVR.empty() && !tmVR.empty()) {
                                    const auto& tmR = tmVR.get<TimeManager>(*tmVR.begin());
                                    std::string haulerName = "Hauler";
                                    if (auto* nm = registry.try_get<Name>(entity))
                                        haulerName = nm->value;
                                    char rbuf[180];
                                    std::snprintf(rbuf, sizeof(rbuf),
                                        "%s is a regular on the %s route",
                                        haulerName.c_str(), currentRoute.c_str());
                                    logVR.get<EventLog>(*logVR.begin()).Push(tmR.day, (int)tmR.hourOfDay, rbuf);
                                }
                            }
                        }
                    }

                    // ---- Hauler retirement: veteran haulers may retire after delivery ----
                    if (hauler.lifetimeTrips >= 20) {
                        if (const auto* money = registry.try_get<Money>(entity)) {
                            if (money->balance >= 200.f) {
                                static std::mt19937 s_retireRng{std::random_device{}()};
                                if (s_retireRng() % 50 == 0) {
                                    retireList.push_back(entity);
                                    auto logVRet = registry.view<EventLog>();
                                    auto tmVRet  = registry.view<TimeManager>();
                                    if (!logVRet.empty() && !tmVRet.empty()) {
                                        const auto& tmRet = tmVRet.get<TimeManager>(*tmVRet.begin());
                                        std::string who = "A hauler";
                                        if (const auto* nm = registry.try_get<Name>(entity))
                                            who = nm->value;
                                        char rbuf[200];
                                        std::snprintf(rbuf, sizeof(rbuf),
                                            "%s retires from hauling after %d trips with %.0fg saved.",
                                            who.c_str(), hauler.lifetimeTrips, money->balance);
                                        logVRet.get<EventLog>(*logVRet.begin()).Push(
                                            tmRet.day, (int)tmRet.hourOfDay, rbuf);
                                    }
                                }
                            }
                        }
                    }

                    // Log loss-making trips
                    if (tripProfit < 0.f) {
                        auto logV3 = registry.view<EventLog>();
                        auto tmV3  = registry.view<TimeManager>();
                        if (!logV3.empty() && !tmV3.empty()) {
                            const auto& tm4 = tmV3.get<TimeManager>(*tmV3.begin());
                            std::string who = "Hauler";
                            if (const auto* nm = registry.try_get<Name>(entity))
                                who = nm->value;
                            std::string dest = destSettl ? destSettl->name : "???";
                            char lbuf[160];
                            std::snprintf(lbuf, sizeof(lbuf),
                                "%s completed a loss-making trip to %s (%.1fg)",
                                who.c_str(), dest.c_str(), tripProfit);
                            logV3.get<EventLog>(*logV3.begin()).Push(tm4.day, (int)tm4.hourOfDay, lbuf);
                        }
                    }
                    // Track worst loss for route avoidance
                    if (tripProfit < 0.f && tripProfit < hauler.worstLoss) {
                        hauler.worstLoss = tripProfit;
                        std::string wsrc = cargoSourceName.empty() ? "???" : cargoSourceName;
                        std::string wdst = destSettl ? destSettl->name : "???";
                        hauler.worstRoute = wsrc + "\xe2\x86\x92" + wdst;
                        hauler.worstRouteTimer = 24.f;  // avoid for 24 game-hours
                    }
                    if (tripProfit > hauler.bestProfit) {
                        hauler.bestProfit = tripProfit;
                        std::string src = cargoSourceName.empty() ? "???" : cargoSourceName;
                        std::string dst = destSettl ? destSettl->name : "???";
                        hauler.bestRoute = src + "\xe2\x86\x92" + dst;  // UTF-8 →
                        auto logV2 = registry.view<EventLog>();
                        auto tmV2  = registry.view<TimeManager>();
                        if (!logV2.empty() && !tmV2.empty()) {
                            const auto& tm3 = tmV2.get<TimeManager>(*tmV2.begin());
                            std::string haulerName = "Hauler";
                            if (auto* nm = registry.try_get<Name>(entity))
                                haulerName = nm->value;
                            char pbuf[160];
                            std::snprintf(pbuf, sizeof(pbuf),
                                "%s sets new personal record: +%.1fg on %s",
                                haulerName.c_str(), tripProfit, hauler.bestRoute.c_str());
                            logV2.get<EventLog>(*logV2.begin()).Push(tm3.day, (int)tm3.hourOfDay, pbuf);
                        }
                    }
                }

                // ---- Hauler mentorship: veterans teach novices at the same settlement ----
                if (hauler.lifetimeTrips < 5) {
                    // Novice just completed a delivery — check for a veteran at home settlement
                    auto* hs = registry.try_get<HomeSettlement>(entity);
                    if (hs && hs->settlement != entt::null) {
                        entt::entity noviceHome = hs->settlement;
                        bool found = false;
                        entt::entity mentorEnt = entt::null;
                        registry.view<Hauler, HomeSettlement>(entt::exclude<PlayerTag>).each(
                            [&](auto other, const Hauler& otherH, const HomeSettlement& otherHs) {
                                if (found || other == entity) return;
                                if (otherHs.settlement == noviceHome && otherH.lifetimeTrips >= 15) {
                                    found = true;
                                    mentorEnt = other;
                                }
                            });
                        if (found) {
                            hauler.mentorBonus = 0.1f;
                            // Log at 1-in-5 frequency
                            static std::mt19937 s_mentorRng{std::random_device{}()};
                            if (s_mentorRng() % 5 == 0) {
                                auto logVM = registry.view<EventLog>();
                                auto tmVM  = registry.view<TimeManager>();
                                if (!logVM.empty() && !tmVM.empty()) {
                                    const auto& tmM = tmVM.get<TimeManager>(*tmVM.begin());
                                    std::string vetName = "A veteran";
                                    if (const auto* nm = registry.try_get<Name>(mentorEnt))
                                        vetName = nm->value;
                                    std::string novName = "a novice";
                                    if (const auto* nm = registry.try_get<Name>(entity))
                                        novName = nm->value;
                                    std::string settlName = "settlement";
                                    if (registry.valid(noviceHome))
                                        if (const auto* s = registry.try_get<Settlement>(noviceHome))
                                            settlName = s->name;
                                    char mbuf[180];
                                    std::snprintf(mbuf, sizeof(mbuf),
                                        "%s shows %s the ropes at %s",
                                        vetName.c_str(), novName.c_str(), settlName.c_str());
                                    logVM.get<EventLog>(*logVM.begin()).Push(tmM.day, (int)tmM.hourOfDay, mbuf);
                                }
                            }
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

    // Process deferred hauler retirements
    for (auto e : retireList) {
        if (registry.valid(e) && registry.all_of<Hauler>(e)) {
            registry.remove<Hauler>(e);
            // Set to idle worker
            if (auto* as = registry.try_get<AgentState>(e)) {
                as->behavior = AgentBehavior::Idle;
                as->target   = entt::null;
            }
            if (auto* v = registry.try_get<Velocity>(e)) {
                v->vx = v->vy = 0.f;
            }
        }
    }
}
