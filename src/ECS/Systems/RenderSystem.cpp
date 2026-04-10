#include "RenderSystem.h"
#include "raylib.h"
#include <cstdio>

void RenderSystem::DrawStockpilePanel(const RenderSnapshot::StockpilePanel& panel) const {
    // Positioned below the player HUD panel, above the event log
    static const int PX = 10, PY = 200;
    static const int PW = 260, LINE_H = 18;

    // 1 header + resources + treasury + pop
    int lines = 1 + (int)panel.quantities.size() + 2;
    int ph    = lines * LINE_H + 16;

    DrawRectangle(PX, PY, PW, ph, Fade(BLACK, 0.7f));
    DrawRectangleLines(PX, PY, PW, ph, LIGHTGRAY);

    char headBuf[48];
    std::snprintf(headBuf, sizeof(headBuf), "%s [pop %d]", panel.name.c_str(), panel.pop);
    DrawText(headBuf, PX + 8, PY + 8, 14, YELLOW);

    int y = PY + 8 + LINE_H;

    // Treasury line
    char tresBuf[48];
    std::snprintf(tresBuf, sizeof(tresBuf), "Treasury: %.0fg", panel.treasury);
    Color tresCol = (panel.treasury < 50.f) ? RED : (panel.treasury < 150.f) ? ORANGE : GOLD;
    DrawText(tresBuf, PX + 8, y, 13, tresCol);
    y += LINE_H;

    for (const auto& [type, qty] : panel.quantities) {
        const char* label = "?";
        Color col = WHITE;
        switch (type) {
            case ResourceType::Food:    label = "Food";    col = GREEN;   break;
            case ResourceType::Water:   label = "Water";   col = SKYBLUE; break;
            case ResourceType::Wood:    label = "Wood";    col = BROWN;   break;
            case ResourceType::Shelter: continue;  // not shown in stockpile
        }

        // Show price and net rate if available
        auto priceIt = panel.prices.find(type);
        auto netIt   = panel.netRatePerHour.find(type);
        char buf[64];

        float netRate = (netIt != panel.netRatePerHour.end()) ? netIt->second : 0.f;
        const char* sign = (netRate >= 0.f) ? "+" : "";

        if (priceIt != panel.prices.end())
            std::snprintf(buf, sizeof(buf), "%s: %.0f @%.2fg  %s%.1f/hr",
                          label, qty, priceIt->second, sign, netRate);
        else
            std::snprintf(buf, sizeof(buf), "%s: %.0f  %s%.1f/hr",
                          label, qty, sign, netRate);

        Color netCol = (netRate >= 0.f) ? col : Fade(RED, 0.9f);
        DrawText(buf, PX + 8, y, 13, netCol);
        y += LINE_H;
    }
}
