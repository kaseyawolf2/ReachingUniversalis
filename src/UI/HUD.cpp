#include "HUD.h"
#include "ECS/Components.h"
#include "raylib.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <map>
#include <string>

static const int SCREEN_W = 1280;
static const int SCREEN_H = 720;
static const int BAR_W    = 160;
static const int BAR_H    = 16;
static const int BAR_X    = 10;
static const int BAR_Y0   = 10;
static const int BAR_GAP  = 26;

static const std::vector<std::string> emptyNames;

static const char* BehaviorLabel(AgentBehavior b) {
    switch (b) {
        case AgentBehavior::Idle:         return "Idle";
        case AgentBehavior::SeekingFood:  return "SeekingFood";
        case AgentBehavior::SeekingWater: return "SeekingWater";
        case AgentBehavior::SeekingSleep: return "SeekingSleep";
        case AgentBehavior::Satisfying:   return "Satisfying";
        case AgentBehavior::Migrating:    return "Migrating";
        case AgentBehavior::Sleeping:     return "Sleeping";
        case AgentBehavior::Working:      return "Working";
        case AgentBehavior::Celebrating:  return "Celebrating";
    }
    return "Unknown";
}

// Maps a modifier/event name to a display colour.
// Plague → RED, Drought → ORANGE, Festival → GOLD, Harvest Bounty → GREEN,
// Heat Wave → warm orange, others → YELLOW.
static Color ModifierColour(const std::string& name) {
    if (name == "Plague")          return RED;
    if (name == "Drought")         return ORANGE;
    if (name == "Festival")        return Fade(GOLD, 0.95f);
    if (name == "Harvest Bounty")  return Fade(GREEN, 0.90f);
    if (name == "Heat Wave")       return Color{255, 160, 40, 255};
    return YELLOW;
}

// ---- HandleInput ----

void HUD::HandleInput(const RenderSnapshot& /*snapshot*/) {
    if (IsKeyPressed(KEY_F1)) debugOverlay = !debugOverlay;
    if (IsKeyPressed(KEY_M))  marketOverlay = !marketOverlay;
    float wheel = GetMouseWheelMove();
    if (wheel != 0.f) {
        logScroll -= (int)wheel;
        if (logScroll < 0) logScroll = 0;
    }
}

// ---- Draw ----

void HUD::Draw(const RenderSnapshot& snap, const Camera2D& camera, bool roadBuildMode) {
    // Take a local copy of the parts we need — snapshot may be updated by sim
    // thread mid-draw if we read directly, so copy once under lock.
    // The lock is already released by GameState::Draw before calling us;
    // we received a const reference to the snapshot and GameState holds no lock
    // here. GameState copies the vectors under lock; HUD reads the scalars.
    // For the scalar HUD fields we do a quick lock here.
    int   day, hour, minute, tickSpeed, speedIndex, pop, deaths;
    bool  paused, roadBlocked, playerAlive;
    float playerAgeDays, playerMaxDays, playerGold;
    std::vector<float> playerSkills;
    std::shared_ptr<const std::vector<std::string>> playerSkillNamesPtr;
    std::shared_ptr<const std::vector<std::string>> needNamesPtr;
    std::shared_ptr<const std::vector<std::string>> resourceNamesPtr;
    std::vector<std::pair<float,float>> playerNeeds;   // (value, critThreshold) per NeedID
    float temperature;
    bool  playerInPlagueZone;
    int   playerReputation;
    std::string playerRank;
    std::map<int, int> playerInventory;
    int         playerInventoryCapacity = 15;
    std::string tradeHint;
    AgentBehavior behavior;
    std::string seasonName;
    std::string seasonRegime;
    float seasonHeatDrainMod = 0.f;
    float seasonProductionMod = 1.f;
    std::vector<RenderSnapshot::TradeRecord> tradeLedger;

    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        day         = snap.day;
        hour        = snap.hour;
        minute      = snap.minute;
        tickSpeed   = snap.tickSpeed;
        speedIndex  = snap.speedIndex;
        pop         = snap.population;
        deaths      = snap.totalDeaths;
        paused      = snap.paused;
        roadBlocked = snap.roadBlocked;
        playerAlive = snap.playerAlive;
        playerNeeds = snap.playerNeeds;
        behavior    = snap.playerBehavior;
        playerAgeDays   = snap.playerAgeDays;
        playerMaxDays   = snap.playerMaxDays;
        playerGold      = snap.playerGold;
        playerSkills         = snap.playerSkills;
        playerSkillNamesPtr  = snap.skillNames;
        needNamesPtr         = snap.needNames;
        resourceNamesPtr     = snap.resourceNames;
        playerInventory         = snap.playerInventory;
        playerInventoryCapacity = snap.playerInventoryCapacity;
        tradeHint            = snap.tradeHint;
        tradeLedger          = snap.tradeLedger;
        seasonName           = snap.seasonName;
        seasonRegime         = snap.seasonRegime;
        seasonHeatDrainMod   = snap.seasonHeatDrainMod;
        seasonProductionMod  = snap.seasonProductionMod;
        temperature          = snap.temperature;
        playerInPlagueZone   = snap.playerInPlagueZone;
        playerReputation     = snap.playerReputation;
        playerRank           = snap.playerRank;
    }

    // Dereference shared name tables once; empty fallback if not yet set.
    const auto& playerSkillNames = playerSkillNamesPtr ? *playerSkillNamesPtr : emptyNames;
    const auto& needNames        = needNamesPtr        ? *needNamesPtr        : emptyNames;
    const auto& resourceNames    = resourceNamesPtr    ? *resourceNamesPtr    : emptyNames;

    // Need bar color palette: indexed by need position (0=food/green, 1=water/cyan,
    // 2=energy/yellow, 3=heat/orange, further needs cycle back).
    static const Color kNeedColors[] = { GREEN, SKYBLUE, YELLOW, ORANGE, PURPLE, PINK };
    static constexpr int kNeedColorCount = (int)(sizeof(kNeedColors)/sizeof(kNeedColors[0]));

    // ---- Player panel (top-left) ----
    if (playerAlive) {
        int numNeeds     = (int)playerNeeds.size();
        int invItemLines = std::max(1, (int)playerInventory.size()); // at least 1 for "(empty)"
        int skillLine    = !playerSkills.empty() ? 1 : 0;
        int tradeLines   = tradeHint.empty() ? 0 : 1;
        int ledgerLines  = tradeLedger.empty() ? 0 : (1 + std::min((int)tradeLedger.size(), 4));
        int plagueLines  = playerInPlagueZone ? 1 : 0;
        int panelH = BAR_GAP * (3 + numNeeds + skillLine) + 90 + (1 + invItemLines) * 16 + tradeLines * 14 + ledgerLines * 11 + plagueLines * 14 + 4;
        // Panel background with border
        DrawRectangle(4, 4, 320, panelH, Fade(BLACK, 0.6f));
        DrawRectangleLines(4, 4, 320, panelH, Fade(LIGHTGRAY, 0.25f));
        // Top accent line
        DrawRectangle(4, 4, 320, 2, Fade(GOLD, 0.4f));

        // Need bars — driven by schema need names and values
        for (int ni = 0; ni < numNeeds; ++ni) {
            const char* label = (ni < (int)needNames.size() && !needNames[ni].empty())
                                ? needNames[ni].c_str() : "Need";
            Color barCol = kNeedColors[ni % kNeedColorCount];
            DrawNeedBar(BAR_X, BAR_Y0 + BAR_GAP * ni,
                        playerNeeds[ni].first, playerNeeds[ni].second,
                        label, barCol);
        }

        // Separator after need bars
        int sepY = BAR_Y0 + BAR_GAP * numNeeds - 4;
        DrawRectangle(BAR_X, sepY, 300, 1, Fade(LIGHTGRAY, 0.15f));

        // Age row
        int ageY = BAR_Y0 + BAR_GAP * numNeeds + 2;
        float ageFrac = (playerMaxDays > 0.f) ? std::min(1.f, playerAgeDays / playerMaxDays) : 0.f;
        Color ageCol  = (ageFrac < 0.6f) ? LIGHTGRAY :
                        (ageFrac < 0.85f) ? YELLOW : RED;
        char ageBuf[32];
        std::snprintf(ageBuf, sizeof(ageBuf), "Age: %.0f / %.0f", playerAgeDays, playerMaxDays);
        DrawText("Age:", BAR_X, ageY, 14, LIGHTGRAY);
        DrawText(ageBuf + 5, BAR_X + 52, ageY, 14, ageCol);  // skip "Age:" prefix

        int stateY = ageY + BAR_GAP;
        DrawText("State:", BAR_X, stateY, 14, LIGHTGRAY);
        DrawText(BehaviorLabel(behavior), BAR_X + 52, stateY, 14, WHITE);

        int roadY = stateY + 20;
        DrawText(roadBlocked ? "!! ROAD BLOCKED !!" : "Road: open",
                 BAR_X, roadY, 13, roadBlocked ? RED : Fade(GREEN, 0.8f));
        if (playerInPlagueZone) {
            DrawText("!! PLAGUE ZONE !! needs drain 1.5x",
                     BAR_X, roadY + 14, 11, Color{200, 80, 240, 255});
        }

        // Gold balance
        int goldY = roadY + (playerInPlagueZone ? 30 : 18);
        char goldBuf[32];
        std::snprintf(goldBuf, sizeof(goldBuf), "%.1fg", playerGold);
        DrawText("Gold:", BAR_X, goldY, 14, LIGHTGRAY);
        DrawText(goldBuf, BAR_X + 52, goldY, 14, YELLOW);

        // Reputation + rank
        int repY = goldY + BAR_GAP;
        char repBuf[40];
        std::snprintf(repBuf, sizeof(repBuf), "%d  [%s]",
                      playerReputation, playerRank.c_str());
        DrawText("Rep:", BAR_X, repY, 13, LIGHTGRAY);
        Color repCol = (playerReputation >= 100) ? GOLD    :
                       (playerReputation >=  50) ? GREEN   :
                       (playerReputation >=  20) ? SKYBLUE : Fade(LIGHTGRAY, 0.7f);
        DrawText(repBuf, BAR_X + 52, repY, 13, repCol);

        // Skills (shown only when player has skills component)
        int skillsEndY = repY + BAR_GAP;
        if (!playerSkills.empty()) {
            auto skCol = [](float s) -> Color {
                return (s >= 0.65f) ? Fade(GOLD, 0.9f) : (s >= 0.35f) ? GREEN : Fade(GRAY, 0.8f);
            };
            // Build a compact skill summary string
            std::string skStr;
            float bestSk = 0.f;
            for (int si = 0; si < (int)playerSkills.size(); ++si) {
                if (si > 0) skStr += " ";
                // Use a short label (first 4 chars of display name)
                std::string label = (si < (int)playerSkillNames.size() && !playerSkillNames[si].empty())
                    ? playerSkillNames[si].substr(0, 4) : "Sk" + std::to_string(si);
                char tmp[24];
                std::snprintf(tmp, sizeof(tmp), "%s:%.0f%%", label.c_str(), playerSkills[si] * 100.f);
                skStr += tmp;
                if (playerSkills[si] > bestSk) bestSk = playerSkills[si];
            }
            DrawText("Skills:", BAR_X, skillsEndY, 11, LIGHTGRAY);
            DrawText(skStr.c_str(), BAR_X + 52, skillsEndY, 11, skCol(bestSk));
            skillsEndY += BAR_GAP;
        }

        // Separator before inventory
        DrawRectangle(BAR_X, skillsEndY - 4, 300, 1, Fade(LIGHTGRAY, 0.15f));

        // Inventory lines
        int invY = skillsEndY;
        {
            int totalCarried = 0;
            for (const auto& [t, q] : playerInventory) totalCarried += q;
            bool full = (totalCarried >= playerInventoryCapacity);
            char cargoHdr[32];
            std::snprintf(cargoHdr, sizeof(cargoHdr), "%d/%d", totalCarried, playerInventoryCapacity);
            DrawText("Cargo:", BAR_X, invY, 13, LIGHTGRAY);
            DrawText(cargoHdr, BAR_X + 52, invY, 13, full ? RED : LIGHTGRAY);
            invY += 16;

            // Resource color palette: same indices as need colors but semantic:
            // RES_FOOD=0 green, RES_WATER=1 skyblue, RES_SHELTER=2 gray, RES_WOOD=3 brown
            static const Color kResColors[] = { GREEN, SKYBLUE, LIGHTGRAY, BROWN };
            static constexpr int kResColorCount = (int)(sizeof(kResColors)/sizeof(kResColors[0]));
            int ci = 0;
            for (const auto& [type, qty] : playerInventory) {
                if (qty <= 0) continue;
                const char* rname = (type >= 0 && type < (int)resourceNames.size() && !resourceNames[type].empty())
                                    ? resourceNames[type].c_str() : "?";
                Color rcol = (type >= 0 && type < kResColorCount) ? kResColors[type] : LIGHTGRAY;
                char cbuf[32];
                std::snprintf(cbuf, sizeof(cbuf), "%s x%d", rname, qty);
                DrawText(cbuf, BAR_X + 10, invY + ci * 16, 12, rcol);
                ++ci;
            }
            if (ci == 0) {
                DrawText("(empty)", BAR_X + 10, invY, 12, Fade(LIGHTGRAY, 0.5f));
                ++ci;
            }
            invY += ci * 16;
        }

        // Trade opportunity hint
        if (!tradeHint.empty()) {
            DrawText(tradeHint.c_str(), BAR_X, invY + 2, 10, Fade(GOLD, 0.85f));
            invY += 14;
        }

        // Trade ledger (last 4 trades)
        if (!tradeLedger.empty()) {
            DrawText("Ledger:", BAR_X, invY + 2, 10, Fade(LIGHTGRAY, 0.55f));
            invY += 12;
            int shown = std::min((int)tradeLedger.size(), 4);
            for (int i = 0; i < shown; ++i) {
                Color tc = tradeLedger[i].profit >= 0.f ? Fade(GREEN, 0.75f) : Fade(RED, 0.75f);
                DrawText(tradeLedger[i].description.c_str(), BAR_X + 4, invY, 9, tc);
                invY += 11;
            }
            invY += 2;
        }

        // Road build mode banner
        if (roadBuildMode) {
            DrawRectangle(BAR_X - 2, invY + 2, 320, 14, Fade(ORANGE, 0.25f));
            DrawText("ROAD BUILD — walk to destination, press N to connect (ESC cancel)",
                     BAR_X, invY + 4, 8, Fade(ORANGE, 0.95f));
            invY += 16;
        }

        DrawText("WASD:Move  E:Work  Q:Buy  C:Build  R:Repair  N:Road  V:Cart  P:Found  Z:Sleep  H:Settle  T:Trade",
                 BAR_X, invY + 4, 8, Fade(LIGHTGRAY, 0.6f));
    }

    // ---- Time panel (top-right) ----
    {
        char timeBuf[64], speedBuf[16], popBuf[32], fpsBuf[32], seasBuf[48];
        std::snprintf(timeBuf,  sizeof(timeBuf),  "Day %d  %02d:%02d", day, hour, minute);
        // Paradox-style speed labels
        static const char* SPEED_LABELS[] = {"", "Speed 1", "Speed 2", "Speed 3", "Speed 4", "Speed 5"};
        if (paused)
            std::snprintf(speedBuf, sizeof(speedBuf), "PAUSED");
        else if (speedIndex >= 1 && speedIndex <= 5)
            std::snprintf(speedBuf, sizeof(speedBuf), "%s%s", SPEED_LABELS[speedIndex],
                          speedIndex == 5 ? " [MAX]" : "");
        else
            std::snprintf(speedBuf, sizeof(speedBuf), "%dx", tickSpeed);
        std::snprintf(popBuf,   sizeof(popBuf),   "Pop: %d  Deaths: %d", pop, deaths);
        std::snprintf(fpsBuf,   sizeof(fpsBuf),   "FPS: %d  (%.1f ms)",
                      GetFPS(), GetFrameTime() * 1000.f);
        std::snprintf(seasBuf, sizeof(seasBuf), "%s  %.0f°C",
                      seasonName.c_str(), temperature);

        int pw = std::max({ MeasureText(timeBuf, 16), MeasureText(speedBuf, 14),
                            MeasureText(seasBuf, 13),
                            MeasureText(seasonRegime.c_str(), 11),
                            MeasureText(popBuf, 13),  MeasureText(fpsBuf, 12) }) + 16;
        int px = SCREEN_W - pw - 4;

        // Season color derived from heat drain: cold=blue, mild=green, warm=yellow, moderate cold=orange
        Color seasonColor = (seasonHeatDrainMod >= 0.8f) ? SKYBLUE :
                            (seasonHeatDrainMod >= 0.3f) ? ORANGE  :
                            (seasonHeatDrainMod > 0.05f) ? GREEN   : YELLOW;
        // Temperature color: blue below 0, grey near 0, normal above
        Color tempColor = (temperature < 0.f) ? Color{150,200,255,255} :
                          (temperature < 5.f) ? LIGHTGRAY : seasonColor;

        // Regime label color: matches season severity
        Color regimeColor = (seasonHeatDrainMod >= 0.8f) ? SKYBLUE :
                            (seasonHeatDrainMod >= 0.3f) ? ORANGE  :
                            (seasonHeatDrainMod > 0.05f) ? GREEN   :
                            (seasonProductionMod >= 1.1f) ? GOLD   :
                            (seasonProductionMod <= 0.5f) ? RED    : YELLOW;

        DrawRectangle(px, 4, pw, 112, Fade(BLACK, 0.6f));
        DrawRectangleLines(px, 4, pw, 112, Fade(LIGHTGRAY, 0.25f));
        DrawRectangle(px, 4, pw, 2, Fade(SKYBLUE, 0.4f));  // accent
        DrawText(timeBuf,  px + 8,  10, 16, WHITE);
        DrawText(speedBuf, px + 8, 30, 14, paused ? ORANGE : LIGHTGRAY);
        DrawRectangle(px + 4, 46, pw - 8, 1, Fade(LIGHTGRAY, 0.15f));  // separator
        DrawText(seasBuf,  px + 8, 50, 13, tempColor);
        DrawText(seasonRegime.c_str(), px + 8, 64, 11, regimeColor);
        DrawText(popBuf,   px + 8, 80, 13, LIGHTGRAY);
        DrawText(fpsBuf,   px + 8, 96, 12, Fade(LIGHTGRAY, 0.6f));
    }

    UpdateNotifications(snap);
    DrawWorldStatus(snap);
    DrawEventLog(snap);
    DrawHoverTooltip(snap, camera);
    DrawFacilityTooltip(snap, camera);
    DrawSettlementTooltip(snap, camera);
    DrawRoadTooltip(snap, camera);
    if (debugOverlay)  DrawDebugOverlay(snap);
    if (debugOverlay) {
        // Mood colour legend — bottom-right corner
        static const int LX = SCREEN_W - 170, LY = SCREEN_H - 70;
        DrawRectangle(LX - 4, LY - 4, 168, 64, Fade(BLACK, 0.7f));
        DrawCircleV({ (float)LX + 5, (float)LY + 7 },  5.f, GREEN);
        DrawText("Thriving (>70%)",  LX + 14, LY,      11, Fade(GREEN, 0.9f));
        DrawCircleV({ (float)LX + 5, (float)LY + 25 }, 5.f, YELLOW);
        DrawText("Stressed (40-70%)", LX + 14, LY + 18, 11, Fade(YELLOW, 0.9f));
        DrawCircleV({ (float)LX + 5, (float)LY + 43 }, 5.f, RED);
        DrawText("Suffering (<40%)", LX + 14, LY + 36, 11, Fade(RED, 0.9f));
    }
    if (marketOverlay) DrawMarketOverlay(snap);
    DrawMinimap(snap);
    DrawNotifications();
}

