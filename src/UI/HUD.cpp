#include "HUD.h"
#include "ECS/Components.h"
#include "raylib.h"
#include <algorithm>
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
    int   day, hour, minute, tickSpeed, pop, deaths;
    bool  paused, roadBlocked, playerAlive;
    float hungerPct, thirstPct, energyPct, heatPct;
    float hungerCrit, thirstCrit, energyCrit, heatCrit;
    float playerAgeDays, playerMaxDays, playerGold;
    float playerFarmSkill, playerWaterSkill, playerWoodSkill;
    float temperature;
    bool  playerInPlagueZone;
    int   playerReputation;
    std::string playerRank;
    std::map<ResourceType, int> playerInventory;
    int         playerInventoryCapacity = 15;
    std::string tradeHint;
    AgentBehavior behavior;
    Season season;
    std::vector<RenderSnapshot::TradeRecord> tradeLedger;

    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        day         = snap.day;
        hour        = snap.hour;
        minute      = snap.minute;
        tickSpeed   = snap.tickSpeed;
        pop         = snap.population;
        deaths      = snap.totalDeaths;
        paused      = snap.paused;
        roadBlocked = snap.roadBlocked;
        playerAlive = snap.playerAlive;
        hungerPct   = snap.hungerPct;   hungerCrit  = snap.hungerCrit;
        thirstPct   = snap.thirstPct;   thirstCrit  = snap.thirstCrit;
        energyPct   = snap.energyPct;   energyCrit  = snap.energyCrit;
        heatPct     = snap.heatPct;     heatCrit    = snap.heatCrit;
        behavior    = snap.playerBehavior;
        playerAgeDays   = snap.playerAgeDays;
        playerMaxDays   = snap.playerMaxDays;
        playerGold      = snap.playerGold;
        playerFarmSkill  = snap.playerFarmSkill;
        playerWaterSkill = snap.playerWaterSkill;
        playerWoodSkill  = snap.playerWoodSkill;
        playerInventory         = snap.playerInventory;
        playerInventoryCapacity = snap.playerInventoryCapacity;
        tradeHint            = snap.tradeHint;
        tradeLedger          = snap.tradeLedger;
        season               = snap.season;
        temperature          = snap.temperature;
        playerInPlagueZone   = snap.playerInPlagueZone;
        playerReputation     = snap.playerReputation;
        playerRank           = snap.playerRank;
    }

    // ---- Player need bars (top-left) ----
    if (playerAlive) {
        int invItemLines = std::max(1, (int)playerInventory.size()); // at least 1 for "(empty)"
        int skillLine    = (playerFarmSkill >= 0.f) ? 1 : 0;
        int tradeLines   = tradeHint.empty() ? 0 : 1;
        int ledgerLines  = tradeLedger.empty() ? 0 : (1 + std::min((int)tradeLedger.size(), 4));
        int plagueLines  = playerInPlagueZone ? 1 : 0;
        // +1 for cargo header row (always shown), invItemLines for item rows, +1 for reputation
        DrawRectangle(4, 4, 320, BAR_GAP * (7 + skillLine) + 90 + (1 + invItemLines) * 16 + tradeLines * 14 + ledgerLines * 11 + plagueLines * 14 + 4, Fade(BLACK, 0.55f));
        DrawNeedBar(BAR_X, BAR_Y0 + BAR_GAP * 0, hungerPct, hungerCrit, "Hunger", GREEN);
        DrawNeedBar(BAR_X, BAR_Y0 + BAR_GAP * 1, thirstPct, thirstCrit, "Thirst", SKYBLUE);
        DrawNeedBar(BAR_X, BAR_Y0 + BAR_GAP * 2, energyPct, energyCrit, "Energy", YELLOW);
        DrawNeedBar(BAR_X, BAR_Y0 + BAR_GAP * 3, heatPct,   heatCrit,   "Heat",   ORANGE);

        // Age row
        int ageY = BAR_Y0 + BAR_GAP * 4 + 2;
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
        if (playerFarmSkill >= 0.f) {
            char skBuf[64];
            auto skCol = [](float s) -> Color {
                return (s >= 0.65f) ? Fade(GOLD, 0.9f) : (s >= 0.35f) ? GREEN : Fade(GRAY, 0.8f);
            };
            std::snprintf(skBuf, sizeof(skBuf), "Farm:%.0f%% Wtr:%.0f%% Wood:%.0f%%",
                          playerFarmSkill*100.f, playerWaterSkill*100.f, playerWoodSkill*100.f);
            float bestSk = std::max({playerFarmSkill, playerWaterSkill, playerWoodSkill});
            DrawText("Skills:", BAR_X, skillsEndY, 11, LIGHTGRAY);
            DrawText(skBuf, BAR_X + 52, skillsEndY, 11, skCol(bestSk));
            skillsEndY += BAR_GAP;
        }

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

            int ci = 0;
            for (const auto& [type, qty] : playerInventory) {
                if (qty <= 0) continue;
                const char* rname = (type == ResourceType::Food)  ? "Food"  :
                                    (type == ResourceType::Water) ? "Water" :
                                    (type == ResourceType::Wood)  ? "Wood"  : "?";
                Color rcol = (type == ResourceType::Food)  ? GREEN  :
                             (type == ResourceType::Water) ? SKYBLUE : BROWN;
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
        std::snprintf(speedBuf, sizeof(speedBuf), paused ? "PAUSED" : "%dx", tickSpeed);
        std::snprintf(popBuf,   sizeof(popBuf),   "Pop: %d  Deaths: %d", pop, deaths);
        std::snprintf(fpsBuf,   sizeof(fpsBuf),   "FPS: %d  (%.1f ms)",
                      GetFPS(), GetFrameTime() * 1000.f);
        std::snprintf(seasBuf, sizeof(seasBuf), "%s  %.0f°C",
                      SeasonName(season), temperature);

        int pw = std::max({ MeasureText(timeBuf, 16), MeasureText(speedBuf, 14),
                            MeasureText(seasBuf, 13),
                            MeasureText(popBuf, 13),  MeasureText(fpsBuf, 12) }) + 16;
        int px = SCREEN_W - pw - 4;

        Color seasonColor = (season == Season::Spring) ? GREEN  :
                            (season == Season::Summer) ? YELLOW :
                            (season == Season::Autumn) ? ORANGE : SKYBLUE;
        // Temperature color: blue below 0, grey near 0, normal above
        Color tempColor = (temperature < 0.f) ? Color{150,200,255,255} :
                          (temperature < 5.f) ? LIGHTGRAY : seasonColor;

        DrawRectangle(px, 4, pw, 98, Fade(BLACK, 0.55f));
        DrawText(timeBuf,  px + 8,  8, 16, WHITE);
        DrawText(speedBuf, px + 8, 28, 14, paused ? ORANGE : LIGHTGRAY);
        DrawText(seasBuf,  px + 8, 46, 13, tempColor);
        DrawText(popBuf,   px + 8, 63, 13, LIGHTGRAY);
        DrawText(fpsBuf,   px + 8, 80, 12, Fade(LIGHTGRAY, 0.6f));
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

void HUD::DrawWorldStatus(const RenderSnapshot& snap) const {
    std::vector<RenderSnapshot::SettlementStatus> ws;
    Season season;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        ws     = snap.worldStatus;
        season = snap.season;
    }
    if (ws.empty()) return;

    // Show Wood stock in Autumn/Winter when fuel matters (hidden in Spring/Summer to save space).
    bool showWood = (season == Season::Autumn || season == Season::Winter);

    static const int STATUS_FONT = 12;
    char bufs[4][128]; bool hasEvent[4] = {}; bool hungerCrisis[4] = {}; std::string eventNames[4]; int count = 0;
    for (const auto& s : ws) {
        if (count >= 4) break;
        // Format: "Name F:stock@price W:stock@price G:treasury [pop+haulers]"
        const char* fmt_base  = showWood
            ? "%s  F:%.0f  W:%.0f  Wd:%.0f  G:%.0f  [%d+%d]"
            : "%s  F:%.0f@%.1f  W:%.0f@%.1f  G:%.0f  [%d+%d]";
        const char* fmt_event = showWood
            ? "%s  F:%.0f  W:%.0f  Wd:%.0f  G:%.0f  [%d+%d] [%s]"
            : "%s  F:%.0f@%.1f  W:%.0f@%.1f  G:%.0f  [%d+%d] [%s]";

        // Population trend symbol
        const char* trendSym = (s.popTrend == '+') ? "↑" :
                               (s.popTrend == '-') ? "↓" : "";

        if (s.hasEvent) {
            if (showWood)
                std::snprintf(bufs[count], 128, fmt_event,
                    s.name.c_str(), s.food, s.water, s.wood, s.treasury,
                    s.pop, s.haulers, s.eventName.c_str());
            else
                std::snprintf(bufs[count], 128, fmt_event,
                    s.name.c_str(), s.food, s.foodPrice, s.water, s.waterPrice,
                    s.treasury, s.pop, s.haulers, s.eventName.c_str());
        } else {
            if (showWood)
                std::snprintf(bufs[count], 128, fmt_base,
                    s.name.c_str(), s.food, s.water, s.wood, s.treasury,
                    s.pop, s.haulers);
            else
                std::snprintf(bufs[count], 128, fmt_base,
                    s.name.c_str(), s.food, s.foodPrice, s.water, s.waterPrice,
                    s.treasury, s.pop, s.haulers);
        }
        // Append trend indicator (UTF-8 arrows may not render — use ASCII instead)
        if (s.popTrend == '+' || s.popTrend == '-') {
            size_t len = strlen(bufs[count]);
            bufs[count][len] = ' ';
            bufs[count][len+1] = s.popTrend;
            bufs[count][len+2] = '\0';
        }
        (void)trendSym;  // suppress warning if UTF-8 arrows not used
        hasEvent[count]    = s.hasEvent;
        hungerCrisis[count] = s.hungerCrisis;
        eventNames[count]  = s.eventName;
        ++count;
    }

    // Second pass: record child counts and food prefix widths per entry.
    int childCounts[4] = {};
    char foodPfx[4][64] = {};  // prefix up to food number, for measuring hunger "!" position
    {
        int ci = 0;
        for (const auto& s : ws) {
            if (ci >= 4) break;
            childCounts[ci] = s.childCount;
            std::snprintf(foodPfx[ci], sizeof(foodPfx[ci]), "%s  F:%.0f", s.name.c_str(), s.food);
            ++ci;
        }
    }
    // Pre-build child suffix strings (e.g. " (3c)") for width measurement and drawing.
    char childSuffix[4][16] = {};
    for (int i = 0; i < count; ++i) {
        if (childCounts[i] > 0)
            std::snprintf(childSuffix[i], sizeof(childSuffix[i]), " (%dc)", childCounts[i]);
    }

    // Pre-build morale label strings (e.g. " M:72%+") for width measurement and drawing.
    // Morale trend tracking: sample once per second, show +/- if delta > 0.03
    static std::map<std::string, float> s_prevMorale;
    static float s_moraleSampleTimer = 0.f;
    s_moraleSampleTimer += GetFrameTime();
    bool moraleSampleNow = (s_moraleSampleTimer >= 1.f);
    if (moraleSampleNow) s_moraleSampleTimer = 0.f;

    char moraleBuf[4][20] = {};
    float moraleVal[4] = {};
    {
        int mi = 0;
        for (const auto& s : ws) {
            if (mi >= 4) break;
            moraleVal[mi] = s.morale;
            char trend = ' ';
            auto it = s_prevMorale.find(s.name);
            if (it != s_prevMorale.end()) {
                float delta = s.morale - it->second;
                if (delta > 0.03f)  trend = '+';
                if (delta < -0.03f) trend = '-';
            }
            if (trend != ' ')
                std::snprintf(moraleBuf[mi], sizeof(moraleBuf[mi]), " M:%.0f%%%c", s.morale * 100.f, trend);
            else
                std::snprintf(moraleBuf[mi], sizeof(moraleBuf[mi]), " M:%.0f%%", s.morale * 100.f);
            if (moraleSampleNow)
                s_prevMorale[s.name] = s.morale;
            ++mi;
        }
    }

    // Pre-build contentment label strings (e.g. " C:85%")
    char contentBuf[4][16] = {};
    float contentVal[4] = {};
    {
        int ci2 = 0;
        for (const auto& s : ws) {
            if (ci2 >= 4) break;
            contentVal[ci2] = s.avgContentment;
            std::snprintf(contentBuf[ci2], sizeof(contentBuf[ci2]), " C:%.0f%%", s.avgContentment * 100.f);
            ++ci2;
        }
    }

    static const int HUNGER_W = 8;  // width reserved for "!" indicator
    int totalW = 0;
    for (int i = 0; i < count; ++i)
        totalW += MeasureText(bufs[i], STATUS_FONT) + MeasureText(childSuffix[i], STATUS_FONT)
                  + MeasureText(moraleBuf[i], STATUS_FONT)
                  + MeasureText(contentBuf[i], STATUS_FONT)
                  + (hungerCrisis[i] ? HUNGER_W : 0) + (i > 0 ? 28 : 0);
    int sx = (SCREEN_W - totalW) / 2;

    DrawRectangle(sx - 8, 4, totalW + 16, 34, Fade(BLACK, 0.55f));
    int cx = sx;
    for (int i = 0; i < count; ++i) {
        if (i > 0) { DrawText("|", cx, 14, STATUS_FONT, DARKGRAY); cx += 16; }
        // Color wood stock red if very low in winter; event lines use modifier-specific colour.
        bool woodLow = showWood && (ws[i].wood < 20.f) && (season == Season::Winter);
        Color col = woodLow ? RED : (hasEvent[i] ? ModifierColour(eventNames[i]) : WHITE);
        DrawText(bufs[i], cx, 14, STATUS_FONT, col);
        // Hunger crisis: draw "!" immediately after the food number in red
        if (hungerCrisis[i]) {
            int pfxW = MeasureText(foodPfx[i], STATUS_FONT);
            DrawText("!", cx + pfxW, 14, STATUS_FONT, Fade(RED, 0.9f));
        }
        cx += MeasureText(bufs[i], STATUS_FONT) + (hungerCrisis[i] ? HUNGER_W : 0);
        if (childCounts[i] > 0) {
            DrawText(childSuffix[i], cx, 14, STATUS_FONT, Fade(LIGHTGRAY, 0.6f));
            cx += MeasureText(childSuffix[i], STATUS_FONT);
        }
        // Morale label: green (≥0.7), yellow (≥0.3), red (<0.3)
        {
            Color moraleCol = (moraleVal[i] >= 0.7f) ? Fade(GREEN, 0.8f)  :
                              (moraleVal[i] >= 0.3f) ? Fade(YELLOW, 0.8f) : Fade(RED, 0.9f);
            DrawText(moraleBuf[i], cx, 14, STATUS_FONT, moraleCol);
            cx += MeasureText(moraleBuf[i], STATUS_FONT);
        }
        // Contentment label: green (≥0.7), yellow (≥0.4), red (<0.4)
        {
            Color cCol = (contentVal[i] >= 0.7f) ? Fade(GREEN, 0.7f)  :
                         (contentVal[i] >= 0.4f) ? Fade(YELLOW, 0.7f) : Fade(RED, 0.8f);
            DrawText(contentBuf[i], cx, 14, STATUS_FONT, cCol);
            cx += MeasureText(contentBuf[i], STATUS_FONT);
        }
        cx += 12;
    }
}

// ---- Event log ----

void HUD::DrawEventLog(const RenderSnapshot& snap) const {
    std::vector<EventLog::Entry> entries;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        entries = snap.logEntries;
    }
    if (entries.empty()) return;

    static const int LINES = 8, LINE_H = 16;
    static const int PX = 330, PW = SCREEN_W - PX - 4;
    static const int PH = LINES * LINE_H + 12;
    static const int PY = SCREEN_H - PH - 4;

    DrawRectangle(PX, PY, PW, PH, Fade(BLACK, 0.6f));
    DrawRectangleLines(PX, PY, PW, PH, Fade(LIGHTGRAY, 0.3f));
    DrawText("Event Log", PX + 6, PY + 4, 11, Fade(YELLOW, 0.7f));

    int maxScroll = std::max(0, (int)entries.size() - LINES);
    int scroll    = std::min(logScroll, maxScroll);

    for (int i = 0; i < LINES; ++i) {
        int idx = i + scroll;
        if (idx >= (int)entries.size()) break;
        const auto& e = entries[idx];
        char buf[96];
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
                     e.message.find("COLLAPSED")  != std::string::npos) ? RED    :
                    (e.message.find("died")        != std::string::npos ||
                     e.message.find("migrating")   != std::string::npos ||
                     e.message.find("MIGRATION")   != std::string::npos ||
                     e.message.find("HEAT WAVE")   != std::string::npos ||
                     e.message.find("bankrupt")    != std::string::npos) ? ORANGE :
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
                     e.message.find("became a hauler") != std::string::npos) ? GREEN  :
                    (e.message.find("--- ")        != std::string::npos) ? SKYBLUE : LIGHTGRAY;
        DrawText(buf, PX + 6, PY + 4 + LINE_H * (i + 1) - 2, 12, col);
    }
}

