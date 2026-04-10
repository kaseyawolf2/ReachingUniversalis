#include "GameState.h"
#include "ECS/Components.h"
#include "World/WorldGenerator.h"
#include <cmath>

void GameState::Initialize() {
    WorldGenerator::Populate(registry);
}

void GameState::Update(float dt) {
    timeSystem.Update(registry, dt);
    hud.HandleInput(registry);
    playerInputSystem.Update(registry, dt);
    cameraSystem.Update(registry, dt);
    renderSystem.HandleInput(registry);
    needDrainSystem.Update(registry, dt);
    consumptionSystem.Update(registry, dt);
    scheduleSystem.Update(registry, dt);
    agentDecisionSystem.Update(registry, dt);
    movementSystem.Update(registry, dt);
    productionSystem.Update(registry, dt);
    transportSystem.Update(registry, dt);
    deathSystem.Update(registry, dt);
}

void GameState::Draw() {
    renderSystem.Draw(registry);
    hud.Draw(registry, deathSystem.totalDeaths);
}

// Interpolate between two colors by factor t (0.0–1.0)
static Color LerpColor(Color a, Color b, float t) {
    return {
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        255
    };
}

Color GameState::SkyColor() const {
    auto view = registry.view<const TimeManager>();
    if (view.empty()) return { 30, 30, 30, 255 };

    float hour = view.get<const TimeManager>(*view.begin()).hourOfDay;

    // Key colors at specific hours
    static const Color midnight = {  10,  10,  30, 255 };
    static const Color dawn     = { 220, 120,  60, 255 };
    static const Color morning  = { 100, 160, 220, 255 };
    static const Color noon     = {  80, 180, 255, 255 };
    static const Color dusk     = { 200,  80,  40, 255 };
    static const Color night    = {  15,  15,  40, 255 };

    // Piecewise linear interpolation across the day
    struct Keyframe { float hour; Color color; };
    static const Keyframe keys[] = {
        {  0.0f, midnight },
        {  5.0f, midnight },
        {  6.5f, dawn     },
        {  9.0f, morning  },
        { 12.0f, noon     },
        { 15.0f, morning  },
        { 18.5f, dusk     },
        { 20.0f, night    },
        { 24.0f, midnight },
    };
    static const int N = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < N - 1; ++i) {
        if (hour >= keys[i].hour && hour < keys[i + 1].hour) {
            float t = (hour - keys[i].hour) / (keys[i + 1].hour - keys[i].hour);
            return LerpColor(keys[i].color, keys[i + 1].color, t);
        }
    }
    return midnight;
}
