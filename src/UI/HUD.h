#pragma once
#include "raylib.h"
#include "Threading/RenderSnapshot.h"

// HUD reads exclusively from RenderSnapshot — it has no access to the ECS
// registry and therefore cannot accidentally introduce sim/render data races.

class HUD {
public:
    void HandleInput(const RenderSnapshot& snapshot);
    void Draw(const RenderSnapshot& snapshot, const Camera2D& camera);

private:
    void DrawNeedBar(int x, int y, float value, float critThreshold,
                     const char* label, Color barColor) const;
    void DrawEventLog(const RenderSnapshot& snapshot) const;
    void DrawWorldStatus(const RenderSnapshot& snapshot) const;
    void DrawDebugOverlay(const RenderSnapshot& snapshot) const;
    void DrawHoverTooltip(const RenderSnapshot& snapshot, const Camera2D& cam) const;
    void DrawFacilityTooltip(const RenderSnapshot& snapshot, const Camera2D& cam) const;

    int  logScroll    = 0;
    bool debugOverlay = false;
    bool marketOverlay = false;

    void DrawMarketOverlay(const RenderSnapshot& snapshot) const;
};
