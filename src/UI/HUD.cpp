#include "HUD.h"
#include "ECS/Components.h"
#include "raylib.h"
#include <algorithm>
#include <cstdio>
#include <string>

static const int BAR_W       = 160;
static const int BAR_H       = 16;
static const int BAR_X       = 10;
static const int BAR_START_Y = 10;
static const int BAR_SPACING = 26;

static const int SCREEN_W = 1280;
static const int SCREEN_H = 720;

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

void HUD::HandleInput(entt::registry& /*registry*/) {
    if (IsKeyPressed(KEY_F1)) debugOverlay = !debugOverlay;

    float wheel = GetMouseWheelMove();
    if (wheel != 0.f) {
        logScroll -= (int)wheel;
        if (logScroll < 0) logScroll = 0;
    }
}

// ---- Draw ----

void HUD::Draw(entt::registry& registry, int totalDeaths) {
    // ---- Player need bars (top-left) ----
    {
        auto playerView = registry.view<PlayerTag, Needs, AgentState>();
        for (auto entity : playerView) {
            const auto& needs = playerView.get<Needs>(entity);
            const auto& state = playerView.get<AgentState>(entity);

            DrawRectangle(4, 4, 320, BAR_SPACING * 4 + 54, Fade(BLACK, 0.55f));

            DrawNeedBar(BAR_X, BAR_START_Y + BAR_SPACING * 0,
                        needs.list[0].value, needs.list[0].criticalThreshold, "Hunger", GREEN);
            DrawNeedBar(BAR_X, BAR_START_Y + BAR_SPACING * 1,
                        needs.list[1].value, needs.list[1].criticalThreshold, "Thirst", SKYBLUE);
            DrawNeedBar(BAR_X, BAR_START_Y + BAR_SPACING * 2,
                        needs.list[2].value, needs.list[2].criticalThreshold, "Energy", YELLOW);

            const int stateY = BAR_START_Y + BAR_SPACING * 3 + 4;
            DrawText("State:", BAR_X, stateY, 14, LIGHTGRAY);
            DrawText(BehaviorLabel(state.behavior), BAR_X + 52, stateY, 14, WHITE);

            // Road status
            const int roadY = stateY + 20;
            bool roadBlocked = false;
            registry.view<Road>().each([&](const Road& r) { if (r.blocked) roadBlocked = true; });
            DrawText(roadBlocked ? "!! ROAD BLOCKED !!" : "Road: open",
                     BAR_X, roadY, 13, roadBlocked ? RED : Fade(GREEN, 0.8f));

            // Key hints
            DrawText("WASD:Move  E:Eat  R:Respawn  B:Road  F:Follow  F1:Debug",
                     BAR_X, roadY + 18, 10, Fade(LIGHTGRAY, 0.6f));

            break;
        }
    }

    // ---- Time display (top-right) ----
    {
        auto timeView = registry.view<TimeManager>();
        for (auto entity : timeView) {
            const auto& tm = timeView.get<TimeManager>(entity);
            int hours   = (int)tm.hourOfDay;
            int minutes = (int)((tm.hourOfDay - hours) * 60.0f);

            char timeBuf[64];
            std::snprintf(timeBuf, sizeof(timeBuf), "Day %d  %02d:%02d", tm.day, hours, minutes);

            char speedBuf[16];
            if (tm.paused)
                std::snprintf(speedBuf, sizeof(speedBuf), "PAUSED");
            else
                std::snprintf(speedBuf, sizeof(speedBuf), "%dx", tm.tickSpeed);

            int pop = 0;
            registry.view<Needs>().each([&](auto e, auto&) {
                if (!registry.all_of<PlayerTag>(e)) ++pop;
            });

            char popBuf[32];
            std::snprintf(popBuf, sizeof(popBuf), "Pop: %d  Deaths: %d", pop, totalDeaths);

            char fpsBuf[16];
            std::snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %d", GetFPS());

            int panelW = std::max({ MeasureText(timeBuf, 16),
                                    MeasureText(speedBuf, 14),
                                    MeasureText(popBuf, 13),
                                    MeasureText(fpsBuf,  12) }) + 16;
            int panelX = SCREEN_W - panelW - 4;

            DrawRectangle(panelX, 4, panelW, 80, Fade(BLACK, 0.55f));
            DrawText(timeBuf,  panelX + 8,  8, 16, WHITE);
            DrawText(speedBuf, panelX + 8, 28, 14, tm.paused ? ORANGE : LIGHTGRAY);
            DrawText(popBuf,   panelX + 8, 46, 13, LIGHTGRAY);
            DrawText(fpsBuf,   panelX + 8, 63, 12, Fade(LIGHTGRAY, 0.6f));

            break;
        }
    }

    DrawWorldStatus(registry);
    DrawEventLog(registry);
    DrawHoverTooltip(registry);
    if (debugOverlay) DrawDebugOverlay(registry);
}

