#include "GameState.h"
#include <cmath>
#include <algorithm>

static constexpr float MAP_W    = 2400.f;
static constexpr float MAP_H    =  720.f;
static constexpr float LERP_SPD =    5.f;

GameState::GameState()
    : m_simThread(m_input, m_snapshot)
{
    m_simThread.Start();
}

GameState::~GameState() {
    m_simThread.Stop();
}

// ---- Update (main thread) ----------------------------------------

void GameState::Update(float dt) {
    PollInput(dt);

    // Camera follow: lerp toward player world position from snapshot
    float px, py;
    bool  follow;
    {
        std::lock_guard<std::mutex> lock(m_snapshot.mutex);
        px     = m_snapshot.playerWorldX;
        py     = m_snapshot.playerWorldY;
        follow = m_followPlayer;
    }
    if (follow) {
        float t = std::min(1.f, LERP_SPD * dt);
        m_camera.target.x += (px - m_camera.target.x) * t;
        m_camera.target.y += (py - m_camera.target.y) * t;
    }
    // Clamp
    m_camera.target.x = std::max(0.f, std::min(MAP_W, m_camera.target.x));
    m_camera.target.y = std::max(0.f, std::min(MAP_H, m_camera.target.y));

    m_hud.HandleInput(m_snapshot);
}

// ---- PollInput (main thread → InputSnapshot) ---------------------

void GameState::PollInput(float dt) {
    // One-shot events
    if (IsKeyPressed(KEY_SPACE)) m_input.pauseToggle.store(true);
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))    m_input.speedUp.store(true);
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) m_input.speedDown.store(true);
    if (IsKeyPressed(KEY_B))    m_input.roadToggle.store(true);
    if (IsKeyPressed(KEY_T))    m_input.playerTrade.store(true);

    if (IsKeyPressed(KEY_F)) {
        m_followPlayer = !m_followPlayer;
        m_input.camFollowToggle.store(true);
    }

    // ---- Camera pan (arrow keys / drag) — handled entirely on main thread ----
    bool panning = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT) ||
                   IsKeyDown(KEY_UP)   || IsKeyDown(KEY_DOWN);
    if (panning) {
        m_followPlayer = false;
        float speed = m_panSpeed / m_camera.zoom;
        if (IsKeyDown(KEY_LEFT))  m_camera.target.x -= speed * dt;
        if (IsKeyDown(KEY_RIGHT)) m_camera.target.x += speed * dt;
        if (IsKeyDown(KEY_UP))    m_camera.target.y -= speed * dt;
        if (IsKeyDown(KEY_DOWN))  m_camera.target.y += speed * dt;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 d = GetMouseDelta();
        if (d.x != 0.f || d.y != 0.f) {
            m_followPlayer      = false;
            m_camera.target.x  -= d.x / m_camera.zoom;
            m_camera.target.y  -= d.y / m_camera.zoom;
        }
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0.f) {
        m_camera.zoom = std::max(m_zoomMin,
                        std::min(m_zoomMax,
                        m_camera.zoom + wheel * 0.1f * m_camera.zoom));
    }

    if (IsKeyPressed(KEY_C)) {
        m_camera.target  = { MAP_W * 0.5f, MAP_H * 0.5f };
        m_camera.zoom    = 0.5f;
        m_followPlayer   = false;
    }

    // ---- Continuous player movement ----
    float mx = 0.f, my = 0.f;
    if (IsKeyDown(KEY_W)) my -= 1.f;
    if (IsKeyDown(KEY_S)) my += 1.f;
    if (IsKeyDown(KEY_A)) mx -= 1.f;
    if (IsKeyDown(KEY_D)) mx += 1.f;
    float len = std::sqrt(mx*mx + my*my);
    if (len > 0.f) { mx /= len; my /= len; }
    m_input.playerMoveX.store(mx);
    m_input.playerMoveY.store(my);

    // ---- Settlement click (left mouse, world-space hit-test on main thread) ----
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 world = GetScreenToWorld2D(GetMousePosition(), m_camera);
        // Forward to sim thread — it resolves the entity
        m_simThread.NotifyWorldClick(world.x, world.y);
    }
}

// ---- Draw (main thread) ------------------------------------------

