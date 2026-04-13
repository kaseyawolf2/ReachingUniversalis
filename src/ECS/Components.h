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

enum class AgentBehavior { Idle, SeekingFood, SeekingWater, SeekingSleep, Satisfying, Migrating, Sleeping, Working, Celebrating };

struct AgentState {
    AgentBehavior behavior = AgentBehavior::Idle;
    entt::entity  target   = entt::null;
};

// ---- Profession ----
// Persistent component assigned at spawn and updated when an NPC changes role.
// Used for migration preference and tooltip display.
enum class ProfessionType { Farmer, WaterCarrier, Lumberjack, Hauler, Idle };

struct Profession {
    ProfessionType type = ProfessionType::Idle;
};

// Helper: map ResourceType → ProfessionType for production-facility matching.
inline ProfessionType ProfessionForResource(ResourceType rt) {
    switch (rt) {
        case ResourceType::Food:  return ProfessionType::Farmer;
        case ResourceType::Water: return ProfessionType::WaterCarrier;
        case ResourceType::Wood:  return ProfessionType::Lumberjack;
        default:                  return ProfessionType::Idle;
    }
}

// Helper: ProfessionType → display string
inline const char* ProfessionLabel(ProfessionType p) {
    switch (p) {
        case ProfessionType::Farmer:       return "Farmer";
        case ProfessionType::WaterCarrier: return "Water Carrier";
        case ProfessionType::Lumberjack:   return "Woodcutter";
        case ProfessionType::Hauler:       return "Merchant";
        default:                           return "Idle";
    }
}

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
    int         popCap   = 35;             // max population; can be expanded via housing
    float       morale        = 0.5f;      // 0-1: NPC happiness/social trust; >0.7 = +10% prod; <0.3 = unrest
    bool        unrest        = false;     // true while morale < 0.3 (logged once on crossing)
    float       strikeCooldown = 0.f;     // game-hours until next work-stoppage can fire (0 = eligible)
    float       ruinTimer     = 0.f;     // game-hours remaining in ruin state after collapse (no births, grey dot)
    // Inter-settlement relations: entity → score (-1 = rival, 0 = neutral, +1 = ally)
    // Updated when haulers complete deliveries. Rival → +10% trade surcharge; Ally → -5% tax.
    std::map<entt::entity, float> relations;
    int   tradeVolume      = 0;    // deliveries received; reset every 24 game-hours
    int   importCount      = 0;    // goods units received via hauler delivery (24h window)
    int   exportCount      = 0;    // goods units shipped out by haulers (24h window)
    int   desperatePurchases = 0;  // emergency market purchases this 24h cycle
    int   theftCount        = 0;   // cumulative NPC theft events at this settlement
    float tradeVolumeTimer = 0.f;  // game-hours until next reset
    float bountyPool       = 0.f;  // gold accumulated from adjacent-road bandits; paid to player on confrontation
    std::string  rivalWith;              // name of rival settlement (empty = no rivalry)
    float        rivalryTimer = 0.f;     // game-hours remaining on rivalry effect
    entt::entity rivalEntity = entt::null;  // entity of rival settlement
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
    float        banditTimer  = 0.f;   // game-hours until auto-unblock (0 = manual only)
    float        condition    = 1.f;   // 0-1, road quality; below ROAD_COLLAPSE threshold → auto-blocked
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
    float                stealCooldown   = 0.f;    // game-hours until NPC can steal again (0 = can steal)
    int                  theftCount      = 0;      // lifetime theft count; >= 3 triggers exile
    float                gossipCooldown  = 0.f;    // game-hours until NPC can gossip prices again (0 = ready)
    float                charityTimer    = 0.f;    // game-hours until NPC can help a starving neighbour again (0 = ready)
    float                helpedTimer     = 0.f;    // game-hours since receiving charity; > 0 → "recently helped"
    entt::entity         gratitudeTarget = entt::null;  // entity to move toward while grateful
    float                gratitudeTimer  = 0.f;          // real-seconds remaining; > 0 → doing gratitude walk
    float                banditPovertyTimer = 0.f;       // game-hours as homeless exile with balance < 2g → bandit at 48h
    float                strikeDuration     = 0.f;       // game-hours remaining on work stoppage (0 = can work normally)
    float                chatTimer          = 0.f;       // game-seconds remaining in evening chat stop (0 = free to move)
    float                personalEventTimer = 0.f;       // game-hours until next personal event (randomised per NPC)
    float                illnessTimer       = 0.f;       // game-hours remaining for minor illness (2× drain on one need)
    int                  illnessNeedIdx     = 0;         // which need index (0=Hunger,1=Thirst,2=Energy) is ill
    float                harvestBonusTimer  = 0.f;       // game-hours remaining for good-harvest bonus (1.5× worker contribution)
    float                gossipNudgeTimer   = 0.f;       // game-seconds remaining for gossip drift animation (0 = eligible)
    float                fleeTimer          = 0.f;       // real-seconds remaining for post-theft flee (sprint away from settlement)
    float                greetCooldown      = 0.f;       // real-seconds until NPC can greet a neighbour again (0 = ready)
    std::string          gangName;                       // bandit gang name (set when lurking at a road with other bandits)
    std::string          lastMealSource;                 // settlement name where NPC last ate; cleared after gratitude log
    float                panicTimer = 0.f;               // real-seconds remaining of panic flight (skip decisions while > 0)
    float                visitTimer = 0.f;               // game-minutes remaining on family visit (0 = not visiting)
    entt::entity         visitTarget = entt::null;       // settlement entity being visited
    entt::entity         lastHelper = entt::null;        // entity who last gave charity; for gratitude greeting
    float                intimidationCooldown = 0.f;    // game-seconds until next bandit intimidation log (0 = ready)
    float                skillCelebrateTimer  = 0.f;    // game-hours remaining for skill milestone celebration
    float                thankCooldown        = 0.f;    // real-seconds until NPC can thank player again (0 = ready)
};

