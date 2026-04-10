#pragma once
#include "raylib.h"
#include "ECS/Components.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

// Written by the simulation thread at the end of each frame.
// Read by the main (render) thread.
//
// Access is protected by `mutex`. Both threads should lock before reading
// or writing the non-atomic fields. The mutex is held briefly — just long
// enough to swap the data — so contention is negligible at our entity count.

struct RenderSnapshot {

    // ---- Drawable world objects ----

    struct AgentEntry {
        float x, y, size;
        Color color, ringColor;
        bool  hasCargoDot;
        Color cargoDotColor;
    };

    struct SettlementEntry {
        float        x, y, radius;
        std::string  name;
        bool         selected;
        // entity handle carried through so main thread can identify clicks
        uint32_t     entityId;
    };

    struct RoadEntry {
        float x1, y1, x2, y2;
        bool  blocked;
    };

    struct FacilityEntry {
        float        x, y;
        ResourceType output;
    };

    // ---- World status bar ----
    struct SettlementStatus {
        std::string name;
        float       food  = 0.f;
        float       water = 0.f;
        int         pop   = 0;
    };

    // ---- Stockpile panel (shown when a settlement is selected) ----
    struct StockpilePanel {
        bool                          open = false;
        std::string                   name;
        std::map<ResourceType, float> quantities;
    };

    // ---- Data fields ----

    std::vector<AgentEntry>       agents;
    std::vector<SettlementEntry>  settlements;
    std::vector<RoadEntry>        roads;
    std::vector<FacilityEntry>    facilities;
    std::vector<SettlementStatus> worldStatus;
    StockpilePanel                stockpilePanel;

    // HUD — clock
    int   day       = 1;
    int   hour      = 6;
    int   minute    = 0;
    float hourOfDay = 6.f;   // float, for sky colour interpolation

    // HUD — simulation state
    int  tickSpeed  = 1;
    bool paused     = false;
    int  population = 0;
    int  totalDeaths = 0;
    bool roadBlocked = false;

    // HUD — player needs
    bool          playerAlive    = true;
    float         hungerPct      = 1.f;
    float         thirstPct      = 1.f;
    float         energyPct      = 1.f;
    float         hungerCrit     = 0.3f;
    float         thirstCrit     = 0.3f;
    float         energyCrit     = 0.3f;
    AgentBehavior playerBehavior = AgentBehavior::Idle;

    // Camera follow target (player world position)
    float playerWorldX = 640.f;
    float playerWorldY = 360.f;

    // Event log
    std::vector<EventLog::Entry> logEntries;

    // ---- Synchronisation ----
    mutable std::mutex mutex;

    // Non-copyable because of mutex; use swap() instead
    RenderSnapshot() = default;
    RenderSnapshot(const RenderSnapshot&) = delete;
    RenderSnapshot& operator=(const RenderSnapshot&) = delete;
};
