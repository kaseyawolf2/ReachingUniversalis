#include "RenderSystem.h"
#include "raylib.h"
#include <cstdio>

void RenderSystem::DrawStockpilePanel(const std::string& name,
                                      const std::map<ResourceType, float>& quantities) const {
    static const int PX = 10, PY = 580;
    static const int PW = 200, LINE_H = 18;

    int lines = 1 + (int)quantities.size();
    int ph    = lines * LINE_H + 16;

    DrawRectangle(PX, PY, PW, ph, Fade(BLACK, 0.7f));
    DrawRectangleLines(PX, PY, PW, ph, LIGHTGRAY);
    DrawText(name.c_str(), PX + 8, PY + 8, 14, YELLOW);

    int y = PY + 8 + LINE_H;
    for (const auto& [type, qty] : quantities) {
        const char* label = "?";
        Color col = WHITE;
        switch (type) {
            case ResourceType::Food:    label = "Food";    col = GREEN;   break;
            case ResourceType::Water:   label = "Water";   col = SKYBLUE; break;
            case ResourceType::Shelter: label = "Shelter"; col = BROWN;   break;
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%s: %.1f", label, qty);
        DrawText(buf, PX + 8, y, 13, col);
        y += LINE_H;
    }
}
