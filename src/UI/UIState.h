#pragma once
#include <string>
#include <set>

// UIState — plain data struct that lives exclusively on the main thread.
//
// Owns all input-driven panel visibility, selection state, scroll positions,
// and pending action display strings.  It has no threading primitives, no locks,
// and is never accessed from SimThread.
//
// Usage:
//   - GameState owns a UIState m_uiState member.
//   - GameState::PollInput writes to m_uiState on key/mouse events.
//   - HUD::Draw reads the relevant fields to decide what to render.

struct UIState {

    // ---- Panel visibility ----
    // F1 key cycles through debug/normal, but event log has its own toggle.
    bool showEventLog        = true;   // F2 toggles the event log panel
    bool showDebugOverlay    = false;  // F1 toggles the debug performance overlay
    bool showMarketOverlay   = false;  // M toggles the market price overlay

    // ---- Road display mode ----
    // false = safety mode (bandit count drives color), true = condition mode
    bool showRoadCondition   = false;  // O key toggles

    // ---- Road build mode (two-press N) ----
    bool  roadBuildMode      = false;
    float roadBuildSrcX      = 0.f;
    float roadBuildSrcY      = 0.f;

    // ---- Camera follow ----
    bool  followPlayer       = true;   // F key toggles

    // ---- Selection state ----
    // Index into the RenderSnapshot settlement list (-1 = none)
    int   selectedSettlementIndex = -1;
    // Index into the RenderSnapshot agent list (-1 = none)
    int   selectedAgentIndex      = -1;

    // ---- Scroll positions ----
    int   logScroll              = 0;   // event log vertical scroll offset (lines)
    int   settlementListScroll   = 0;   // settlement list scroll offset

    // ---- Event log source filter ----
    // Set of source tag strings to HIDE from the event log; empty = show all.
    // Lives here so HUD can read it without owning it as private state.
    std::set<std::string> logHiddenSources;

    // ---- Pending action display ----
    // Set immediately when the player presses an action key, before the sim
    // confirms the action.  Cleared after pendingActionFrames reaches 0, or
    // when the sim confirms via a RenderSnapshot flag (future extension).
    std::string pendingAction;          // e.g. "Buying cheapest resource..."
    int         pendingActionFrames = 0;// countdown; cleared when it hits 0

    // Convenience: set a pending action that will display for ~2 seconds (120 frames).
    void SetPendingAction(const std::string& msg, int frames = 120) {
        pendingAction       = msg;
        pendingActionFrames = frames;
    }

    // Call once per frame to tick the countdown.
    void Update() {
        if (pendingActionFrames > 0) {
            --pendingActionFrames;
            if (pendingActionFrames <= 0) {
                pendingAction.clear();
                pendingActionFrames = 0;
            }
        }
    }
};
