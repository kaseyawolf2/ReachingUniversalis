#pragma once
#include "raylib.h"
#include "Threading/RenderSnapshot.h"
#include "World/WorldSchema.h"
#include "UI/UIState.h"
#include <set>
#include <string>
#include <vector>

// HUD reads exclusively from RenderSnapshot — it has no access to the ECS
// registry and therefore cannot accidentally introduce sim/render data races.

class HUD {
public:
    void HandleInput(const RenderSnapshot& snapshot,
                     UIState& uiState,
                     const KeyBindings* keyBindings = nullptr);
    void Draw(const RenderSnapshot& snapshot, const Camera2D& camera,
              UIState& uiState,
              const KeyBindings* keyBindings = nullptr);

private:
    void DrawNeedBar(int x, int y, float value, float critThreshold,
                     const char* label, Color barColor) const;
    void DrawEventLog(const RenderSnapshot& snapshot, UIState& uiState);
    void DrawWorldStatus(const RenderSnapshot& snapshot) const;
    void DrawDebugOverlay(const RenderSnapshot& snapshot) const;
    void DrawHoverTooltip(const RenderSnapshot& snapshot, const Camera2D& cam) const;
    void DrawFacilityTooltip(const RenderSnapshot& snapshot, const Camera2D& cam) const;
    void DrawSettlementTooltip(const RenderSnapshot& snapshot, const Camera2D& cam) const;
    void DrawRoadTooltip(const RenderSnapshot& snapshot, const Camera2D& cam) const;
    void DrawPendingAction(const UIState& uiState) const;
    void UpdateNotifications(const RenderSnapshot& snapshot);
    void DrawNotifications();

    void DrawMarketOverlay(const RenderSnapshot& snapshot) const;
    void DrawMinimap(const RenderSnapshot& snapshot) const;

    // Critical event notifications (screen-centre flash)
    struct Notification {
        std::string message;
        float       timeLeft;   // seconds at 60fps
        Color       color;
    };
    std::vector<Notification> m_notifications;
    int m_lastLogSize = 0;  // tracks when new event log entries appear
};