// ---- World status bar ----

// Helper: draw a tiny resource bar inline (width x height pixels)
static void DrawMiniBar(int x, int y, int w, int h, float value, float maxVal,
                        Color fill, Color bg, Color border) {
    float ratio = (maxVal > 0.f) ? std::min(1.f, value / maxVal) : 0.f;
    DrawRectangle(x, y, w, h, bg);
    int fillW = (int)(ratio * w);
    if (fillW > 0) DrawRectangle(x, y, fillW, h, fill);
    DrawRectangleLines(x, y, w, h, border);
}

void HUD::DrawWorldStatus(const RenderSnapshot& snap) const {
    std::vector<RenderSnapshot::SettlementStatus> ws;
    float snapHeatDrainMod = 0.f;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        ws     = snap.worldStatus;
        snapHeatDrainMod = snap.seasonHeatDrainMod;
    }
    if (ws.empty()) return;

    bool showWood = (snapHeatDrainMod > 0.2f);  // show wood in cold seasons

    // Morale trend tracking
    static std::map<std::string, float> s_prevMorale;
    static float s_moraleSampleTimer = 0.f;
    s_moraleSampleTimer += GetFrameTime();
    bool moraleSampleNow = (s_moraleSampleTimer >= 1.f);
    if (moraleSampleNow) s_moraleSampleTimer = 0.f;

    int count = std::min((int)ws.size(), 4);

    // Card layout: evenly spaced across top of screen
    static const int CARD_H   = 42;
    static const int CARD_PAD = 6;
    static const int TOP_Y    = 2;
    static const int NAME_FONT = 12;
    static const int DATA_FONT = 10;
    static const int MINI_BAR_W = 28;
    static const int MINI_BAR_H = 6;

    int totalGap  = (count - 1) * CARD_PAD;
    int cardW     = (SCREEN_W - 20 - totalGap) / count;  // 10px margin each side
    int startX    = (SCREEN_W - (cardW * count + totalGap)) / 2;

    for (int i = 0; i < count; ++i) {
        const auto& s = ws[i];
        int cx = startX + i * (cardW + CARD_PAD);

        // Card background
        Color cardBg = Fade(BLACK, 0.6f);
        Color cardBorder = Fade(LIGHTGRAY, 0.25f);
        if (s.hasEvent) cardBorder = Fade(ModifierColour(s.eventName), 0.6f);
        DrawRectangle(cx, TOP_Y, cardW, CARD_H, cardBg);
        DrawRectangleLines(cx, TOP_Y, cardW, CARD_H, cardBorder);

        int tx = cx + 4;
        int ty = TOP_Y + 3;

        // Row 1: Settlement name + pop + trend + event
        Color nameCol = s.hasEvent ? Color{255, 200, 80, 230} : WHITE;
        DrawText(s.name.c_str(), tx, ty, NAME_FONT, nameCol);
        int nameW = MeasureText(s.name.c_str(), NAME_FONT);

        // Population with trend
        char popBuf[32];
        char trendCh = (s.popTrend == '+') ? '+' : (s.popTrend == '-') ? '-' : ' ';
        if (s.childCount > 0)
            std::snprintf(popBuf, sizeof(popBuf), " %d%c (%dc)", s.pop, trendCh, s.childCount);
        else
            std::snprintf(popBuf, sizeof(popBuf), " %d%c", s.pop, trendCh);
        Color popCol = (s.popTrend == '+') ? Fade(GREEN, 0.8f) :
                       (s.popTrend == '-') ? Fade(RED, 0.8f) : Fade(LIGHTGRAY, 0.7f);
        DrawText(popBuf, tx + nameW, ty, DATA_FONT, popCol);

        // Haulers count (right side of row 1)
        if (s.haulers > 0) {
            char hBuf[16];
            std::snprintf(hBuf, sizeof(hBuf), "H:%d", s.haulers);
            int hW = MeasureText(hBuf, DATA_FONT);
            DrawText(hBuf, cx + cardW - hW - 4, ty, DATA_FONT, Fade(SKYBLUE, 0.7f));
        }

        // Event badge (right of haulers or right side)
        if (s.hasEvent && !s.eventName.empty()) {
            char evBuf[16];
            std::snprintf(evBuf, sizeof(evBuf), "!%-.8s", s.eventName.c_str());
            int evW = MeasureText(evBuf, 9);
            int evX = cx + cardW - evW - (s.haulers > 0 ? 30 : 4);
            DrawText(evBuf, evX, ty, 9, Fade(ModifierColour(s.eventName), 0.85f));
        }

        // Row 2: Resource mini-bars + gold + morale/contentment
        int ry = ty + 16;
        int rx = tx;

        // Food
        Color foodFill = (s.food < 10.f) ? RED : (s.food < 30.f) ? ORANGE : Fade(GREEN, 0.8f);
        DrawText("F", rx, ry, DATA_FONT, Fade(LIGHTGRAY, 0.6f));
        rx += 9;
        DrawMiniBar(rx, ry + 1, MINI_BAR_W, MINI_BAR_H, s.food, 150.f,
                    foodFill, Fade(WHITE, 0.08f), Fade(WHITE, 0.3f));
        if (s.hungerCrisis) DrawText("!", rx + MINI_BAR_W + 1, ry - 1, DATA_FONT, RED);
        rx += MINI_BAR_W + (s.hungerCrisis ? 10 : 4);

        // Water
        Color waterFill = (s.water < 10.f) ? RED : (s.water < 30.f) ? ORANGE : Fade(BLUE, 0.8f);
        DrawText("W", rx, ry, DATA_FONT, Fade(LIGHTGRAY, 0.6f));
        rx += 10;
        DrawMiniBar(rx, ry + 1, MINI_BAR_W, MINI_BAR_H, s.water, 150.f,
                    waterFill, Fade(WHITE, 0.08f), Fade(WHITE, 0.3f));
        rx += MINI_BAR_W + 4;

        // Wood (always show but dim in spring/summer)
        Color woodFill = (s.wood < 20.f && (snapHeatDrainMod >= 0.8f)) ? RED :
                         (s.wood < 30.f) ? ORANGE : Fade(BROWN, 0.9f);
        float woodAlpha = showWood ? 1.f : 0.5f;
        DrawText("D", rx, ry, DATA_FONT, Fade(LIGHTGRAY, 0.6f * woodAlpha));
        rx += 9;
        DrawMiniBar(rx, ry + 1, MINI_BAR_W, MINI_BAR_H, s.wood, 150.f,
                    Fade(woodFill, woodAlpha), Fade(WHITE, 0.08f * woodAlpha),
                    Fade(WHITE, 0.3f * woodAlpha));
        rx += MINI_BAR_W + 6;

        // Gold
        char goldBuf[16];
        std::snprintf(goldBuf, sizeof(goldBuf), "%.0fg", s.treasury);
        DrawText(goldBuf, rx, ry, DATA_FONT, Fade(GOLD, 0.85f));
        rx += MeasureText(goldBuf, DATA_FONT) + 6;

        // Morale + Contentment on right side of row 2
        float morale = s.morale;
        char trend = ' ';
        auto it = s_prevMorale.find(s.name);
        if (it != s_prevMorale.end()) {
            float delta = morale - it->second;
            if (delta > 0.03f)  trend = '+';
            if (delta < -0.03f) trend = '-';
        }
        if (moraleSampleNow) s_prevMorale[s.name] = morale;

        Color moraleCol = (morale >= 0.7f) ? Fade(GREEN, 0.8f) :
                          (morale >= 0.3f) ? Fade(YELLOW, 0.8f) : Fade(RED, 0.9f);
        char mBuf[16];
        if (trend != ' ')
            std::snprintf(mBuf, sizeof(mBuf), "M%.0f%%%c", morale * 100.f, trend);
        else
            std::snprintf(mBuf, sizeof(mBuf), "M%.0f%%", morale * 100.f);

        Color cCol = (s.avgContentment >= 0.7f) ? Fade(GREEN, 0.7f) :
                     (s.avgContentment >= 0.4f) ? Fade(YELLOW, 0.7f) : Fade(RED, 0.8f);
        char cBuf[16];
        std::snprintf(cBuf, sizeof(cBuf), "C%.0f%%", s.avgContentment * 100.f);

        int mW = MeasureText(mBuf, DATA_FONT);
        int cW = MeasureText(cBuf, DATA_FONT);
        DrawText(mBuf, cx + cardW - mW - cW - 10, ry, DATA_FONT, moraleCol);
        DrawText(cBuf, cx + cardW - cW - 4, ry, DATA_FONT, cCol);
    }
}

// ---- Event log ----

// Source filter tags shown in the event log title bar.
// Bright cyan = source visible; dim red-brown = source hidden/filtered out.
static const struct { const char* tag; const char* label; } LOG_FILTER_TAGS[] = {
    { "Rand",  "[Rand]"  },
    { "Econ",  "[Econ]"  },
    { "Trade", "[Trade]" },
    { "Agent", "[Agent]" },
    { "Sched", "[Sched]" },
    { "Prod",  "[Prod]"  },
    { "Build", "[Build]" },
    { "Death", "[Death]" },
    { "Birth", "[Birth]" },
    { "Sim",   "[Sim]"   },
    { "Time",  "[Time]"  },
    { "Price", "[Price]" },
};
static constexpr int LOG_FILTER_TAG_COUNT = (int)(sizeof(LOG_FILTER_TAGS) / sizeof(LOG_FILTER_TAGS[0]));

