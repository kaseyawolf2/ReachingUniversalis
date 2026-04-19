#pragma once
#include <string>
#include <set>

// UIState — plain data struct that lives exclusively on the main thread.
//
// Owns all input-driven panel visibility, selection state, scroll positions,
// and pending action display strings.  It has no threading primitives, no locks,
// and is never accessed from SimThread.
struct UIState {
    // ---- Panel visibility ----
    bool showEventLog        = true;   // F2 toggles the event log panel
    bool showDebugOverlay    = false;  // F1 toggles the debug performance overlay
    bool showMarketOverlay   = false;  // M toggles the market price overlay

    // ---- Road display mode ----
    bool showRoadCondition   = false;  // O key toggles

    // ---- Road build mode (two-press N) ----
    bool  roadBuildMode      = false;
    float roadBuildSrcX      = 0.f;
    float roadBuildSrcY      = 0.f;

    // ---- Camera follow ----
    bool  followPlayer       = true;   // F key toggles

    // ---- Scroll positions ----
    int   logScroll              = 0;

    // ---- Event log source filter ----
    std::set<std::string> logHiddenSources;

    // ---- Pending action display ----
    std::string pendingAction;
    float       pendingActionTimer = 0.f;

    void SetPendingAction(const std::string& msg) {
        pendingAction      = msg;
        pendingActionTimer = 2.0f;
    }

    void Update(float dt) {
        if (pendingActionTimer > 0.f) {
            pendingActionTimer -= dt;
            if (pendingActionTimer <= 0.f) {
                pendingAction.clear();
                pendingActionTimer = 0.f;
            }
        }
    }
};
