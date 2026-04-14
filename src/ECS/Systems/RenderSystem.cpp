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
        if (type != RES_SHELTER) resLines += 2;
    int eventLines  = (int)panel.recentEvents.size();
    int sparklineH  = panel.popHistory.empty() ? 0 : (12 + 24 + 8);  // label + chart + gap
    bool hasSpecialty = !panel.specialty.empty();
    bool hasTheft    = (panel.theftCount > 0);
    bool hasStruggling = (panel.strugglingHaulers > 0);
    bool hasSkillSummary = false;
    for (int i = 0; i < (int)panel.masterCount.size(); ++i)
        if (panel.masterCount[i] + panel.journeymanCount[i] > 0) { hasSkillSummary = true; break; }
    // Pre-compute largest family for header line and height calc
    std::string largestFamName;
    int largestFamCount = 0;
    {
        std::map<std::string, int> famCount;
        for (const auto& r : panel.residents)
            if (!r.familyName.empty())
                famCount[r.familyName]++;
        for (const auto& [name, cnt] : famCount)
            if (cnt > largestFamCount) { largestFamCount = cnt; largestFamName = name; }
    }
    bool hasLargestFamily = (largestFamCount >= 2);
    bool hasFriendships = (panel.friendshipPairs > 0);
    int haulerRouteH = (int)panel.haulerRoutes.size() * (LINE_H - 3);
    int totalLines  = 1 + 2 + resLines + (eventLines > 0 ? 1 + eventLines : 0)
                      + (hasSpecialty ? 1 : 0) + (hasTheft ? 1 : 0)
                      + (hasStruggling ? 1 : 0) + (hasSkillSummary ? 1 : 0)
                      + (hasLargestFamily ? 1 : 0)
                      + (hasFriendships ? 1 : 0);
    int residentH   = panel.residents.empty() ? 0
                        : 2 + (LINE_H - 2) + 2*(LINE_H - 3) + (int)panel.residents.size() * (LINE_H - 3);
    int barChartH   = 4 + 3 * (6 + 3);  // stockpile bar chart (3 bars + gaps)
    int ph          = totalLines * LINE_H + 14 + barChartH + sparklineH + residentH + haulerRouteH;

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

    // Specialty label (if present)
    if (hasSpecialty) {
        char specBuf[48];
        std::snprintf(specBuf, sizeof(specBuf), "Specialty: %s", panel.specialty.c_str());
        DrawText(specBuf, PX + 8, y, 11, Fade(LIGHTGRAY, 0.6f));
        y += LINE_H;
    }

    // Treasury + workers line
    char tresBuf[80];
    std::snprintf(tresBuf, sizeof(tresBuf), "Treasury: %.0fg   Working: %d / Idle: %d",
                  panel.treasury, panel.workers, panel.idle);
    Color tresCol = (panel.treasury < 50.f) ? RED : (panel.treasury < 150.f) ? ORANGE : GOLD;
    DrawText(tresBuf, PX + 8, y, 13, tresCol);
    y += LINE_H;

    // Largest family (only shown when a family has ≥2 members at this settlement)
    if (hasLargestFamily) {
        char famBuf[64];
        std::snprintf(famBuf, sizeof(famBuf), "Largest family: %s \xc3\x97%d",
                      largestFamName.c_str(), largestFamCount);
        DrawText(famBuf, PX + 8, y, 11, Fade(LIGHTGRAY, 0.6f));
        y += LINE_H;
    }

    // Bounty pool (only shown when > 0)
    if (panel.bountyPool > 0.f) {
        char bountyBuf[48];
        std::snprintf(bountyBuf, sizeof(bountyBuf), "Bounty: %.0fg", panel.bountyPool);
        DrawText(bountyBuf, PX + 8, y, 11, Fade(GOLD, 0.7f));
        y += LINE_H;
    }

    // Theft count (only shown when > 0)
    if (panel.theftCount > 0) {
        char theftBuf[32];
        std::snprintf(theftBuf, sizeof(theftBuf), "Thefts: %d", panel.theftCount);
        DrawText(theftBuf, PX + 8, y, 11, Fade(RED, 0.7f));
        y += LINE_H;
    }

    // Morale bar
    {
        int labelW = MeasureText("Morale", 11);
        DrawText("Morale", PX + 8, y + 0, 11, Fade(LIGHTGRAY, 0.7f));
        int barX = PX + 8 + labelW + 6;
        int barW = 100, barH = 8;
        float fill = std::max(0.f, std::min(1.f, panel.morale));
        Color barCol = (fill >= 0.7f) ? GREEN : (fill >= 0.3f) ? YELLOW : RED;
        DrawRectangle(barX, y + 1, barW, barH, Fade(DARKGRAY, 0.5f));
        DrawRectangle(barX, y + 1, (int)(barW * fill), barH, barCol);
        char pctBuf[8];
        std::snprintf(pctBuf, sizeof(pctBuf), "%d%%", (int)(fill * 100));
        DrawText(pctBuf, barX + barW + 4, y + 0, 11, barCol);
        y += LINE_H;
    }

    // Friendship pairs
    if (panel.friendshipPairs > 0) {
        char fbuf[32];
        std::snprintf(fbuf, sizeof(fbuf), "%d friendship%s",
                      panel.friendshipPairs, panel.friendshipPairs == 1 ? "" : "s");
        DrawText(fbuf, PX + 8, y, 11, Fade(LIME, 0.6f));
        y += LINE_H;
    }

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
            case RES_FOOD:    label = "Food";    col = GREEN;   break;
            case RES_WATER:   label = "Water";   col = SKYBLUE; break;
            case RES_WOOD:    label = "Wood";    col = BROWN;   break;
            case RES_SHELTER: continue;  // not shown in stockpile
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

    // Stockpile bar chart — visual at-a-glance resource levels
    {
        static constexpr float BAR_MAX_QTY = 200.f;
        static constexpr int   BAR_MAX_W   = 100;
        static constexpr int   BAR_H       = 6;
        static constexpr int   BAR_GAP     = 3;
        struct BarDef { int type; const char* label; Color col; };
        BarDef bars[] = {
            { RES_FOOD,  "F", GREEN },
            { RES_WATER, "W", SKYBLUE },
            { RES_WOOD,  "Wd", BROWN },
        };
        y += 2;
        int labelX = PX + 8;
        int barX   = PX + 28;
        for (const auto& b : bars) {
            auto it = panel.quantities.find(b.type);
            float qty = (it != panel.quantities.end()) ? it->second : 0.f;
            int fillW = (int)(std::min(qty / BAR_MAX_QTY, 1.f) * BAR_MAX_W);
            DrawText(b.label, labelX, y, 10, Fade(b.col, 0.7f));
            DrawRectangle(barX, y + 1, BAR_MAX_W, BAR_H, Fade(DARKGRAY, 0.4f));
            DrawRectangle(barX, y + 1, fillW, BAR_H, Fade(b.col, 0.6f));
            y += BAR_H + BAR_GAP;
        }
        y += 2;
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
    int residentsHeaderY = -1;  // track for hover tooltip
    if (!panel.residents.empty()) {
        y += 2;
        residentsHeaderY = y;
        char resBuf[32];
        std::snprintf(resBuf, sizeof(resBuf), "Residents (%d):", (int)panel.residents.size());
        DrawText(resBuf, PX + 8, y, 11, Fade(YELLOW, 0.7f));
        y += LINE_H - 2;

        // Profession distribution: compact "Fa:4 Wa:3 Lu:2" line
        {
            int nFa = 0, nWa = 0, nLu = 0, nOther = 0;
            for (const auto& r : panel.residents) {
                if      (r.profession == "Farmer")       ++nFa;
                else if (r.profession == "Water Carrier") ++nWa;
                else if (r.profession == "Woodcutter")   ++nLu;
                else                                      ++nOther;
            }
            char profBuf[64];
            int pos = 0;
            if (nFa > 0)    pos += std::snprintf(profBuf + pos, sizeof(profBuf) - pos, "Fa:%d ", nFa);
            if (nWa > 0)    pos += std::snprintf(profBuf + pos, sizeof(profBuf) - pos, "Wa:%d ", nWa);
            if (nLu > 0)    pos += std::snprintf(profBuf + pos, sizeof(profBuf) - pos, "Lu:%d ", nLu);
            if (nOther > 0) pos += std::snprintf(profBuf + pos, sizeof(profBuf) - pos, "?:%d",   nOther);
            if (pos > 0) {
                DrawText(profBuf, PX + 8, y, 10, Fade(LIGHTGRAY, 0.55f));
                y += LINE_H - 3;
            }
        }

        // Family dynasty line: count surnames appearing 2+ times
        {
            std::map<std::string, int> dynCount;
            for (const auto& r : panel.residents)
                if (!r.familyName.empty())
                    dynCount[r.familyName]++;
            int dynasties = 0;
            for (const auto& [name, cnt] : dynCount)
                if (cnt >= 2) ++dynasties;
            char dynBuf[48];
            if (dynasties > 0)
                std::snprintf(dynBuf, sizeof(dynBuf), "Families: %d dynast%s",
                              dynasties, dynasties == 1 ? "y" : "ies");
            else
                std::snprintf(dynBuf, sizeof(dynBuf), "No established families");
            DrawText(dynBuf, PX + 8, y, 10, Fade(LIGHTGRAY, 0.5f));
            y += LINE_H - 3;
        }

        // Count family members and sum wealth for the " ×N (total Xg)" suffix
        std::map<std::string, int> familyCount;
        std::map<std::string, float> familyWealth;
        for (const auto& r : panel.residents) {
            if (!r.familyName.empty()) {
                familyCount[r.familyName]++;
                familyWealth[r.familyName] += r.balance;
            }
        }

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
                Color abbrCol = (r.profession == "Farmer")       ? Fade(GREEN, 0.6f)   :
                                (r.profession == "Water Carrier") ? Fade(SKYBLUE, 0.6f) :
                                (r.profession == "Woodcutter")   ? Fade(BROWN, 0.7f)   :
                                (r.profession == "Merchant")     ? Fade(GOLD, 0.5f)    :
                                                                   Fade(GRAY, 0.75f);
                DrawText(abbrBuf, rx, y, 11, abbrCol);
                rx += MeasureText(abbrBuf, 11);
            }

            DrawText(goldBuf, rx, y, 11, rc);
            rx += MeasureText(goldBuf, 11);

            // Append " ×N (total Xg)" family count + wealth when 2+ members present
            if (!r.familyName.empty()) {
                auto it = familyCount.find(r.familyName);
                if (it != familyCount.end() && it->second >= 2) {
                    char famBuf[8];
                    std::snprintf(famBuf, sizeof(famBuf), " \xc3\x97%d", it->second);
                    DrawText(famBuf, rx, y, 11, Fade(DARKGRAY, 0.85f));
                    rx += MeasureText(famBuf, 11);

                    auto wit = familyWealth.find(r.familyName);
                    if (wit != familyWealth.end()) {
                        char wBuf[16];
                        std::snprintf(wBuf, sizeof(wBuf), " (%.0fg)", wit->second);
                        DrawText(wBuf, rx, y, 11, Fade(GOLD, 0.6f));
                        rx += MeasureText(wBuf, 11);
                    }
                }
            }

            // Eldest resident badge — settlement patriarch/matriarch
            if (r.isEldest) {
                DrawText(" [Elder]", rx, y, 11, Fade(ORANGE, 0.8f));
                rx += MeasureText(" [Elder]", 11);
            }

            // Mood dot: green (happy), yellow (neutral), red (struggling)
            {
                Color moodCol = (r.contentment > 0.7f) ? GREEN :
                                (r.contentment > 0.4f) ? YELLOW : RED;
                DrawCircleV({ (float)(rx + 6), (float)(y + 5) }, 3.f, Fade(moodCol, 0.7f));
            }

            y += LINE_H - 3;
        }

        // Wealth tooltip: when hovering the "Residents (N):" header, show richest & poorest
        if (residentsHeaderY >= 0 && panel.residents.size() >= 2) {
            Vector2 mouse = GetMousePosition();
            if (mouse.x >= PX && mouse.x <= PX + PW
                && mouse.y >= residentsHeaderY && mouse.y < residentsHeaderY + (LINE_H - 2)) {
                const auto& richest = panel.residents.front();
                const auto& poorest = panel.residents.back();
                char tipA[64], tipB[64];
                std::snprintf(tipA, sizeof(tipA), "Richest: %s  %.0fg",
                              richest.name.c_str(), richest.balance);
                std::snprintf(tipB, sizeof(tipB), "Poorest: %s  %.0fg",
                              poorest.name.c_str(), poorest.balance);
                int tw = std::max(MeasureText(tipA, 11), MeasureText(tipB, 11)) + 12;
                int tx = PX + PW + 4;
                int ty = residentsHeaderY;
                DrawRectangle(tx, ty, tw, 28, Fade(BLACK, 0.85f));
                DrawRectangleLines(tx, ty, tw, 28, Fade(LIGHTGRAY, 0.5f));
                DrawText(tipA, tx + 4, ty + 2,  11, GOLD);
                DrawText(tipB, tx + 4, ty + 15, 11, Fade(LIGHTGRAY, 0.8f));
            }
        }
    }

    // Struggling haulers warning
    if (panel.strugglingHaulers > 0) {
        char shBuf[48];
        std::snprintf(shBuf, sizeof(shBuf), "Struggling haulers: %d", panel.strugglingHaulers);
        DrawText(shBuf, PX + 8, y, 11, Fade(RED, 0.7f));
        y += LINE_H;
    }

    // Hauler routes — up to 3 haulers homed at this settlement
    for (const auto& hi : panel.haulerRoutes) {
        char hrBuf[80];
        std::snprintf(hrBuf, sizeof(hrBuf), "  %s: %s", hi.name.c_str(), hi.route.c_str());
        Color hrCol = hi.struggling ? Fade(RED, 0.7f) : Fade(SKYBLUE, 0.7f);
        DrawText(hrBuf, PX + 8, y, 11, hrCol);
        // Append lifetime profit after route text
        if (hi.lifetimeProfit != 0.f) {
            int routeW = MeasureText(hrBuf, 11);
            char profBuf[32];
            if (hi.lifetimeProfit > 0.f)
                std::snprintf(profBuf, sizeof(profBuf), " (+%.0fg)", hi.lifetimeProfit);
            else
                std::snprintf(profBuf, sizeof(profBuf), " (%.0fg)", hi.lifetimeProfit);
            Color profCol = (hi.lifetimeProfit > 0.f) ? Fade(GREEN, 0.8f) : Fade(RED, 0.8f);
            DrawText(profBuf, PX + 8 + routeW, y, 11, profCol);
        }
        y += LINE_H - 3;
    }

    // Settlement skill summary — top skill type with master/journeyman counts
    {
        int bestIdx = -1, bestTotal = 0;
        for (int i = 0; i < (int)panel.masterCount.size(); ++i) {
            int total = panel.masterCount[i] + panel.journeymanCount[i];
            if (total > bestTotal) { bestTotal = total; bestIdx = i; }
        }
        if (bestIdx >= 0) {
            std::string skName = (bestIdx < (int)panel.skillNames.size() && !panel.skillNames[bestIdx].empty())
                ? panel.skillNames[bestIdx] : "Unknown";
            char skBuf[80];
            std::snprintf(skBuf, sizeof(skBuf), "Top skill: %s (%d master%s, %d journeyman%s)",
                skName.c_str(),
                panel.masterCount[bestIdx], panel.masterCount[bestIdx] == 1 ? "" : "s",
                panel.journeymanCount[bestIdx], panel.journeymanCount[bestIdx] == 1 ? "" : "s");
            DrawText(skBuf, PX + 8, y, 11, Fade(GOLD, 0.7f));
            y += LINE_H;
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

        // Draw line graph: GREEN for growth segments, RED for decline
        float segW = (n > 1) ? (float)CHART_W / (n - 1) : 0.f;
        for (int i = 0; i < n - 1; ++i) {
            float f0 = (float)(panel.popHistory[i]     - pMin) / range;
            float f1 = (float)(panel.popHistory[i + 1] - pMin) / range;
            float x0 = PX + 8 + i * segW;
            float y0 = y + CHART_H - f0 * CHART_H;
            float x1 = PX + 8 + (i + 1) * segW;
            float y1 = y + CHART_H - f1 * CHART_H;
            Color segCol = (panel.popHistory[i + 1] >= panel.popHistory[i])
                         ? Fade(GREEN, 0.8f) : Fade(RED, 0.8f);
            DrawLineEx({ x0, y0 }, { x1, y1 }, 2.f, segCol);
        }
        // Draw dots at each data point
        for (int i = 0; i < n; ++i) {
            float frac = (float)(panel.popHistory[i] - pMin) / range;
            float px = PX + 8 + i * segW;
            float py = y + CHART_H - frac * CHART_H;
            DrawCircleV({ px, py }, 2.f, Fade(WHITE, 0.6f));
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
