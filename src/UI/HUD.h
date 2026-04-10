#pragma once
#include "raylib.h"
#include <entt/entt.hpp>

class HUD {
public:
    void HandleInput(entt::registry& registry);
    void Draw(entt::registry& registry, int totalDeaths = 0);

private:
    void DrawNeedBar(int x, int y, float value, float critThreshold,
                     const char* label, Color barColor) const;
    void DrawEventLog(entt::registry& registry) const;
    void DrawWorldStatus(entt::registry& registry) const;
    void DrawDebugOverlay(entt::registry& registry) const;
    void DrawHoverTooltip(entt::registry& registry) const;

    int  logScroll    = 0;    // lines scrolled up in event log
    bool debugOverlay = false;
};
