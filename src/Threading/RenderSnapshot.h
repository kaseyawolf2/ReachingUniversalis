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

    enum class AgentRole { NPC, Hauler, Player, Child };

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
        // Hauler cargo contents (populated for Hauler role only)
        std::map<ResourceType, int> cargo;
        std::string destSettlName;   // name of destination settlement
        // NPC's inferred profession based on home settlement's primary output
        std::string profession;
        // Settlement name the agent calls home (empty if player or no home)
        std::string homeSettlementName;
        // Skill levels (0-1); -1 if entity has no Skills component
        float farmingSkill     = -1.f;
        float waterSkill       = -1.f;
        float woodcuttingSkill = -1.f;
        // Contentment: weighted average of all needs (0 = miserable, 1 = thriving)
        // Hunger 30%, Thirst 30%, Energy 20%, Heat 20%
        float contentment = 1.f;
        // For Child role: name of the adult they are currently following (empty if none)
        std::string followingName;
        // FamilyTag name if this NPC belongs to a named household (empty if none)
        std::string familyName;
        // True if this NPC received charity within the last game-hour
        bool recentlyHelped = false;
        // True if this NPC stole from a stockpile within the last 2 game-hours
        bool recentlyStole = false;
        // True if this NPC is currently walking toward a helper in gratitude
        bool isGrateful = false;
        // True if this NPC recently gave charity AND has very high heat (warmth glow)
        bool recentWarmthGlow = false;
        // True if this NPC's charity cooldown is 0 (ready to give charity)
        bool charityReady = false;
        // True if this NPC has the BanditTag (exiled, desperate, lurking on roads)
        bool isBandit = false;
        // True if this NPC is currently on strike (DeprivationTimer::strikeDuration > 0)
        bool onStrike = false;
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
        int          popCap     = 35;    // max population cap (for visual indicator)
        Season       season     = Season::Spring;   // snapshot season for ring logic
        std::string  specialty;          // e.g. "Farming", "Water", "Lumber"
        std::string  modifierName;       // active event name (e.g. "Plague", "Drought")
        float        ruinTimer  = 0.f;   // > 0 while settlement is in post-collapse ruin state
    };

    struct RoadEntry {
        float x1, y1, x2, y2;
        bool  blocked;
        float condition = 1.f;   // 0-1 road quality; affects render color
        // Settlement names and prices at each end (for hover tooltip)
        std::string nameA, nameB;
        float foodA  = 0.f, waterA = 0.f, woodA = 0.f;
        float foodB  = 0.f, waterB = 0.f, woodB = 0.f;
        // Inter-settlement relations: A's view of B and B's view of A (-1=rival, 0=neutral, +1=ally)
        float relAtoB = 0.f, relBtoA = 0.f;
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
        int         childCount = 0;    // NPCs under age 15 (subset of pop)
        float       treasury  = 0.f;   // settlement gold reserves
        bool        hasEvent  = false;  // active random event
        std::string eventName;
        char        popTrend       = '=';  // '+', '=', or '-' over last few days
        char        foodPriceTrend = '=';  // price trend: '+' rising, '-' falling
        char        waterPriceTrend = '=';
        char        woodPriceTrend  = '=';
        bool        hungerCrisis   = false; // any NPC < 15% hunger at this settlement
        int         elderCount     = 0;     // NPCs over age 60
        float       elderBonus     = 0.f;   // production bonus from elders (0–5%)
    };

    // ---- Stockpile panel (shown when a settlement is selected) ----
    struct StockpilePanel {
        bool                          open = false;
        std::string                   name;
        std::map<ResourceType, float> quantities;
        std::map<ResourceType, float> prices;          // market prices for display
        std::map<ResourceType, float> netRatePerHour;  // estimated net flow (production - consumption), game-hours
        std::map<ResourceType, float> prodRatePerHour; // gross production rate estimate
        std::map<ResourceType, float> consRatePerHour; // gross consumption rate estimate
        float                         treasury  = 0.f;
        int                           pop       = 0;
        int                           childCount = 0;   // NPCs under age 15
        int                           popCap    = 35;   // max pop from BirthSystem
        float                         stability = 0.f;   // 0-1 composite settlement health (internal)
        float                         morale    = 0.5f;  // 0-1 settlement morale (from Settlement component)
        int                           workers   = 0;     // current number of Working NPCs
        std::string                   modifierName;      // active event ("Plague", "Drought", etc.)
        float                         modifierHoursLeft = 0.f;
        std::vector<EventLog::Entry>  recentEvents;      // last 5 events mentioning this settlement
        // Population history — one entry per sample day, newest last (up to 30 points)
        std::vector<int>              popHistory;
        // Residents list — homed NPCs sorted by gold balance descending, max 12
        struct AgentInfo {
            std::string name;
            float       balance    = 0.f;
            std::string profession;
            std::string familyName;   // FamilyTag::name, empty if no family
        };
        std::vector<AgentInfo>        residents;
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
    int                         playerInventoryCapacity = 15; // max carry capacity

    // Player reputation — earned by trading, building, founding, repairing
    int   playerReputation    = 0;
    // Rank title corresponding to reputation level
    std::string playerRank;

    // Camera follow target (player world position)
    float playerWorldX = 640.f;
    float playerWorldY = 360.f;

    // Player trade ledger — last few completed trades (newest first)
    struct TradeRecord {
        std::string  description;  // e.g. "Sold 3 food at Wellsworth +18.0g"
        float        profit;       // positive = gain, negative = loss
    };
    std::vector<TradeRecord> tradeLedger;   // max 6 entries

    // Best available trade hint for the player HUD
    // e.g. "Food: buy Ashford 1.2g → sell Wellsworth 7.8g (+6.6g)"
    // Empty string means prices are too balanced to suggest a trade.
    std::string tradeHint;

    // Set when the player is standing inside a plague-afflicted settlement.
    // Triggers a warning in the player HUD panel.
    bool playerInPlagueZone = false;

    // Event log
    std::vector<EventLog::Entry> logEntries;

    // ---- Economy-wide statistics ----
    float econTotalGold     = 0.f;  // sum of all NPC + player balances + settlement treasuries
    float econAvgNpcWealth  = 0.f;  // mean NPC (non-player) gold balance
    float econRichestWealth = 0.f;  // wealthiest individual NPC's balance
    std::string econRichestName;    // name of wealthiest NPC
    int   econHaulerCount   = 0;    // total haulers in the world

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