// Social standing; accrued by charity & trade deliveries, lost by theft.
struct Reputation {
    float score = 0.f;
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
    entt::entity cargoSource      = entt::null;  // settlement where current cargo was loaded (for rivalry tracking)
    bool         nearBankrupt    = false;       // true when bankruptcy timer > 75% threshold
    float        bankruptProgress = 0.f;      // game-hours toward bankruptcy (0–24)
    bool         inConvoy       = false;       // true when travelling near another hauler headed same way
    float        bestProfit     = 0.f;        // highest single-trip profit achieved
    std::string  bestRoute;                   // "A→B" label of the best-profit trip
    bool         bankruptWarned = false;      // true after logging the 50% bankruptcy warning
    int          lifetimeTrips  = 0;         // total completed deliveries
    float        lifetimeProfit = 0.f;       // cumulative net profit across all trips
    std::string  worstRoute;                  // "A→B" label of the worst recent loss
    float        worstLoss       = 0.f;      // worst single-trip loss (negative profit)
    float        worstRouteTimer = 0.f;      // game-hours remaining on worst-route avoidance
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

// ---- Migration memory ----
// Carried by each NPC. Records the last-known market prices at each
// settlement they have visited (home, destinations, and settlements
// learned about through gossip). Updated on departure, on arrival,
// and during gossip exchanges. Used by FindMigrationTarget to bias
// NPCs toward historically cheaper destinations (better life signal).

struct MigrationMemory {
    struct PriceSnapshot {
        float food  = 1.f;
        float water = 1.f;
        float wood  = 1.f;
    };
    // settlement_name → last-known prices (capped to MAX_KNOWN entries)
    std::map<std::string, PriceSnapshot> known;
    static constexpr int MAX_KNOWN = 12;

    void Record(const std::string& name, float food, float water, float wood) {
        if ((int)known.size() >= MAX_KNOWN && known.find(name) == known.end())
            known.erase(known.begin());  // evict one entry to stay bounded
        known[name] = { food, water, wood };
    }

