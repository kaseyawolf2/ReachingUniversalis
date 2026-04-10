#include "HUD.h"
#include "ECS/Components.h"
#include "raylib.h"
#include <algorithm>
#include <cstdio>

static const int BAR_W       = 160;
static const int BAR_H       = 16;
static const int BAR_X       = 10;
static const int BAR_START_Y = 10;
static const int BAR_SPACING = 26;

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

void HUD::Draw(entt::registry& registry, int totalDeaths) {
    // ---- Need bars (player entity) ----
    auto playerView = registry.view<PlayerTag, Needs, AgentState>();
    for (auto entity : playerView) {
        const auto& needs = playerView.get<Needs>(entity);
        const auto& state = playerView.get<AgentState>(entity);

        DrawRectangle(4, 4, 320, BAR_SPACING * 4 + 30, Fade(BLACK, 0.5f));

        DrawNeedBar(BAR_X, BAR_START_Y + BAR_SPACING * 0,
                    needs.list[0].value, needs.list[0].criticalThreshold, "Hunger", GREEN);
        DrawNeedBar(BAR_X, BAR_START_Y + BAR_SPACING * 1,
                    needs.list[1].value, needs.list[1].criticalThreshold, "Thirst", SKYBLUE);
        DrawNeedBar(BAR_X, BAR_START_Y + BAR_SPACING * 2,
                    needs.list[2].value, needs.list[2].criticalThreshold, "Energy", YELLOW);

        const int stateY = BAR_START_Y + BAR_SPACING * 3 + 4;
        DrawText("State:", BAR_X, stateY, 14, LIGHTGRAY);
        DrawText(BehaviorLabel(state.behavior), BAR_X + 52, stateY, 14, WHITE);

        // Key hints
        const int hintY = stateY + 20;
        DrawText("WASD:Move  E:Eat/Drink  R:Respawn  F:Follow", BAR_X, hintY, 11, Fade(LIGHTGRAY, 0.7f));

        break;
    }

    // ---- Time display (top-right) ----
    auto timeView = registry.view<TimeManager>();
    for (auto entity : timeView) {
        const auto& tm = timeView.get<TimeManager>(entity);

        int hours   = (int)tm.hourOfDay;
        int minutes = (int)((tm.hourOfDay - hours) * 60.0f);

        char timeBuf[64];
        std::snprintf(timeBuf, sizeof(timeBuf),
                      "Day %d  %02d:%02d", tm.day, hours, minutes);

        char speedBuf[16];
        if (tm.paused)
            std::snprintf(speedBuf, sizeof(speedBuf), "PAUSED");
        else
            std::snprintf(speedBuf, sizeof(speedBuf), "%dx", tm.tickSpeed);

        // Population count (entities with Needs, excluding PlayerTag)
        int pop = 0;
        registry.view<Needs>().each([&](auto e, auto&) {
            if (!registry.all_of<PlayerTag>(e)) ++pop;
        });

        char popBuf[32];
        std::snprintf(popBuf, sizeof(popBuf), "Pop: %d  Deaths: %d", pop, totalDeaths);

        // Right-align panel
        int timeW  = MeasureText(timeBuf,  16);
        int speedW = MeasureText(speedBuf, 14);
        int popW   = MeasureText(popBuf,   13);
        int panelW = std::max({timeW, speedW, popW}) + 16;
        int panelX = 1280 - panelW - 4;

        DrawRectangle(panelX, 4, panelW, 64, Fade(BLACK, 0.5f));
        DrawText(timeBuf,  panelX + 8,  8, 16, WHITE);
        DrawText(speedBuf, panelX + 8, 28, 14, tm.paused ? ORANGE : LIGHTGRAY);
        DrawText(popBuf,   panelX + 8, 46, 13, LIGHTGRAY);

        break;
    }
}

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
