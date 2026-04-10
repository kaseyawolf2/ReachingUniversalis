#include "RenderSystem.h"
#include "raylib.h"
#include <cstdio>

void RenderSystem::DrawStockpilePanel(const RenderSnapshot::StockpilePanel& panel) const {
    // Positioned below the player HUD panel, above the event log
    static const int PX = 10, PY = 200;
    static const int PW = 220, LINE_H = 18;

    int lines = 1 + (int)panel.quantities.size();
    int ph    = lines * LINE_H + 16;

    DrawRectangle(PX, PY, PW, ph, Fade(BLACK, 0.7f));
    DrawRectangleLines(PX, PY, PW, ph, LIGHTGRAY);
    DrawText(panel.name.c_str(), PX + 8, PY + 8, 14, YELLOW);

    int y = PY + 8 + LINE_H;
    for (const auto& [type, qty] : panel.quantities) {
        const char* label = "?";
        Color col = WHITE;
        switch (type) {
            case ResourceType::Food:    label = "Food";    col = GREEN;   break;
            case ResourceType::Water:   label = "Water";   col = SKYBLUE; break;
            case ResourceType::Shelter: label = "Shelter"; col = BROWN;   break;
        }

        // Show price if available
        auto priceIt = panel.prices.find(type);
        char buf[48];
        if (priceIt != panel.prices.end())
            std::snprintf(buf, sizeof(buf), "%s: %.1f @ %.2fg", label, qty, priceIt->second);
        else
            std::snprintf(buf, sizeof(buf), "%s: %.1f", label, qty);

        DrawText(buf, PX + 8, y, 13, col);
        y += LINE_H;
    }
}
