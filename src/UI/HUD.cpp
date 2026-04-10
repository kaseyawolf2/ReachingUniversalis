#include "HUD.h"
#include "ECS/Components.h"
#include "raylib.h"

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
    }
    return "Unknown";
}

void HUD::Draw(entt::registry& registry) {
    auto view = registry.view<PlayerTag, Needs, AgentState>();
    for (auto entity : view) {
        const auto& needs = view.get<Needs>(entity);
        const auto& state = view.get<AgentState>(entity);

        DrawRectangle(4, 4, BAR_W + 70, BAR_SPACING * 3 + 14, Fade(BLACK, 0.5f));

        DrawNeedBar(BAR_X, BAR_START_Y + BAR_SPACING * 0,
                    needs.list[0].value, needs.list[0].criticalThreshold, "Hunger", GREEN);
        DrawNeedBar(BAR_X, BAR_START_Y + BAR_SPACING * 1,
                    needs.list[1].value, needs.list[1].criticalThreshold, "Thirst", SKYBLUE);
        DrawNeedBar(BAR_X, BAR_START_Y + BAR_SPACING * 2,
                    needs.list[2].value, needs.list[2].criticalThreshold, "Energy", YELLOW);

        const int labelY = BAR_START_Y + BAR_SPACING * 3 + 4;
        DrawText("State:", BAR_X, labelY, 14, LIGHTGRAY);
        DrawText(BehaviorLabel(state.behavior), BAR_X + 52, labelY, 14, WHITE);

        break; // only one player
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
