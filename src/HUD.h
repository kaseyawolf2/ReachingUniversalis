#pragma once
#include "NPC.h"

class HUD {
public:
    void Draw(const NPC& npc) const;

private:
    void DrawNeedBar(int x, int y, const Need& need, const char* label, Color barColor) const;
};