void GameState::Draw() {
    // Take a local copy of the snapshot so we hold the lock for minimum time.
    // All rendering is then done lock-free from the local copy.
    std::vector<RenderSnapshot::AgentEntry>       agents;
    std::vector<RenderSnapshot::SettlementEntry>  settlements;
    std::vector<RenderSnapshot::RoadEntry>        roads;
    std::vector<RenderSnapshot::FacilityEntry>    facilities;
    RenderSnapshot::StockpilePanel                panel;

    {
        std::lock_guard<std::mutex> lock(m_snapshot.mutex);
        agents      = m_snapshot.agents;
        settlements = m_snapshot.settlements;
        roads       = m_snapshot.roads;
        facilities  = m_snapshot.facilities;
        panel       = m_snapshot.stockpilePanel;
    }

    // World drawing
    BeginMode2D(m_camera);

    // Roads
    for (const auto& r : roads) {
        Color col = r.blocked ? RED : Fade(BEIGE, 0.6f);
        DrawLineEx({ r.x1, r.y1 }, { r.x2, r.y2 }, 4.f, col);
        if (r.blocked) {
            float mx = (r.x1 + r.x2) * 0.5f, my = (r.y1 + r.y2) * 0.5f;
            DrawText("X", (int)mx - 8, (int)my - 10, 24, RED);
        }
    }

    // Settlements — ring color reflects food+water health
    for (const auto& s : settlements) {
        Color fill = Fade(DARKGREEN, 0.15f);
        Color ring;
        if (s.pop == 0) {
            // Collapsed settlement — grey fill, dark ring
            fill = Fade(DARKGRAY, 0.15f);
            ring = s.selected ? YELLOW : Fade(DARKGRAY, 0.7f);
        } else {
            float minStock = std::min(s.foodStock, s.waterStock);
            ring = s.selected ? YELLOW :
                   (minStock > 30.f) ? Fade(GREEN, 0.7f)  :
                   (minStock > 10.f) ? Fade(YELLOW, 0.8f) : Fade(RED, 0.9f);
        }
        DrawCircleV({ s.x, s.y }, s.radius, fill);
        DrawCircleLinesV({ s.x, s.y }, s.radius, ring);
        Color nameCol = (s.pop == 0) ? Fade(DARKGRAY, 0.8f) : WHITE;
        DrawText(s.name.c_str(),
                 (int)(s.x - MeasureText(s.name.c_str(), 14) / 2),
                 (int)(s.y - s.radius - 18), 14, nameCol);
    }

    // Facilities
    for (const auto& f : facilities) {
        Color col;
        const char* label;
        switch (f.output) {
            case ResourceType::Food:    col = GREEN;  label = "F"; break;
            case ResourceType::Water:   col = SKYBLUE; label = "W"; break;
            case ResourceType::Wood:    col = BROWN;  label = "L"; break;
            default: continue;  // Shelter — no visual marker
        }
        DrawRectangle((int)f.x - 10, (int)f.y - 10, 20, 20, Fade(col, 0.8f));
        DrawRectangleLines((int)f.x - 10, (int)f.y - 10, 20, 20, WHITE);
        DrawText(label, (int)f.x - 4, (int)f.y - 7, 14, WHITE);
    }

    // Agents
    for (const auto& a : agents) {
        DrawCircleV({ a.x, a.y }, a.size, a.color);
        DrawCircleLinesV({ a.x, a.y }, a.size + 1.f, a.ringColor);
        if (a.hasCargoDot)
            DrawCircleV({ a.x + a.size + 4.f, a.y - a.size }, 4.f, a.cargoDotColor);
    }

    EndMode2D();

    // Stockpile panel (screen-space)
    if (panel.open)
        m_renderSystem.DrawStockpilePanel(panel);

    // HUD
    m_hud.Draw(m_snapshot, m_camera);
}

// ---- SkyColor --------------------------------------------------------

// Interpolate between two colours by t (0–1)
static Color LerpColor(Color a, Color b, float t) {
    return {
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t),
        255
    };
}

Color GameState::SkyColor() const {
    float hour;
    {
        std::lock_guard<std::mutex> lock(m_snapshot.mutex);
        hour = m_snapshot.hourOfDay;
    }

    static const Color midnight = {  10,  10,  30, 255 };
    static const Color dawn     = { 220, 120,  60, 255 };
    static const Color morning  = { 100, 160, 220, 255 };
    static const Color noon     = {  80, 180, 255, 255 };
    static const Color dusk     = { 200,  80,  40, 255 };
    static const Color night    = {  15,  15,  40, 255 };

    struct Keyframe { float hour; Color color; };
    static const Keyframe keys[] = {
        {  0.f, midnight }, {  5.f, midnight }, {  6.5f, dawn    },
        {  9.f, morning  }, { 12.f, noon     }, { 15.f, morning  },
        { 18.5f, dusk    }, { 20.f, night    }, { 24.f, midnight },
    };
    static const int N = sizeof(keys) / sizeof(keys[0]);

    for (int i = 0; i < N - 1; ++i) {
        if (hour >= keys[i].hour && hour < keys[i+1].hour) {
            float t = (hour - keys[i].hour) / (keys[i+1].hour - keys[i].hour);
            return LerpColor(keys[i].color, keys[i+1].color, t);
        }
    }
    return midnight;
}
