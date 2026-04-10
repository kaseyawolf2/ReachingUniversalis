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
    float hungerPct, thirstPct, energyPct;
    float hungerCrit, thirstCrit, energyCrit;
    float playerAgeDays, playerMaxDays, playerGold;
    std::map<ResourceType, int> playerInventory;
    AgentBehavior behavior;

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
        behavior    = snap.playerBehavior;
        playerAgeDays = snap.playerAgeDays;
        playerMaxDays = snap.playerMaxDays;
        playerGold    = snap.playerGold;
        playerInventory = snap.playerInventory;
    }

    // ---- Player need bars (top-left) ----
    if (playerAlive) {
        int invLines = (int)playerInventory.size();
        DrawRectangle(4, 4, 320, BAR_GAP * 5 + 90 + invLines * 16, Fade(BLACK, 0.55f));
        DrawNeedBar(BAR_X, BAR_Y0 + BAR_GAP * 0, hungerPct, hungerCrit, "Hunger", GREEN);
        DrawNeedBar(BAR_X, BAR_Y0 + BAR_GAP * 1, thirstPct, thirstCrit, "Thirst", SKYBLUE);
        DrawNeedBar(BAR_X, BAR_Y0 + BAR_GAP * 2, energyPct, energyCrit, "Energy", YELLOW);

        // Age row
        int ageY = BAR_Y0 + BAR_GAP * 3 + 2;
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

        DrawText("WASD:Move  B:Road  T:Trade  F:Follow  F1:Debug",
                 BAR_X, invY + 4, 10, Fade(LIGHTGRAY, 0.6f));
    }

    // ---- Time panel (top-right) ----
    {
        char timeBuf[64], speedBuf[16], popBuf[32], fpsBuf[32];
        std::snprintf(timeBuf,  sizeof(timeBuf),  "Day %d  %02d:%02d", day, hour, minute);
        std::snprintf(speedBuf, sizeof(speedBuf), paused ? "PAUSED" : "%dx", tickSpeed);
        std::snprintf(popBuf,   sizeof(popBuf),   "Pop: %d  Deaths: %d", pop, deaths);
        std::snprintf(fpsBuf,   sizeof(fpsBuf),   "FPS: %d  (%.1f ms)",
                      GetFPS(), GetFrameTime() * 1000.f);

        int pw = std::max({ MeasureText(timeBuf, 16), MeasureText(speedBuf, 14),
                            MeasureText(popBuf, 13),  MeasureText(fpsBuf, 12) }) + 16;
        int px = SCREEN_W - pw - 4;

        DrawRectangle(px, 4, pw, 80, Fade(BLACK, 0.55f));
        DrawText(timeBuf,  px + 8,  8, 16, WHITE);
        DrawText(speedBuf, px + 8, 28, 14, paused ? ORANGE : LIGHTGRAY);
        DrawText(popBuf,   px + 8, 46, 13, LIGHTGRAY);
        DrawText(fpsBuf,   px + 8, 63, 12, Fade(LIGHTGRAY, 0.6f));
    }

    DrawWorldStatus(snap);
    DrawEventLog(snap);
    DrawHoverTooltip(snap, camera);
    if (debugOverlay) DrawDebugOverlay(snap);
}

// ---- World status bar ----