// ---- NPC hover tooltip ----

void HUD::DrawHoverTooltip(const RenderSnapshot& snap, const Camera2D& cam) const {
    Vector2 mouse = GetMousePosition();
    Vector2 world = GetScreenToWorld2D(mouse, cam);

    std::vector<RenderSnapshot::AgentEntry> agents;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        agents = snap.agents;
    }

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
    if (hasName)
        std::snprintf(line2, sizeof(line2), "%s", BehaviorLabel(best->behavior));
    else
        std::snprintf(line2, sizeof(line2), "H:%.0f%%  T:%.0f%%  E:%.0f%%  Ht:%.0f%%",
                      best->hungerPct*100.f, best->thirstPct*100.f,
                      best->energyPct*100.f, best->heatPct*100.f);

    if (hasName)
        std::snprintf(line3, sizeof(line3), "H:%.0f%%  T:%.0f%%  E:%.0f%%  Ht:%.0f%%",
                      best->hungerPct*100.f, best->thirstPct*100.f,
                      best->energyPct*100.f, best->heatPct*100.f);
    else
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
            const char* rn = (type == ResourceType::Food)  ? "Food"  :
                             (type == ResourceType::Water) ? "Water" :
                             (type == ResourceType::Wood)  ? "Wood"  : "?";
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
    // "Bandit" line: shown for BanditTag entities
    bool showBandit   = best->isBandit;
    // "On strike" line: shown when NPC has active strikeDuration
    bool showStrike   = best->onStrike;
    // "Good harvest" line: shown when NPC has active harvest bonus
    bool showHarvest  = best->harvestBonus;

    // Elder will line: surfaces the inheritance mechanic
    bool showWill = hasName && (best->ageDays > 60.f) && (best->balance > 0.f);

    // Rumour carrier line
    bool showRumour = best->hasRumour;
    char rumourLine[64] = {};
    if (showRumour)
        std::snprintf(rumourLine, sizeof(rumourLine), "(spreading: %s)", best->rumourLabel.c_str());

    // Hauler profit estimation
    bool showProfit = false;
    char profitLine[64] = {};
    Color profitColor = GREEN;
    if (isHauler && best->haulerCargoQty > 0) {
        float cost = best->haulerBuyPrice * best->haulerCargoQty;
        float profit = best->balance - cost;
        std::snprintf(profitLine, sizeof(profitLine), "Profit: ~%.0fg", profit);
        profitColor = (profit >= 0.f) ? Fade(GREEN, 0.8f) : Fade(RED, 0.8f);
        showProfit = true;
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

    // Near-bankrupt warning
    bool showNearBankrupt = best->nearBankrupt;

    // Illness suffix: appended inline on the needs line when illnessTimer > 0
    const char* illLabel = nullptr;
    if (best->ill) {
        illLabel = (best->illNeedIdx == 0) ? "(ill: hunger)"  :
                   (best->illNeedIdx == 1) ? "(ill: thirst)"  :
                   (best->illNeedIdx == 2) ? "(ill: fatigue)" : "(ill)";
    }

    // Skill line: show the relevant skill for this agent's profession
    bool showSkill = false;
    Color skillColor = GREEN;
    if (best->farmingSkill >= 0.f) {
        float sk = 0.5f;
        const char* skLabel = "Skill";
        if (best->profession == "Farmer") {
            sk = best->farmingSkill; skLabel = "Farming";
        } else if (best->profession == "Water Carrier") {
            sk = best->waterSkill; skLabel = "Water";
        } else if (best->profession == "Woodcutter") {
            sk = best->woodcuttingSkill; skLabel = "Woodcutting";
        } else {
            // For unspecialized/child: show highest skill
            sk = std::max({best->farmingSkill, best->waterSkill, best->woodcuttingSkill});
            skLabel = (sk == best->farmingSkill) ? "Farming" :
                      (sk == best->waterSkill)   ? "Water"   : "Woodcutting";
        }
        const char* rank = (sk >= 0.85f) ? " [Master]"  :
                           (sk >= 0.65f) ? " [Expert]"  :
                           (sk >= 0.40f) ? " [Trained]" :
                           (sk >= 0.15f) ? " [Novice]"  : " [Unskilled]";
        skillColor = (sk >= 0.65f) ? Fade(GOLD, 0.9f) : (sk >= 0.35f) ? GREEN : Fade(GRAY, 0.8f);
        std::snprintf(line6, sizeof(line6), "%s: %.0f%%%s", skLabel, sk * 100.f, rank);
        showSkill = true;
    }

    int lineCount = hasName ? 5 : 3;   // +1 for age line
    if (showGold)     lineCount++;
    if (showWill)     lineCount++;
    if (showFollow)   lineCount++;
    if (showHelped)   lineCount++;
    if (showGrateful) lineCount++;
    if (showWarmth)   lineCount++;
    if (showBandit)   lineCount++;
    if (showStrike)   lineCount++;
    if (showSkill)    lineCount++;
    if (showCargo)    lineCount++;
    if (showRumour)   lineCount++;
    if (showHarvest)  lineCount++;
    if (showProfit)   lineCount++;
    if (showRoute)        lineCount++;
    if (showNearBankrupt) lineCount++;

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
    int wb  = showBandit   ? MeasureText("Bandit (press E to confront)", 11) : 0;
    int wsk = showStrike   ? MeasureText("On strike", 11) : 0;
    int wwl = showWill     ? MeasureText("Will: 80% to treasury", 11) : 0;
    int wr  = showRumour  ? MeasureText(rumourLine,              11) : 0;
    int whv = showHarvest ? MeasureText("Good harvest bonus",     11) : 0;
    int wpr = showProfit ? MeasureText(profitLine,               11) : 0;
    int wrt = showRoute  ? MeasureText(routeLine,                11) : 0;
    int wnb = showNearBankrupt ? MeasureText("!! Near bankruptcy !!", 11) : 0;
    int pw  = std::max({w1, wa, w2, w3, w4, w5, wf, w6, wc, wh, wg, ww, wb, wsk, wwl, wr, whv, wpr, wrt, wnb}) + 10;
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
        DrawText(line2, tx, ly, 11, LIGHTGRAY);
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
    if (showGold)   { DrawText(line5,               tx, ly, 11, YELLOW);               ly += 16; }
    if (showWill)   { DrawText("Will: 80% to treasury", tx, ly, 11, Fade(GOLD, 0.5f)); ly += 16; }
    if (showFollow) { DrawText(followLine,          tx, ly, 11, Fade(SKYBLUE, 0.8f)); ly += 16; }
    if (showHelped)   { DrawText("Fed by neighbour",            tx, ly, 11, Fade(LIME, 0.75f));     ly += 16; }
    if (showGrateful) { DrawText("Grateful to neighbour",       tx, ly, 11, Fade(LIME, 0.55f));    ly += 16; }
    if (showWarmth)   { DrawText("Warm from giving",            tx, ly, 11, Fade(ORANGE, 0.75f));  ly += 16; }
    if (showBandit)   { DrawText("Bandit (press E to confront)",tx, ly, 11, Color{220, 60, 60, 220}); ly += 16; }
    if (showStrike)   { DrawText("On strike",                   tx, ly, 11, RED);                    ly += 16; }
    if (showRumour)   { DrawText(rumourLine,                   tx, ly, 11, Fade(YELLOW, 0.6f));    ly += 16; }
    if (showHarvest)  { DrawText("Good harvest bonus",          tx, ly, 11, Fade(GOLD, 0.6f));     ly += 16; }
    if (showSkill)    { DrawText(line6,                         tx, ly, 11, skillColor);             ly += 16; }
    if (showCargo)  { DrawText(cargoLine,           tx, ly, 11, Fade(SKYBLUE, 0.9f)); ly += 16; }
    if (showProfit) { DrawText(profitLine,          tx, ly, 11, profitColor);          ly += 16; }
    if (showRoute)        { DrawText(routeLine,               tx, ly, 11, Fade(LIGHTGRAY, 0.8f)); ly += 16; }
    if (showNearBankrupt) { DrawText("!! Near bankruptcy !!", tx, ly, 11, Fade(RED, 0.9f));       }
}

