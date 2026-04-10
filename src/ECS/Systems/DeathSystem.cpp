#include "DeathSystem.h"
#include "ECS/Components.h"
#include <vector>
#include <cstdio>

// An NPC dies if any single need stays at 0 for this many gameDt seconds.
// 12 game-hours = 12 * 60 gameDt seconds at 1x speed.
static constexpr float DEATH_THRESHOLD = 12.f * 60.f;

// 1 game-day = 24 game-hours × 60 game-minutes = 1440 game-seconds
static constexpr float SECS_PER_GAME_DAY = 24.f * 60.f;

void DeathSystem::Update(entt::registry& registry, float realDt) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    float gameDt = timeView.get<TimeManager>(*timeView.begin()).GameDt(realDt);
    if (gameDt <= 0.f) return;

    float agingDays = gameDt / SECS_PER_GAME_DAY;  // age advance per tick

    std::vector<entt::entity> toRemove;

    // ---- Age-based natural death ----
    registry.view<Age>().each([&](auto entity, Age& age) {
        age.days += agingDays;
        if (age.days >= age.maxDays) {
            toRemove.push_back(entity);
            auto logView = registry.view<EventLog>();
            if (!logView.empty()) {
                auto& tm2 = timeView.get<TimeManager>(*timeView.begin());
                logView.get<EventLog>(*logView.begin()).Push(
                    tm2.day, (int)tm2.hourOfDay,
                    "NPC died of old age");
            }
        }
    });

    // ---- Need-based death ----
    auto view = registry.view<Needs, DeprivationTimer>();
    for (auto entity : view) {
        if (!registry.valid(entity)) continue;   // may have been queued above
        // Skip if already queued for removal
        bool already = false;
        for (auto e : toRemove) if (e == entity) { already = true; break; }
        if (already) continue;

        auto& needs = view.get<Needs>(entity);
        auto& timer = view.get<DeprivationTimer>(entity);

        for (int i = 0; i < 3; ++i) {
            if (needs.list[i].value <= 0.001f) {
                timer.needsAtZero[i] += gameDt;
                if (timer.needsAtZero[i] >= DEATH_THRESHOLD) {
                    toRemove.push_back(entity);
                    const char* cause = (i == 0) ? "hunger" :
                                        (i == 1) ? "thirst" : "exhaustion";
                    std::printf("[DEATH] Entity %u died of %s\n",
                                (unsigned)entity, cause);
                    auto logView = registry.view<EventLog>();
                    if (!logView.empty()) {
                        auto& tm2 = timeView.get<TimeManager>(*timeView.begin());
                        logView.get<EventLog>(*logView.begin()).Push(
                            tm2.day, (int)tm2.hourOfDay,
                            std::string("NPC died of ") + cause);
                    }
                    break;
                }
            } else {
                timer.needsAtZero[i] = 0.f;
            }
        }
    }

    for (auto e : toRemove) {
        if (registry.valid(e)) {
            ++totalDeaths;
            registry.destroy(e);
        }
    }

    // ---- Settlement collapse check ----
    // After deaths, log any settlement that just hit 0 population.
    auto logView2 = registry.view<EventLog>();
    if (!logView2.empty() && !toRemove.empty()) {
        auto& log2 = logView2.get<EventLog>(*logView2.begin());
        auto& tm2  = timeView.get<TimeManager>(*timeView.begin());

        registry.view<Settlement>().each([&](auto settl, const Settlement& s) {
            int pop = 0;
            registry.view<HomeSettlement>(entt::exclude<PlayerTag>).each(
                [&](const HomeSettlement& hs) { if (hs.settlement == settl) ++pop; });
            if (pop == 0 && !m_collapsed.count(settl)) {
                m_collapsed.insert(settl);
                log2.Push(tm2.day, (int)tm2.hourOfDay,
                    s.name + " has COLLAPSED — population zero");
            } else if (pop > 0) {
                m_collapsed.erase(settl);  // recovered
            }
        });
    }
}