void HUD::DrawEventLog(const RenderSnapshot& snap) {
    std::vector<EventLog::Entry> entries;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        entries = snap.logEntries;
    }
    if (entries.empty()) return;

    static const int LINES    = 8, LINE_H = 16;
    static const int PX       = 330, PW = SCREEN_W - PX - 4;
    static const int TITLE_H  = 14;
    static const int FILTER_H = 14;
    static const int PH       = LINES * LINE_H + TITLE_H + FILTER_H + 4;
    static const int PY       = SCREEN_H - PH - 4;

    DrawRectangle(PX, PY, PW, PH, Fade(BLACK, 0.6f));
    DrawRectangleLines(PX, PY, PW, PH, Fade(LIGHTGRAY, 0.25f));

    // Title bar
    DrawRectangle(PX, PY, PW, TITLE_H, Fade(WHITE, 0.04f));
    DrawRectangle(PX, PY + TITLE_H, PW, 1, Fade(LIGHTGRAY, 0.15f));
    DrawText("Event Log", PX + 6, PY + 2, 11, Fade(YELLOW, 0.7f));

    // Filter toggle row — compact [Tag] labels, click to toggle visibility
    {
        static const int FILTER_FONT = 9;
        Vector2 mouse      = GetMousePosition();
        bool mouseClicked  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        int fx = PX + 6;
        int fy = PY + TITLE_H + 2;

        for (int t = 0; t < LOG_FILTER_TAG_COUNT; ++t) {
            const char* tag   = LOG_FILTER_TAGS[t].tag;
            const char* label = LOG_FILTER_TAGS[t].label;
            bool hidden = (m_logHiddenSources.count(tag) > 0);

            int lw = MeasureText(label, FILTER_FONT);
            // Dim red = hidden/filtered, bright cyan = visible
            Color lc = hidden ? Fade(Color{140, 70, 70, 255}, 0.9f) : Fade(SKYBLUE, 0.75f);

            Rectangle hitbox = { (float)fx - 1, (float)fy - 1, (float)(lw + 2), (float)(FILTER_FONT + 4) };
            if (mouseClicked && CheckCollisionPointRec(mouse, hitbox)) {
                if (hidden)
                    m_logHiddenSources.erase(tag);
                else
                    m_logHiddenSources.insert(tag);
            }

            DrawText(label, fx, fy, FILTER_FONT, lc);
            fx += lw + 3;
            if (fx > PX + PW - 8) break;
        }
    }

    // Build filtered view: skip entries whose source is in the hidden set
    std::vector<const EventLog::Entry*> visible;
    visible.reserve(entries.size());
    for (const auto& e : entries) {
        if (!e.sourceSystem.empty() && m_logHiddenSources.count(e.sourceSystem) > 0)
            continue;
        visible.push_back(&e);
    }

    int maxScroll = std::max(0, (int)visible.size() - LINES);
    logScroll     = std::min(logScroll, maxScroll);

    int contentY = PY + TITLE_H + FILTER_H + 4;

    for (int i = 0; i < LINES; ++i) {
        int idx = i + logScroll;
        if (idx >= (int)visible.size()) break;
        const auto& e = *visible[idx];

        int lineX = PX + 6;
        int lineY = contentY + LINE_H * i;

        // Source prefix in dim blue-grey — identifies which system emitted this entry
        if (!e.sourceSystem.empty()) {
            char prefix[12];
            std::snprintf(prefix, sizeof(prefix), "[%s]", e.sourceSystem.c_str());
            DrawText(prefix, lineX, lineY, 10, Fade(Color{100, 130, 160, 255}, 0.85f));
            lineX += MeasureText(prefix, 10) + 3;
        }

        char buf[120];
        std::snprintf(buf, sizeof(buf), "D%d %02d:xx  %s", e.day, e.hour, e.message.c_str());
        Color col = (e.message.find("BLOCKED")    != std::string::npos ||
                     e.message.find("BANDITS")    != std::string::npos ||
                     e.message.find("DROUGHT")    != std::string::npos ||
                     e.message.find("BLIGHT")     != std::string::npos ||
                     e.message.find("DISEASE")    != std::string::npos ||
                     e.message.find("PLAGUE")     != std::string::npos ||
                     e.message.find("BLIZZARD")   != std::string::npos ||
                     e.message.find("FLOOD")      != std::string::npos ||
                     e.message.find("FIRE")       != std::string::npos ||
                     e.message.find("EARTHQUAKE") != std::string::npos ||
                     e.message.find("cold")       != std::string::npos ||
                     e.message.find("stole")      != std::string::npos ||
                     e.message.find("COLLAPSED")  != std::string::npos) ? RED    :
                    (e.message.find("died")        != std::string::npos ||
                     e.message.find("migrating")   != std::string::npos ||
                     e.message.find("MIGRATION")   != std::string::npos ||
                     e.message.find("HEAT WAVE")   != std::string::npos ||
                     e.message.find("bankrupt")    != std::string::npos ||
                     e.message.find("saw through") != std::string::npos) ? ORANGE :
                    (e.message.find("CLEARED")        != std::string::npos ||
                     e.message.find("restored")       != std::string::npos ||
                     e.message.find("reopened")       != std::string::npos ||
                     e.message.find("Born")           != std::string::npos ||
                     e.message.find("TRADE BOOM")     != std::string::npos ||
                     e.message.find("BOUNTY")         != std::string::npos ||
                     e.message.find("WINDFALL")       != std::string::npos ||
                     e.message.find("FESTIVAL")       != std::string::npos ||
                     e.message.find("RAINSTORM")      != std::string::npos ||
                     e.message.find("reaches")        != std::string::npos ||
                     e.message.find("OFF-MAP")        != std::string::npos ||
                     e.message.find("respawned")      != std::string::npos ||
                     e.message.find("built a new")    != std::string::npos ||
                     e.message.find("funded road")    != std::string::npos ||
                     e.message.find("became a hauler") != std::string::npos ||
                     e.message.find("Ally trade")      != std::string::npos) ? GREEN  :
                    (e.message.find("--- ")        != std::string::npos) ? SKYBLUE : LIGHTGRAY;
        DrawText(buf, lineX, lineY, 12, col);
    }
}

// ---- NPC hover tooltip ----

