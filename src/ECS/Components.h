#pragma once
#include "raylib.h"
#include <array>
#include <cmath>
#include <deque>
#include <map>
#include <string>
#include <entt/entt.hpp>

// ---- Domain enums ----

enum class NeedType     { Hunger = 0, Thirst = 1, Energy = 2, Heat = 3 };
enum class ResourceType { Food, Water, Shelter, Wood };

// ---- Need data ----

struct Need {
    NeedType type;
    float value;              // 0.0 (depleted) to 1.0 (full)
    float drainRate;          // units drained per real second
    float criticalThreshold;  // below this = urgent
    float refillRate;         // units refilled per real second while satisfying
};

struct Needs {
    std::array<Need, 4> list;
};

// ---- Spatial / movement ----

struct Position  { float x, y; };
struct Velocity  { float vx, vy; };
struct MoveSpeed { float value; };

// ---- Agent behaviour ----

enum class AgentBehavior { Idle, SeekingFood, SeekingWater, SeekingSleep, Satisfying, Migrating, Sleeping, Working };

struct AgentState {
    AgentBehavior behavior = AgentBehavior::Idle;
    entt::entity  target   = entt::null;
};

// ---- World components ----

struct ResourceNode {
    ResourceType type;
    float interactionRadius;
};

struct Settlement {
    std::string name;
    float       radius = 120.0f;       // visual/interaction radius
    float       productionModifier = 1.f;   // multiplied into all production output
    float       modifierDuration   = 0.f;   // game-hours remaining on modifier
    std::string modifierName;              // e.g. "Drought", shown in HUD
    float       treasury = 200.f;          // gold; pays NPC wages; replenished by trade tax
};

struct Stockpile {
    std::map<ResourceType, float> quantities;
};

struct ProductionFacility {
    ResourceType  output;
    float         baseRate;                              // units per game-hour at 1 worker
    entt::entity  settlement = entt::null;
    std::map<ResourceType, float> inputsPerOutput;       // input consumed per 1 unit output (empty = none)
};

struct Road {
    entt::entity from = entt::null;
    entt::entity to   = entt::null;
    bool         blocked = false;
    float        banditTimer = 0.f;   // game-hours until auto-unblock (0 = manual only)
};

struct HomeSettlement {
    entt::entity settlement = entt::null;
};

// Tracks how long needs / stockpiles have been deprived (in gameDt seconds).
// Used by DeathSystem and AgentDecisionSystem for migration triggering.
struct DeprivationTimer {
    std::array<float, 4> needsAtZero     = { 0.f, 0.f, 0.f, 0.f };
    float                stockpileEmpty  = 0.f;    // seconds with no food, water, OR heat
    float                migrateThreshold = 2.f * 60.f; // game-min before migrating; randomised at spawn
    float                purchaseTimer   = 0.f;    // game-hours since last emergency market purchase
};

// ---- Inventory / Transport ----

struct Inventory {
    std::map<ResourceType, int> contents;
    int maxCapacity = 5;

    int TotalItems() const {
        int t = 0;
        for (const auto& [k, v] : contents) t += v;
        return t;
    }
};

enum class HaulerState { Idle, GoingToDeposit, GoingHome };

struct Hauler {
    HaulerState  state            = HaulerState::Idle;
    entt::entity targetSettlement = entt::null;
    float        waitTimer        = 0.f;   // game-hours before re-evaluating trade
    float        buyPrice         = 0.f;   // price per unit paid at pickup
    int          waitCycles       = 0;     // consecutive evaluations with no good route
};

// ---- Economy ----

struct Money {
    float balance = 50.f;   // gold coins
};

struct Market {
    std::map<ResourceType, float> price;   // mid-market price per unit

    float GetPrice(ResourceType t) const {
        auto it = price.find(t);
        return (it != price.end()) ? it->second : 1.f;
    }
};

// ---- Schedule ----

struct Schedule {
    int wakeHour  =  6;
    int workStart =  7;
    int workEnd   = 17;
    int sleepHour = 22;
};

// ---- Rendering ----

struct Renderable {
    Color color;
    float size;
};

// ---- Time ----

// At 1x speed: 1 real second = 1 game minute → 1 game day = 24 real minutes.
// GAME_MINS_PER_REAL_SEC controls how fast game time runs relative to real time.
static constexpr float GAME_MINS_PER_REAL_SEC = 1.0f;

// Season cycle: 30 game-days each, repeating Spring→Summer→Autumn→Winter.
enum class Season { Spring = 0, Summer = 1, Autumn = 2, Winter = 3 };

static constexpr int DAYS_PER_SEASON = 30;

inline const char* SeasonName(Season s) {
    switch (s) {
        case Season::Spring: return "Spring";
        case Season::Summer: return "Summer";
        case Season::Autumn: return "Autumn";
        case Season::Winter: return "Winter";
    }
    return "Spring";
}