// ---- World status bar (top-centre) ----

void HUD::DrawWorldStatus(entt::registry& registry) const {
    auto settlView = registry.view<Settlement, Stockpile, Position>();
    if (settlView.begin() == settlView.end()) return;

    static const int PY = 4;
    static const int PH = 36;

    // Build status strings for each settlement
    char bufs[4][48];
    int  count = 0;
    for (auto e : settlView) {
        if (count >= 4) break;
        const auto& s  = settlView.get<Settlement>(e);
        const auto& sp = settlView.get<Stockpile>(e);

        float food  = sp.quantities.count(ResourceType::Food)  ? sp.quantities.at(ResourceType::Food)  : 0.f;
        float water = sp.quantities.count(ResourceType::Water) ? sp.quantities.at(ResourceType::Water) : 0.f;

        int pop = 0;
        registry.view<HomeSettlement, AgentState>(entt::exclude<PlayerTag>)
            .each([&](auto ent, const HomeSettlement& hs, const AgentState&) {
                if (hs.settlement == e) ++pop;
            });

        std::snprintf(bufs[count], sizeof(bufs[count]),
                      "%s  F:%.0f W:%.0f  [%d]",
                      s.name.c_str(), food, water, pop);
        ++count;
    }

    // Measure total width and centre
    int totalW = 0;
    for (int i = 0; i < count; ++i)
        totalW += MeasureText(bufs[i], 13) + (i > 0 ? 30 : 0);
    int startX = (SCREEN_W - totalW) / 2;

    DrawRectangle(startX - 8, PY, totalW + 16, PH, Fade(BLACK, 0.55f));

    int cx = startX;
    for (int i = 0; i < count; ++i) {
        if (i > 0) {
            DrawText("|", cx, PY + 11, 13, DARKGRAY);
            cx += 18;
        }
        DrawText(bufs[i], cx, PY + 11, 13, WHITE);
        cx += MeasureText(bufs[i], 13) + 12;
    }
}

// ---- Event log panel (bottom) ----

void HUD::DrawEventLog(entt::registry& registry) const {
    auto logView = registry.view<EventLog>();
    if (logView.begin() == logView.end()) return;
    const auto& log = logView.get<EventLog>(*logView.begin());

    if (log.entries.empty()) return;

    static const int LINES    = 8;
    static const int LINE_H   = 16;
    static const int PX       = 330;
    static const int PW       = SCREEN_W - PX - 4;
    static const int PH       = LINES * LINE_H + 12;
    static const int PY       = SCREEN_H - PH - 4;

    DrawRectangle(PX, PY, PW, PH, Fade(BLACK, 0.6f));
    DrawRectangleLines(PX, PY, PW, PH, Fade(LIGHTGRAY, 0.3f));
    DrawText("Event Log", PX + 6, PY + 4, 11, Fade(YELLOW, 0.7f));

    int maxScroll = std::max(0, (int)log.entries.size() - LINES);
    int scroll    = std::min(logScroll, maxScroll);

    for (int i = 0; i < LINES; ++i) {
        int idx = i + scroll;
        if (idx >= (int)log.entries.size()) break;
        const auto& entry = log.entries[idx];

        char buf[96];
        std::snprintf(buf, sizeof(buf), "D%d %02d:xx  %s",
                      entry.day, entry.hour, entry.message.c_str());

        Color col = (entry.message.find("BLOCKED") != std::string::npos) ? RED :
                    (entry.message.find("died")    != std::string::npos) ? ORANGE :
                    (entry.message.find("CLEARED") != std::string::npos) ? GREEN  : LIGHTGRAY;

        int y = PY + 4 + LINE_H * (i + 1) - 2;
        DrawText(buf, PX + 6, y, 12, col);
    }
}

// ---- NPC hover tooltip ----

