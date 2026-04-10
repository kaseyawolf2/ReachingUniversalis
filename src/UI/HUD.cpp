#include "HUD.h"
#include "ECS/Components.h"
#include "raylib.h"
#include <algorithm>
#include <cstdio>
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
    }
    return "Unknown";
}

// ---- HandleInput ----

void HUD::HandleInput(const RenderSnapshot& /*snapshot*/) {
    if (IsKeyPressed(KEY_F1)) debugOverlay = !debugOverlay;
    float wheel = GetMouseWheelMove();
    if (wheel != 0.f) {
        logScroll -= (int)wheel;
        if (logScroll < 0) logScroll = 0;
    }
}

// ---- Draw ----

void HUD::Draw(const RenderSnapshot& snap, const Camera2D& camera) {
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
    float temperature;
    std::map<ResourceType, int> playerInventory;
    AgentBehavior behavior;
    Season season;

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
        playerAgeDays = snap.playerAgeDays;
        playerMaxDays = snap.playerMaxDays;
        playerGold    = snap.playerGold;
        playerInventory = snap.playerInventory;
        season      = snap.season;
        temperature = snap.temperature;
    }

    // ---- Player need bars (top-left) ----
    if (playerAlive) {
        int invLines = (int)playerInventory.size();
        DrawRectangle(4, 4, 320, BAR_GAP * 6 + 90 + invLines * 16, Fade(BLACK, 0.55f));
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

        // Gold balance
        int goldY = roadY + 18;
        char goldBuf[32];
        std::snprintf(goldBuf, sizeof(goldBuf), "%.1fg", playerGold);
        DrawText("Gold:", BAR_X, goldY, 14, LIGHTGRAY);
        DrawText(goldBuf, BAR_X + 52, goldY, 14, YELLOW);

        // Inventory lines
        int invY = goldY + BAR_GAP;
        if (!playerInventory.empty()) {
            DrawText("Cargo:", BAR_X, invY, 13, LIGHTGRAY);
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
                DrawText(cbuf, BAR_X + 52, invY + ci * 16, 12, rcol);
                ++ci;
            }
            invY += (int)playerInventory.size() * 16;
        } else {
            DrawText("Cargo: empty", BAR_X, invY, 13, Fade(LIGHTGRAY, 0.5f));
            invY += 16;
        }

        DrawText("WASD:Move  Z:Sleep  T:Trade  B:Road  F:Follow  F1:Debug",
                 BAR_X, invY + 4, 10, Fade(LIGHTGRAY, 0.6f));
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

    DrawWorldStatus(snap);
    DrawEventLog(snap);
    DrawHoverTooltip(snap, camera);
    if (debugOverlay) DrawDebugOverlay(snap);
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
    char bufs[4][128]; bool hasEvent[4] = {}; int count = 0;
    for (const auto& s : ws) {
        if (count >= 4) break;
        const char* fmt_base  = showWood
            ? "%s  F:%.0f  W:%.0f  Wd:%.0f  G:%.0f  [%d]"
            : "%s  F:%.0f@%.1f  W:%.0f@%.1f  G:%.0f  [%d]";
        const char* fmt_event = showWood
            ? "%s  F:%.0f  W:%.0f  Wd:%.0f  G:%.0f  [%d] [%s]"
            : "%s  F:%.0f@%.1f  W:%.0f@%.1f  G:%.0f  [%d] [%s]";

        if (s.hasEvent) {
            if (showWood)
                std::snprintf(bufs[count], 128, fmt_event,
                    s.name.c_str(), s.food, s.water, s.wood, s.treasury, s.pop,
                    s.eventName.c_str());
            else
                std::snprintf(bufs[count], 128, fmt_event,
                    s.name.c_str(), s.food, s.foodPrice, s.water, s.waterPrice,
                    s.treasury, s.pop, s.eventName.c_str());
        } else {
            if (showWood)
                std::snprintf(bufs[count], 128, fmt_base,
                    s.name.c_str(), s.food, s.water, s.wood, s.treasury, s.pop);
            else
                std::snprintf(bufs[count], 128, fmt_base,
                    s.name.c_str(), s.food, s.foodPrice, s.water, s.waterPrice,
                    s.treasury, s.pop);
        }
        hasEvent[count] = s.hasEvent;
        ++count;
    }
    int totalW = 0;
    for (int i = 0; i < count; ++i)
        totalW += MeasureText(bufs[i], STATUS_FONT) + (i > 0 ? 28 : 0);
    int sx = (SCREEN_W - totalW) / 2;

    DrawRectangle(sx - 8, 4, totalW + 16, 34, Fade(BLACK, 0.55f));
    int cx = sx;
    for (int i = 0; i < count; ++i) {
        if (i > 0) { DrawText("|", cx, 14, STATUS_FONT, DARKGRAY); cx += 16; }
        // Color wood stock red if very low in winter
        bool woodLow = showWood && (ws[i].wood < 20.f) && (season == Season::Winter);
        Color col = woodLow ? RED : (hasEvent[i] ? ORANGE : WHITE);
        DrawText(bufs[i], cx, 14, STATUS_FONT, col);
        cx += MeasureText(bufs[i], STATUS_FONT) + 12;
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
        Color col = (e.message.find("BLOCKED")   != std::string::npos ||
                     e.message.find("BANDITS")   != std::string::npos ||
                     e.message.find("DROUGHT")   != std::string::npos ||
                     e.message.find("BLIGHT")    != std::string::npos ||
                     e.message.find("DISEASE")   != std::string::npos ||
                     e.message.find("BLIZZARD")  != std::string::npos ||
                     e.message.find("FLOOD")     != std::string::npos ||
                     e.message.find("cold")      != std::string::npos ||
                     e.message.find("COLLAPSED") != std::string::npos) ? RED    :
                    (e.message.find("died")       != std::string::npos ||
                     e.message.find("migrating")  != std::string::npos ||
                     e.message.find("MIGRATION")  != std::string::npos) ? ORANGE :
                    (e.message.find("CLEARED")    != std::string::npos ||
                     e.message.find("restored")   != std::string::npos ||
                     e.message.find("reopened")   != std::string::npos ||
                     e.message.find("Born")       != std::string::npos ||
                     e.message.find("TRADE BOOM") != std::string::npos ||
                     e.message.find("BOUNTY")     != std::string::npos ||
                     e.message.find("OFF-MAP")    != std::string::npos ||
                     e.message.find("respawned")  != std::string::npos) ? GREEN  :
                    (e.message.find("--- ")       != std::string::npos) ? SKYBLUE : LIGHTGRAY;
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
                     : (best->role == RenderSnapshot::AgentRole::Hauler) ? "Hauler"
                     : "NPC";
    bool isHauler  = (best->role == RenderSnapshot::AgentRole::Hauler);
    bool showGold  = (best->balance > 0.f || isHauler);

    char line1[64], line2[64], line3[64] = {}, line4[64] = {}, line5[64] = {}, line6[48] = {};
    // First line: name (if known) or role
    if (!best->npcName.empty())
        std::snprintf(line1, sizeof(line1), "%s (%s)", best->npcName.c_str(), role);
    else
        std::snprintf(line1, sizeof(line1), "%s | %s", role, BehaviorLabel(best->behavior));

    bool hasName = !best->npcName.empty();
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

    if (hasName)
        std::snprintf(line4, sizeof(line4), "Age: %.0f / %.0f days",
                      best->ageDays, best->maxDays);
    if (showGold)
        std::snprintf(line5, sizeof(line5), "Gold: %.1f", best->balance);

    int lineCount = hasName ? (showGold ? 5 : 4) : (showGold ? 4 : 3);

    int w1 = MeasureText(line1, 12), w2 = MeasureText(line2, 11);
    int w3 = MeasureText(line3, 11), w4 = MeasureText(line4, 11);
    int w5 = showGold ? MeasureText(line5, 11) : 0;
    int pw = std::max({w1, w2, w3, w4, w5}) + 10;
    int ph = lineCount * 16;

    int tx = (int)screen.x + 14, ty = (int)screen.y - ph;
    if (tx + pw > SCREEN_W) tx = (int)screen.x - pw - 10;
    if (ty < 0) ty = (int)screen.y + 12;

    DrawRectangle(tx - 4, ty - 2, pw, ph, Fade(BLACK, 0.75f));

    DrawText(line1, tx, ty,      12, WHITE);
    DrawText(line2, tx, ty + 16, 11, LIGHTGRAY);
    DrawText(line3, tx, ty + 32, 11, hasName ? LIGHTGRAY : ageCol);
    if (hasName) {
        DrawText(line4, tx, ty + 48, 11, ageCol);
        if (showGold) DrawText(line5, tx, ty + 64, 11, YELLOW);
    } else {
        if (showGold) DrawText(line4, tx, ty + 48, 11, YELLOW);
    }
}

// ---- Debug overlay ----

void HUD::DrawDebugOverlay(const RenderSnapshot& snap) const {
    int agents, tickSpeed, pop, deaths, simSteps, entities;
    bool paused;
    Season dbgSeason;
    float  dbgTemp;
    std::vector<RenderSnapshot::AgentEntry> agentCopy;
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
    char lines[14][64];
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

    int rows = 14;
    DrawRectangle(OX, OY, OW, rows*OLH + 8, Fade(BLACK, 0.75f));
    DrawRectangleLines(OX, OY, OW, rows*OLH + 8, DARKGRAY);
    for (int i = 0; i < rows; ++i) {
        Color col = (i == 0) ? YELLOW : (i == 7) ? Fade(LIGHTGRAY, 0.6f) : LIGHTGRAY;
        DrawText(lines[i], OX+6, OY+4+i*OLH, 13, col);
    }
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
