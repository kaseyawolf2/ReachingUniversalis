#include "GameState.h"
#include "World/WorldLoader.h"
#include <cmath>
#include <algorithm>
#include <utility>

static constexpr float MAP_W    = 2400.f;
static constexpr float MAP_H    =  720.f;
static constexpr float LERP_SPD =    5.f;

GameState::GameState(const WorldSchema& schema, std::vector<LoadWarning> loadWarnings)
    : m_simThread(m_input, m_snapshot, schema, std::move(loadWarnings))
    , m_schema(schema)  // const& — GameState must not outlive the WorldSchema
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
    if (IsKeyPressed(KEY_Z))    m_input.playerSleep.store(true);
    if (IsKeyPressed(KEY_H))    m_input.playerSettle.store(true);
    if (IsKeyPressed(KEY_E))    m_input.playerWork.store(true);
    if (IsKeyPressed(KEY_Q))    m_input.playerBuy.store(true);
    if (IsKeyPressed(KEY_C))    m_input.playerBuild.store(true);
    if (IsKeyPressed(KEY_V))    m_input.playerBuyCart.store(true);
    if (IsKeyPressed(KEY_P))    m_input.playerFoundSettlement.store(true);
    if (IsKeyPressed(KEY_R))    m_input.roadRepair.store(true);

    if (IsKeyPressed(KEY_F)) {
        m_followPlayer = !m_followPlayer;
        m_input.camFollowToggle.store(true);
    }

    if (IsKeyPressed(KEY_O)) {
        m_showRoadCondition = !m_showRoadCondition;
    }

    // Two-press N: first press selects road start, second press builds the road.
    if (IsKeyPressed(KEY_N)) {
        float px, py;
        {
            std::lock_guard<std::mutex> lock(m_snapshot.mutex);
            px = m_snapshot.playerWorldX;
            py = m_snapshot.playerWorldY;
        }
        if (!m_roadBuildMode) {
            m_roadBuildMode = true;
            m_roadBuildSrcX = px;
            m_roadBuildSrcY = py;
        } else {
            m_input.roadBuildFromX.store(m_roadBuildSrcX);
            m_input.roadBuildFromY.store(m_roadBuildSrcY);
            m_input.roadBuildToX.store(px);
            m_input.roadBuildToY.store(py);
            m_input.roadBuild.store(true);
            m_roadBuildMode = false;
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) m_roadBuildMode = false;

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

    if (IsKeyPressed(KEY_HOME)) {
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
    std::shared_ptr<const std::vector<std::string>> sharedSkillNames;
    float snapHour = 12.f;

    {
        std::lock_guard<std::mutex> lock(m_snapshot.mutex);
        agents           = m_snapshot.agents;
        settlements      = m_snapshot.settlements;
        roads            = m_snapshot.roads;
        facilities       = m_snapshot.facilities;
        panel            = m_snapshot.stockpilePanel;
        sharedSkillNames = m_snapshot.skillNames;
        snapHour         = m_snapshot.hourOfDay;
    }

    // World drawing
    BeginMode2D(m_camera);

    // Roads — color-coded by condition or bandit presence (toggle with O key)
    for (const auto& r : roads) {
        Color col;
        if (r.blocked) {
            col = RED;
        } else if (!m_showRoadCondition && r.banditCount >= 3) {
            col = Fade(RED, 0.6f);
        } else if (!m_showRoadCondition && r.banditCount > 0) {
            col = Fade(ORANGE, 0.5f);
        } else {
            float c = r.condition;
            if      (c >= 0.75f) col = Fade(BEIGE, 0.6f);
            else if (c >= 0.50f) col = Fade(YELLOW,  0.7f);
            else if (c >= 0.25f) col = Fade(ORANGE,  0.75f);
            else                 col = Fade(RED,      0.6f);
        }
        float lineW = r.blocked ? 3.f : 2.f + r.condition * 2.f;  // thinner as road degrades
        DrawLineEx({ r.x1, r.y1 }, { r.x2, r.y2 }, lineW, col);
        if (r.blocked) {
            float mx = (r.x1 + r.x2) * 0.5f, my = (r.y1 + r.y2) * 0.5f;
            DrawText("X", (int)mx - 8, (int)my - 10, 24, RED);
        }
    }

    // Pending road-build line: dashed orange from source to player's current position
    if (m_roadBuildMode) {
        float px, py;
        {
            std::lock_guard<std::mutex> lock(m_snapshot.mutex);
            px = m_snapshot.playerWorldX;
            py = m_snapshot.playerWorldY;
        }
        // Draw dashes manually: segment length 12px, gap 8px
        float dx = px - m_roadBuildSrcX, dy = py - m_roadBuildSrcY;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist > 1.f) {
            float ux = dx / dist, uy = dy / dist;
            static constexpr float SEG = 12.f, GAP = 8.f;
            float t = 0.f;
            while (t < dist) {
                float t2 = std::min(t + SEG, dist);
                DrawLineEx({ m_roadBuildSrcX + ux*t,  m_roadBuildSrcY + uy*t  },
                            { m_roadBuildSrcX + ux*t2, m_roadBuildSrcY + uy*t2 },
                            2.5f, Fade(ORANGE, 0.8f));
                t = t2 + GAP;
            }
        }
        DrawCircleV({ m_roadBuildSrcX, m_roadBuildSrcY }, 8.f, Fade(ORANGE, 0.6f));
    }

    // Active trade routes: thin lines from hauler to destination
    for (const auto& a : agents) {
        if (!a.hasRouteDest) continue;
        Color lineCol = a.hasCargoDot ? Fade(a.cargoDotColor, 0.35f) : Fade(WHITE, 0.25f);
        DrawLineEx({ a.x, a.y }, { a.destX, a.destY }, 1.5f, lineCol);
    }

    // Convoy lines: connect nearby haulers travelling together
    for (size_t i = 0; i < agents.size(); ++i) {
        const auto& a = agents[i];
        if (!a.inConvoy || a.haulerState != 1) continue;
        for (size_t j = i + 1; j < agents.size(); ++j) {
            const auto& b = agents[j];
            if (!b.inConvoy || b.haulerState != 1) continue;
            float dx = a.x - b.x, dy = a.y - b.y;
            if (dx * dx + dy * dy > 60.f * 60.f) continue;
            DrawLineEx({ a.x, a.y }, { b.x, b.y }, 1.5f, Fade(GREEN, 0.25f));
        }
    }

    // Settlements — ring color reflects food+water health
    for (const auto& s : settlements) {
        Color fill = Fade(DARKGREEN, 0.15f);
        Color ring;
        if (s.pop == 0) {
            // Collapsed settlement — grey fill; lighter ring during ruin cooldown
            fill = Fade(DARKGRAY, 0.15f);
            ring = s.selected ? YELLOW :
                   (s.ruinTimer > 0.f) ? Fade(GRAY, 0.85f) : Fade(DARKGRAY, 0.7f);
        } else {
            float minStock = std::min(s.foodStock, s.waterStock);
            // In cold seasons, include wood shortage in ring health assessment
            bool coldSeason = (s.heatDrainMod > m_schema.seasonThresholds.coldSeason);
            if (coldSeason && s.woodStock < 20.f)
                minStock = std::min(minStock, s.woodStock);
            ring = s.selected ? YELLOW :
                   (minStock > 30.f) ? Fade(GREEN, 0.7f)  :
                   (minStock > 10.f) ? Fade(YELLOW, 0.8f) : Fade(RED, 0.9f);
            // Morale tint: when no event modifier is active, blend ring by morale
            if (s.modifierName.empty() && !s.selected) {
                ring = (s.morale >= 0.7f) ? Fade(GREEN, 0.7f) :
                       (s.morale >= 0.3f) ? Fade(YELLOW, 0.8f) : Fade(RED, 0.9f);
            }
            // Plague override — ring pulses purple
            if (s.modifierName == "Plague")
                ring = s.selected ? YELLOW : Color{ 180, 60, 220, 200 };
            // Festival override — ring glows gold
            if (s.modifierName == "Festival")
                ring = s.selected ? YELLOW : Fade(GOLD, 0.85f);
        }
        DrawCircleV({ s.x, s.y }, s.radius, fill);
        // During plague, also draw a second outer ring to make it more visible
        if (s.pop > 0 && s.modifierName == "Plague")
            DrawCircleLinesV({ s.x, s.y }, s.radius + 4.f, Fade(Color{180,60,220,255}, 0.5f));
        DrawCircleLinesV({ s.x, s.y }, s.radius, ring);
        // Mood glow: faint green for thriving, faint red for struggling
        if (s.pop > 0 && s.modifierName.empty()) {
            if (s.moodScore >= 0.7f)
                DrawCircleV({ s.x, s.y }, s.radius - 2.f, Fade(GREEN, 0.08f));
            else if (s.moodScore < 0.3f)
                DrawCircleV({ s.x, s.y }, s.radius - 2.f, Fade(RED, 0.10f));
        }
        // Pop cap warning: inner dashed ring (orange) when pop >= 90% of cap
        if (s.pop > 0 && s.popCap > 0 && s.pop >= (int)(s.popCap * 0.9f)) {
            float pulse = 0.5f + 0.4f * std::sin(GetTime() * 3.f);
            DrawCircleLinesV({ s.x, s.y }, s.radius - 5.f,
                             Fade(ORANGE, pulse * 0.65f));
        }
        Color nameCol = (s.pop == 0) ? Fade(DARKGRAY, 0.8f) : WHITE;
        DrawText(s.name.c_str(),
                 (int)(s.x - MeasureText(s.name.c_str(), 14) / 2),
                 (int)(s.y - s.radius - 18), 14, nameCol);
        // Danger indicator: faint red "!" when adjacent roads have 3+ bandits total
        if (s.pop > 0) {
            int totalBandits = 0;
            for (const auto& rd : roads)
                if (rd.nameA == s.name || rd.nameB == s.name)
                    totalBandits += rd.banditCount;
            if (totalBandits >= 3) {
                float pulse = 0.5f + 0.3f * sinf((float)GetTime() * 2.5f);
                int nameW = MeasureText(s.name.c_str(), 14);
                DrawText("!", (int)(s.x + nameW / 2 + 3),
                         (int)(s.y - s.radius - 18), 14, Fade(RED, pulse));
            }
        }
        // Specialty label below name
        if (!s.specialty.empty() && s.pop > 0) {
            Color specCol = (s.specialty == "Farming") ? Fade(GREEN, 0.75f) :
                            (s.specialty == "Water")   ? Fade(SKYBLUE, 0.75f) : Fade(BROWN, 0.75f);
            DrawText(s.specialty.c_str(),
                     (int)(s.x - MeasureText(s.specialty.c_str(), 11) / 2),
                     (int)(s.y - s.radius - 4), 11, specCol);
        }
        // Settlement tier badge (I–IV) drawn at top-right of circle
        if (s.pop > 0) {
            const char* tier = (s.pop <= 10)  ? "I"   :
                               (s.pop <= 20)  ? "II"  :
                               (s.pop <= 35)  ? "III" : "IV";
            Color tierCol   = (s.pop <= 10)   ? Fade(LIGHTGRAY, 0.6f) :
                               (s.pop <= 20)  ? Fade(GREEN, 0.7f)    :
                               (s.pop <= 35)  ? Fade(GOLD, 0.8f)     : Fade(ORANGE, 0.9f);
            int tierW = MeasureText(tier, 10);
            float ta = 0.785f;  // 45° = top-right
            int tx2 = (int)(s.x + std::cos(ta) * (s.radius + 4.f)) - tierW/2;
            int ty2 = (int)(s.y - std::sin(ta) * (s.radius + 4.f)) - 5;
            DrawText(tier, tx2, ty2, 10, tierCol);
        }

        // Active event modifier label (Plague, Drought, etc.) — drawn below the circle
        if (!s.modifierName.empty() && s.pop > 0) {
            Color modCol = (s.modifierName == "Plague")         ? Color{200, 80, 240, 220} :
                           (s.modifierName == "Drought")         ? Fade(ORANGE, 0.90f) :
                           (s.modifierName == "Heat Wave")       ? Color{255, 160, 40, 220} :
                           (s.modifierName == "Festival")        ? Fade(GOLD, 0.90f)   :
                           (s.modifierName == "Harvest Bounty") ? Fade(GREEN, 0.90f)  :
                                                                   Fade(YELLOW, 0.85f);
            DrawText(s.modifierName.c_str(),
                     (int)(s.x - MeasureText(s.modifierName.c_str(), 10) / 2),
                     (int)(s.y + s.radius + 5), 10, modCol);
        }
    }

    // Evening gathering density ring: show how many NPCs are clustered near each settlement
    if (snapHour >= 18.f && snapHour < 21.f) {
        for (const auto& s : settlements) {
            if (s.pop <= 0 || s.popCap <= 0) continue;
            int gathered = 0;
            for (const auto& a : agents) {
                if (a.homeSettlementName == s.name &&
                    a.role == RenderSnapshot::AgentRole::NPC) {
                    float dx = a.x - s.x, dy = a.y - s.y;
                    if (dx*dx + dy*dy < 40.f * 40.f) ++gathered;
                }
            }
            if (gathered < 2) continue;
            float ratio = std::min(1.f, (float)gathered / (float)s.popCap);
            float ringRadius = s.radius + 8.f + ratio * 20.f;
            DrawCircleLinesV({ s.x, s.y }, ringRadius, Fade(SKYBLUE, 0.15f));
        }
    }

    // Facilities
    for (const auto& f : facilities) {
        Color col;
        const char* label;
        switch (f.output) {
            case RES_FOOD:    col = GREEN;  label = "F"; break;
            case RES_WATER:   col = SKYBLUE; label = "W"; break;
            case RES_WOOD:    col = BROWN;  label = "L"; break;
            default: continue;  // Shelter — no visual marker
        }
        DrawRectangle((int)f.x - 10, (int)f.y - 10, 20, 20, Fade(col, 0.8f));
        DrawRectangleLines((int)f.x - 10, (int)f.y - 10, 20, 20, WHITE);
        DrawText(label, (int)f.x - 4, (int)f.y - 7, 14, WHITE);
    }

    // Charity-radius overlay: when hovering a canHelp NPC, draw a faint LIME circle
    // showing the radius within which they can help starving neighbours.
    {
        Vector2 mouse = GetMousePosition();
        Vector2 world = GetScreenToWorld2D(mouse, m_camera);
        const RenderSnapshot::AgentEntry* hovered = nullptr;
        float bestDist = 12.f;
        for (const auto& a : agents) {
            float dx = world.x - a.x, dy = world.y - a.y;
            float d  = std::sqrt(dx*dx + dy*dy) - a.size;
            if (d < bestDist) { bestDist = d; hovered = &a; }
        }
        if (hovered && hovered->role == RenderSnapshot::AgentRole::NPC &&
            hovered->hungerPct > 0.8f && hovered->balance > 20.f &&
            hovered->charityReady) {
            DrawCircleLinesV({ hovered->x, hovered->y }, 80.f, Fade(LIME, 0.2f));
        }
        // Bandit threat radius: show intercept range when hovering a bandit
        if (hovered && hovered->isBandit) {
            DrawCircleLinesV({ hovered->x, hovered->y }, 80.f, Fade(RED, 0.2f));
        }
    }

    // Agents
    for (const auto& a : agents) {
        // Colour priority: Celebrating → gold; plain NPC → contentment tint; others → snapshot colour
        Color drawColor;
        if (a.behavior == AgentBehavior::Celebrating) {
            drawColor = Fade(GOLD, 0.85f);
        } else if (a.recentlyStole) {
            drawColor = Fade(MAROON, 0.9f);
        } else if (a.role == RenderSnapshot::AgentRole::NPC) {
            drawColor = (a.contentment >= 0.7f) ? Fade(GREEN,  0.85f) :
                        (a.contentment >= 0.4f) ? YELLOW          : Fade(RED, 0.85f);
        } else {
            drawColor = a.color;
        }
        // Illness tint: blend toward purple for sick NPCs and children
        if (a.ill && (a.role == RenderSnapshot::AgentRole::NPC ||
                      a.role == RenderSnapshot::AgentRole::Child)) {
            Color sick = Fade(PURPLE, 0.5f);
            drawColor.r = (unsigned char)((drawColor.r + sick.r) / 2);
            drawColor.g = (unsigned char)((drawColor.g + sick.g) / 2);
            drawColor.b = (unsigned char)((drawColor.b + sick.b) / 2);
        }
        // Idle haulers with no cargo drawn at half opacity
        if (a.role == RenderSnapshot::AgentRole::Hauler
            && a.behavior == AgentBehavior::Idle && a.haulerCargoQty == 0)
            drawColor = Fade(drawColor, 0.5f);
        DrawCircleV({ a.x, a.y }, a.size, drawColor);
        // Children have no ring — keeps them visually distinct from working adults
        if (a.role != RenderSnapshot::AgentRole::Child)
            DrawCircleLinesV({ a.x, a.y }, a.size + 1.f, a.ringColor);
        if (a.hasCargoDot)
            DrawCircleV({ a.x + a.size + 4.f, a.y - a.size }, 4.f, a.cargoDotColor);
        // Harvest bonus glow: faint gold ring around workers with active good-harvest bonus
        if (a.harvestBonus)
            DrawCircleLinesV({ a.x, a.y }, 10.f, Fade(GOLD, 0.4f));
        // Vocation indicator: small gold ring when working in their best-skill profession
        if (a.inVocation && a.behavior == AgentBehavior::Working)
            DrawCircleLinesV({ a.x, a.y }, 5.f, Fade(GOLD, 0.5f));
        // Gratitude glow: faint lime ring while walking toward helper
        if (a.isGrateful && a.role == RenderSnapshot::AgentRole::NPC)
            DrawCircleLinesV({ a.x, a.y }, a.size + 2.f, Fade(LIME, 0.5f));
        // Celebrating glow: pulsating gold ring during celebrations
        if (a.behavior == AgentBehavior::Celebrating) {
            float alpha = 0.4f + 0.2f * sinf((float)GetTime() * 3.f);
            DrawCircleLinesV({ a.x, a.y }, 12.f, Fade(GOLD, alpha));
        }
        // Near-bankrupt hauler warning: pulsating red ring
        if (a.nearBankrupt) {
            float alpha = 0.3f + 0.3f * sinf((float)GetTime() * 4.f);
            DrawCircleLinesV({ a.x, a.y }, 10.f, Fade(RED, alpha));
        }
        // Chat indicator: pulsing yellow ring when NPC is in conversation
        if (a.chatting && a.role == RenderSnapshot::AgentRole::NPC) {
            float alpha = 0.3f + 0.15f * sinf((float)GetTime() * 5.f);
            DrawCircleLinesV({ a.x, a.y }, a.size + 3.f, Fade(YELLOW, alpha));
        }
        // Sleep commuting indicator: faint blue ring while walking home to sleep
        if (a.behavior == AgentBehavior::Sleeping && !a.atHome)
            DrawCircleLinesV({ a.x, a.y }, a.size + 2.f, Fade(BLUE, 0.35f));
        // Bandit flee trail: fading line in opposite direction of velocity
        if (a.isBandit && a.fleeTimer > 0.f) {
            float vLen = std::sqrt(a.fleeVx * a.fleeVx + a.fleeVy * a.fleeVy);
            if (vLen > 0.1f) {
                // Trail points opposite to velocity, length proportional to flee intensity
                float trailLen = 20.f * std::min(1.f, a.fleeTimer);
                float nx = -a.fleeVx / vLen, ny = -a.fleeVy / vLen;
                float alpha = 0.5f * std::min(1.f, a.fleeTimer);
                DrawLineEx({ a.x, a.y },
                           { a.x + nx * trailLen, a.y + ny * trailLen },
                           2.f, Fade(RED, alpha));
                // Second shorter trail for visual depth
                DrawLineEx({ a.x + nx * 3.f, a.y + ny * 3.f },
                           { a.x + nx * trailLen * 0.6f, a.y + ny * trailLen * 0.6f },
                           1.5f, Fade(ORANGE, alpha * 0.6f));
            }
        }
        // Wealthy NPC: faint gold outer ring for NPCs with balance > 80g
        if (a.role == RenderSnapshot::AgentRole::NPC && a.balance > 80.f)
            DrawCircleLinesV({ a.x, a.y }, 8.f, Fade(GOLD, 0.25f));
        // Charity radius: draw 80u LIME circle when hovering a charity-ready NPC
        if (a.charityReady && a.role == RenderSnapshot::AgentRole::NPC) {
            Vector2 mouseWorld = GetScreenToWorld2D(GetMousePosition(), m_camera);
            float hdx = mouseWorld.x - a.x, hdy = mouseWorld.y - a.y;
            if (hdx*hdx + hdy*hdy <= (a.size + 8.f) * (a.size + 8.f))
                DrawCircleLinesV({ a.x, a.y }, 80.f, Fade(LIME, 0.2f));
        }
        // Hauler route line: faint line from hauler to destination, coloured by cargo type
        if (a.hasRouteDest) {
            Color routeCol = a.hasCargoDot ? Fade(a.cargoDotColor, 0.3f) : Fade(SKYBLUE, 0.3f);
            DrawLineV({ a.x, a.y }, { a.destX, a.destY }, routeCol);
        }
        // Return trip line: faint gray line back toward home when hauler has no route dest
        else if (a.role == RenderSnapshot::AgentRole::Hauler && a.hasHome && !a.hasRouteDest) {
            DrawLineV({ a.x, a.y }, { a.homeX, a.homeY }, Fade(GRAY, 0.15f));
        }
    }

    EndMode2D();

    // ---- Night overlay: darken the world during night hours ----
    // Day (8-17): fully transparent; dawn/dusk: smooth ramp; night (20-5): max darkness.
    {
        float hourOfDay;
        {
            std::lock_guard<std::mutex> lock(m_snapshot.mutex);
            hourOfDay = m_snapshot.hourOfDay;
        }
        // Build darkness factor: 0 = full day, 1 = full night
        float darkness = 0.f;
        if      (hourOfDay >= 20.f || hourOfDay < 5.f)  darkness = 1.f;
        else if (hourOfDay >= 18.f && hourOfDay < 20.f) darkness = (hourOfDay - 18.f) / 2.f;
        else if (hourOfDay >=  5.f && hourOfDay <  8.f) darkness = 1.f - (hourOfDay - 5.f) / 3.f;
        // Max alpha 0.55 — enough to make night feel dark without obscuring gameplay
        if (darkness > 0.f) {
            unsigned char alpha = (unsigned char)(darkness * 140.f);
            DrawRectangle(0, 0, 1280, 720, Color{ 0, 0, 20, alpha });
        }
    }

    // Stockpile panel (screen-space)
    static const std::vector<std::string> emptyNames;
    const auto& skillNames = sharedSkillNames ? *sharedSkillNames : emptyNames;
    if (panel.open)
        m_renderSystem.DrawStockpilePanel(panel, skillNames);

    // HUD
    m_hud.Draw(m_snapshot, m_camera, m_roadBuildMode);

    // Road overlay mode label (bottom-left corner)
    {
        const char* modeLabel = m_showRoadCondition ? "Road: Condition" : "Road: Safety";
        DrawText(modeLabel, 8, 720 - 18, 10, Fade(LIGHTGRAY, 0.5f));
    }
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
    float  hour;
    float  heatDrainMod;
    {
        std::lock_guard<std::mutex> lock(m_snapshot.mutex);
        hour          = m_snapshot.hourOfDay;
        heatDrainMod  = m_snapshot.seasonHeatDrainMod;
    }

    // Base day/night colors (summer palette — warmest)
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

    Color base = midnight;
    for (int i = 0; i < N - 1; ++i) {
        if (hour >= keys[i].hour && hour < keys[i+1].hour) {
            float t = (hour - keys[i].hour) / (keys[i+1].hour - keys[i].hour);
            base = LerpColor(keys[i].color, keys[i+1].color, t);
            break;
        }
    }

    // Season tint derived from season properties:
    // Cold seasons (high heatDrainMod) get blue tint, warm/harvest seasons get orange/green.
    const auto& st = m_schema.seasonThresholds;
    if (heatDrainMod >= st.harshCold) {
        // Harsh cold: icy blue tint
        Color tint = { 60, 80, 120, 255 };
        base = LerpColor(base, tint, 0.18f);
    } else if (heatDrainMod >= st.moderateCold) {
        // Moderate cold (autumn-like): amber/orange tint
        Color tint = { 200, 130, 60, 255 };
        base = LerpColor(base, tint, 0.10f);
    } else if (heatDrainMod > st.mildCold) {
        // Mild cold (spring-like): slight green tint
        Color tint = { 100, 180, 140, 255 };
        base = LerpColor(base, tint, 0.07f);
    }
    // heatDrainMod == 0 (summer-like): no tint, use base

    return base;
}