void HUD::DrawHoverTooltip(entt::registry& registry) const {
    // Get camera to convert screen → world
    auto camView = registry.view<CameraState>();
    if (camView.begin() == camView.end()) return;
    const auto& cs = camView.get<CameraState>(*camView.begin());

    Vector2 mouse     = GetMousePosition();
    Vector2 worldMouse = GetScreenToWorld2D(mouse, cs.cam);

    // Find closest NPC within 12px world-radius
    entt::entity hovered  = entt::null;
    float        bestDist = 12.f;

    auto agentView = registry.view<Position, AgentState, Renderable>();
    for (auto e : agentView) {
        const auto& pos  = agentView.get<Position>(e);
        const auto& rend = agentView.get<Renderable>(e);
        float dx = worldMouse.x - pos.x, dy = worldMouse.y - pos.y;
        float d  = std::sqrt(dx * dx + dy * dy) - rend.size;
        if (d < bestDist) { bestDist = d; hovered = e; }
    }

    if (hovered == entt::null) return;

    const auto& hPos   = registry.get<Position>(hovered);
    const auto& hState = registry.get<AgentState>(hovered);

    Vector2 screenPos = GetWorldToScreen2D({ hPos.x, hPos.y }, cs.cam);

    char buf[128];
    std::string extra;

    if (const auto* needs = registry.try_get<Needs>(hovered)) {
        char nb[64];
        std::snprintf(nb, sizeof(nb), "H:%.0f%% T:%.0f%% E:%.0f%%",
                      needs->list[0].value * 100.f,
                      needs->list[1].value * 100.f,
                      needs->list[2].value * 100.f);
        extra = nb;
    }

    bool isHauler = registry.all_of<Hauler>(hovered);
    bool isPlayer = registry.all_of<PlayerTag>(hovered);
    const char* role = isPlayer ? "Player" : (isHauler ? "Hauler" : "NPC");

    std::snprintf(buf, sizeof(buf), "%s | %s\n%s",
                  role, BehaviorLabel(hState.behavior), extra.c_str());

    // Split at newline for two-line draw
    const char* line2 = extra.empty() ? nullptr : extra.c_str();

    int tw = MeasureText(buf, 12);
    int tx = (int)screenPos.x + 14;
    int ty = (int)screenPos.y - 28;
    if (tx + tw + 8 > SCREEN_W) tx = (int)screenPos.x - tw - 16;
    if (ty < 0) ty = (int)screenPos.y + 12;

    char line1[64];
    std::snprintf(line1, sizeof(line1), "%s | %s", role, BehaviorLabel(hState.behavior));

    int w1 = MeasureText(line1, 12);
    int w2 = line2 ? MeasureText(line2, 11) : 0;
    int pw = std::max(w1, w2) + 10;

    DrawRectangle(tx - 4, ty - 2, pw, line2 ? 32 : 18, Fade(BLACK, 0.75f));
    DrawText(line1, tx, ty, 12, WHITE);
    if (line2) DrawText(line2, tx, ty + 16, 11, LIGHTGRAY);
}

// ---- Debug overlay (F1) ----

void HUD::DrawDebugOverlay(entt::registry& registry) const {
    static const int OX = 4, OY = 140;
    static const int OW = 220, OLH = 17;

    int entities = (int)registry.storage<entt::entity>().size();

    int npcs = 0, haulers = 0, sleeping = 0, working = 0;
    registry.view<AgentState>(entt::exclude<PlayerTag>).each([&](auto e, const AgentState& as) {
        if (registry.all_of<Hauler>(e)) { ++haulers; return; }
        ++npcs;
        if (as.behavior == AgentBehavior::Sleeping) ++sleeping;
        if (as.behavior == AgentBehavior::Working)  ++working;
    });

    int logSize = 0;
    registry.view<EventLog>().each([&](const EventLog& l) { logSize = (int)l.entries.size(); });

    char lines[8][64];
    std::snprintf(lines[0], 64, "[F1] DEBUG OVERLAY");
    std::snprintf(lines[1], 64, "FPS:       %d", GetFPS());
    std::snprintf(lines[2], 64, "Entities:  %d", entities);
    std::snprintf(lines[3], 64, "NPCs:      %d  (Haulers: %d)", npcs, haulers);
    std::snprintf(lines[4], 64, "Sleeping:  %d", sleeping);
    std::snprintf(lines[5], 64, "Working:   %d", working);
    std::snprintf(lines[6], 64, "Log entries: %d", logSize);

    int rows = 7;
    DrawRectangle(OX, OY, OW, rows * OLH + 8, Fade(BLACK, 0.75f));
    DrawRectangleLines(OX, OY, OW, rows * OLH + 8, DARKGRAY);
    for (int i = 0; i < rows; ++i)
        DrawText(lines[i], OX + 6, OY + 4 + i * OLH, 13, i == 0 ? YELLOW : LIGHTGRAY);
}

// ---- DrawNeedBar ----

void HUD::DrawNeedBar(int x, int y, float value, float critThreshold,
                      const char* label, Color barColor) const {
    DrawText(label, x, y, 14, LIGHTGRAY);

    int barX = x + 60;
    DrawRectangle(barX, y, BAR_W, BAR_H, Fade(WHITE, 0.15f));

    float clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    int fillW = (int)(clamped * BAR_W);

    Color fillColor = (value < critThreshold) ? RED : barColor;
    if (fillW > 0) DrawRectangle(barX, y, fillW, BAR_H, fillColor);

    DrawRectangleLines(barX, y, BAR_W, BAR_H, WHITE);
}