void HUD::DrawWorldStatus(const RenderSnapshot& snap) const {
    std::vector<RenderSnapshot::SettlementStatus> ws;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        ws = snap.worldStatus;
    }
    if (ws.empty()) return;

    // Format: "Greenfield  F:120@2.1  W:20@8.3  Wd:0@6.0  G:200  [20]  [DROUGHT]"
    char bufs[4][112]; bool hasEvent[4] = {}; int count = 0;
    for (const auto& s : ws) {
        if (count >= 4) break;
        if (s.hasEvent)
            std::snprintf(bufs[count], 112,
                          "%s  F:%.0f@%.1f  W:%.0f@%.1f  Wd:%.0f@%.1f  G:%.0f  [%d] [%s]",
                          s.name.c_str(), s.food, s.foodPrice,
                          s.water, s.waterPrice, s.wood, s.woodPrice,
                          s.treasury, s.pop, s.eventName.c_str());
        else
            std::snprintf(bufs[count], 112,
                          "%s  F:%.0f@%.1f  W:%.0f@%.1f  Wd:%.0f@%.1f  G:%.0f  [%d]",
                          s.name.c_str(), s.food, s.foodPrice,
                          s.water, s.waterPrice, s.wood, s.woodPrice,
                          s.treasury, s.pop);
        hasEvent[count] = s.hasEvent;
        ++count;
    }
    int totalW = 0;
    for (int i = 0; i < count; ++i)
        totalW += MeasureText(bufs[i], 13) + (i > 0 ? 30 : 0);
    int sx = (SCREEN_W - totalW) / 2;

    DrawRectangle(sx - 8, 4, totalW + 16, 36, Fade(BLACK, 0.55f));
    int cx = sx;
    for (int i = 0; i < count; ++i) {
        if (i > 0) { DrawText("|", cx, 15, 13, DARKGRAY); cx += 18; }
        Color col = hasEvent[i] ? ORANGE : WHITE;
        DrawText(bufs[i], cx, 15, 13, col);
        cx += MeasureText(bufs[i], 13) + 12;
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
        Color col = (e.message.find("BLOCKED")  != std::string::npos ||
                     e.message.find("BANDITS")  != std::string::npos ||
                     e.message.find("DROUGHT")  != std::string::npos ||
                     e.message.find("BLIGHT")   != std::string::npos) ? RED    :
                    (e.message.find("died")      != std::string::npos ||
                     e.message.find("migrating") != std::string::npos) ? ORANGE :
                    (e.message.find("CLEARED")   != std::string::npos ||
                     e.message.find("restored")  != std::string::npos ||
                     e.message.find("reopened")  != std::string::npos ||
                     e.message.find("Born")      != std::string::npos) ? GREEN  : LIGHTGRAY;
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
    bool isHauler = (best->role == RenderSnapshot::AgentRole::Hauler);

    char line1[64], line2[64], line3[48] = {}, line4[48] = {}, line5[48] = {};
    // First line: name (if known) or role
    if (!best->npcName.empty())
        std::snprintf(line1, sizeof(line1), "%s (%s)", best->npcName.c_str(), role);
    else
        std::snprintf(line1, sizeof(line1), "%s | %s", role, BehaviorLabel(best->behavior));
    // Second line: behavior state (only if we used name as line1)
    bool hasName = !best->npcName.empty();
    if (hasName)
        std::snprintf(line2, sizeof(line2), "%s", BehaviorLabel(best->behavior));
    else
        std::snprintf(line2, sizeof(line2), "H:%.0f%%  T:%.0f%%  E:%.0f%%",
                      best->hungerPct * 100.f, best->thirstPct * 100.f, best->energyPct * 100.f);
    std::snprintf(line3, sizeof(line3), hasName ? "H:%.0f%%  T:%.0f%%  E:%.0f%%" : "Age: %.0f/%.0f days",
                  hasName ? best->hungerPct * 100.f : best->ageDays,
                  hasName ? best->thirstPct * 100.f : best->maxDays,
                  hasName ? best->energyPct * 100.f : 0.f);
    if (!hasName)
        std::snprintf(line3, sizeof(line3), "Age: %.0f / %.0f days",
                      best->ageDays, best->maxDays);
    if (hasName)
        std::snprintf(line4, sizeof(line4), "Age: %.0f / %.0f days",
                      best->ageDays, best->maxDays);
    if (isHauler)
        std::snprintf(line5, sizeof(line5), "Gold: %.1f", best->balance);

    int lineCount = hasName ? (isHauler ? 5 : 4) : (isHauler ? 4 : 3);
    int w1 = MeasureText(line1, 12), w2 = MeasureText(line2, 11);
    int w3 = MeasureText(line3, 11), w4 = MeasureText(line4, 11);
    int w5 = isHauler ? MeasureText(line5, 11) : 0;
    int pw = std::max({w1, w2, w3, w4, w5}) + 10;
    int ph = lineCount * 16;

    int tx = (int)screen.x + 14, ty = (int)screen.y - ph;
    if (tx + pw > SCREEN_W) tx = (int)screen.x - pw - 10;
    if (ty < 0) ty = (int)screen.y + 12;

    DrawRectangle(tx - 4, ty - 2, pw, ph, Fade(BLACK, 0.75f));

    float ageFrac = (best->maxDays > 0.f) ? std::min(1.f, best->ageDays / best->maxDays) : 0.f;
    Color ageCol  = (ageFrac < 0.6f) ? Fade(GREEN, 0.9f) :
                    (ageFrac < 0.85f) ? YELLOW : RED;

    DrawText(line1, tx, ty,      12, WHITE);
    DrawText(line2, tx, ty + 16, 11, hasName ? LIGHTGRAY : LIGHTGRAY);
    DrawText(line3, tx, ty + 32, 11, hasName ? LIGHTGRAY : ageCol);
    if (hasName) {
        DrawText(line4, tx, ty + 48, 11, ageCol);
        if (isHauler) DrawText(line5, tx, ty + 64, 11, YELLOW);
    } else {
        if (isHauler) DrawText(line4, tx, ty + 48, 11, YELLOW);
    }
}

// ---- Debug overlay ----

void HUD::DrawDebugOverlay(const RenderSnapshot& snap) const {
    int agents, tickSpeed, pop, deaths, simSteps, entities;
    bool paused;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        agents    = (int)snap.agents.size();
        tickSpeed = snap.tickSpeed;
        pop       = snap.population;
        deaths    = snap.totalDeaths;
        paused    = snap.paused;
        simSteps  = snap.simStepsPerSec;
        entities  = snap.totalEntities;
    }

    static const int OX = 4, OY = 148, OW = 240, OLH = 17;
    char lines[8][64];
    std::snprintf(lines[0], 64, "[F1] DEBUG OVERLAY");
    std::snprintf(lines[1], 64, "Render FPS:   %d  (%.1f ms)", GetFPS(), GetFrameTime()*1000.f);
    std::snprintf(lines[2], 64, "Sim steps/s:  %d", simSteps);
    std::snprintf(lines[3], 64, "Tick speed:   %dx%s", tickSpeed, paused ? " (PAUSED)" : "");
    std::snprintf(lines[4], 64, "Entities:     %d", entities);
    std::snprintf(lines[5], 64, "Render agents:%d", agents);
    std::snprintf(lines[6], 64, "Population:   %d", pop);
    std::snprintf(lines[7], 64, "Deaths:       %d", deaths);

    int rows = 8;
    DrawRectangle(OX, OY, OW, rows*OLH + 8, Fade(BLACK, 0.75f));
    DrawRectangleLines(OX, OY, OW, rows*OLH + 8, DARKGRAY);
    for (int i = 0; i < rows; ++i)
        DrawText(lines[i], OX+6, OY+4+i*OLH, 13, i==0 ? YELLOW : LIGHTGRAY);
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
