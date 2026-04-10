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
    }
    return "Unknown";
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
    float playerFarmSkill, playerWaterSkill, playerWoodSkill;
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
        playerAgeDays   = snap.playerAgeDays;
        playerMaxDays   = snap.playerMaxDays;
        playerGold      = snap.playerGold;
        playerFarmSkill  = snap.playerFarmSkill;
        playerWaterSkill = snap.playerWaterSkill;
        playerWoodSkill  = snap.playerWoodSkill;
        playerInventory = snap.playerInventory;
        season      = snap.season;
        temperature = snap.temperature;
    }

    // ---- Player need bars (top-left) ----
    if (playerAlive) {
        int invLines  = (int)playerInventory.size();
        int skillLine = (playerFarmSkill >= 0.f) ? 1 : 0;
        DrawRectangle(4, 4, 320, BAR_GAP * (6 + skillLine) + 90 + invLines * 16, Fade(BLACK, 0.55f));
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

        // Skills (shown only when player has skills component)
        int skillsEndY = goldY + BAR_GAP;
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

        DrawText("WASD:Move  E:Work  Z:Sleep  H:Settle  T:Trade  B:Road  F:Follow  M:Market  F1:Debug",
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

    DrawWorldStatus(snap);
    DrawEventLog(snap);
    DrawHoverTooltip(snap, camera);
    DrawFacilityTooltip(snap, camera);
    if (debugOverlay)  DrawDebugOverlay(snap);
    if (marketOverlay) DrawMarketOverlay(snap);
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
                     : (!best->profession.empty()) ? best->profession.c_str()
                     : "NPC";
    bool isHauler  = (best->role == RenderSnapshot::AgentRole::Hauler);
    bool showGold  = (best->balance > 0.f || isHauler);

    char line1[80], line2[64], line3[64] = {}, line4[64] = {}, line5[64] = {}, line6[64] = {};
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

    if (hasName) {
        const char* lifeStage = (best->ageDays < 15.f)  ? "Child"   :
                                (best->ageDays < 25.f)  ? "Youth"   :
                                (best->ageDays > 70.f)  ? "Elderly" : "Adult";
        std::snprintf(line4, sizeof(line4), "Age: %.0f / %.0f  [%s]",
                      best->ageDays, best->maxDays, lifeStage);
    }
    if (showGold)
        std::snprintf(line5, sizeof(line5), "Gold: %.1f", best->balance);

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

    int lineCount = hasName ? 4 : 3;
    if (showGold) lineCount++;
    if (showSkill) lineCount++;

    int w1 = MeasureText(line1, 12), w2 = MeasureText(line2, 11);
    int w3 = MeasureText(line3, 11), w4 = MeasureText(line4, 11);
    int w5 = showGold  ? MeasureText(line5, 11) : 0;
    int w6 = showSkill ? MeasureText(line6, 11) : 0;
    int pw = std::max({w1, w2, w3, w4, w5, w6}) + 10;
    int ph = lineCount * 16;

    int tx = (int)screen.x + 14, ty = (int)screen.y - ph;
    if (tx + pw > SCREEN_W) tx = (int)screen.x - pw - 10;
    if (ty < 0) ty = (int)screen.y + 12;

    DrawRectangle(tx - 4, ty - 2, pw, ph, Fade(BLACK, 0.75f));

    int ly = ty;
    DrawText(line1, tx, ly, 12, WHITE);  ly += 16;
    DrawText(line2, tx, ly, 11, LIGHTGRAY); ly += 16;
    DrawText(line3, tx, ly, 11, hasName ? LIGHTGRAY : ageCol); ly += 16;
    if (hasName) { DrawText(line4, tx, ly, 11, ageCol); ly += 16; }
    if (showGold)  { DrawText(line5, tx, ly, 11, YELLOW); ly += 16; }
    if (showSkill) { DrawText(line6, tx, ly, 11, skillColor); }
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

    const char* typeName = (best->output == ResourceType::Food)  ? "Farm"        :
                           (best->output == ResourceType::Water) ? "Well"        :
                           (best->output == ResourceType::Wood)  ? "Lumber Mill" : "Facility";
    const char* resUnit  = (best->output == ResourceType::Food)  ? "food"  :
                           (best->output == ResourceType::Water) ? "water" : "wood";

    static constexpr int BASE_WORKERS = 5;
    float scale    = std::min(2.0f, std::max(0.1f, (float)best->workerCount / BASE_WORKERS));
    float skillMult = 0.5f + best->avgSkill;   // same formula as ProductionSystem
    float estOutput = best->baseRate * scale * skillMult;  // units/game-hour (no season)

    char line1[64], line2[64], line3[64], line4[64];
    std::snprintf(line1, sizeof(line1), "%s @ %s", typeName,
                  best->settlementName.empty() ? "?" : best->settlementName.c_str());
    std::snprintf(line2, sizeof(line2), "Workers: %d  Skill: %.0f%%",
                  best->workerCount, best->avgSkill * 100.f);
    std::snprintf(line3, sizeof(line3), "Base: %.1f %s/hr", best->baseRate, resUnit);
    std::snprintf(line4, sizeof(line4), "Est. output: ~%.1f %s/hr", estOutput, resUnit);

    Vector2 screen = GetWorldToScreen2D({ best->x, best->y }, cam);
    int w = std::max({ MeasureText(line1, 12), MeasureText(line2, 11),
                       MeasureText(line3, 11), MeasureText(line4, 11) }) + 12;
    int h = 4 * 16 + 4;
    int tx = (int)screen.x + 18, ty = (int)screen.y - h;
    if (tx + w > SCREEN_W) tx = (int)screen.x - w - 10;
    if (ty < 0) ty = (int)screen.y + 14;

    DrawRectangle(tx - 4, ty - 2, w, h, Fade(BLACK, 0.75f));
    Color typeCol = (best->output == ResourceType::Food)  ? GREEN  :
                   (best->output == ResourceType::Water) ? SKYBLUE : BROWN;
    DrawText(line1, tx, ty,      12, typeCol);
    DrawText(line2, tx, ty + 16, 11, LIGHTGRAY);
    DrawText(line3, tx, ty + 32, 11, Fade(LIGHTGRAY, 0.7f));
    Color outCol = (estOutput >= best->baseRate) ? GREEN : (estOutput >= best->baseRate * 0.5f) ? YELLOW : RED;
    DrawText(line4, tx, ty + 48, 11, outCol);
}

// ---- Market overlay (M key) ----
// Full price + stock table across all settlements — useful for merchant planning.

void HUD::DrawMarketOverlay(const RenderSnapshot& snap) const {
    std::vector<RenderSnapshot::SettlementStatus> ws;
    {
        std::lock_guard<std::mutex> lock(snap.mutex);
        ws = snap.worldStatus;
    }
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
