#include "RenderSystem.h"
#include "raylib.h"
#include <cstdio>
#include <algorithm>
#include <cstring>

void RenderSystem::DrawStockpilePanel(const RenderSnapshot::StockpilePanel& panel) const {
    // Positioned below the player HUD panel, above the event log
    static const int PX = 10, PY = 200;
    static const int PW = 280, LINE_H = 17;

    // Each resource gets two display lines: qty/price row + prod/cons row
    int resLines    = 0;
    for (const auto& [type, qty] : panel.quantities)
        if (type != ResourceType::Shelter) resLines += 2;
    int eventLines  = (int)panel.recentEvents.size();
    int sparklineH  = panel.popHistory.empty() ? 0 : (12 + 24 + 8);  // label + chart + gap
    int totalLines  = 1 + 2 + resLines + (eventLines > 0 ? 1 + eventLines : 0);
    int residentH   = panel.residents.empty() ? 0
                        : 2 + (LINE_H - 2) + (int)panel.residents.size() * (LINE_H - 3);
    int ph          = totalLines * LINE_H + 14 + sparklineH + residentH;

    DrawRectangle(PX, PY, PW, ph, Fade(BLACK, 0.75f));
    DrawRectangleLines(PX, PY, PW, ph, LIGHTGRAY);

    // Header with stability bar
    // Split into prefix + pop number + suffix so crowding can tint the pop in ORANGE.
    bool crowded = (panel.popCap > 0 && panel.pop >= panel.popCap - 2);
    char headPfx[80], headPop[16], headSfx[32];
    std::snprintf(headPfx, sizeof(headPfx), "%s  [", panel.name.c_str());
    std::snprintf(headPop, sizeof(headPop), "%d/%d", panel.pop, panel.popCap);
    if (panel.childCount > 0)
        std::snprintf(headSfx, sizeof(headSfx), " pop, %d child]", panel.childCount);
    else
        std::snprintf(headSfx, sizeof(headSfx), " pop]");
    int hx = PX + 8;
    DrawText(headPfx, hx, PY + 6, 14, YELLOW);
    hx += MeasureText(headPfx, 14);
    DrawText(headPop, hx, PY + 6, 14, crowded ? ORANGE : YELLOW);
    hx += MeasureText(headPop, 14);
    DrawText(headSfx, hx, PY + 6, 14, YELLOW);

    // Morale bar (right side of header) — shows settlement social health
    // Green = high morale (+10% prod), Yellow = neutral, Red = unrest (-15% prod)
    int stbX = PX + PW - 80, stbY = PY + 8, stbW = 68, stbH = 12;
    float mor = panel.morale;
    Color morCol = (mor > 0.7f) ? GREEN : (mor > 0.3f) ? YELLOW : RED;
    DrawRectangle(stbX, stbY, stbW, stbH, Fade(WHITE, 0.1f));
    DrawRectangle(stbX, stbY, (int)(stbW * mor), stbH, Fade(morCol, 0.75f));
    DrawRectangleLines(stbX, stbY, stbW, stbH, Fade(LIGHTGRAY, 0.5f));
    char morBuf[20];
    std::snprintf(morBuf, sizeof(morBuf), "%.0f%% mor", mor * 100.f);
    DrawText(morBuf, stbX + 2, stbY, 11, WHITE);

    int y = PY + 6 + LINE_H;

    // Treasury + workers line
    char tresBuf[64];
    std::snprintf(tresBuf, sizeof(tresBuf), "Treasury: %.0fg   Workers: %d",
                  panel.treasury, panel.workers);
    Color tresCol = (panel.treasury < 50.f) ? RED : (panel.treasury < 150.f) ? ORANGE : GOLD;
    DrawText(tresBuf, PX + 8, y, 13, tresCol);
    y += LINE_H;

    // Active event modifier (if any)
    if (!panel.modifierName.empty()) {
        char modBuf[64];
        std::snprintf(modBuf, sizeof(modBuf), "Event: %s (%.0fh left)",
                      panel.modifierName.c_str(), panel.modifierHoursLeft);
        Color modCol = (panel.modifierName == "Plague")         ? Color{200, 80, 240, 255} :
                       (panel.modifierName == "Drought")         ? ORANGE :
                       (panel.modifierName == "Heat Wave")       ? Color{255, 160, 40, 255} :
                       (panel.modifierName == "Harvest Bounty") ? Fade(GREEN, 0.9f) :
                       (panel.modifierName == "Festival")       ? Fade(GOLD, 0.9f) :
                                                                   YELLOW;
        DrawText(modBuf, PX + 8, y, 12, modCol);
        y += LINE_H;
    }

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
        auto prodIt  = panel.prodRatePerHour.find(type);
        auto consIt  = panel.consRatePerHour.find(type);
        char buf[80];

        float netRate  = (netIt  != panel.netRatePerHour.end()) ? netIt->second  : 0.f;
        float prodRate = (prodIt != panel.prodRatePerHour.end()) ? prodIt->second : 0.f;
        float consRate = (consIt != panel.consRatePerHour.end()) ? consIt->second : 0.f;
        const char* netSign = (netRate >= 0.f) ? "+" : "";

        // First line: qty, price, net rate
        if (priceIt != panel.prices.end())
            std::snprintf(buf, sizeof(buf), "%s: %.0fu @%.2fg  net:%s%.1f/hr",
                          label, qty, priceIt->second, netSign, netRate);
        else
            std::snprintf(buf, sizeof(buf), "%s: %.0fu  net:%s%.1f/hr",
                          label, qty, netSign, netRate);

        Color netCol = (netRate >= 0.f) ? col : Fade(RED, 0.9f);
        DrawText(buf, PX + 8, y, 13, netCol);
        y += LINE_H - 3;

        // Second line: production vs consumption breakdown (smaller text)
        char buf2[80];
        std::snprintf(buf2, sizeof(buf2), "  prod:+%.1f  cons:-%.1f  /hr",
                      prodRate, consRate);
        Color prodCol = Fade(col, 0.7f);
        DrawText(buf2, PX + 8, y, 11, prodCol);
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
                           e.message.find("WINDFALL")   != std::string::npos ||
                           e.message.find("FESTIVAL")   != std::string::npos ||
                           e.message.find("reaches")    != std::string::npos ||
                           e.message.find("restored")   != std::string::npos ||
                           e.message.find("CONVOY")     != std::string::npos ||
                           e.message.find("BOOM")       != std::string::npos ||
                           e.message.find("RAIN")       != std::string::npos ||
                           e.message.find("funded road") != std::string::npos ||
                           e.message.find("built a new") != std::string::npos ||
                           e.message.find("became a hauler") != std::string::npos ||
                           e.message.find("IMMIGRANT")  != std::string::npos);
            bool isBad  = (e.message.find("COLLAPSED")  != std::string::npos ||
                           e.message.find("died")       != std::string::npos ||
                           e.message.find("DISEASE")    != std::string::npos ||
                           e.message.find("PLAGUE")     != std::string::npos ||
                           e.message.find("FIRE")       != std::string::npos ||
                           e.message.find("EARTHQUAKE") != std::string::npos ||
                           e.message.find("DROUGHT")    != std::string::npos ||
                           e.message.find("BLIGHT")     != std::string::npos ||
                           e.message.find("EMPTY")      != std::string::npos ||
                           e.message.find("BLIZZARD")   != std::string::npos ||
                           e.message.find("stole")      != std::string::npos ||
                           e.message.find("BANDITS")    != std::string::npos ||
                           e.message.find("CRISIS")     != std::string::npos);
            Color ec = isGood ? Fade(GREEN, 0.8f) : isBad ? Fade(RED, 0.8f) : Fade(LIGHTGRAY, 0.7f);
            DrawText(ebuf, PX + 8, y, 11, ec);
            y += LINE_H - 3;
        }
    }

    // Residents list — NPCs homed at this settlement, richest highlighted in gold
    if (!panel.residents.empty()) {
        y += 2;
        char resBuf[32];
        std::snprintf(resBuf, sizeof(resBuf), "Residents (%d):", (int)panel.residents.size());
        DrawText(resBuf, PX + 8, y, 11, Fade(YELLOW, 0.7f));
        y += LINE_H - 2;
        for (const auto& r : panel.residents) {
            // First entry is richest (sorted descending); highlight in gold
            Color rc = (&r == &panel.residents.front()) ? GOLD : Fade(LIGHTGRAY, 0.85f);

            // Map full profession string to 2-letter abbreviation
            const char* abbr = nullptr;
            if      (r.profession == "Farmer")       abbr = "Fa";
            else if (r.profession == "Water Carrier") abbr = "Wa";
            else if (r.profession == "Woodcutter")   abbr = "Lu";
            else if (r.profession == "Merchant")     abbr = "Me";

            // Draw: "  Name" in NPC colour, then " [Fa]" in grey, then "  45g" in NPC colour
            char nameBuf[48], goldBuf[16];
            std::snprintf(nameBuf, sizeof(nameBuf), "  %s", r.name.c_str());
            std::snprintf(goldBuf, sizeof(goldBuf), "  %.0fg", r.balance);

            int rx = PX + 8;
            DrawText(nameBuf, rx, y, 11, rc);
            rx += MeasureText(nameBuf, 11);

            if (abbr) {
                char abbrBuf[8];
                std::snprintf(abbrBuf, sizeof(abbrBuf), " [%s]", abbr);
                DrawText(abbrBuf, rx, y, 11, Fade(GRAY, 0.75f));
                rx += MeasureText(abbrBuf, 11);
            }

            DrawText(goldBuf, rx, y, 11, rc);
            y += LINE_H - 3;
        }
    }

    // Population sparkline — mini chart of historical population
    if (!panel.popHistory.empty()) {
        y += 4;
        DrawText("Pop history:", PX + 8, y, 10, Fade(LIGHTGRAY, 0.6f));
        y += 12;

        // Find min/max for scaling
        int pMin = panel.popHistory[0], pMax = panel.popHistory[0];
        for (int v : panel.popHistory) {
            if (v < pMin) pMin = v;
            if (v > pMax) pMax = v;
        }
        int range = std::max(1, pMax - pMin);

        static const int CHART_H = 24;
        static const int CHART_W = PW - 20;
        int n = (int)panel.popHistory.size();

        // Draw background
        DrawRectangle(PX + 8, y, CHART_W, CHART_H, Fade(WHITE, 0.05f));

        // Draw bars
        float barW = (float)CHART_W / n;
        for (int i = 0; i < n; ++i) {
            float frac = (float)(panel.popHistory[i] - pMin) / range;
            int bh    = std::max(1, (int)(frac * CHART_H));
            int bx    = PX + 8 + (int)(i * barW);
            int bw    = std::max(1, (int)barW - 1);
            // Color by trend: newest bar is brightest, oldest faded
            float alpha = 0.4f + 0.6f * ((float)(i + 1) / n);
            Color barCol = (panel.popHistory[i] == pMax) ? Fade(GOLD, alpha) :
                           (panel.popHistory[i] > (pMin + pMax) / 2) ? Fade(GREEN, alpha) :
                           Fade(RED, alpha);
            DrawRectangle(bx, y + CHART_H - bh, bw, bh, barCol);
        }
        // Min/max labels
        char minBuf[8], maxBuf[8];
        std::snprintf(minBuf, sizeof(minBuf), "%d", pMin);
        std::snprintf(maxBuf, sizeof(maxBuf), "%d", pMax);
        DrawText(minBuf, PX + 8 + CHART_W + 3, y + CHART_H - 8, 9, Fade(LIGHTGRAY, 0.6f));
        DrawText(maxBuf, PX + 8 + CHART_W + 3, y,                9, Fade(LIGHTGRAY, 0.6f));
        y += CHART_H + 4;
    }
}
