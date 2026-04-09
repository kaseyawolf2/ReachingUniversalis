#include "HUD.h"

static const int BAR_W      = 160;
static const int BAR_H      = 16;
static const int BAR_X      = 10;
static const int BAR_START_Y = 10;
static const int BAR_SPACING = 26;

void HUD::Draw(const NPC& npc) const {
    // Semi-transparent background panel
    DrawRectangle(4, 4, BAR_W + 70, BAR_SPACING * 3 + 14, Fade(BLACK, 0.5f));

    DrawNeedBar(BAR_X, BAR_START_Y + BAR_SPACING * 0, npc.needs[0], "Hunger", GREEN);
    DrawNeedBar(BAR_X, BAR_START_Y + BAR_SPACING * 1, npc.needs[1], "Thirst", SKYBLUE);
    DrawNeedBar(BAR_X, BAR_START_Y + BAR_SPACING * 2, npc.needs[2], "Energy", YELLOW);

    // State label
    const int labelY = BAR_START_Y + BAR_SPACING * 3 + 4;
    DrawText("State:", BAR_X, labelY, 14, LIGHTGRAY);
    DrawText(npc.GetStateLabel(), BAR_X + 52, labelY, 14, WHITE);
}

void HUD::DrawNeedBar(int x, int y, const Need& need, const char* label, Color barColor) const {
    // Label
    DrawText(label, x, y, 14, LIGHTGRAY);

    int barX = x + 60;

    // Background track
    DrawRectangle(barX, y, BAR_W, BAR_H, Fade(WHITE, 0.15f));

    // Filled portion — clamp to valid range
    float clamped = need.value < 0.0f ? 0.0f : (need.value > 1.0f ? 1.0f : need.value);
    int fillW = (int)(clamped * BAR_W);

    // Turn red when below critical threshold
    Color fillColor = (need.value < need.criticalThreshold) ? RED : barColor;
    if (fillW > 0) DrawRectangle(barX, y, fillW, BAR_H, fillColor);

    // Border
    DrawRectangleLines(barX, y, BAR_W, BAR_H, WHITE);
}