    const PriceSnapshot* Get(const std::string& name) const {
        auto it = known.find(name);
        return (it != known.end()) ? &it->second : nullptr;
    }
};

// ---- Schedule ----

struct Schedule {
    int wakeHour  =  6;
    int workStart =  7;
    int workEnd   = 17;
    int sleepHour = 22;
    bool fatigued = false;   // energy < 0.2 while working → production penalty
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

// ---- Skills ----
// Each value is 0-1: 0 = unskilled, 0.5 = average, 1 = mastery.
// Skills improve through practice. Skill multiplies a worker's production
// contribution in ProductionSystem (average skill of working NPCs scales output).
struct Skills {
    float farming       = 0.5f;  // food production efficiency
    float water_drawing = 0.5f;  // water production efficiency
    float woodcutting   = 0.5f;  // wood production efficiency

    // Returns the relevant skill for a given resource output type.
    float ForResource(ResourceType rt) const {
        switch (rt) {
            case ResourceType::Food:  return farming;
            case ResourceType::Water: return water_drawing;
            case ResourceType::Wood:  return woodcutting;
            default:                  return 0.5f;
        }
    }

    // Advances the relevant skill by delta (capped at 1).
    void Advance(ResourceType rt, float delta) {
        float* target = nullptr;
        switch (rt) {
            case ResourceType::Food:  target = &farming;       break;
            case ResourceType::Water: target = &water_drawing; break;
            case ResourceType::Wood:  target = &woodcutting;   break;
            default: return;
        }
        *target = std::min(1.f, *target + delta);
    }
};

// ---- Tags ----

struct PlayerTag {};   // marks the entity the HUD observes
struct ChildTag  {};   // NPC under age 15; removed at graduation (age 15)

// ---- Family ----
// Assigned when two adults at the same settlement form a household.
// Children inherit the tag at birth.
struct FamilyTag {
    std::string name;   // family/household name (usually a shared surname)
};

// ---- Bandit ----
// Applied to exiled NPCs (home.settlement == entt::null) who have had
// money.balance < 2g for 48+ game-hours. They lurk near road midpoints
// and intercept haulers within 40 units. Removed when balance recovers
// above 20g or when the player confronts them (E key).
struct BanditTag {};

// ---- NPC social relations ----
// Tracks pairwise affinity with other NPCs. Built up through evening
// proximity (chat events). Friends (affinity > 0.5) receive preferential
// charity and may follow each other during migration.
struct Relations {
    std::map<entt::entity, float> affinity;   // entity → score [0, 1]
};

// ---- NPC rumour propagation ----
// Attached to an NPC who has heard a rumour about a distant settlement event.
// Spreads via gossip exchanges; hops decrements on each transfer.
// When a rumour reaches a new settlement, its Market price is nudged (fear/hope).
enum class RumourType { PlagueNearby, DroughtNearby, BanditRoads, GoodHarvest };

struct Rumour {
    RumourType   type;
    entt::entity origin = entt::null;  // settlement entity where the rumour originated
    int          hops   = 3;           // remaining propagation hops; 0 = stale, no longer spreads
};

// ---- Personal goals ----
// Each NPC holds one active goal. When met, a celebration fires and a new
// goal is assigned. Goals influence behaviour (SaveGold → hoard; BecomeHauler
// → work harder for production bonus).
enum class GoalType { SaveGold, ReachAge, FindFamily, BecomeHauler };

struct Goal {
    GoalType type          = GoalType::SaveGold;
    float    progress      = 0.f;   // current measured value
    float    target        = 100.f; // threshold to complete
    float    celebrateTimer = 0.f;  // game-hours remaining for personal celebration
};

// Helper: human-readable goal description (e.g. for the event log)
inline const char* GoalLabel(GoalType g) {
    switch (g) {
        case GoalType::SaveGold:     return "Save Gold";
        case GoalType::ReachAge:     return "Reach Age";
        case GoalType::FindFamily:   return "Find Family";
        case GoalType::BecomeHauler: return "Become Merchant";
    }
    return "Unknown";
}
