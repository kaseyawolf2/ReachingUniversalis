#include "RenderSystem.h"
#include "raylib.h"
#include <cstdio>
#include <algorithm>
#include <cstring>

void RenderSystem::DrawStockpilePanel(const RenderSnapshot::StockpilePanel& panel) const {
    // Positioned below the player HUD panel, above the event log
    static const int PX = 10, PY = 200;
    static const int PW = 265, LINE_H = 17;

    // Header + treasury + stability + resources + event log header + events
    int resLines    = 0;
    for (const auto& [type, qty] : panel.quantities)
        if (type != ResourceType::Shelter) ++resLines;
    int eventLines  = (int)panel.recentEvents.size();
    int totalLines  = 1 + 2 + resLines + (eventLines > 0 ? 1 + eventLines : 0);
    int ph          = totalLines * LINE_H + 14;

    DrawRectangle(PX, PY, PW, ph, Fade(BLACK, 0.75f));
    DrawRectangleLines(PX, PY, PW, ph, LIGHTGRAY);

    // Header with stability bar
    char headBuf[64];
    std::snprintf(headBuf, sizeof(headBuf), "%s  [%d pop]", panel.name.c_str(), panel.pop);
    DrawText(headBuf, PX + 8, PY + 6, 14, YELLOW);

    // Stability bar (right side of header)
    int stbX = PX + PW - 80, stbY = PY + 8, stbW = 68, stbH = 12;
    float stb = panel.stability;
    Color stbCol = (stb > 0.65f) ? GREEN : (stb > 0.35f) ? YELLOW : RED;
    DrawRectangle(stbX, stbY, stbW, stbH, Fade(WHITE, 0.1f));
    DrawRectangle(stbX, stbY, (int)(stbW * stb), stbH, Fade(stbCol, 0.75f));
    DrawRectangleLines(stbX, stbY, stbW, stbH, Fade(LIGHTGRAY, 0.5f));
    char stbBuf[16];
    std::snprintf(stbBuf, sizeof(stbBuf), "%.0f%%", stb * 100.f);
    DrawText(stbBuf, stbX + 2, stbY, 11, WHITE);

    int y = PY + 6 + LINE_H;

    // Treasury line
    char tresBuf[48];
    std::snprintf(tresBuf, sizeof(tresBuf), "Treasury: %.0fg", panel.treasury);
    Color tresCol = (panel.treasury < 50.f) ? RED : (panel.treasury < 150.f) ? ORANGE : GOLD;
    DrawText(tresBuf, PX + 8, y, 13, tresCol);
    y += LINE_H;

    // Blank separator line
    y += 2;

    for (const auto& [type, qty] : panel.quantities) {
        const char* label = "?";
        Color col = WHITE;
        switch (type) {
            case ResourceType::Food:    label = "Food";    col = GREEN;   break;
            case ResourceType::Water:   label = "Water";   col = SKYBLUE; break;
            case ResourceType::Wood:    label = "Wood";    col = BROWN;   break;
            case ResourceType::Shelter: continue;  // not shown in stockpile
        }

        auto priceIt = panel.prices.find(type);
        auto netIt   = panel.netRatePerHour.find(type);
        char buf[72];

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

    // Recent events filtered to this settlement
    if (!panel.recentEvents.empty()) {
        y += 2;
        DrawText("Recent:", PX + 8, y, 11, Fade(YELLOW, 0.7f));
        y += LINE_H - 2;
        for (const auto& e : panel.recentEvents) {
            char ebuf[80];
            std::snprintf(ebuf, sizeof(ebuf), "D%d %02d: %.50s",
                          e.day, e.hour, e.message.c_str());
            // Color-code events
            bool isGood = (e.message.find("Born")       != std::string::npos ||
                           e.message.find("BOUNTY")     != std::string::npos ||
                           e.message.find("restored")   != std::string::npos ||
                           e.message.find("CONVOY")     != std::string::npos ||
                           e.message.find("BOOM")       != std::string::npos ||
                           e.message.find("RAIN")       != std::string::npos);
            bool isBad  = (e.message.find("COLLAPSED")  != std::string::npos ||
                           e.message.find("died")       != std::string::npos ||
                           e.message.find("DISEASE")    != std::string::npos ||
                           e.message.find("DROUGHT")    != std::string::npos ||
                           e.message.find("BLIGHT")     != std::string::npos ||
                           e.message.find("EMPTY")      != std::string::npos ||
                           e.message.find("BLIZZARD")   != std::string::npos);
            Color ec = isGood ? Fade(GREEN, 0.8f) : isBad ? Fade(RED, 0.8f) : Fade(LIGHTGRAY, 0.7f);
            DrawText(ebuf, PX + 8, y, 11, ec);
            y += LINE_H - 3;
        }
    }
}
