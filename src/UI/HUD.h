#pragma once
#include "raylib.h"
#include <entt/entt.hpp>

class HUD {
public:
    void Draw(entt::registry& registry);

private:
    void DrawNeedBar(int x, int y, float value, float critThreshold,
                     const char* label, Color barColor) const;
};