void HUD::DrawHoverTooltip(const RenderSnapshot& snap, const Camera2D& cam) const {
    Vector2 mouse = GetMousePosition();
    Vector2 world = GetScreenToWorld2D(mouse, cam);

    std::vector<RenderSnapshot::AgentEntry> agents;
    std::shared_ptr<const std::vector<std::string>> skillNamesPtr;
    std::shared_ptr<const std::vector<std::string>> needNamesHoverPtr;
    std::shared_ptr<const std::vector<std::string>> resourceNamesHoverPtr;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        agents = snap.agents;
        skillNamesPtr          = snap.skillNames;
        needNamesHoverPtr      = snap.needNames;
        resourceNamesHoverPtr  = snap.resourceNames;
    }
    // Dereference once; empty fallback if not yet set.
    const auto& skillNames     = skillNamesPtr          ? *skillNamesPtr          : emptyNames;
    const auto& needNamesHover = needNamesHoverPtr      ? *needNamesHoverPtr      : emptyNames;
    const auto& resNamesHover  = resourceNamesHoverPtr  ? *resourceNamesHoverPtr  : emptyNames;

    // Build surname→count and familyName→count maps for family cluster display.
    std::map<std::string, int> surnameCount;
    std::map<std::string, int> familyNameCount;
    for (const auto& a : agents) {
        auto sp = a.npcName.rfind(' ');
        if (sp != std::string::npos)
            ++surnameCount[a.npcName.substr(sp + 1)];
        if (!a.familyName.empty())
            ++familyNameCount[a.familyName];
    }

    const RenderSnapshot::AgentEntry* best = nullptr;
    float bestDist = 12.f;
    for (const auto& a : agents) {
        float dx = world.x - a.x, dy = world.y - a.y;
        float d  = std::sqrt(dx*dx + dy*dy) - a.size;
        if (d < bestDist) { bestDist = d; best = &a; }
    }
    if (!best) return;

    Vector2 screen = GetWorldToScreen2D({ best->x, best->y }, cam);

    const char* role = (best->role == RenderSnapshot::AgentRole::Player) ? "Player"
                     : (best->role == RenderSnapshot::AgentRole::Child)  ? "Child"
                     : (!best->profession.empty()) ? best->profession.c_str()
                     : "NPC";
    bool isHauler  = (best->role == RenderSnapshot::AgentRole::Hauler);
    bool showGold  = (best->balance > 0.f || isHauler);

    char line1[128], lineAge[32] = {}, line2[64], line3[64] = {}, line4[64] = {}, line5[64] = {}, line6[64] = {};
    // First line: name + profession + home settlement (if known)
    if (!best->npcName.empty()) {
        if (!best->homeSettlementName.empty())
            std::snprintf(line1, sizeof(line1), "%s  [%s @ %s]",
                          best->npcName.c_str(), role, best->homeSettlementName.c_str());
        else
            std::snprintf(line1, sizeof(line1), "%s (%s)", best->npcName.c_str(), role);
    } else {
        std::snprintf(line1, sizeof(line1), "%s | %s", role, BehaviorLabel(best->behavior));
    }
    // Append family indicator.
    // Prefer the explicit FamilyTag name; fall back to surname-count heuristic.
    {
        if (!best->familyName.empty()) {
            int n = 1;
            auto fit = familyNameCount.find(best->familyName);
            if (fit != familyNameCount.end()) n = fit->second;
            size_t used = std::strlen(line1);
            std::snprintf(line1 + used, sizeof(line1) - used,
                          "  (Family: %s x%d)", best->familyName.c_str(), n);
        } else {
            auto sp = best->npcName.rfind(' ');
            if (sp != std::string::npos) {
                std::string surname = best->npcName.substr(sp + 1);
                auto it = surnameCount.find(surname);
                if (it != surnameCount.end() && it->second >= 2) {
                    size_t used = std::strlen(line1);
                    std::snprintf(line1 + used, sizeof(line1) - used,
                                  "  (Family: %s x%d)", surname.c_str(), it->second);
                }
            }
        }
    }
    // Vocation tag is drawn separately in gold after line1 (see draw section below)

    bool hasName = !best->npcName.empty();
    if (hasName) {
        int ageInt = (int)best->ageDays;
        if (best->ageDays < 15.f)
            std::snprintf(lineAge, sizeof(lineAge), "Age: %d (child)", ageInt);
        else if (best->ageDays > 60.f)
            std::snprintf(lineAge, sizeof(lineAge), "Age: %d (elder)", ageInt);
        else
            std::snprintf(lineAge, sizeof(lineAge), "Age: %d", ageInt);
    }
    // Build a compact need summary string into `dest` (size `destSz`) from needValues.
    auto buildNeedSummary = [&](char* dest, size_t destSz) {
        int off = 0;
        const auto& nv = best->needValues;
        for (int ni = 0; ni < (int)nv.size() && off < (int)destSz - 4; ++ni) {
            if (ni > 0 && off < (int)destSz - 4) { dest[off++] = ' '; dest[off++] = ' '; }
            const char* abbr = (ni < (int)needNamesHover.size() && !needNamesHover[ni].empty())
                               ? needNamesHover[ni].c_str() : "?";
            off += std::snprintf(dest + off, destSz - off, "%c:%.0f%%", abbr[0], nv[ni].first * 100.f);
        }
        dest[off] = '\0';
    };

    if (hasName) {
        const char* bLabel = BehaviorLabel(best->behavior);
        if (best->isExiled && best->fatigued)
            std::snprintf(line2, sizeof(line2), "%s (fatigued) [Exiled]", bLabel);
        else if (best->isExiled)
            std::snprintf(line2, sizeof(line2), "%s [Exiled]", bLabel);
        else if (best->fatigued)
            std::snprintf(line2, sizeof(line2), "%s (fatigued)", bLabel);
        else
            std::snprintf(line2, sizeof(line2), "%s", bLabel);
    } else {
        char needSummary[64] = {};
        buildNeedSummary(needSummary, sizeof(needSummary));
        std::snprintf(line2, sizeof(line2), "%s", needSummary);
    }

    if (hasName) {
        char needSummary[64] = {};
        buildNeedSummary(needSummary, sizeof(needSummary));
        std::snprintf(line3, sizeof(line3), "%s", needSummary);
    } else
        std::snprintf(line3, sizeof(line3), "Age: %.0f / %.0f days",
                      best->ageDays, best->maxDays);

    float ageFrac = (best->maxDays > 0.f) ? std::min(1.f, best->ageDays / best->maxDays) : 0.f;
    Color ageCol  = (ageFrac < 0.6f) ? Fade(GREEN, 0.9f) :
                    (ageFrac < 0.85f) ? YELLOW : RED;

    if (hasName) {
        const char* lifeStage = (best->ageDays < 15.f)  ? "Child"   :
                                (best->ageDays < 25.f)  ? "Youth"   :
                                (best->ageDays > 70.f)  ? "Elderly" : "Adult";
        // Contentment rating label
        const char* mood = (best->contentment >= 0.85f) ? "Thriving"  :
                           (best->contentment >= 0.65f) ? "Content"   :
                           (best->contentment >= 0.40f) ? "Stressed"  :
                           (best->contentment >= 0.20f) ? "Suffering" : "Desperate";
        std::snprintf(line4, sizeof(line4), "Age: %.0f / %.0f  [%s]  %s",
                      best->ageDays, best->maxDays, lifeStage, mood);
    }
    // Hauler cargo line
    char cargoLine[64] = {};
    bool showCargo = false;
    if (isHauler && !best->cargo.empty()) {
        int off = 0;
        off += std::snprintf(cargoLine + off, sizeof(cargoLine) - off, "Cargo: ");
        for (const auto& [type, qty] : best->cargo) {
            const char* rn = (type >= 0 && type < (int)resNamesHover.size() && !resNamesHover[type].empty())
                             ? resNamesHover[type].c_str() : "?";
            off += std::snprintf(cargoLine + off, sizeof(cargoLine) - off, "%s×%d ", rn, qty);
        }
        if (!best->destSettlName.empty()) {
            std::snprintf(cargoLine + off, sizeof(cargoLine) - off,
                          "→ %s", best->destSettlName.c_str());
        }
        showCargo = true;
    } else if (isHauler) {
        std::snprintf(cargoLine, sizeof(cargoLine), "Cargo: (empty — seeking route)");
        showCargo = true;
    }

    if (showGold)
        std::snprintf(line5, sizeof(line5), "Gold: %.1f", best->balance);

    // Following line: shown for children that have a cached follow target
    char followLine[64] = {};
    bool showFollow = false;
    if (best->role == RenderSnapshot::AgentRole::Child && !best->followingName.empty()) {
        std::snprintf(followLine, sizeof(followLine), "Following: %s", best->followingName.c_str());
        showFollow = true;
    }

    // "Fed by neighbour" line: shown when this NPC recently received charity
    bool showHelped   = best->recentlyHelped;
    // "Grateful to neighbour" line: shown while the NPC is walking toward their helper
    bool showGrateful = best->isGrateful;
    // "Warm from giving" line: shown when this NPC gave charity and has high heat
    bool showWarmth   = best->recentWarmthGlow;
    // "Gave charity" line: shown when charityTimer > 0 (recently gave charity)
    bool showCharity  = (best->charityTimerLeft > 0.f);
    char charityLine[48] = {};
    if (showCharity)
        std::snprintf(charityLine, sizeof(charityLine), "Gave charity (%.1fh ago)", best->charityTimerLeft);
    // "Bandit" line: shown for BanditTag entities
    bool showBandit   = best->isBandit;
    char banditLine[128] = {};
    if (showBandit) {
        if (!best->gangName.empty())
            std::snprintf(banditLine, sizeof(banditLine), "Bandit [%s] (press E)", best->gangName.c_str());
        else
            std::snprintf(banditLine, sizeof(banditLine), "Bandit (press E to confront)");
    }
    // "On strike" line: shown when NPC has active strikeDuration
    bool showStrike   = best->onStrike;
    char strikeLine[48] = {};
    if (showStrike) {
        if (best->strikeHoursLeft > 0.f)
            std::snprintf(strikeLine, sizeof(strikeLine), "On strike (%.0fh left)", best->strikeHoursLeft);
        else
            std::snprintf(strikeLine, sizeof(strikeLine), "On strike");
    }
    // "Good harvest" line: shown when NPC has active harvest bonus
    bool showHarvest  = best->harvestBonus;
    // "Recently taught/learned" line: shown when teachCooldown > 0
    bool showTaught   = best->recentlyTaught;
    // "Grieving" line: shown when griefTimer > 0
    bool showGrief    = best->isGrieving;
    char griefLine[48] = {};
    if (showGrief)
        std::snprintf(griefLine, sizeof(griefLine), "Grieving (%.1fh left)", best->griefHoursLeft);

    // Elder will line: surfaces the inheritance mechanic
    bool showWill = hasName && (best->ageDays > 60.f) && (best->balance > 0.f);

    // Hauler graduation progress: shown for non-hauler NPCs with balance > 50
    bool showGraduation = false;
    char gradLine[48] = {};
    if (!isHauler && best->role == RenderSnapshot::AgentRole::NPC
        && best->balance > 50.f) {
        std::snprintf(gradLine, sizeof(gradLine), "Hauler at: 100g (%.0f%%)",
                      best->balance);
        showGraduation = true;
    }

    // Wage line: shown for working NPCs
    bool showWage = (best->wage > 0.f);
    char wageLine[48] = {};
    if (showWage)
        std::snprintf(wageLine, sizeof(wageLine), "Wage: ~%.1fg/hr", best->wage);

    // Reputation line: shown when non-zero
    bool showRep = (std::abs(best->reputation) >= 0.05f);
    char repLine[32] = {};
    if (showRep)
        std::snprintf(repLine, sizeof(repLine), "Rep: %+.1f", best->reputation);

    // Satisfaction line
    char satLine[32] = {};
    std::snprintf(satLine, sizeof(satLine), "Satisfaction: %d%%", (int)(best->satisfaction * 100));
    Color satColor = (best->satisfaction < 0.3f)  ? Fade(RED, 0.8f)    :
                     (best->satisfaction < 0.6f)  ? Fade(YELLOW, 0.8f) :
                                                    Fade(GREEN, 0.8f);

    // Home morale line: shown for NPCs with a home settlement
    bool showHomeMorale = (best->homeMorale >= 0.f);
    char homeMoraleLine[32] = {};
    if (showHomeMorale)
        std::snprintf(homeMoraleLine, sizeof(homeMoraleLine), "Home morale: %d%%", (int)(best->homeMorale * 100));

    // Rumour carrier line
    bool showRumour = best->hasRumour;
    char rumourLine[64] = {};
    if (showRumour)
        std::snprintf(rumourLine, sizeof(rumourLine), "(spreading: %s)", best->rumourLabel.c_str());

    // Hauler estimated trip profit (computed in WriteSnapshot from destination prices)
    bool showProfit = false;
    char profitLine[64] = {};
    Color profitColor = GREEN;
    if (isHauler && best->haulerCargoQty > 0 && best->haulerState == 1) {
        std::snprintf(profitLine, sizeof(profitLine), "Est. profit: %+.1fg",
                      best->estimatedProfit);
        profitColor = (best->estimatedProfit >= 0.f) ? Fade(GREEN, 0.8f) : Fade(RED, 0.8f);
        showProfit = true;
    }

    // Hauler best-profit record
    bool showBestRoute = false;
    char bestRouteLine[80] = {};
    if (isHauler && best->bestProfit > 0.f && !best->bestRoute.empty()) {
        std::snprintf(bestRouteLine, sizeof(bestRouteLine), "Best: %s +%.1fg",
                      best->bestRoute.c_str(), best->bestProfit);
        showBestRoute = true;
    }

    // Hauler lifetime trip history
    bool showTripHistory = false;
    char tripHistoryLine[80] = {};
    if (isHauler && best->lifetimeTrips > 0) {
        std::snprintf(tripHistoryLine, sizeof(tripHistoryLine), "Trips: %d (total %+.1fg)",
                      best->lifetimeTrips, best->lifetimeProfit);
        showTripHistory = true;
    }

    // Hauler route distance
    bool showRoute = false;
    char routeLine[64] = {};
    if (isHauler && best->hasRouteDest) {
        float dx = best->destX - best->x;
        float dy = best->destY - best->y;
        float dist = std::sqrt(dx * dx + dy * dy) / 100.f;
        std::snprintf(routeLine, sizeof(routeLine), "Route: %.1fkm", dist);
        showRoute = true;
    }

    // Hauler state label
    bool showHaulerState = false;
    char haulerStateLine[48] = {};
    if (isHauler) {
        const char* stLabel = (best->haulerState == 1) ?
                                  (best->inConvoy ? "Delivering [Convoy]" : "Delivering") :
                              (best->haulerState == 2) ? "Returning home" :
                                                         "Idle — seeking route";
        std::snprintf(haulerStateLine, sizeof(haulerStateLine), "Status: %s", stLabel);
        showHaulerState = true;
    }

    // Near-bankrupt warning with countdown
    bool showNearBankrupt = best->nearBankrupt;
    char bankruptLine[48] = {};
    if (showNearBankrupt) {
        float remaining = std::max(0.f, 24.f - best->bankruptProgress);
        std::snprintf(bankruptLine, sizeof(bankruptLine),
                      "!! Near bankruptcy — ~%.0fh left !!", remaining);
    }

    // Illness suffix: appended inline on the needs line when illnessTimer > 0
    char illLabelBuf[32] = {};
    const char* illLabel = nullptr;
    if (best->ill) {
        int idx = best->illNeedIdx;
        const char* needName = (idx >= 0 && idx < (int)needNamesHover.size() && !needNamesHover[idx].empty())
                               ? needNamesHover[idx].c_str() : "need";
        std::snprintf(illLabelBuf, sizeof(illLabelBuf), "(ill: %s)", needName);
        illLabel = illLabelBuf;
    }

    // Skill line: show the best skill for this agent
    bool showSkill = false;
    Color skillColor = GREEN;
    if (!best->skillLevels.empty()) {
        float sk = 0.f;
        std::string skLabel = "Skill";
        // Find best skill
        int bestIdx = 0;
        for (int i = 1; i < (int)best->skillLevels.size(); ++i)
            if (best->skillLevels[i] > best->skillLevels[bestIdx]) bestIdx = i;
        sk = best->skillLevels[bestIdx];
        if (bestIdx < (int)skillNames.size() && !skillNames[bestIdx].empty())
            skLabel = skillNames[bestIdx];
        const char* rank = (sk >= 0.85f) ? " [Master]"  :
                           (sk >= 0.65f) ? " [Expert]"  :
                           (sk >= 0.40f) ? " [Trained]" :
                           (sk >= 0.15f) ? " [Novice]"  : " [Unskilled]";
        skillColor = (sk >= 0.65f) ? Fade(GOLD, 0.9f) : (sk >= 0.35f) ? GREEN : Fade(GRAY, 0.8f);
        std::snprintf(line6, sizeof(line6), "%s: %.0f%%%s", skLabel.c_str(), sk * 100.f, rank);
        showSkill = true;
    }

    // Skill milestone title from snapshot (computed in WriteSnapshot)
    char milestoneLine[48] = {};
    Color milestoneColor = Fade(GOLD, 0.7f);
    bool showMilestone = false;
    if (!best->specialisation.empty()) {
        std::snprintf(milestoneLine, sizeof(milestoneLine), "%s", best->specialisation.c_str());
        milestoneColor = Fade(GOLD, 0.9f);
        showMilestone = true;
    } else if (!best->skillLevels.empty()) {
        // Journeyman title for skills >= 0.5 (below master threshold)
        float bestSk = -1.f;
        std::string bestType;
        for (int i = 0; i < (int)best->skillLevels.size(); ++i) {
            if (best->skillLevels[i] > bestSk) {
                bestSk = best->skillLevels[i];
                bestType = (i < (int)skillNames.size()) ? skillNames[i] : "Unknown";
            }
        }
        if (bestSk >= 0.5f) {
            std::snprintf(milestoneLine, sizeof(milestoneLine), "Journeyman %s", bestType.c_str());
            milestoneColor = Fade(GOLD, 0.6f);
            showMilestone = true;
        }
    }

    // Generous donor badge
    bool showGenerous = best->generousDonor;

    // Reconciliation glow badge
    bool showReconciling = best->reconciling;

    // Wisdom heir badge
    bool showHeir = best->wisdomHeir;

    // Expert badge
    bool showExpert = best->isExpert;

    // Crisis survivor badge (home settlement under Drought or Plague)
    bool showCrisis = best->crisisSurvivor;

    // Skill decay warning: non-working NPCs with any skill >= 0.5
    bool showSkillDecay = false;
    if (best->behavior != AgentBehavior::Working &&
        best->role != RenderSnapshot::AgentRole::Player &&
        std::any_of(best->skillLevels.begin(), best->skillLevels.end(), [](float v){ return v >= 0.5f; })) {
        showSkillDecay = true;
    }

    // Goal description
    bool showGoal = !best->goalDescription.empty();
    bool showMigMem = !best->migrationMemorySummary.empty();

    // Best friend line
    char friendLine[64] = {};
    bool showFriend = (best->bestFriendAffinity >= 0.5f && !best->bestFriendName.empty());
    if (showFriend) {
        std::snprintf(friendLine, sizeof(friendLine), "Friend: %s (%d%%)",
                      best->bestFriendName.c_str(), (int)(best->bestFriendAffinity * 100));
    }

    // Career changes line
    char careerLine[48] = {};
    bool showCareer = (best->careerChanges > 0);
    if (showCareer) {
        std::snprintf(careerLine, sizeof(careerLine), "Career changes: %d", best->careerChanges);
    }

    // Mood comment based on contentment
    char moodLine[32] = {};
    Color moodColor = WHITE;
    bool showMood = false;
    if (best->role != RenderSnapshot::AgentRole::Player) {
        const char* moodText = (best->contentment > 0.8f)  ? "Content"    :
                               (best->contentment > 0.5f)  ? "Getting by" :
                               (best->contentment > 0.3f)  ? "Struggling" : "Desperate";
        moodColor = (best->contentment > 0.8f)  ? GREEN  :
                    (best->contentment > 0.5f)  ? YELLOW :
                    (best->contentment > 0.3f)  ? ORANGE : RED;
        std::snprintf(moodLine, sizeof(moodLine), "Mood: %s", moodText);
        showMood = true;
    }

    int lineCount = hasName ? 5 : 3;   // +1 for age line
    if (showGold)     lineCount++;
    if (showWill)     lineCount++;
    if (showFollow)   lineCount++;
    if (showHelped)   lineCount++;
    if (showGrateful) lineCount++;
    if (showWarmth)   lineCount++;
    if (showCharity)  lineCount++;
    if (showBandit)   lineCount++;
    if (showStrike)   lineCount++;
    if (showSkill)     lineCount++;
    if (showMilestone) lineCount++;
    if (showGenerous)  lineCount++;
    if (showReconciling) lineCount++;
    if (showHeir)        lineCount++;
    if (showExpert)      lineCount++;
    if (showCrisis)      lineCount++;
    if (showSkillDecay) lineCount++;
    if (showMood)      lineCount++;
    if (showCargo)    lineCount++;
    if (showRumour)   lineCount++;
    if (showHarvest)  lineCount++;
    if (showTaught)   lineCount++;
    if (showGrief)    lineCount++;
    if (showProfit)   lineCount++;
    if (showBestRoute)    lineCount++;
    if (showTripHistory)  lineCount++;
    if (showRoute)        lineCount++;
    if (showNearBankrupt)  lineCount++;
    if (showHaulerState)   lineCount++;
    bool showRivalryTariff = best->rivalryTariff;
    if (showRivalryTariff) lineCount++;
    if (showGraduation)    lineCount++;
    if (showWage)          lineCount++;
    if (showRep)           lineCount++;
    lineCount++;  // satisfaction line (always shown)
    if (showHomeMorale)    lineCount++;
    if (showGoal)          lineCount++;
    if (showMigMem)        lineCount++;
    if (showFriend)        lineCount++;
    if (showCareer)        lineCount++;

    int illSuffixW = illLabel ? (4 + MeasureText(illLabel, 11)) : 0;
    int w1  = MeasureText(line1, 12) + (best->recentlyStole ? MeasureText("  (thief)", 12) : 0);
    bool showApprentice = (best->role == RenderSnapshot::AgentRole::Child && best->ageDays >= 12.f);
    int wa  = hasName ? MeasureText(lineAge, 11) + (showApprentice ? MeasureText(" [Apprentice]", 11) : 0) : 0;
    // Needs line is line3 for named NPCs, line2 for unnamed. Account for illness suffix width.
    int w2  = MeasureText(line2, 11) + ((!hasName && illLabel) ? illSuffixW : 0);
    int w3  = MeasureText(line3, 11) + ((hasName && illLabel) ? illSuffixW : 0);
    int w4  = MeasureText(line4, 11);
    int w5  = showGold   ? MeasureText(line5,                 11) : 0;
    int wf  = showFollow ? MeasureText(followLine,            11) : 0;
    int w6  = showSkill  ? MeasureText(line6,                 11) : 0;
    int wc  = showCargo  ? MeasureText(cargoLine,             11) : 0;
    int wh  = showHelped   ? MeasureText("Fed by neighbour",      11) : 0;
    int wg  = showGrateful ? MeasureText("Grateful to neighbour", 11) : 0;
    int ww  = showWarmth   ? MeasureText("Warm from giving",      11) : 0;
    int wch = showCharity  ? MeasureText(charityLine,            11) : 0;
    int wb  = showBandit   ? MeasureText(banditLine, 11) : 0;
    int wsk = showStrike   ? MeasureText(strikeLine, 11) : 0;
    int wwl = showWill     ? MeasureText("Will: 80% to treasury", 11) : 0;
    int wr  = showRumour  ? MeasureText(rumourLine,              11) : 0;
    int whv = showHarvest ? MeasureText("Good harvest bonus",     11) : 0;
    int wtl = showTaught ? MeasureText("Recently taught/learned", 11) : 0;
    int wgf = showGrief  ? MeasureText(griefLine, 11) : 0;
    int wpr = showProfit ? MeasureText(profitLine,               11) : 0;
    int wbr = showBestRoute ? MeasureText(bestRouteLine,         11) : 0;
    int wth = showTripHistory ? MeasureText(tripHistoryLine,     11) : 0;
    int wrt = showRoute  ? MeasureText(routeLine,                11) : 0;
    int wnb = showNearBankrupt ? MeasureText(bankruptLine, 11) : 0;
    int whs = showHaulerState ? MeasureText(haulerStateLine, 11) : 0;
    int wrt2 = showRivalryTariff ? MeasureText("Rivalry tariff (+30%)", 11) : 0;
    int wgr = showGraduation ? MeasureText(gradLine, 11) : 0;
    int wwg = showWage ? MeasureText(wageLine, 11) : 0;
    int whm = showHomeMorale ? MeasureText(homeMoraleLine, 11) : 0;
    int wrp = showRep ? MeasureText(repLine, 11) : 0;
    int wst = MeasureText(satLine, 11);
    int wmd = showMood ? MeasureText(moodLine, 11) : 0;
    int wml = showMilestone ? MeasureText(milestoneLine, 11) : 0;
    int wgd = showGenerous ? MeasureText("[Generous]", 11) : 0;
    int wrc = showReconciling ? MeasureText("[Harmonious]", 11) : 0;
    int whe = showHeir ? MeasureText("[Heir]", 11) : 0;
    int wex = showExpert ? MeasureText("[Expert]", 11) : 0;
    int wcs = showCrisis ? MeasureText("[Crisis]", 11) : 0;
    int wsd = showSkillDecay ? MeasureText("Skills rusting", 11) : 0;
    int wgo = showGoal ? MeasureText(best->goalDescription.c_str(), 11) : 0;
    int wmm = showMigMem ? MeasureText(best->migrationMemorySummary.c_str(), 11) : 0;
    int wfr = showFriend ? MeasureText(friendLine, 11) : 0;
    int wcr = showCareer ? MeasureText(careerLine, 11) : 0;
    int pw  = std::max({w1, wa, w2, w3, w4, w5, wf, w6, wc, wh, wg, ww, wch, wb, wsk, wwl, wr, whv, wtl, wgf, wsd, wpr, wbr, wth, wrt, wnb, whs, wgr, wwg, whm, wrp, wst, wmd, wml, wgd, wrc, whe, wex, wcs, wgo, wmm, wfr, wrt2, wcr}) + 10;
    int ph = lineCount * 16;

    int tx = (int)screen.x + 14, ty = (int)screen.y - ph;
    if (tx + pw > SCREEN_W) tx = (int)screen.x - pw - 10;
    if (ty < 0) ty = (int)screen.y + 12;

    DrawRectangle(tx - 4, ty - 2, pw, ph, Fade(BLACK, 0.75f));

    int ly = ty;
    DrawText(line1, tx, ly, 12, WHITE);
    int line1W = MeasureText(line1, 12);
    if (best->recentlyStole)
        DrawText("  (thief)", tx + line1W, ly, 12, Fade(RED, 0.85f));
    if (best->inVocation) {
        int afterThief = best->recentlyStole ? MeasureText("  (thief)", 12) : 0;
        DrawText(" [vocation]", tx + line1W + afterThief, ly, 12, Fade(GOLD, 0.6f));
    }
    ly += 16;
    if (hasName) {
        Color ageLineCol = (best->ageDays < 15.f) ? Fade(SKYBLUE, 0.85f) :
                           (best->ageDays > 60.f) ? Fade(ORANGE, 0.80f)  : Fade(LIGHTGRAY, 0.85f);
        DrawText(lineAge, tx, ly, 11, ageLineCol);
        // Apprentice badge: children aged 12+ are old enough to assist in production
        if (showApprentice) {
            int agW = MeasureText(lineAge, 11);
            DrawText(" [Apprentice]", tx + agW, ly, 11, Fade(YELLOW, 0.6f));
        }
        ly += 16;
    }
    {
        Color line2Col = best->isExiled ? Fade(RED, 0.8f) : best->fatigued ? ORANGE : LIGHTGRAY;
        DrawText(line2, tx, ly, 11, line2Col);
        if (!hasName && illLabel)
            DrawText(illLabel, tx + MeasureText(line2, 11) + 4, ly, 11, Fade(RED, 0.75f));
        ly += 16;
    }
    {
        DrawText(line3, tx, ly, 11, hasName ? LIGHTGRAY : ageCol);
        if (hasName && illLabel)
            DrawText(illLabel, tx + MeasureText(line3, 11) + 4, ly, 11, Fade(RED, 0.75f));
        ly += 16;
    }
    if (hasName) { DrawText(line4, tx, ly, 11, ageCol); ly += 16; }
    if (showGold)       { DrawText(line5,               tx, ly, 11, YELLOW);               ly += 16; }
    if (showWage)       { DrawText(wageLine,            tx, ly, 11, Fade(GOLD, 0.7f));    ly += 16; }
    if (showGraduation) { DrawText(gradLine,            tx, ly, 11, Fade(SKYBLUE, 0.6f)); ly += 16; }
    if (showWill)       { DrawText("Will: 80% to treasury", tx, ly, 11, Fade(GOLD, 0.5f)); ly += 16; }
    if (showFollow) { DrawText(followLine,          tx, ly, 11, Fade(SKYBLUE, 0.8f)); ly += 16; }
    if (showHelped)   { DrawText("Fed by neighbour",            tx, ly, 11, Fade(LIME, 0.75f));     ly += 16; }
    if (showGrateful) { DrawText("Grateful to neighbour",       tx, ly, 11, Fade(LIME, 0.55f));    ly += 16; }
    if (showWarmth)   { DrawText("Warm from giving",            tx, ly, 11, Fade(ORANGE, 0.75f));  ly += 16; }
    if (showCharity)  { DrawText(charityLine,                   tx, ly, 11, Fade(LIME, 0.5f));     ly += 16; }
    if (showBandit)   { DrawText(banditLine, tx, ly, 11, Color{220, 60, 60, 220}); ly += 16; }
    if (showStrike)   { DrawText(strikeLine,                    tx, ly, 11, RED);                    ly += 16; }
    if (showRumour)   { DrawText(rumourLine,                   tx, ly, 11, Fade(YELLOW, 0.6f));    ly += 16; }
    if (showHarvest)  { DrawText("Good harvest bonus",          tx, ly, 11, Fade(GOLD, 0.6f));     ly += 16; }
    if (showTaught)   { DrawText("Recently taught/learned",    tx, ly, 11, Fade(SKYBLUE, 0.6f));  ly += 16; }
    if (showGrief)    { DrawText(griefLine,                    tx, ly, 11, Fade(PURPLE, 0.7f));   ly += 16; }
    if (showSkill)    { DrawText(line6,                         tx, ly, 11, skillColor);             ly += 16; }
    if (showMilestone) { DrawText(milestoneLine, tx, ly, 11, milestoneColor); ly += 16; }
    if (showGenerous)  { DrawText("[Generous]", tx, ly, 11, Fade(GOLD, 0.9f)); ly += 16; }
    if (showReconciling) { DrawText("[Harmonious]", tx, ly, 11, Fade(GREEN, 0.7f)); ly += 16; }
    if (showHeir)        { DrawText("[Heir]", tx, ly, 11, Fade(VIOLET, 0.7f)); ly += 16; }
    if (showExpert)      { DrawText("[Expert]", tx, ly, 11, Fade(ORANGE, 0.8f)); ly += 16; }
    if (showCrisis)      { DrawText("[Crisis]", tx, ly, 11, Fade(ORANGE, 0.9f)); ly += 16; }
    if (showSkillDecay) { DrawText("Skills rusting", tx, ly, 11, Fade(ORANGE, 0.6f)); ly += 16; }
    if (showGoal)     { DrawText(best->goalDescription.c_str(), tx, ly, 11, Fade(SKYBLUE, 0.6f)); ly += 16; }
    if (showMigMem)   { DrawText(best->migrationMemorySummary.c_str(), tx, ly, 11, Fade(GRAY, 0.6f)); ly += 16; }
    if (showFriend)   { DrawText(friendLine, tx, ly, 11, Fade(LIME, 0.75f)); ly += 16; }
    if (showCareer)   { DrawText(careerLine, tx, ly, 11, Fade(LIGHTGRAY, 0.7f)); ly += 16; }
    if (showMood)     { DrawText(moodLine, tx, ly, 11, moodColor); ly += 16; }
    if (showRep) {
        Color repCol = (best->reputation >= 0.f) ? Fade(GREEN, 0.7f) : Fade(RED, 0.7f);
        DrawText(repLine, tx, ly, 11, repCol); ly += 16;
    }
    DrawText(satLine, tx, ly, 11, satColor); ly += 16;
    if (showCargo)       { DrawText(cargoLine,        tx, ly, 11, Fade(SKYBLUE, 0.9f));    ly += 16; }
    if (showHaulerState) {
        Color hsCol = (isHauler && best->inConvoy) ? Fade(GREEN, 0.7f) : Fade(LIGHTGRAY, 0.7f);
        DrawText(haulerStateLine, tx, ly, 11, hsCol); ly += 16;
    }
    if (showRivalryTariff) { DrawText("Rivalry tariff (+30%)", tx, ly, 11, Fade(RED, 0.8f)); ly += 16; }
    if (showProfit) { DrawText(profitLine,          tx, ly, 11, profitColor);          ly += 16; }
    if (showBestRoute) { DrawText(bestRouteLine, tx, ly, 11, Fade(GOLD, 0.5f)); ly += 16; }
    if (showTripHistory) { DrawText(tripHistoryLine, tx, ly, 11, Fade(LIGHTGRAY, 0.5f)); ly += 16; }
    if (showRoute)        { DrawText(routeLine,               tx, ly, 11, Fade(LIGHTGRAY, 0.8f)); ly += 16; }
    if (showNearBankrupt) { DrawText(bankruptLine, tx, ly, 11, Fade(RED, 0.9f)); ly += 16; }
    if (showHomeMorale) {
        Color hmCol = (best->homeMorale >= 0.7f) ? Fade(GREEN, 0.5f)
                    : (best->homeMorale >= 0.4f) ? Fade(YELLOW, 0.5f) : Fade(RED, 0.5f);
        DrawText(homeMoraleLine, tx, ly, 11, hmCol);
    }
}

