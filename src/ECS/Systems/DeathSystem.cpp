#include "DeathSystem.h"
#include "ECS/Components.h"
#include <vector>
#include <cstdio>

// An NPC dies if any single need stays at 0 for this many gameDt seconds.
// 12 game-hours = 12 * 60 gameDt seconds at 1x speed.
static constexpr float DEATH_THRESHOLD = 12.f * 60.f;

void DeathSystem::Update(entt::registry& registry, float realDt) {
    auto timeView = registry.view<TimeManager>();
    if (timeView.empty()) return;
    float gameDt = timeView.get<TimeManager>(*timeView.begin()).GameDt(realDt);
    if (gameDt <= 0.f) return;

    std::vector<entt::entity> toRemove;

    auto view = registry.view<Needs, DeprivationTimer>();
    for (auto entity : view) {
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
                    // Push to event log
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
}
