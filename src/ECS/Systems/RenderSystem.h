#pragma once
#include "ECS/Components.h"
#include "Threading/RenderSnapshot.h"
#include <map>
#include <string>

// RenderSystem now only handles the stockpile panel draw call.
// All world-space and agent rendering is done directly in GameState::Draw
// from the RenderSnapshot, keeping render and simulation fully decoupled.

class RenderSystem {
public:
    void DrawStockpilePanel(const RenderSnapshot::StockpilePanel& panel) const;
};
