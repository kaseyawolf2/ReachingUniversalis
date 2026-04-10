#include "RenderSystem.h"
#include "ECS/Components.h"
#include "raylib.h"
#include <cstdio>
#include <cmath>

// ---- helpers ----

static void DrawSettlement(const Position& pos, const Settlement& s, bool selected) {
    Color ring  = selected ? YELLOW : Fade(WHITE, 0.5f);
    Color fill  = Fade(DARKGREEN, 0.15f);

    DrawCircleV({ pos.x, pos.y }, s.radius, fill);
    DrawCircleLinesV({ pos.x, pos.y }, s.radius, ring);
    DrawText(s.name.c_str(),
             (int)(pos.x - MeasureText(s.name.c_str(), 14) / 2),
             (int)(pos.y - s.radius - 18),
             14, WHITE);
}

static void DrawRoad(const Position& fromPos, const Position& toPos, bool blocked) {
    Color col = blocked ? RED : Fade(BEIGE, 0.6f);
    DrawLineEx({ fromPos.x, fromPos.y }, { toPos.x, toPos.y }, 4.0f, col);
    if (blocked) {
        // Draw an X at the midpoint
        float mx = (fromPos.x + toPos.x) * 0.5f;
        float my = (fromPos.y + toPos.y) * 0.5f;
        DrawText("X", (int)mx - 8, (int)my - 10, 24, RED);
    }
}

static void DrawFacility(const Position& pos, ResourceType output) {
    Color col = (output == ResourceType::Food) ? GREEN : SKYBLUE;
    const char* label = (output == ResourceType::Food) ? "F" : "W";
    DrawRectangle((int)pos.x - 10, (int)pos.y - 10, 20, 20, Fade(col, 0.8f));
    DrawRectangleLines((int)pos.x - 10, (int)pos.y - 10, 20, 20, WHITE);
    DrawText(label, (int)pos.x - 4, (int)pos.y - 7, 14, WHITE);
}

// ---- RenderSystem::Draw ----

void RenderSystem::Draw(entt::registry& registry) {
    auto camView = registry.view<CameraState>();
    if (camView.empty()) return;
    const auto& cs = camView.get<CameraState>(*camView.begin());

    BeginMode2D(cs.cam);

    // ---- Roads (drawn first, beneath everything) ----
    {
        auto roadView = registry.view<Road>();
        for (auto entity : roadView) {
            const auto& road = roadView.get<Road>(entity);
            if (road.from == entt::null || road.to == entt::null) continue;
            if (!registry.valid(road.from) || !registry.valid(road.to)) continue;

            const auto& fromPos = registry.get<Position>(road.from);
            const auto& toPos   = registry.get<Position>(road.to);
            DrawRoad(fromPos, toPos, road.blocked);
        }
    }

    // ---- Settlements ----
    {
        auto settlView = registry.view<Position, Settlement>();
        for (auto entity : settlView) {
            const auto& pos = settlView.get<Position>(entity);
            const auto& s   = settlView.get<Settlement>(entity);
            bool selected = (entity == selectedSettlement);
            DrawSettlement(pos, s, selected);
        }
    }

    // ---- Production facilities ----
    {
        auto facView = registry.view<Position, ProductionFacility>();
        for (auto entity : facView) {
            const auto& pos = facView.get<Position>(entity);
            const auto& fac = facView.get<ProductionFacility>(entity);
            DrawFacility(pos, fac.output);
        }
    }

    // ---- Agents (NPCs) — color encodes health state ----
    {
        auto agentView = registry.view<Position, AgentState, Renderable>();
        for (auto entity : agentView) {
            const auto& pos  = agentView.get<Position>(entity);
            const auto& rend = agentView.get<Renderable>(entity);

            // Override color based on worst need — keep PlayerTag color as-is
            Color drawColor = rend.color;
            if (!registry.all_of<PlayerTag>(entity)) {
                if (const auto* needs = registry.try_get<Needs>(entity)) {
                    float worst = 1.f;
                    for (const auto& n : needs->list)
                        if (n.value < worst) worst = n.value;

                    if      (worst < 0.15f) drawColor = RED;
                    else if (worst < 0.30f) drawColor = ORANGE;
                    else if (worst < 0.55f) drawColor = YELLOW;
                    else                    drawColor = WHITE;
                }
            }

            DrawCircleV({ pos.x, pos.y }, rend.size, drawColor);
            DrawCircleLinesV({ pos.x, pos.y }, rend.size, DARKGRAY);
        }
    }

    EndMode2D();

    // ---- Stockpile panel (screen space, shown when settlement is selected) ----
    if (selectedSettlement != entt::null && registry.valid(selectedSettlement)) {
        auto* stockpile  = registry.try_get<Stockpile>(selectedSettlement);
        auto* settlement = registry.try_get<Settlement>(selectedSettlement);
        if (stockpile && settlement) {
            DrawStockpilePanel(*settlement, *stockpile);
        }
    }
}

void RenderSystem::HandleInput(entt::registry& registry) {
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) return;

    auto camView = registry.view<CameraState>();
    if (camView.empty()) return;
    const auto& cs = camView.get<CameraState>(*camView.begin());

    // Convert screen click to world position
    Vector2 screenPos = GetMousePosition();
    Vector2 worldPos  = GetScreenToWorld2D(screenPos, cs.cam);

    // Check if click is inside any settlement radius
    entt::entity clicked = entt::null;
    auto settlView = registry.view<Position, Settlement>();
    for (auto entity : settlView) {
        const auto& pos = settlView.get<Position>(entity);
        const auto& s   = settlView.get<Settlement>(entity);
        float dx = worldPos.x - pos.x;
        float dy = worldPos.y - pos.y;
        if (dx * dx + dy * dy <= s.radius * s.radius) {
            clicked = entity;
            break;
        }
    }

    // Toggle: clicking same settlement deselects it
    selectedSettlement = (clicked == selectedSettlement) ? entt::null : clicked;
}

void RenderSystem::DrawStockpilePanel(const Settlement& s, const Stockpile& sp) const {
    static const int PX = 10, PY = 580;
    static const int PW = 200, LINE_H = 18;

    int lines = 1 + (int)sp.quantities.size();  // header + one per resource
    int ph    = lines * LINE_H + 16;

    DrawRectangle(PX, PY, PW, ph, Fade(BLACK, 0.7f));
    DrawRectangleLines(PX, PY, PW, ph, LIGHTGRAY);
    DrawText(s.name.c_str(), PX + 8, PY + 8, 14, YELLOW);

    int y = PY + 8 + LINE_H;
    for (const auto& [type, qty] : sp.quantities) {
        const char* label = "?";
        Color        col  = WHITE;
        switch (type) {
            case ResourceType::Food:    label = "Food";    col = GREEN;   break;
            case ResourceType::Water:   label = "Water";   col = SKYBLUE; break;
            case ResourceType::Shelter: label = "Shelter"; col = BROWN;   break;
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%s: %.1f", label, qty);
        DrawText(buf, PX + 8, y, 13, col);
        y += LINE_H;
    }
}