// ---- Facility hover tooltip ----
// Shows production rate, worker count, and skill when hovering near a facility square.

void HUD::DrawFacilityTooltip(const RenderSnapshot& snap, const Camera2D& cam) const {
    Vector2 mouse = GetMousePosition();
    Vector2 world = GetScreenToWorld2D(mouse, cam);

    std::vector<RenderSnapshot::FacilityEntry> facs;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        facs = snap.facilities;
    }

    const RenderSnapshot::FacilityEntry* best = nullptr;
    float bestDist = 20.f;   // max hover distance in world units
    for (const auto& f : facs) {
        float dx = world.x - f.x, dy = world.y - f.y;
        float d  = std::sqrt(dx*dx + dy*dy);
        if (d < bestDist) { bestDist = d; best = &f; }
    }
    if (!best) return;

    Season curSeason;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        curSeason = snap.season;
    }

    const char* typeName = (best->output == ResourceType::Food)  ? "Farm"        :
                           (best->output == ResourceType::Water) ? "Well"        :
                           (best->output == ResourceType::Wood)  ? "Lumber Mill" : "Facility";
    const char* resUnit  = (best->output == ResourceType::Food)  ? "food"  :
                           (best->output == ResourceType::Water) ? "water" : "wood";

    static constexpr int BASE_WORKERS = 5;
    float scale      = std::min(2.0f, std::max(0.1f, (float)best->workerCount / BASE_WORKERS));
    float skillMult  = 0.5f + best->avgSkill;             // same formula as ProductionSystem
    float seasonMult = SeasonProductionModifier(curSeason);
    float estOutput  = best->baseRate * scale * skillMult * seasonMult;

    char line1[64], line2[64], line3[64], line4[64], line5[64];
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

    Vector2 screen = GetWorldToScreen2D({ best->x, best->y }, cam);
    int w = std::max({ MeasureText(line1, 12), MeasureText(line2, 11),
                       MeasureText(line3, 11), MeasureText(line4, 11),
                       MeasureText(line5, 11) }) + 12;
    int h = 5 * 16 + 4;
    int tx = (int)screen.x + 18, ty = (int)screen.y - h;
    if (tx + w > SCREEN_W) tx = (int)screen.x - w - 10;
    if (ty < 0) ty = (int)screen.y + 14;

    DrawRectangle(tx - 4, ty - 2, w, h, Fade(BLACK, 0.75f));
    Color typeCol = (best->output == ResourceType::Food)  ? GREEN  :
                   (best->output == ResourceType::Water) ? SKYBLUE : BROWN;
    DrawText(line1, tx, ty,      12, typeCol);
    DrawText(line2, tx, ty + 16, 11, LIGHTGRAY);
    Color seasonCol = (seasonMult >= 1.0f) ? GREEN : (seasonMult >= 0.5f) ? YELLOW : RED;
    DrawText(line3, tx, ty + 32, 11, seasonCol);
    Color outCol = (estOutput >= best->baseRate) ? GREEN
                 : (estOutput >= best->baseRate * 0.5f) ? YELLOW : RED;
    DrawText(line4, tx, ty + 48, 11, outCol);
    Color healthCol = (healthPct >= 80.f) ? GREEN : (healthPct >= 50.f) ? YELLOW : RED;
    DrawText(line5, tx, ty + 64, 11, healthCol);
}

