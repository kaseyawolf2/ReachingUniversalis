#pragma once
#include "raylib.h"
#include <array>
#include <deque>
#include <map>
#include <string>
#include <entt/entt.hpp>

// ---- Domain enums ----

enum class NeedType     { Hunger = 0, Thirst = 1, Energy = 2 };
enum class ResourceType { Food, Water, Shelter };

// ---- Need data ----

struct Need {
    NeedType type;
    float value;              // 0.0 (depleted) to 1.0 (full)
    float drainRate;          // units drained per real second
    float criticalThreshold;  // below this = urgent
    float refillRate;         // units refilled per real second while satisfying
};

struct Needs {
    std::array<Need, 3> list;
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
};

struct Stockpile {
    std::map<ResourceType, float> quantities;
};

struct ProductionFacility {
    ResourceType  output;
    float         baseRate;          // units per game-hour at 1 worker
    entt::entity  settlement = entt::null;
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
    std::array<float, 3> needsAtZero     = { 0.f, 0.f, 0.f };
    float                stockpileEmpty  = 0.f;    // seconds with no food OR water
    float                migrateThreshold = 2.f * 60.f; // game-min before migrating; randomised at spawn
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
    bool foodLow    = false;   // food < LOW_THRESHOLD
    bool foodEmpty  = false;   // food < EMPTY_THRESHOLD
    bool waterLow   = false;
    bool waterEmpty = false;
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

// ---- Tags ----

struct PlayerTag {};   // marks the entity the HUD observes
