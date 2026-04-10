#include "HUD.h"
#include "ECS/Components.h"
#include "raylib.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
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
    }

    // ---- Player need bars (top-left) ----
    if (playerAlive) {
        DrawRectangle(4, 4, 320, BAR_GAP * 4 + 72, Fade(BLACK, 0.55f));
        DrawNeedBar(BAR_X, BAR_Y0 + BAR_GAP * 0, hungerPct, hungerCrit, "Hunger", GREEN);
        DrawNeedBar(BAR_X, BAR_Y0 + BAR_GAP * 1, thirstPct, thirstCrit, "Thirst", SKYBLUE);
        DrawNeedBar(BAR_X, BAR_Y0 + BAR_GAP * 2, energyPct, energyCrit, "Energy", YELLOW);

        int stateY = BAR_Y0 + BAR_GAP * 3 + 4;
        DrawText("State:", BAR_X, stateY, 14, LIGHTGRAY);
        DrawText(BehaviorLabel(behavior), BAR_X + 52, stateY, 14, WHITE);

        int roadY = stateY + 20;
        DrawText(roadBlocked ? "!! ROAD BLOCKED !!" : "Road: open",
                 BAR_X, roadY, 13, roadBlocked ? RED : Fade(GREEN, 0.8f));

        DrawText("WASD:Move  E:Eat  R:Respawn  B:Road  F:Follow  F1:Debug",
                 BAR_X, roadY + 18, 10, Fade(LIGHTGRAY, 0.6f));
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

    char bufs[4][48]; int count = 0;
    for (const auto& s : ws) {
        if (count >= 4) break;
        std::snprintf(bufs[count], 48, "%s  F:%.0f W:%.0f  [%d]",
                      s.name.c_str(), s.food, s.water, s.pop);
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
        DrawText(bufs[i], cx, 15, 13, WHITE);
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
        Color col = (e.message.find("BLOCKED") != std::string::npos) ? RED    :
                    (e.message.find("died")    != std::string::npos) ? ORANGE :
                    (e.message.find("CLEARED") != std::string::npos) ? GREEN  : LIGHTGRAY;
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

    // Determine role by ring colour (render thread heuristic — no registry access)
    const char* role = (best->ringColor.r == WHITE.r &&
                        best->ringColor.g == WHITE.g &&
                        best->ringColor.b == WHITE.b) ? "Player"
                     : (best->ringColor.b > 100 && best->ringColor.r < 50) ? "Hauler"
                     : "NPC";

    char line1[48];
    std::snprintf(line1, sizeof(line1), "%s", role);

    int tx = (int)screen.x + 14, ty = (int)screen.y - 28;
    int pw = MeasureText(line1, 12) + 10;
    if (tx + pw > SCREEN_W) tx = (int)screen.x - pw - 10;
    if (ty < 0) ty = (int)screen.y + 12;

    DrawRectangle(tx - 4, ty - 2, pw, 18, Fade(BLACK, 0.75f));
    DrawText(line1, tx, ty, 12, WHITE);
}

// ---- Debug overlay ----

void HUD::DrawDebugOverlay(const RenderSnapshot& snap) const {
    int agents, tickSpeed, pop, deaths;
    bool paused;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        agents    = (int)snap.agents.size();
        tickSpeed = snap.tickSpeed;
        pop       = snap.population;
        deaths    = snap.totalDeaths;
        paused    = snap.paused;
    }

    static const int OX = 4, OY = 148, OW = 220, OLH = 17;
    char lines[6][64];
    std::snprintf(lines[0], 64, "[F1] DEBUG OVERLAY");
    std::snprintf(lines[1], 64, "FPS:        %d", GetFPS());
    std::snprintf(lines[2], 64, "Render ents:%d", agents);
    std::snprintf(lines[3], 64, "Population: %d  Deaths: %d", pop, deaths);
    std::snprintf(lines[4], 64, "Tick speed: %dx%s", tickSpeed, paused ? " PAUSED" : "");
    std::snprintf(lines[5], 64, "Sim thread: background");

    DrawRectangle(OX, OY, OW, 6*OLH + 8, Fade(BLACK, 0.75f));
    DrawRectangleLines(OX, OY, OW, 6*OLH + 8, DARKGRAY);
    for (int i = 0; i < 6; ++i)
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
