#pragma once
#include "raylib.h"
#include "Threading/RenderSnapshot.h"
#include <set>
#include <string>
#include <vector>

// HUD reads exclusively from RenderSnapshot — it has no access to the ECS
// registry and therefore cannot accidentally introduce sim/render data races.

class HUD {
public:
    void HandleInput(const RenderSnapshot& snapshot);
    void Draw(const RenderSnapshot& snapshot, const Camera2D& camera, bool roadBuildMode = false);

private:
    void DrawNeedBar(int x, int y, float value, float critThreshold,
                     const char* label, Color barColor) const;
    void DrawEventLog(const RenderSnapshot& snapshot);
    void DrawWorldStatus(const RenderSnapshot& snapshot) const;
    void DrawDebugOverlay(const RenderSnapshot& snapshot) const;
    void DrawHoverTooltip(const RenderSnapshot& snapshot, const Camera2D& cam) const;
    void DrawFacilityTooltip(const RenderSnapshot& snapshot, const Camera2D& cam) const;
    void DrawSettlementTooltip(const RenderSnapshot& snapshot, const Camera2D& cam) const;
    void DrawRoadTooltip(const RenderSnapshot& snapshot, const Camera2D& cam) const;
    void UpdateNotifications(const RenderSnapshot& snapshot);
    void DrawNotifications();

    int  logScroll    = 0;
    bool debugOverlay = false;
    bool marketOverlay = false;

    // Event log source filter: set of source tags to HIDE (empty = show all).
    // Mutable so DrawEventLog can handle mouse clicks on filter labels.
    std::set<std::string> m_logHiddenSources;

    void DrawMarketOverlay(const RenderSnapshot& snapshot) const;
    void DrawMinimap(const RenderSnapshot& snapshot) const;
    void DrawLoadWarnings(const RenderSnapshot& snapshot) const;

    // Critical event notifications (screen-centre flash)
    struct Notification {
        std::string message;
        float       timeLeft;   // seconds at 60fps
        Color       color;
    };
    std::vector<Notification> m_notifications;
    int m_lastLogSize = 0;  // tracks when new event log entries appear
};
