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

    enum class AgentRole { NPC, Hauler, Player };

    struct AgentEntry {
        float     x, y, size;
        Color     color, ringColor;
        bool      hasCargoDot;
        Color     cargoDotColor;
        AgentRole role;
        float     hungerPct, thirstPct, energyPct, heatPct = 1.f;
        AgentBehavior behavior;
        float       balance = 0.f;    // gold balance (all agents with Money component)
        float       ageDays = 0.f;    // current age in game-days
        float       maxDays = 80.f;   // life expectancy
        std::string npcName;          // name for tooltip display
        // Hauler route destination (only set when hauler is carrying goods to a settlement)
        bool  hasRouteDest = false;
        float destX = 0.f, destY = 0.f;
        // NPC's inferred profession based on home settlement's primary output
        std::string profession;
        // Settlement name the agent calls home (empty if player or no home)
        std::string homeSettlementName;
        // Skill levels (0-1); -1 if entity has no Skills component
        float farmingSkill     = -1.f;
        float waterSkill       = -1.f;
        float woodcuttingSkill = -1.f;
    };

    struct SettlementEntry {
        float        x, y, radius;
        std::string  name;
        bool         selected;
        // entity handle carried through so main thread can identify clicks
        uint32_t     entityId;
        float        foodStock  = 0.f;   // for health ring color
        float        waterStock = 0.f;
        float        woodStock  = 0.f;   // for health ring in winter
        int          pop        = 0;     // 0 = collapsed
        Season       season     = Season::Spring;   // snapshot season for ring logic
        std::string  specialty;          // e.g. "Farming", "Water", "Lumber"
    };

    struct RoadEntry {
        float x1, y1, x2, y2;
        bool  blocked;
        // Settlement names and prices at each end (for hover tooltip)
        std::string nameA, nameB;
        float foodA  = 0.f, waterA = 0.f, woodA = 0.f;
        float foodB  = 0.f, waterB = 0.f, woodB = 0.f;
    };

    struct FacilityEntry {
        float        x, y;
        ResourceType output;
        float        baseRate    = 0.f;   // units/game-hour at 1 worker, 1x season
        int          workerCount = 0;     // Working NPCs currently assigned here
        float        avgSkill    = 0.5f;  // average relevant skill of workers
        std::string  settlementName;      // home settlement name
    };

    // ---- World status bar ----
    struct SettlementStatus {
        std::string name;
        float       food  = 0.f;
        float       water = 0.f;
        float       wood  = 0.f;
        float       foodPrice  = 1.f;   // current market price
        float       waterPrice = 1.f;
        float       woodPrice  = 1.f;
        int         pop       = 0;
        int         haulers   = 0;     // hauler count (separate from pop)
        float       treasury  = 0.f;   // settlement gold reserves
        bool        hasEvent  = false;  // active random event
        std::string eventName;
        char        popTrend       = '=';  // '+', '=', or '-' over last few days
        char        foodPriceTrend = '=';  // price trend: '+' rising, '-' falling
        char        waterPriceTrend = '=';
        char        woodPriceTrend  = '=';
    };

    // ---- Stockpile panel (shown when a settlement is selected) ----
    struct StockpilePanel {
        bool                          open = false;
        std::string                   name;
        std::map<ResourceType, float> quantities;
        std::map<ResourceType, float> prices;        // market prices for display
        std::map<ResourceType, float> netRatePerHour; // estimated net flow (production - consumption), game-hours
        float                         treasury  = 0.f;
        int                           pop       = 0;
        int                           popCap    = 35;   // max pop from BirthSystem
        float                         stability = 0.f;   // 0-1 composite settlement health
        std::vector<EventLog::Entry>  recentEvents;      // last 5 events mentioning this settlement
    };

    // ---- Data fields ----

    std::vector<AgentEntry>       agents;
    std::vector<SettlementEntry>  settlements;
    std::vector<RoadEntry>        roads;
    std::vector<FacilityEntry>    facilities;
    std::vector<SettlementStatus> worldStatus;
    StockpilePanel                stockpilePanel;

    // HUD — clock
    int    day         = 1;
    int    hour        = 6;
    int    minute      = 0;
    float  hourOfDay   = 6.f;   // float, for sky colour interpolation
    Season season      = Season::Spring;
    float  temperature = 10.f;  // ambient °C

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
    float         heatPct        = 1.f;
    float         hungerCrit     = 0.3f;
    float         thirstCrit     = 0.3f;
    float         energyCrit     = 0.3f;
    float         heatCrit       = 0.3f;
    AgentBehavior playerBehavior = AgentBehavior::Idle;
    float         playerAgeDays  = 0.f;
    float         playerMaxDays  = 80.f;
    float         playerGold     = 0.f;
    float         playerFarmSkill  = -1.f;   // -1 = no Skills component
    float         playerWaterSkill = -1.f;
    float         playerWoodSkill  = -1.f;
    std::map<ResourceType, int> playerInventory;   // current carried goods

    // Camera follow target (player world position)
    float playerWorldX = 640.f;
    float playerWorldY = 360.f;

    // Event log
    std::vector<EventLog::Entry> logEntries;

    // ---- Sim thread diagnostics ----
    int simStepsPerSec = 0;   // sim steps executed in the last real second
    int totalEntities  = 0;   // total live entities in the registry

    // ---- Synchronisation ----
    mutable std::mutex mutex;

    // Non-copyable because of mutex; use swap() instead
    RenderSnapshot() = default;
    RenderSnapshot(const RenderSnapshot&) = delete;
    RenderSnapshot& operator=(const RenderSnapshot&) = delete;
};