// ---- Settlement hover tooltip ----
// Shown when the mouse hovers inside a settlement circle.
// Displays name, pop, resources, treasury, and children count.

void HUD::DrawSettlementTooltip(const RenderSnapshot& snap, const Camera2D& cam) const {
    Vector2 mouse = GetMousePosition();
    Vector2 world = GetScreenToWorld2D(mouse, cam);

    std::vector<RenderSnapshot::SettlementEntry>  settls;
    std::vector<RenderSnapshot::SettlementStatus> ws;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        settls = snap.settlements;
        ws     = snap.worldStatus;
    }

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
    char line1[96];
    if (!best->modifierName.empty())
        std::snprintf(line1, sizeof(line1), "%s  [%d/%d pop%s]  — %s",
                      best->name.c_str(), best->pop, best->popCap,
                      popTrendStr, best->modifierName.c_str());
    else
        std::snprintf(line1, sizeof(line1), "%s  [%d/%d pop%s]",
                      best->name.c_str(), best->pop, best->popCap, popTrendStr);

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

    // Line 6 (optional): pending estates
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

    // Line 9: trade volume
    char lineTrade[32] = {};
    bool showTrade = (best->tradeVolume > 0);
    if (showTrade)
        std::snprintf(lineTrade, sizeof(lineTrade), "Trades: %d/day", best->tradeVolume);

    int lineCount = 3 + (showChildren ? 1 : 0) + (showElders ? 1 : 0)
                      + (showEstates ? 1 : 0)
                      + (showSpecialty ? 1 : 0) + (showMorale ? 1 : 0)
                      + (showTrade ? 1 : 0);
    int w = std::max({ MeasureText(line1, 12), MeasureText(line2, 11),
                       MeasureText(line3, 11),
                       showChildren  ? MeasureText(line4, 11) : 0,
                       showElders    ? MeasureText(line5, 11) : 0,
                       showEstates   ? MeasureText(lineEst, 11) : 0,
                       showSpecialty ? MeasureText(line6, 11) : 0,
                       showMorale   ? MeasureText(line7, 11) : 0,
                       showTrade    ? MeasureText(lineTrade, 11) : 0 }) + 12;
    int h = lineCount * 16 + 4;

    Vector2 screen = GetWorldToScreen2D({ best->x, best->y }, cam);
    int tx = (int)screen.x + 14, ty = (int)screen.y - h - (int)best->radius;
    if (tx + w > SCREEN_W) tx = (int)screen.x - w - 10;
    if (ty < 0)             ty = (int)screen.y + (int)best->radius + 6;

    DrawRectangle(tx - 4, ty - 2, w, h, Fade(BLACK, 0.75f));

    Color nameCol = (best->pop == 0) ? Fade(DARKGRAY, 0.8f) : WHITE;
    DrawText(line1, tx, ty,      12, nameCol);             ty += 16;
    DrawText(line2, tx, ty,      11, LIGHTGRAY);           ty += 16;
    Color tresCol = (treasury < 50.f) ? RED : (treasury < 150.f) ? ORANGE : GOLD;
    DrawText(line3, tx, ty,      11, tresCol);             ty += 16;
    if (showChildren) {
        DrawText(line4, tx, ty,  11, Fade(LIGHTGRAY, 0.7f)); ty += 16;
    }
    if (showElders) {
        DrawText(line5, tx, ty,  11, Fade(ORANGE, 0.75f)); ty += 16;
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
    if (showTrade) {
        DrawText(lineTrade, tx, ty, 11, Fade(SKYBLUE, 0.8f));
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
    int agents, tickSpeed, pop, deaths, simSteps, entities, haulerCount;
    bool paused;
    Season dbgSeason;
    float  dbgTemp, econTotal, econAvg, econRichest;
    std::string econRichestName;
    std::vector<RenderSnapshot::AgentEntry>        agentCopy;
    std::vector<RenderSnapshot::SettlementStatus>  settlCopy;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        agents    = (int)snap.agents.size();
        tickSpeed = snap.tickSpeed;
        pop       = snap.population;
        deaths    = snap.totalDeaths;
        paused    = snap.paused;
        simSteps  = snap.simStepsPerSec;
        entities  = snap.totalEntities;
        dbgSeason = snap.season;
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
    std::snprintf(lines[2],  64, "Sim steps/s:  %d", simSteps);
    std::snprintf(lines[3],  64, "Tick speed:   %dx%s", tickSpeed, paused ? " (PAUSED)" : "");
    std::snprintf(lines[4],  64, "Entities:     %d", entities);
    std::snprintf(lines[5],  64, "Population:   %d  Deaths: %d", pop, deaths);
    std::snprintf(lines[6],  64, "Season:       %s  %.1f°C", SeasonName(dbgSeason), dbgTemp);
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
                          "  F:%.0f W:%.0f D:%.0f  @%.1f/%.1f/%.1f",
                          s.food, s.water, s.wood,
                          s.foodPrice, s.waterPrice, s.woodPrice);
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

    // Background
    DrawRectangle(MM_X - 1, MM_Y - 1, MM_W + 2, MM_H + 2, Fade(BLACK, 0.70f));
    DrawRectangleLines(MM_X - 1, MM_Y - 1, MM_W + 2, MM_H + 2, Fade(LIGHTGRAY, 0.4f));

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
            DrawCircleLines((int)p.x, (int)p.y, dotR + 2.f, Fade(YELLOW, 0.8f));
    }

    // Player position — small white dot
    Vector2 pp = worldToMM(playerX, playerY);
    DrawCircleV(pp, 2.5f, WHITE);

    // Label
    DrawText("MAP", MM_X + 2, MM_Y + 2, 8, Fade(LIGHTGRAY, 0.4f));
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

    // Position tooltip at midpoint of road
    float midWX = (best->x1 + best->x2) * 0.5f;
    float midWY = (best->y1 + best->y2) * 0.5f;
    Vector2 screen = GetWorldToScreen2D({ midWX, midWY }, cam);

    int extraLine = (line7[0] != '\0') ? 1 : 0;
    int w = std::max({ MeasureText(line1, 12), MeasureText(line2, 11),
                       MeasureText(line3, 11), MeasureText(line4, 11),
                       MeasureText(line5, 11), MeasureText(line6, 10),
                       extraLine ? MeasureText(line7, 11) : 0 }) + 12;
    int h = (6 + extraLine) * 16 + 4;
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
    if (line7[0] != '\0')
        DrawText(line7, tx, ty + 96, 11, relColor);
}

// ---- DrawNeedBar ----

void HUD::DrawNeedBar(int x, int y, float value, float critThreshold,
                      const char* label, Color barColor) const {
    DrawText(label, x, y, 14, LIGHTGRAY);
    int barX = x + 60;
    DrawRectangle(barX, y, BAR_W, BAR_H, Fade(WHITE, 0.15f));
    float clamped = std::max(0.f, std::min(1.f, value));
    int   fillW   = (int)(clamped * BAR_W);
    Color fill    = (value < critThreshold) ? RED : barColor;
    if (fillW > 0) DrawRectangle(barX, y, fillW, BAR_H, fill);
    DrawRectangleLines(barX, y, BAR_W, BAR_H, WHITE);
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