// ---- Facility hover tooltip ----
// Shows production rate, worker count, and skill when hovering near a facility square.

void HUD::DrawFacilityTooltip(const RenderSnapshot& snap, const Camera2D& cam) const {
    Vector2 mouse = GetMousePosition();
    Vector2 world = GetScreenToWorld2D(mouse, cam);

    std::vector<RenderSnapshot::FacilityEntry> facs;
    std::shared_ptr<const std::vector<std::string>> resNamesFacPtr;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        facs = snap.facilities;
        resNamesFacPtr = snap.resourceNames;
    }
    const auto& resNamesFac = resNamesFacPtr ? *resNamesFacPtr : emptyNames;

    const RenderSnapshot::FacilityEntry* best = nullptr;
    float bestDist = 20.f;   // max hover distance in world units
    for (const auto& f : facs) {
        float dx = world.x - f.x, dy = world.y - f.y;
        float d  = std::sqrt(dx*dx + dy*dy);
        if (d < bestDist) { bestDist = d; best = &f; }
    }
    if (!best) return;

    float curSeasonProductionMod;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        curSeasonProductionMod = snap.seasonProductionMod;
    }

    // Facility type name and resource unit from schema
    const char* resDisplayName = (best->output >= 0 && best->output < (int)resNamesFac.size()
                                  && !resNamesFac[best->output].empty())
                                 ? resNamesFac[best->output].c_str() : "Resource";
    char typeNameBuf[128];
    std::snprintf(typeNameBuf, sizeof(typeNameBuf), "%s Facility", resDisplayName);
    const char* typeName = typeNameBuf;
    // Lower-case resource name as unit label
    std::string resUnitStr;
    for (char c : std::string(resDisplayName)) resUnitStr += (char)std::tolower((unsigned char)c);
    const char* resUnit = resUnitStr.c_str();

    static constexpr int BASE_WORKERS = 5;
    float scale      = std::min(2.0f, std::max(0.1f, (float)best->workerCount / BASE_WORKERS));
    float skillMult  = 0.5f + best->avgSkill;             // same formula as ProductionSystem
    float seasonMult = curSeasonProductionMod;
    // Morale production factor: same formula as ProductionSystem
    float moraleFactor = 1.0f + 0.3f * (best->morale - 0.5f);
    float estOutput  = best->baseRate * scale * skillMult * seasonMult * moraleFactor;
    float moralePct    = (moraleFactor - 1.0f) * 100.f;
    bool showMorale    = (std::abs(moralePct) >= 0.5f);  // only show when non-trivial

    char line1[64], line2[64], line3[64], line4[64], line5[64], line6[64] = {};
    std::snprintf(line1, sizeof(line1), "%s @ %s", typeName,
                  best->settlementName.empty() ? "?" : best->settlementName.c_str());
    std::snprintf(line2, sizeof(line2), "Workers: %d  Skill: %.0f%%",
                  best->workerCount, best->avgSkill * 100.f);
    std::snprintf(line3, sizeof(line3), "Base: %.1f %s/hr  Season: %.0f%%",
                  best->baseRate, resUnit, seasonMult * 100.f);
    std::snprintf(line4, sizeof(line4), "Est. output: ~%.1f %s/hr", estOutput, resUnit);
    // Degradation indicator: base rate below starting value (4.0) means decay has happened
    float healthPct = std::min(100.f, best->baseRate / 4.0f * 100.f);
    std::snprintf(line5, sizeof(line5), "Facility health: %.0f%%", healthPct);
    if (showMorale)
        std::snprintf(line6, sizeof(line6), "Morale: %+.0f%%", moralePct);

    int lineCount = showMorale ? 6 : 5;
    Vector2 screen = GetWorldToScreen2D({ best->x, best->y }, cam);
    int w = std::max({ MeasureText(line1, 12), MeasureText(line2, 11),
                       MeasureText(line3, 11), MeasureText(line4, 11),
                       MeasureText(line5, 11),
                       showMorale ? MeasureText(line6, 11) : 0 }) + 12;
    int h = lineCount * 16 + 4;
    int tx = (int)screen.x + 18, ty = (int)screen.y - h;
    if (tx + w > SCREEN_W) tx = (int)screen.x - w - 10;
    if (ty < 0) ty = (int)screen.y + 14;

    DrawRectangle(tx - 4, ty - 2, w, h, Fade(BLACK, 0.75f));
    static const Color kFacResColors[] = { GREEN, SKYBLUE, LIGHTGRAY, BROWN };
    static constexpr int kFacResColorCount = (int)(sizeof(kFacResColors)/sizeof(kFacResColors[0]));
    Color typeCol = (best->output >= 0 && best->output < kFacResColorCount)
                   ? kFacResColors[best->output] : LIGHTGRAY;
    DrawText(line1, tx, ty,      12, typeCol);
    DrawText(line2, tx, ty + 16, 11, LIGHTGRAY);
    Color seasonCol = (seasonMult >= 1.0f) ? GREEN : (seasonMult >= 0.5f) ? YELLOW : RED;
    DrawText(line3, tx, ty + 32, 11, seasonCol);
    Color outCol = (estOutput >= best->baseRate) ? GREEN
                 : (estOutput >= best->baseRate * 0.5f) ? YELLOW : RED;
    DrawText(line4, tx, ty + 48, 11, outCol);
    Color healthCol = (healthPct >= 80.f) ? GREEN : (healthPct >= 50.f) ? YELLOW : RED;
    DrawText(line5, tx, ty + 64, 11, healthCol);
    if (showMorale) {
        Color moraleCol = (moralePct > 0.f) ? GREEN : RED;
        DrawText(line6, tx, ty + 80, 11, moraleCol);
    }
}

// ---- Settlement hover tooltip ----
// Shown when the mouse hovers inside a settlement circle.
// Displays name, pop, resources, treasury, and children count.