// Production modifier per season (applied on top of all other modifiers).
inline float SeasonProductionModifier(Season s) {
    switch (s) {
        case Season::Spring: return 0.8f;   // growth season, moderate output
        case Season::Summer: return 1.0f;   // peak production
        case Season::Autumn: return 1.2f;   // harvest bonus
        case Season::Winter: return 0.2f;   // very low production
    }
    return 1.0f;
}

// Energy drain multiplier per season (1.0 = normal; winter = more drain).
inline float SeasonEnergyDrainMult(Season s) {
    return (s == Season::Winter) ? 1.8f : 1.0f;
}

// Approximate air temperature in degrees Celsius.
// Combines season baseline with time-of-day variation (cooler at night).
inline float AmbientTemperature(Season s, float hourOfDay) {
    // Season baseline (°C at noon)
    float base = (s == Season::Spring) ? 12.f :
                 (s == Season::Summer) ? 28.f :
                 (s == Season::Autumn) ? 8.f  : -8.f;  // winter
    // Diurnal swing: ±8°C, coldest at 4am, hottest at 2pm
    float swing = -8.f * std::cos((hourOfDay - 14.f) * 3.14159f / 12.f);
    return base + swing;
}

// Heat drain multiplier per season — summer = no cold, winter = full cold.
inline float SeasonHeatDrainMult(Season s) {
    switch (s) {
        case Season::Spring: return 0.15f;
        case Season::Summer: return 0.0f;
        case Season::Autumn: return 0.4f;
        case Season::Winter: return 1.0f;
    }
    return 0.0f;
}

struct TimeManager {
    float gameSeconds  = 0.0f;   // total elapsed game-time seconds
    int   day          = 1;      // current day (1-indexed)
    float hourOfDay    = 6.0f;   // 0.0–24.0, starts at dawn
    int   tickSpeed    = 1;      // multiplier: 1, 2, or 4
    bool  paused       = false;

    // Returns realDt unchanged — tickSpeed is handled by the sub-tick loop in
    // GameState::Update, not by scaling dt. Kept as a pass-through so systems
    // don't need to change their call sites.
    float GameDt(float realDt) const {
        return realDt;
    }

    Season CurrentSeason() const {
        int seasonDay = (day - 1) % (DAYS_PER_SEASON * 4);
        if (seasonDay < DAYS_PER_SEASON)     return Season::Spring;
        if (seasonDay < DAYS_PER_SEASON * 2) return Season::Summer;
        if (seasonDay < DAYS_PER_SEASON * 3) return Season::Autumn;
        return Season::Winter;
    }
};

// ---- Camera ----

struct CameraState {
    Camera2D cam = {
        { 640.0f, 360.0f },   // offset: screen centre
        { 640.0f, 360.0f },   // target: start looking at map centre
        0.0f,                 // rotation
        1.0f                  // zoom
    };
    float panSpeed    = 400.0f;   // pixels per second at zoom 1
    float zoomMin     = 0.25f;
    float zoomMax     = 3.0f;
    bool  followPlayer = true;    // if true, camera lerps to player position
};

// ---- Stockpile alert state (one per settlement) ----
// Tracks whether low/empty warnings have already been logged so they
// fire once on the way down and once on recovery, not every tick.
struct StockpileAlert {
    bool foodLow      = false;   // food < LOW_THRESHOLD
    bool foodEmpty    = false;   // food < EMPTY_THRESHOLD
    bool waterLow     = false;
    bool waterEmpty   = false;
    bool woodLow      = false;
    bool woodEmpty    = false;
    bool treasuryLow  = false;   // treasury < LOW_TREASURY
    bool treasuryEmpty = false;  // treasury < 1 gold
};

// ---- Birth tracker (one per settlement entity) ----

struct BirthTracker {
    float accumulator = 0.f;   // game-hours accumulated toward next birth
};

// ---- Event log (singleton component) ----

struct EventLog {
    struct Entry {
        int         day;
        int         hour;
        std::string message;
    };
    std::deque<Entry> entries;
    static constexpr int MAX_ENTRIES = 50;

    void Push(int day, int hour, const std::string& msg) {
        entries.push_front({ day, hour, msg });
        if ((int)entries.size() > MAX_ENTRIES) entries.pop_back();
    }
};

// ---- Name ----
struct Name {
    std::string value;
};

// ---- Age ----
// Tracks an agent's age in game-days; maxDays is the life expectancy.
// DeathSystem advances age and destroys the entity on reaching maxDays.
struct Age {
    float days    = 0.f;    // current age in game-days
    float maxDays = 80.f;   // life expectancy; randomised at spawn (60–100)
};

// ---- Tags ----

struct PlayerTag {};   // marks the entity the HUD observes
