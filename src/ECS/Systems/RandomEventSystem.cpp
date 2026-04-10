#include "RandomEventSystem.h"
#include "ECS/Components.h"
#include <algorithm>
#include <vector>
#include <string>

static constexpr float DROUGHT_MODIFIER  = 0.2f;   // production factor during drought
static constexpr float DROUGHT_DURATION  = 8.f;    // game-hours
static constexpr float BLIGHT_FRACTION   = 0.35f;  // fraction of food stockpile destroyed
static constexpr float BANDIT_DURATION   = 3.f;    // game-hours road is blocked
static constexpr float EVENT_MEAN_HOURS  = 72.f;   // ~3 game-days between events
static constexpr float EVENT_JITTER      = 36.f;   // ±jitter in game-hours

void RandomEventSystem::Update(entt::registry& registry, float realDt) {
    auto tv = registry.view<TimeManager>();
    if (tv.begin() == tv.end()) return;
    const auto& tm = tv.get<TimeManager>(*tv.begin());
    float gameDt = tm.GameDt(realDt);
    if (gameDt <= 0.f) return;
    float gameHoursDt = gameDt * GAME_MINS_PER_REAL_SEC / 60.f;

    // Tick down active settlement modifiers (drought recovery)
    registry.view<Settlement>().each([&](Settlement& s) {
        if (s.modifierDuration > 0.f) {
            s.modifierDuration -= gameHoursDt;
            if (s.modifierDuration <= 0.f) {
                s.modifierDuration   = 0.f;
                s.productionModifier = 1.f;

                auto lv = registry.view<EventLog>();
                if (lv.begin() != lv.end())
                    lv.get<EventLog>(*lv.begin()).Push(
                        tm.day, (int)tm.hourOfDay,
                        s.modifierName + " ends at " + s.name + " — production restored");

                s.modifierName.clear();
            }
        }
    });

    // Tick down bandit timers (auto-clear road)
    registry.view<Road>().each([&](Road& road) {
        if (road.banditTimer > 0.f) {
            road.banditTimer -= gameHoursDt;
            if (road.banditTimer <= 0.f) {
                road.banditTimer = 0.f;
                road.blocked     = false;

                auto lv = registry.view<EventLog>();
                if (lv.begin() != lv.end())
                    lv.get<EventLog>(*lv.begin()).Push(
                        tm.day, (int)tm.hourOfDay,
                        "Bandits dispersed — road reopened");
            }
        }
    });

    // Count down to next event
    m_nextEvent -= gameHoursDt;
    if (m_nextEvent > 0.f) return;

    // Schedule next
    std::uniform_real_distribution<float> jitter(-EVENT_JITTER, EVENT_JITTER);
    m_nextEvent = std::max(12.f, EVENT_MEAN_HOURS + jitter(m_rng));

    TriggerEvent(registry, tm.day, (int)tm.hourOfDay);
}

void RandomEventSystem::TriggerEvent(entt::registry& registry, int day, int hour) {
    auto lv = registry.view<EventLog>();
    EventLog* log = (lv.begin() == lv.end())
                    ? nullptr : &lv.get<EventLog>(*lv.begin());

    // Collect valid settlements
    std::vector<entt::entity> settlements;
    registry.view<Settlement>().each([&](auto e, const Settlement&) {
        settlements.push_back(e);
    });
    if (settlements.empty()) return;

    std::uniform_int_distribution<int> pickSettl(0, (int)settlements.size() - 1);
    std::uniform_int_distribution<int> pickType(0, 2);  // 0=drought 1=blight 2=bandits

    entt::entity target = settlements[pickSettl(m_rng)];
    auto* settl = registry.try_get<Settlement>(target);
    if (!settl) return;

    switch (pickType(m_rng)) {

    case 0: {   // Drought — cripple production
        if (settl->modifierDuration > 0.f) break;   // already has an event
        settl->productionModifier = DROUGHT_MODIFIER;
        settl->modifierDuration   = DROUGHT_DURATION;
        settl->modifierName       = "Drought";
        if (log) log->Push(day, hour,
            "DROUGHT strikes " + settl->name + " ("
            + std::to_string((int)DROUGHT_DURATION) + "h)");
        break;
    }

    case 1: {   // Blight — destroy food stockpile
        auto* sp = registry.try_get<Stockpile>(target);
        if (!sp) break;
        auto it = sp->quantities.find(ResourceType::Food);
        if (it == sp->quantities.end() || it->second < 5.f) break;
        float lost = it->second * BLIGHT_FRACTION;
        it->second -= lost;
        if (log) log->Push(day, hour,
            "BLIGHT hits " + settl->name + " — "
            + std::to_string((int)lost) + " food destroyed");
        break;
    }

    case 2: {   // Bandits — block road temporarily
        bool any = false;
        registry.view<Road>().each([&](Road& road) {
            if (!road.blocked && road.banditTimer <= 0.f) {
                road.blocked     = true;
                road.banditTimer = BANDIT_DURATION;
                any = true;
            }
        });
        if (any && log) log->Push(day, hour,
            "BANDITS blocking road ("
            + std::to_string((int)BANDIT_DURATION) + "h)");
        break;
    }
    }
}