void HUD::DrawSettlementTooltip(const RenderSnapshot& snap, const Camera2D& cam) const {
    Vector2 mouse = GetMousePosition();
    Vector2 world = GetScreenToWorld2D(mouse, cam);

    std::vector<RenderSnapshot::SettlementEntry>  settls;
    std::vector<RenderSnapshot::SettlementStatus> ws;
    std::shared_ptr<const std::vector<std::string>> sharedSkillNamesPtr;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        settls = snap.settlements;
        ws     = snap.worldStatus;
        sharedSkillNamesPtr = snap.skillNames;
    }
    const auto& sharedSkillNames = sharedSkillNamesPtr ? *sharedSkillNamesPtr : emptyNames;

    // Find settlement the mouse is inside (by world-space radius)
    const RenderSnapshot::SettlementEntry* best = nullptr;
    float bestDist = 1e9f;
    for (const auto& s : settls) {
        float dx = world.x - s.x, dy = world.y - s.y;
        float d  = std::sqrt(dx*dx + dy*dy);
        if (d < s.radius && d < bestDist) { bestDist = d; best = &s; }
    }
    if (!best) return;

    // Match to SettlementStatus for childCount, treasury, haulers, prices
    const RenderSnapshot::SettlementStatus* status = nullptr;
    for (const auto& st : ws)
        if (st.name == best->name) { status = &st; break; }

    int   childCount  = status ? status->childCount  : 0;
    float treasury    = status ? status->treasury    : 0.f;
    int   haulers     = status ? status->haulers     : 0;
    float food        = status ? status->food  : best->foodStock;
    float water       = status ? status->water : best->waterStock;
    float wood        = status ? status->wood  : best->woodStock;
    int   elderCount  = status ? status->elderCount  : 0;
    float elderBonus  = status ? status->elderBonus  : 0.f;

    // Line 1: name + pop/cap + trend + optional event
    char popTrendStr[4] = {};
    if (status && status->popTrend == '+') std::snprintf(popTrendStr, sizeof(popTrendStr), " +");
    else if (status && status->popTrend == '-') std::snprintf(popTrendStr, sizeof(popTrendStr), " -");
    char line1[128];
    if (!best->modifierName.empty())
        std::snprintf(line1, sizeof(line1), "%s  [%d/%d pop%s]  — %s",
                      best->name.c_str(), best->pop, best->popCap,
                      popTrendStr, best->modifierName.c_str());
    else
        std::snprintf(line1, sizeof(line1), "%s  [%d/%d pop%s]",
                      best->name.c_str(), best->pop, best->popCap, popTrendStr);

    // Trade hub badge: appended when settlement has ≥5 deliveries/day
    bool isTradeHub = (best->tradeVolume >= 5);
    bool isDiverse  = best->diverse;
    bool isAfterglow = best->afterglow;
    bool isVigil    = best->vigil;
    bool isMourning = best->mourning;

    // Line 2: resource stocks
    char line2[64];
    std::snprintf(line2, sizeof(line2), "Food: %.0f  Water: %.0f  Wood: %.0f",
                  food, water, wood);

    // Line 3: treasury + haulers
    char line3[64];
    std::snprintf(line3, sizeof(line3), "Treasury: %.0fg  Haulers: %d",
                  treasury, haulers);

    // Line 4 (optional): children
    char line4[32] = {};
    bool showChildren = (childCount > 0);
    if (showChildren)
        std::snprintf(line4, sizeof(line4), "Children: %d", childCount);

    // Line 5 (optional): elders with production bonus
    char line5[48] = {};
    bool showElders = (elderCount > 0);
    if (showElders)
        std::snprintf(line5, sizeof(line5), "Elders: %d (+%.0f%% prod)", elderCount, elderBonus * 100.f);

    // Line 6 (optional): masters
    char lineMasters[32] = {};
    bool showMasters = (best->masterCount > 0);
    if (showMasters)
        std::snprintf(lineMasters, sizeof(lineMasters), "Masters: %d", best->masterCount);

    // Line: average skills
    char lineSkills[256] = {};
    bool showSkills = std::any_of(best->avgSkills.begin(), best->avgSkills.end(), [](float v){ return v > 0.f; });
    if (showSkills) {
        std::string skillStr = "Skills:";
        for (int i = 0; i < (int)best->avgSkills.size(); ++i) {
            if (best->avgSkills[i] <= 0.f) continue;
            std::string label = (i < (int)sharedSkillNames.size() && !sharedSkillNames[i].empty())
                ? sharedSkillNames[i].substr(0, 4) : "?";
            char tmp[32];
            std::snprintf(tmp, sizeof(tmp), " %s %d%%", label.c_str(), (int)(best->avgSkills[i] * 100));
            skillStr += tmp;
        }
        std::snprintf(lineSkills, sizeof(lineSkills), "%s", skillStr.c_str());
    }

    // Line 7 (optional): pending estates
    char lineEst[48] = {};
    float estates = status ? status->pendingEstates : 0.f;
    bool showEstates = (estates > 0.f);
    if (showEstates)
        std::snprintf(lineEst, sizeof(lineEst), "Estates: ~%.0fg", estates);

    // Line 7 (optional): specialty
    char line6[48] = {};
    bool showSpecialty = !best->specialty.empty();
    if (showSpecialty)
        std::snprintf(line6, sizeof(line6), "Specialty: %s", best->specialty.c_str());

    // Line 8: morale (always shown when status available)
    char line7[32] = {};
    float morale = status ? status->morale : 0.5f;
    bool showMorale = (status != nullptr);
    if (showMorale)
        std::snprintf(line7, sizeof(line7), "Morale: %d%%", (int)(morale * 100));

    // Harmony line: shown after morale
    char lineHarmony[32] = {};
    bool showHarmony = (best->pop >= 2);
    if (showHarmony)
        std::snprintf(lineHarmony, sizeof(lineHarmony), "Harmony: %d%%", (int)(best->harmony * 100));

    // Line 9: mood score (always shown)
    char lineMood[32] = {};
    float moodScore = best->moodScore;
    bool showMood = true;
    std::snprintf(lineMood, sizeof(lineMood), "Mood: %d%%", (int)(moodScore * 100));

    // Line 10: trade volume
    char lineTrade[32] = {};
    bool showTrade = (best->tradeVolume > 0);
    if (showTrade)
        std::snprintf(lineTrade, sizeof(lineTrade), "Trades: %d/day", best->tradeVolume);

    // Line 10: import/export balance
    char lineImpExp[48] = {};
    bool showImpExp = (best->imports > 0 || best->exports > 0);
    if (showImpExp)
        std::snprintf(lineImpExp, sizeof(lineImpExp), "Trade: +%d imports / -%d exports",
                      best->imports, best->exports);

    // Line 11 (optional): desperation purchases
    char lineDesp[48] = {};
    bool showDesp = (best->desperatePurchases > 0);
    if (showDesp)
        std::snprintf(lineDesp, sizeof(lineDesp), "Desperation buys: %d/day", best->desperatePurchases);

    // Line 12 (optional): fatigued workers
    char lineFatigue[32] = {};
    int fatiguedW = status ? status->fatiguedWorkers : 0;
    bool showFatigue = (fatiguedW > 0);
    if (showFatigue)
        std::snprintf(lineFatigue, sizeof(lineFatigue), "Fatigued workers: %d", fatiguedW);

    // Line 13 (optional): recent charity givers
    char lineGivers[32] = {};
    int givers = status ? status->recentGivers : 0;
    bool showGivers = (givers > 0);
    if (showGivers)
        std::snprintf(lineGivers, sizeof(lineGivers), "Charity givers: %d", givers);

    // Line 14 (optional): bounty pool
    char lineBounty[48] = {};
    float bountyPool = status ? status->bountyPool : 0.f;
    bool showBounty = (bountyPool > 0.5f);
    if (showBounty)
        std::snprintf(lineBounty, sizeof(lineBounty), "Bounty: %.0fg", bountyPool);

    // Line 15: production output
    char lineOutput[80] = {};
    bool showOutput = false;
    if (status && (status->foodRate > 0.f || status->waterRate > 0.f || status->woodRate > 0.f)) {
        int off = std::snprintf(lineOutput, sizeof(lineOutput), "Output:");
        if (status->foodRate > 0.f)
            off += std::snprintf(lineOutput + off, sizeof(lineOutput) - off, " food %.1f/h", status->foodRate);
        if (status->waterRate > 0.f)
            off += std::snprintf(lineOutput + off, sizeof(lineOutput) - off, " water %.1f/h", status->waterRate);
        if (status->woodRate > 0.f)
            std::snprintf(lineOutput + off, sizeof(lineOutput) - off, " wood %.1f/h", status->woodRate);
        showOutput = true;
    }

    int lineCount = 3 + (showChildren ? 1 : 0) + (showElders ? 1 : 0)
                      + (showMasters ? 1 : 0) + (showSkills ? 1 : 0) + (showEstates ? 1 : 0)
                      + (showSpecialty ? 1 : 0) + (showMorale ? 1 : 0) + (showHarmony ? 1 : 0)
                      + (showMood ? 1 : 0)
                      + (showTrade ? 1 : 0) + (showImpExp ? 1 : 0)
                      + (showDesp ? 1 : 0) + (showFatigue ? 1 : 0)
                      + (showGivers ? 1 : 0) + (showBounty ? 1 : 0)
                      + (showOutput ? 1 : 0);
    int hubW = isTradeHub ? MeasureText("  [Trade Hub]", 12) : 0;
    int divW = isDiverse  ? MeasureText("  [Diverse]", 12)  : 0;
    int aglW = isAfterglow ? MeasureText("  [Afterglow]", 12) : 0;
    int vigW = isVigil    ? MeasureText("  [Vigil]", 12)    : 0;
    int mouW = isMourning ? MeasureText("  [Mourning]", 12)  : 0;
    int w = std::max({ MeasureText(line1, 12) + hubW + divW + aglW + vigW + mouW, MeasureText(line2, 11),
                       MeasureText(line3, 11),
                       showChildren  ? MeasureText(line4, 11) : 0,
                       showElders    ? MeasureText(line5, 11) : 0,
                       showMasters   ? MeasureText(lineMasters, 11) : 0,
                       showSkills    ? MeasureText(lineSkills, 11) : 0,
                       showEstates   ? MeasureText(lineEst, 11) : 0,
                       showSpecialty ? MeasureText(line6, 11) : 0,
                       showMorale   ? MeasureText(line7, 11) : 0,
                       showHarmony  ? MeasureText(lineHarmony, 11) : 0,
                       showMood     ? MeasureText(lineMood, 11) : 0,
                       showTrade    ? MeasureText(lineTrade, 11) : 0,
                       showImpExp   ? MeasureText(lineImpExp, 11) : 0,
                       showDesp     ? MeasureText(lineDesp, 11) : 0,
                       showFatigue  ? MeasureText(lineFatigue, 11) : 0,
                       showGivers   ? MeasureText(lineGivers, 11) : 0,
                       showBounty   ? MeasureText(lineBounty, 11) : 0,
                       showOutput   ? MeasureText(lineOutput, 11) : 0 }) + 12;
    int h = lineCount * 16 + 4;

    Vector2 screen = GetWorldToScreen2D({ best->x, best->y }, cam);
    int tx = (int)screen.x + 14, ty = (int)screen.y - h - (int)best->radius;
    if (tx + w > SCREEN_W) tx = (int)screen.x - w - 10;
    if (ty < 0)             ty = (int)screen.y + (int)best->radius + 6;

    DrawRectangle(tx - 4, ty - 2, w, h, Fade(BLACK, 0.75f));

    Color nameCol = (best->pop == 0) ? Fade(DARKGRAY, 0.8f) : WHITE;
    DrawText(line1, tx, ty,      12, nameCol);
    int badgeOff = MeasureText(line1, 12);
    if (isTradeHub) {
        DrawText("  [Trade Hub]", tx + badgeOff, ty, 12, Fade(GOLD, 0.8f));
        badgeOff += MeasureText("  [Trade Hub]", 12);
    }
    if (isDiverse) {
        DrawText("  [Diverse]", tx + badgeOff, ty, 12, Fade(GOLD, 0.9f));
        badgeOff += MeasureText("  [Diverse]", 12);
    }
    if (isAfterglow) {
        DrawText("  [Afterglow]", tx + badgeOff, ty, 12, Fade(ORANGE, 0.9f));
        badgeOff += MeasureText("  [Afterglow]", 12);
    }
    if (isVigil) {
        DrawText("  [Vigil]", tx + badgeOff, ty, 12, Fade(PURPLE, 0.7f));
        badgeOff += MeasureText("  [Vigil]", 12);
    }
    if (isMourning)
        DrawText("  [Mourning]", tx + badgeOff, ty, 12, Fade(GRAY, 0.7f));
    ty += 16;
    DrawText(line2, tx, ty,      11, LIGHTGRAY);           ty += 16;
    Color tresCol = (treasury < 50.f) ? RED : (treasury < 150.f) ? ORANGE : GOLD;
    DrawText(line3, tx, ty,      11, tresCol);             ty += 16;
    if (showChildren) {
        DrawText(line4, tx, ty,  11, Fade(LIGHTGRAY, 0.7f)); ty += 16;
    }
    if (showElders) {
        DrawText(line5, tx, ty,  11, Fade(ORANGE, 0.75f)); ty += 16;
    }
    if (showMasters) {
        DrawText(lineMasters, tx, ty, 11, Fade(GOLD, 0.8f)); ty += 16;
    }
    if (showSkills) {
        DrawText(lineSkills, tx, ty, 11, Fade(SKYBLUE, 0.7f)); ty += 16;
    }
    if (showEstates) {
        DrawText(lineEst, tx, ty, 11, Fade(GOLD, 0.5f));   ty += 16;
    }
    if (showSpecialty) {
        DrawText(line6, tx, ty,  11, WHITE);                ty += 16;
    }
    if (showMorale) {
        Color moraleCol = (morale >= 0.7f) ? GREEN : (morale >= 0.4f) ? YELLOW : RED;
        DrawText(line7, tx, ty,  11, moraleCol);            ty += 16;
    }
    if (showHarmony) {
        float h = best->harmony;
        Color harmCol = (h > 0.5f) ? GREEN : (h >= 0.25f) ? YELLOW : RED;
        DrawText(lineHarmony, tx, ty, 11, harmCol);         ty += 16;
    }
    if (showMood) {
        Color moodCol = (moodScore >= 0.7f) ? GREEN : (moodScore >= 0.4f) ? YELLOW : RED;
        DrawText(lineMood, tx, ty, 11, moodCol);            ty += 16;
    }
    if (showTrade) {
        DrawText(lineTrade, tx, ty, 11, Fade(SKYBLUE, 0.8f)); ty += 16;
    }
    if (showImpExp) {
        DrawText(lineImpExp, tx, ty, 11, Fade(LIGHTGRAY, 0.7f)); ty += 16;
    }
    if (showDesp) {
        DrawText(lineDesp, tx, ty, 11, Fade(RED, 0.9f)); ty += 16;
    }
    if (showFatigue) {
        DrawText(lineFatigue, tx, ty, 11, ORANGE); ty += 16;
    }
    if (showGivers) {
        DrawText(lineGivers, tx, ty, 11, Fade(LIME, 0.7f)); ty += 16;
    }
    if (showBounty) {
        DrawText(lineBounty, tx, ty, 11, Fade(GOLD, 0.7f)); ty += 16;
    }
    if (showOutput) {
        DrawText(lineOutput, tx, ty, 11, Fade(GREEN, 0.7f));
    }
}

// ---- Market overlay (M key) ----
// Full price + stock table across all settlements — useful for merchant planning.

void HUD::DrawMarketOverlay(const RenderSnapshot& snap) const {
    std::vector<RenderSnapshot::SettlementStatus> ws;
    std::vector<RenderSnapshot::RoadEntry>        roads;
    int playerRep = 0;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        ws        = snap.worldStatus;
        roads     = snap.roads;
        playerRep = snap.playerReputation;
    }
    // Tax-adjusted margin: net = sell*(1-tax) - buy, where tax falls with reputation
    float repDiscount  = std::min(0.10f, playerRep * 0.0005f);
    float effectiveTax = 0.20f - repDiscount;
    if (ws.empty()) return;

    static const int MX = 330, MY = 10, ML_H = 18, MFONT = 12;
    static const int C_NAME = MX+8, C_FOOD = MX+120, C_WAT = MX+230, C_WD = MX+340;

    int rows = 1 + (int)ws.size();
    int pw   = 410;
    int ph   = rows * ML_H + 16;

    DrawRectangle(MX, MY, pw, ph, Fade(BLACK, 0.8f));
    DrawRectangleLines(MX, MY, pw, ph, LIGHTGRAY);

    // Header
    DrawText("[M] Market Prices", MX + 8, MY + 4, MFONT, YELLOW);
    DrawText("Settlement", C_NAME, MY + 4 + ML_H, MFONT, Fade(LIGHTGRAY,0.8f));
    DrawText("Food stk@p",  C_FOOD, MY + 4 + ML_H, MFONT, Fade(GREEN,0.8f));
    DrawText("Water stk@p", C_WAT,  MY + 4 + ML_H, MFONT, Fade(SKYBLUE,0.8f));
    DrawText("Wood stk@p",  C_WD,   MY + 4 + ML_H, MFONT, Fade(BROWN,0.8f));

    for (int i = 0; i < (int)ws.size(); ++i) {
        const auto& s = ws[i];
        int y = MY + 4 + ML_H * (i + 2);

        // Trend symbol: '+' = rising (red = prices high = bad for buyers),
        //               '-' = falling (green), '=' = stable (grey)
        auto trendStr = [](char t) -> const char* {
            return (t == '+') ? "^" : (t == '-') ? "v" : " ";
        };
        auto trendCol = [](char t) -> Color {
            return (t == '+') ? RED : (t == '-') ? GREEN : Fade(LIGHTGRAY, 0.4f);
        };

        char nameBuf[20], foodBuf[20], watBuf[20], wdBuf[20];
        std::snprintf(nameBuf, sizeof(nameBuf), "%.12s [%d]", s.name.c_str(), s.pop);
        std::snprintf(foodBuf, sizeof(foodBuf), "%.0f@%.1fg", s.food,  s.foodPrice);
        std::snprintf(watBuf,  sizeof(watBuf),  "%.0f@%.1fg", s.water, s.waterPrice);
        std::snprintf(wdBuf,   sizeof(wdBuf),   "%.0f@%.1fg", s.wood,  s.woodPrice);

        Color nameCol = (s.pop == 0) ? DARKGRAY : WHITE;
        Color foodCol = (s.food  < 10.f) ? RED : (s.food  < 30.f) ? ORANGE : GREEN;
        Color watCol  = (s.water < 10.f) ? RED : (s.water < 30.f) ? ORANGE : SKYBLUE;
        Color wdCol   = (s.wood  < 10.f) ? RED : (s.wood  < 30.f) ? ORANGE : BROWN;

        DrawText(nameBuf, C_NAME, y, MFONT, nameCol);
        DrawText(foodBuf, C_FOOD, y, MFONT, foodCol);
        DrawText(trendStr(s.foodPriceTrend),  C_FOOD - 10, y, MFONT, trendCol(s.foodPriceTrend));
        DrawText(watBuf,  C_WAT,  y, MFONT, watCol);
        DrawText(trendStr(s.waterPriceTrend), C_WAT  - 10, y, MFONT, trendCol(s.waterPriceTrend));
        DrawText(wdBuf,   C_WD,   y, MFONT, wdCol);
        DrawText(trendStr(s.woodPriceTrend),  C_WD   - 10, y, MFONT, trendCol(s.woodPriceTrend));
    }

    // ---- Best Trade Routes section ----
    // For each non-blocked road compute the best single-resource arbitrage.
    // Show top 5 opportunities sorted by profit margin (buy-low sell-high price diff).
    if (roads.empty()) return;

    struct TradeOpp {
        std::string from, to;  // buy-at → sell-at
        const char* resource;
        float       buyPrice, sellPrice, netPerUnit;
        float       condition;
        bool        blocked;
        Color       resColor;
    };

    std::vector<TradeOpp> opps;
    opps.reserve(roads.size() * 6);  // up to 3 resources × 2 directions per road

    for (const auto& r : roads) {
        if (r.nameA.empty() || r.nameB.empty()) continue;

        // For each resource, consider both directions
        struct ResData { const char* name; float pA, pB; Color col; };
        ResData res[3] = {
            { "Food",  r.foodA,  r.foodB,  GREEN   },
            { "Water", r.waterA, r.waterB, SKYBLUE },
            { "Wood",  r.woodA,  r.woodB,  BROWN   },
        };
        for (const auto& rd : res) {
            if (rd.pA <= 0.f || rd.pB <= 0.f) continue;
            float buyP = 0.f, sellP = 0.f;
            std::string from, to;
            if (rd.pA > rd.pB * 1.05f) {
                // buy at B, sell at A
                buyP = rd.pB; sellP = rd.pA;
                from = r.nameB; to = r.nameA;
            } else if (rd.pB > rd.pA * 1.05f) {
                // buy at A, sell at B
                buyP = rd.pA; sellP = rd.pB;
                from = r.nameA; to = r.nameB;
            } else continue;  // less than 5% difference — not worth showing

            float net = sellP * (1.f - effectiveTax) - buyP;
            if (net <= 0.f) continue;  // not profitable after tax

            opps.push_back({ from, to, rd.name,
                             buyP, sellP, net,
                             r.condition, r.blocked, rd.col });
        }
    }

    // Sort descending by tax-adjusted net
    std::sort(opps.begin(), opps.end(),
              [](const TradeOpp& a, const TradeOpp& b){ return a.netPerUnit > b.netPerUnit; });

    static const int MAX_SHOW = 5;
    int showN = std::min((int)opps.size(), MAX_SHOW);
    if (showN == 0) return;

    static const int TX = MX, TY_START = MY + ph + 6;
    static const int TW = pw, TLH = 16, TFONT = 11;
    int tph = (2 + showN) * TLH + 8;

    DrawRectangle(TX, TY_START, TW, tph, Fade(BLACK, 0.8f));
    DrawRectangleLines(TX, TY_START, TW, tph, DARKGRAY);
    DrawText("Best trade routes (buy→sell, net/unit after tax):",
             TX + 6, TY_START + 4, TFONT, Fade(GOLD, 0.9f));

    // Column headers
    int hy = TY_START + 4 + TLH;
    DrawText("Route",    TX + 6,   hy, TFONT, Fade(LIGHTGRAY, 0.6f));
    DrawText("Res",      TX + 190, hy, TFONT, Fade(LIGHTGRAY, 0.6f));
    DrawText("Buy@",     TX + 235, hy, TFONT, Fade(LIGHTGRAY, 0.6f));
    DrawText("Sell@",    TX + 280, hy, TFONT, Fade(LIGHTGRAY, 0.6f));
    DrawText("Net/u",    TX + 330, hy, TFONT, Fade(LIGHTGRAY, 0.6f));
    DrawText("Rd",       TX + 375, hy, TFONT, Fade(LIGHTGRAY, 0.6f));

    for (int i = 0; i < showN; ++i) {
        const auto& op = opps[i];
        int ry = TY_START + 4 + TLH * (i + 2);

        char routeBuf[40], buyBuf[12], sellBuf[12], margBuf[10], condBuf[8];
        std::snprintf(routeBuf, sizeof(routeBuf), "%.10s→%.10s",
                      op.from.c_str(), op.to.c_str());
        std::snprintf(buyBuf,  sizeof(buyBuf),  "%.2fg", op.buyPrice);
        std::snprintf(sellBuf, sizeof(sellBuf), "%.2fg", op.sellPrice);
        std::snprintf(margBuf, sizeof(margBuf), "+%.2fg", op.netPerUnit);
        std::snprintf(condBuf, sizeof(condBuf), "%.0f%%", op.condition * 100.f);

        // Fade out blocked/degraded routes
        float alpha = op.blocked ? 0.4f : (op.condition < 0.25f ? 0.6f : 0.9f);
        Color routeCol = op.blocked ? Fade(RED, alpha) : Fade(WHITE, alpha);
        Color condCol  = (op.condition >= 0.75f) ? Fade(GREEN, alpha) :
                         (op.condition >= 0.50f) ? Fade(YELLOW, alpha) :
                         (op.condition >= 0.25f) ? Fade(ORANGE, alpha) : Fade(RED, alpha);
        Color margCol  = Fade(GOLD, alpha);

        DrawText(routeBuf, TX + 6,   ry, TFONT, routeCol);
        DrawText(op.resource, TX + 190, ry, TFONT, Fade(op.resColor, alpha));
        DrawText(buyBuf,   TX + 235, ry, TFONT, Fade(GREEN, alpha));
        DrawText(sellBuf,  TX + 280, ry, TFONT, Fade(RED, alpha));
        DrawText(margBuf,  TX + 330, ry, TFONT, margCol);
        DrawText(condBuf,  TX + 375, ry, TFONT, condCol);
        if (op.blocked) DrawText("[BLK]", TX + 350, ry, 9, Fade(RED, 0.7f));
    }
}

// ---- Debug overlay ----

void HUD::DrawDebugOverlay(const RenderSnapshot& snap) const {
    int agents, tickSpeed, speedIndex, pop, deaths, simSteps, entities, haulerCount;
    bool paused;
    std::string dbgSeasonName;
    float  dbgTemp, econTotal, econAvg, econRichest;
    std::string econRichestName;
    std::vector<RenderSnapshot::AgentEntry>        agentCopy;
    std::vector<RenderSnapshot::SettlementStatus>  settlCopy;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        agents    = (int)snap.agents.size();
        tickSpeed  = snap.tickSpeed;
        speedIndex = snap.speedIndex;
        pop       = snap.population;
        deaths    = snap.totalDeaths;
        paused    = snap.paused;
        simSteps  = snap.simStepsPerSec;
        entities  = snap.totalEntities;
        dbgSeasonName = snap.seasonName;
        dbgTemp   = snap.temperature;
        agentCopy = snap.agents;
        settlCopy = snap.worldStatus;
        haulerCount     = snap.econHaulerCount;
        econTotal       = snap.econTotalGold;
        econAvg         = snap.econAvgNpcWealth;
        econRichest     = snap.econRichestWealth;
        econRichestName = snap.econRichestName;
    }

    // Count NPCs (not haulers, not player) by behavior
    int nWorking = 0, nSleeping = 0, nIdle = 0, nMigrating = 0,
        nSeeking = 0, nHauling = 0;
    for (const auto& a : agentCopy) {
        if (a.role == RenderSnapshot::AgentRole::Hauler) { ++nHauling; continue; }
        if (a.role == RenderSnapshot::AgentRole::Player) continue;
        switch (a.behavior) {
            case AgentBehavior::Working:  ++nWorking;   break;
            case AgentBehavior::Sleeping: ++nSleeping;  break;
            case AgentBehavior::Migrating:++nMigrating; break;
            case AgentBehavior::Idle:     ++nIdle;      break;
            default:                      ++nSeeking;   break;  // seeking/satisfying
        }
    }

    static const int OX = 4, OY = 200, OW = 260, OLH = 17;
    char lines[20][64];
    std::snprintf(lines[0],  64, "[F1] DEBUG OVERLAY");
    std::snprintf(lines[1],  64, "Render FPS:   %d  (%.1f ms)", GetFPS(), GetFrameTime()*1000.f);
    int desiredSteps = (tickSpeed == 0) ? simSteps : 60 * tickSpeed;
    std::snprintf(lines[2],  64, "Sim steps/s:  %d%s", simSteps,
                  tickSpeed == 0 ? " (uncapped)" : "");
    std::snprintf(lines[3],  64, "Speed:        %d/5%s%s",
                  speedIndex, tickSpeed == 0 ? " [MAX]" : "",
                  paused ? " (PAUSED)" : "");
    std::snprintf(lines[4],  64, "Entities:     %d", entities);
    std::snprintf(lines[5],  64, "Population:   %d  Deaths: %d", pop, deaths);
    std::snprintf(lines[6],  64, "Season:       %s  %.1f°C", dbgSeasonName.c_str(), dbgTemp);
    std::snprintf(lines[7],  64, "--- NPC behavior ---");
    std::snprintf(lines[8],  64, "  Working:    %d", nWorking);
    std::snprintf(lines[9],  64, "  Sleeping:   %d", nSleeping);
    std::snprintf(lines[10], 64, "  Idle/leisure:%d", nIdle);
    std::snprintf(lines[11], 64, "  Seeking:    %d", nSeeking);
    std::snprintf(lines[12], 64, "  Migrating:  %d", nMigrating);
    std::snprintf(lines[13], 64, "  Haulers:    %d", nHauling);
    std::snprintf(lines[14], 64, "--- Economy ---");
    std::snprintf(lines[15], 64, "  Total gold: %.0fg", econTotal);
    std::snprintf(lines[16], 64, "  Avg NPC:    %.1fg", econAvg);
    std::snprintf(lines[17], 64, "  Richest:    %s (%.0fg)",
                  econRichestName.empty() ? "?" : econRichestName.c_str(), econRichest);
    std::snprintf(lines[18], 64, "  Haulers:    %d", haulerCount);

    int rows = 19;
    DrawRectangle(OX, OY, OW, rows*OLH + 8, Fade(BLACK, 0.75f));
    DrawRectangleLines(OX, OY, OW, rows*OLH + 8, DARKGRAY);
    for (int i = 0; i < rows; ++i) {
        Color col = (i == 0)  ? YELLOW :
                    (i == 7 || i == 14) ? Fade(LIGHTGRAY, 0.6f) :
                    LIGHTGRAY;
        DrawText(lines[i], OX+6, OY+4+i*OLH, 13, col);
    }

    // ---- Per-settlement breakdown (appended below global panel) ----
    if (!settlCopy.empty()) {
        static const int SX = OX, SLH = 15, SFONT = 11;
        int sy0 = OY + rows * OLH + 14;
        // Each settlement: 2 lines (name+pop, resources)
        int sph = (int)settlCopy.size() * SLH * 2 + SLH + 10;
        int sw  = OW + 60;   // slightly wider for resource data

        DrawRectangle(SX, sy0, sw, sph, Fade(BLACK, 0.72f));
        DrawRectangleLines(SX, sy0, sw, sph, DARKGRAY);
        DrawText("Settlements:", SX + 4, sy0 + 4, SFONT, Fade(YELLOW, 0.8f));

        int sy = sy0 + 4 + SLH;
        for (const auto& s : settlCopy) {
            // Line 1: Name [pop] treasury trend
            char line1[64], line2[80];
            const char* trendC = (s.popTrend == '+') ? "+" :
                                 (s.popTrend == '-') ? "-" : "=";
            Color trendCol = (s.popTrend == '+') ? GREEN :
                             (s.popTrend == '-') ? RED : Fade(LIGHTGRAY, 0.5f);
            std::snprintf(line1, sizeof(line1), "%-10.10s [%d] %.0fg",
                          s.name.c_str(), s.pop, s.treasury);
            Color nameCol = s.hasEvent ? Color{255,200,80,230} : WHITE;
            DrawText(line1, SX + 4, sy, SFONT, nameCol);
            DrawText(trendC, SX + sw - 14, sy, SFONT, trendCol);
            if (s.hasEvent && !s.eventName.empty()) {
                char evBuf[16];
                std::snprintf(evBuf, sizeof(evBuf), "!%-.8s", s.eventName.c_str());
                DrawText(evBuf, SX + 4 + MeasureText(line1, SFONT) + 4,
                         sy, 9, Fade(ModifierColour(s.eventName), 0.85f));
            }
            sy += SLH;

            // Line 2: F/W/D stocks with color coding
            std::snprintf(line2, sizeof(line2),
                          "  F:%.0f W:%.0f D:%.0f  @%.1f/%.1f/%.1f T:%d",
                          s.food, s.water, s.wood,
                          s.foodPrice, s.waterPrice, s.woodPrice, s.tradeVolume);
            Color l2col = Fade(LIGHTGRAY, 0.65f);
            // Color by worst resource
            if (s.food < 10.f || s.water < 10.f || s.wood < 10.f)
                l2col = Fade(RED, 0.75f);
            else if (s.food < 30.f || s.water < 30.f || s.wood < 30.f)
                l2col = Fade(ORANGE, 0.75f);
            DrawText(line2, SX + 4, sy, SFONT - 1, l2col);
            sy += SLH;
        }
    }

    // ---- Per-system profiler (appended below settlements) ----
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        if (!snap.profiling.empty()) {
            // Compute panel position below global debug + settlement panels
            int profY = OY + rows * OLH + 14;
            if (!settlCopy.empty()) {
                int sph = (int)settlCopy.size() * 15 * 2 + 15 + 10;
                profY += sph + 6;
            }
            int profRows = (int)snap.profiling.size() + 2;  // header + total + entries
            int profH = profRows * 15 + 8;
            int profW = OW + 60;

            DrawRectangle(OX, profY, profW, profH, Fade(BLACK, 0.75f));
            DrawRectangleLines(OX, profY, profW, profH, DARKGRAY);
            DrawText("--- Profiler (us/step) ---", OX + 4, profY + 4, 11, Fade(YELLOW, 0.8f));

            int py = profY + 4 + 15;
            float totalUs = 0.f;

            // Sort by cost descending for display
            auto sorted = snap.profiling;
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b) { return a.avgUs > b.avgUs; });

            for (const auto& entry : sorted) {
                totalUs += entry.avgUs;
                char pbuf[64];
                if (entry.avgUs >= 1000.f)
                    std::snprintf(pbuf, sizeof(pbuf), "%-15s %7.1f ms", entry.name.c_str(), entry.avgUs / 1000.f);
                else
                    std::snprintf(pbuf, sizeof(pbuf), "%-15s %7.0f us", entry.name.c_str(), entry.avgUs);
                Color pcol = (entry.avgUs > 500.f) ? Fade(RED, 0.9f) :
                             (entry.avgUs > 100.f) ? Fade(ORANGE, 0.8f) :
                             Fade(LIGHTGRAY, 0.7f);
                DrawText(pbuf, OX + 4, py, 11, pcol);
                py += 15;
            }
            // Total line
            char tbuf[64];
            std::snprintf(tbuf, sizeof(tbuf), "TOTAL           %7.1f ms", totalUs / 1000.f);
            DrawText(tbuf, OX + 4, py, 11, Fade(WHITE, 0.9f));
        }
    }
}

// ---- Minimap ----
// Always-visible overview in the bottom-right corner.
// World bounds: 2400 × 720 world units.
// Shows: roads (color by condition), settlements (dot, color by health), player (white dot).

void HUD::DrawMinimap(const RenderSnapshot& snap) const {
    static constexpr float WORLD_W = 2400.f;
    static constexpr float WORLD_H  =  720.f;

    // Minimap display rectangle (screen coords)
    static constexpr int MM_W  = 240;   // pixel width
    static constexpr int MM_H  =  72;   // pixel height (same aspect as world: 2400/720 → 10:3)
    static constexpr int MM_X  = SCREEN_W - MM_W - 6;
    static constexpr int MM_Y  = SCREEN_H - MM_H - 6;

    // Scale factors
    auto worldToMM = [&](float wx, float wy) -> Vector2 {
        return {
            MM_X + (wx / WORLD_W) * MM_W,
            MM_Y + (wy / WORLD_H) * MM_H
        };
    };

    std::vector<RenderSnapshot::SettlementEntry>   settls;
    std::vector<RenderSnapshot::RoadEntry>         roads;
    float playerX, playerY;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        settls  = snap.settlements;
        roads   = snap.roads;
        playerX = snap.playerWorldX;
        playerY = snap.playerWorldY;
    }

    // Background with title area
    DrawRectangle(MM_X - 1, MM_Y - 13, MM_W + 2, MM_H + 14, Fade(BLACK, 0.70f));
    DrawRectangle(MM_X - 1, MM_Y - 13, MM_W + 2, 12, Fade(WHITE, 0.04f));
    DrawText("MAP", MM_X + 2, MM_Y - 12, 8, Fade(LIGHTGRAY, 0.5f));
    DrawRectangle(MM_X - 1, MM_Y - 1, MM_W + 2, 1, Fade(LIGHTGRAY, 0.15f));
    DrawRectangleLines(MM_X - 1, MM_Y - 13, MM_W + 2, MM_H + 14, Fade(LIGHTGRAY, 0.25f));

    // Roads
    for (const auto& r : roads) {
        Vector2 a = worldToMM(r.x1, r.y1);
        Vector2 b = worldToMM(r.x2, r.y2);
        Color col = r.blocked ? Fade(RED, 0.6f) :
                    (r.condition >= 0.5f) ? Fade(BEIGE, 0.45f) :
                    Fade(ORANGE, 0.50f);
        DrawLineV(a, b, col);
    }

    // Settlements — colored by health (food/water/wood stock)
    for (const auto& s : settls) {
        Vector2 p = worldToMM(s.x, s.y);
        // Color: red = some stock critically low, orange = any low, green = ok
        bool critical = (s.foodStock < 5.f || s.waterStock < 5.f);
        bool low      = (s.foodStock < 20.f || s.waterStock < 20.f);
        Color dotCol  = critical ? RED : low ? ORANGE : GREEN;
        if (s.pop == 0) dotCol = DARKGRAY;
        // Active event → yellow border
        float dotR = s.selected ? 4.f : 3.f;
        DrawCircleV(p, dotR + 1.f, Fade(s.selected ? WHITE : BLACK, 0.7f));
        DrawCircleV(p, dotR,       dotCol);
        if (!s.modifierName.empty())
            DrawCircleLines((int)p.x, (int)p.y, dotR + 2.f, Fade(ModifierColour(s.modifierName), 0.8f));
        // Settlement name label (truncated)
        if (!s.name.empty()) {
            char shortName[8];
            std::snprintf(shortName, sizeof(shortName), "%.6s", s.name.c_str());
            int labelW = MeasureText(shortName, 7);
            int lx = (int)p.x - labelW / 2;
            // Clamp within minimap bounds
            if (lx < MM_X + 1) lx = MM_X + 1;
            if (lx + labelW > MM_X + MM_W - 1) lx = MM_X + MM_W - labelW - 1;
            DrawText(shortName, lx, (int)p.y - (int)dotR - 8, 7, Fade(WHITE, 0.5f));
        }
    }

    // Player position — small white dot
    Vector2 pp = worldToMM(playerX, playerY);
    DrawCircleV(pp, 2.5f, WHITE);

}

// ---- Road hover tooltip ----
// Shows price differential between the two connected settlements.

void HUD::DrawRoadTooltip(const RenderSnapshot& snap, const Camera2D& cam) const {
    Vector2 mouse = GetMousePosition();
    Vector2 world = GetScreenToWorld2D(mouse, cam);

    std::vector<RenderSnapshot::RoadEntry> roads;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        roads = snap.roads;
    }

    // Find the closest road (point-to-segment distance)
    const RenderSnapshot::RoadEntry* best = nullptr;
    float bestDist = 25.f;   // max hover distance in world units

    for (const auto& r : roads) {
        // Point-to-segment distance
        float dx = r.x2 - r.x1, dy = r.y2 - r.y1;
        float lenSq = dx*dx + dy*dy;
        float t = 0.f;
        if (lenSq > 0.f)
            t = std::max(0.f, std::min(1.f,
                ((world.x - r.x1) * dx + (world.y - r.y1) * dy) / lenSq));
        float cx = r.x1 + t * dx, cy = r.y1 + t * dy;
        float d = std::sqrt((world.x - cx)*(world.x - cx) +
                            (world.y - cy)*(world.y - cy));
        if (d < bestDist) { bestDist = d; best = &r; }
    }
    if (!best) return;

    // Format: show settlement names + price table + condition + status
    char line1[80], line2[48], line3[48], line4[48], line5[48], line6[48], line7[64] = {};
    std::snprintf(line1, sizeof(line1), "%s ←→ %s%s",
                  best->nameA.c_str(), best->nameB.c_str(),
                  best->blocked ? "  [BLOCKED]" : "");

    // Price comparison: arrows show profit direction
    auto arrow = [](float pA, float pB) -> const char* {
        if (pA > pB * 1.15f) return "→";   // A expensive → sell A→B
        if (pB > pA * 1.15f) return "←";   // B expensive → sell B→A
        return "=";
    };
    std::snprintf(line2, sizeof(line2), "Food:  %4.1f %s %4.1f",
                  best->foodA,  arrow(best->foodA,  best->foodB),  best->foodB);
    std::snprintf(line3, sizeof(line3), "Water: %4.1f %s %4.1f",
                  best->waterA, arrow(best->waterA, best->waterB), best->waterB);
    std::snprintf(line4, sizeof(line4), "Wood:  %4.1f %s %4.1f",
                  best->woodA,  arrow(best->woodA,  best->woodB),  best->woodB);

    const char* condStr = (best->condition >= 0.75f) ? "Good"   :
                          (best->condition >= 0.50f) ? "Fair"   :
                          (best->condition >= 0.25f) ? "Poor"   : "Critical";
    std::snprintf(line5, sizeof(line5), "Condition: %.0f%%  [%s]",
                  best->condition * 100.f, condStr);
    std::snprintf(line6, sizeof(line6), "← %s prices / %s prices →",
                  best->nameA.c_str(), best->nameB.c_str());

    // Relationship line: show rivalry/alliance status based on each side's view
    // relAtoB = A's opinion of B (how much A benefits); relBtoA = B's opinion of A
    bool hasRelInfo = (best->relAtoB != 0.f || best->relBtoA != 0.f);
    Color relColor = LIGHTGRAY;
    if (hasRelInfo) {
        // Determine status from importer's perspective (who resents whom)
        bool aRivalsB = (best->relAtoB < -0.5f);   // A resents B's exports
        bool bRivalsA = (best->relBtoA < -0.5f);   // B resents A's exports
        bool aAlliedB = (best->relAtoB > 0.5f);
        bool bAlliedA = (best->relBtoA > 0.5f);
        if (aRivalsB || bRivalsA) {
            const char* who = (aRivalsB && bRivalsA) ? "Mutual rivals"
                            : aRivalsB ? (std::string(best->nameA) + " resents " + best->nameB).c_str()
                            :            (std::string(best->nameB) + " resents " + best->nameA).c_str();
            std::snprintf(line7, sizeof(line7), "Relations: %s (+10%% tariff)", who);
            relColor = RED;
        } else if (aAlliedB && bAlliedA) {
            std::snprintf(line7, sizeof(line7), "Allied: -5%% tax, faster price convergence");
            relColor = GREEN;
        } else {
            // Show numeric scores when not yet at threshold
            std::snprintf(line7, sizeof(line7), "Relations: %s→%s %.2f  %s→%s %.2f",
                best->nameA.c_str(), best->nameB.c_str(), best->relAtoB,
                best->nameB.c_str(), best->nameA.c_str(), best->relBtoA);
            relColor = Fade(LIGHTGRAY, 0.7f);
        }
    }

    // Bandit warning line
    char line8[48] = {};
    if (best->banditCount > 0)
        std::snprintf(line8, sizeof(line8), "Bandits: %d", best->banditCount);

    // Position tooltip at midpoint of road
    float midWX = (best->x1 + best->x2) * 0.5f;
    float midWY = (best->y1 + best->y2) * 0.5f;
    Vector2 screen = GetWorldToScreen2D({ midWX, midWY }, cam);

    int extraLines = ((line7[0] != '\0') ? 1 : 0) + ((line8[0] != '\0') ? 1 : 0);
    int w = std::max({ MeasureText(line1, 12), MeasureText(line2, 11),
                       MeasureText(line3, 11), MeasureText(line4, 11),
                       MeasureText(line5, 11), MeasureText(line6, 10),
                       (line7[0] != '\0') ? MeasureText(line7, 11) : 0,
                       (line8[0] != '\0') ? MeasureText(line8, 11) : 0 }) + 12;
    int h = (6 + extraLines) * 16 + 4;
    int tx = (int)screen.x - w / 2, ty = (int)screen.y - h - 8;
    if (tx < 4) tx = 4;
    if (tx + w > SCREEN_W - 4) tx = SCREEN_W - 4 - w;
    if (ty < 4) ty = (int)screen.y + 10;

    // Condition color: green → yellow → orange → red
    Color condCol = (best->condition >= 0.75f) ? GREEN  :
                    (best->condition >= 0.50f) ? YELLOW :
                    (best->condition >= 0.25f) ? ORANGE : RED;

    DrawRectangle(tx - 4, ty - 2, w, h, Fade(BLACK, 0.78f));
    Color c1 = best->blocked ? RED : WHITE;
    DrawText(line1, tx, ty,       12, c1);
    DrawText(line2, tx, ty + 16,  11, GREEN);
    DrawText(line3, tx, ty + 32,  11, SKYBLUE);
    DrawText(line4, tx, ty + 48,  11, BROWN);
    DrawText(line5, tx, ty + 64,  11, condCol);
    DrawText(line6, tx, ty + 80,  10, Fade(LIGHTGRAY, 0.5f));
    int ly = ty + 96;
    if (line7[0] != '\0') { DrawText(line7, tx, ly, 11, relColor); ly += 16; }
    if (line8[0] != '\0') DrawText(line8, tx, ly, 11, Fade(RED, 0.7f));
}

// ---- DrawNeedBar ----

void HUD::DrawNeedBar(int x, int y, float value, float critThreshold,
                      const char* label, Color barColor) const {
    // Label with color hint matching the bar
    Color labelCol = (value < critThreshold) ? Fade(RED, 0.9f) : LIGHTGRAY;
    DrawText(label, x, y, 14, labelCol);

    int barX = x + 60;
    // Background: darker when emptier for depth
    DrawRectangle(barX, y, BAR_W, BAR_H, Fade(WHITE, 0.10f));

    float clamped = std::max(0.f, std::min(1.f, value));
    int   fillW   = (int)(clamped * BAR_W);
    bool  isCrit  = (value < critThreshold);
    Color fill    = isCrit ? RED : barColor;

    // Critical pulse: bar flashes brighter when below threshold
    if (isCrit) {
        float pulse = 0.5f + 0.5f * sinf((float)GetTime() * 4.f);
        fill = Fade(RED, 0.6f + 0.4f * pulse);
    }

    // Main fill
    if (fillW > 0) {
        DrawRectangle(barX, y, fillW, BAR_H, fill);
        // Subtle highlight on top half for depth
        DrawRectangle(barX, y, fillW, BAR_H / 2, Fade(WHITE, 0.12f));
    }

    // Border
    DrawRectangleLines(barX, y, BAR_W, BAR_H, Fade(WHITE, 0.6f));

    // Percentage text inside the bar
    char pctBuf[8];
    std::snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", clamped * 100.f);
    int textW = MeasureText(pctBuf, 10);
    int textX = barX + (BAR_W - textW) / 2;
    // Draw with shadow for readability
    DrawText(pctBuf, textX + 1, y + 3, 10, Fade(BLACK, 0.6f));
    DrawText(pctBuf, textX, y + 2, 10, WHITE);
}

// ---- Notification overlay -----------------------------------------------

static bool ContainsAny(const std::string& s,
                         std::initializer_list<const char*> keywords) {
    for (const char* kw : keywords)
        if (s.find(kw) != std::string::npos) return true;
    return false;
}

void HUD::UpdateNotifications(const RenderSnapshot& snap) {
    // Tick down existing notifications
    float dt = GetFrameTime();
    for (auto& n : m_notifications) n.timeLeft -= dt;
    m_notifications.erase(
        std::remove_if(m_notifications.begin(), m_notifications.end(),
                       [](const Notification& n){ return n.timeLeft <= 0.f; }),
        m_notifications.end());

    // Scan new log entries for critical keywords
    std::vector<EventLog::Entry> entries;
    {
        std::lock_guard<std::mutex> lk(snap.mutex);
        entries = snap.logEntries;
    }
    int newSize = (int)entries.size();
    if (newSize <= m_lastLogSize) { m_lastLogSize = newSize; return; }

    // entries[0] is newest — check entries added since last frame
    int newCount = newSize - m_lastLogSize;
    m_lastLogSize = newSize;
    for (int i = 0; i < std::min(newCount, newSize); ++i) {
        const std::string& msg = entries[i].message;

        Color col = WHITE;
        bool  show = false;

        if (ContainsAny(msg, {"PLAGUE erupts", "PLAGUE spreads"})) {
            col = Color{200, 80, 240, 255}; show = true;
        } else if (ContainsAny(msg, {"has COLLAPSED"})) {
            col = Fade(RED, 0.95f); show = true;  // settlement collapse
        } else if (ContainsAny(msg, {"COLLAPSED from disrepair"})) {
            col = Fade(ORANGE, 0.90f); show = true;  // facility collapse
        } else if (ContainsAny(msg, {"EARTHQUAKE"})) {
            col = Fade(RED, 0.95f); show = true;
        } else if (ContainsAny(msg, {"FIRE at"})) {
            col = Color{255, 120, 30, 255}; show = true;
        } else if (ContainsAny(msg, {"BLIZZARD"})) {
            col = Fade(SKYBLUE, 0.95f); show = true;
        } else if (ContainsAny(msg, {"HEAT WAVE"})) {
            col = Color{255, 180, 50, 255}; show = true;
        } else if (ContainsAny(msg, {"DISEASE", "DROUGHT"})) {
            col = Fade(ORANGE, 0.95f); show = true;
        } else if (ContainsAny(msg, {"BANDITS blocking"})) {
            col = Fade(RED, 0.85f); show = true;
        } else if (ContainsAny(msg, {"HARVEST BOUNTY", "TRADE BOOM", "MIGRATION WAVE", "FESTIVAL"})) {
            col = Fade(GREEN, 0.95f); show = true;
        } else if (ContainsAny(msg, {"RAINSTORM", "OFF-MAP CONVOY", "LUMBER WINDFALL"})) {
            col = Fade(SKYBLUE, 0.85f); show = true;
        } else if (ContainsAny(msg, {"reaches", " citizens!"})) {
            col = Fade(GOLD, 0.95f); show = true;  // population milestone
        } else if (ContainsAny(msg, {"funded road"})) {
            col = Fade(GREEN, 0.85f); show = true;  // new autonomous road
        } else if (ContainsAny(msg, {"SKILLED IMMIGRANT"})) {
            col = Fade(GOLD, 0.90f); show = true;
        } else if (ContainsAny(msg, {"MARKET CRISIS"})) {
            col = Fade(ORANGE, 0.95f); show = true;
        }

        if (show) {
            // Don't stack identical messages
            bool dup = false;
            for (const auto& existing : m_notifications)
                if (existing.message == msg) { dup = true; break; }
            if (!dup)
                m_notifications.push_back({ msg, 5.f, col });
        }
    }
}

void HUD::DrawNotifications() {
    if (m_notifications.empty()) return;

    static const float NOTIF_DURATION = 5.f;
    const int cx = SCREEN_W / 2;
    int ny = SCREEN_H / 2 - (int)(m_notifications.size() * 22) / 2;

    for (const auto& n : m_notifications) {
        float alpha = std::min(1.f, n.timeLeft / 1.0f);  // fade out in last 1s
        Color col = n.color;
        col.a = (unsigned char)(col.a * alpha);

        int tw = MeasureText(n.message.c_str(), 14);
        int bx = cx - tw / 2 - 6, bw = tw + 12, bh = 20;
        DrawRectangle(bx, ny - 2, bw, bh, Fade(BLACK, 0.55f * alpha));
        DrawText(n.message.c_str(), cx - tw / 2, ny, 14, col);
        ny += 24;
    }
}
