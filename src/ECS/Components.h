#pragma once
#include "raylib.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <string>
#include <vector>
#include <entt/entt.hpp>
#include "World/WorldSchema.h"

// ---- Domain enums ----

// DEPRECATED: NeedType is retained for backward compatibility during the
// transition to data-driven needs (WorldSchema).  Prefer int needId instead.
enum class NeedType     { Hunger = 0, Thirst = 1, Energy = 2, Heat = 3 };
enum class ResourceType { Food, Water, Shelter, Wood };  // DEPRECATED — use ResourceID (int) for new code

// Medieval resource IDs (matches worlds/medieval/resources.toml order)
// TODO: These will come from WorldSchema once systems are data-driven
static constexpr int RES_FOOD    = 0;
static constexpr int RES_WATER   = 1;
static constexpr int RES_SHELTER = 2;
static constexpr int RES_WOOD    = 3;

// ---- Need data ----

struct Need {
    NeedType type;            // DEPRECATED — use needId instead
    int needId = -1;          // generic need identifier (index into WorldSchema::needs)
    float value;              // 0.0 (depleted) to 1.0 (full)
    float drainRate;          // units drained per real second
    float criticalThreshold;  // below this = urgent
    float refillRate;         // units refilled per real second while satisfying

    // Convenience constructor: NeedType-based (backward compat)
    Need(NeedType t, float v, float dr, float crit, float rr)
        : type(t), needId(static_cast<int>(t)), value(v), drainRate(dr),
          criticalThreshold(crit), refillRate(rr) {}

    // Convenience constructor: needId-based (generic)
    Need(int id, float v, float dr, float crit, float rr)
        : type(static_cast<NeedType>(id)), needId(id), value(v), drainRate(dr),
          criticalThreshold(crit), refillRate(rr) {}

    Need() : type(NeedType::Hunger), needId(0), value(1.f), drainRate(0.f),
             criticalThreshold(0.3f), refillRate(0.f) {}
};

struct Needs {
    std::vector<Need> list;

    // Find a need by its integer ID; returns nullptr if not found.
    Need* ByID(int needId) {
        auto it = std::find_if(list.begin(), list.end(),
                               [needId](const Need& n) { return n.needId == needId; });
        return (it != list.end()) ? &*it : nullptr;
    }
    const Need* ByID(int needId) const {
        auto it = std::find_if(list.begin(), list.end(),
                               [needId](const Need& n) { return n.needId == needId; });
        return (it != list.end()) ? &*it : nullptr;
    }
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
    float         decisionCooldown = 0.f;  // real-seconds until next full decision re-evaluation
};

// ---- Profession ----
// Persistent component assigned at spawn and updated when an NPC changes role.
// Uses ProfessionID (int) from WorldSchema. Display names come from ProfessionDef.

struct Profession {
    int type     = -1;   // ProfessionID; -1 = unset (defaults to Idle at runtime)
    int prevType = -1;   // previous ProfessionID
    int careerChanges    = 0;
};

// Helper: ProfessionID → SkillID via flat professionToSkill vector.
// Returns INVALID_ID if the profession ID is out of range.
inline SkillID SkillForProfession(int profId, const WorldSchema& schema) {
    return schema.SkillForProfession(profId);
}

// ---- World components ----

struct ResourceNode {
    int type;
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
    float        afterglowHours = 0.f;  // game-hours of post-festival morale afterglow (halves drift)
};

struct Stockpile {
    std::map<int, float> quantities;
};

struct ProductionFacility {
    int           output;
    float         baseRate;                              // units per game-hour at 1 worker
    entt::entity  settlement = entt::null;
    std::map<int, float> inputsPerOutput;                // input consumed per 1 unit output (empty = none)
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
    entt::entity prevSettlement = entt::null; // previous home; used for homesickness return
};

// Tracks how long needs / stockpiles have been deprived (in gameDt seconds).
// Used by DeathSystem and AgentDecisionSystem for migration triggering.
// Core fields only — medieval-specific social mechanics live in optional
// components: SocialBehavior, BanditState, GriefState, TheftRecord,
// PersonalEventState, CharityState.
//
// WARNING: Use DeprivationTimer::Make(schema) to construct — the default
// constructor leaves needsAtZero empty (required for entt compatibility).
struct DeprivationTimer {
    static constexpr float DEFAULT_MIGRATE_THRESHOLD = 2.f * 60.f;  // 2 game-hours in game-minutes

    std::vector<float> needsAtZero;                     // indexed by NeedID; sized from schema.needs
    float              stockpileEmpty   = 0.f;          // seconds with no food, water, OR heat
    float              migrateThreshold = DEFAULT_MIGRATE_THRESHOLD;  // game-min before migrating; randomised at spawn
    float              purchaseTimer    = 0.f;           // game-hours since last emergency market purchase
    float              lastSatisfaction = 0.5f;          // rolling average of all needs (0-1); updated in ConsumptionSystem

    // Factory: construct with needsAtZero sized from schema.needs.
    // Optional migrateThreshold param (game-seconds); defaults to
    // DEFAULT_MIGRATE_THRESHOLD (2 game-hours = 2*60).  Most NPC spawns pass a randomised value.
    static DeprivationTimer Make(const WorldSchema& schema,
                                 float migThreshold = DEFAULT_MIGRATE_THRESHOLD) {
        DeprivationTimer dt;
        dt.needsAtZero.assign(schema.needs.size(), 0.f);
        dt.migrateThreshold = migThreshold;
        return dt;
    }
};

// ---- Mandatory social-mechanic components (factored out of DeprivationTimer) ----

// Social interaction cooldowns and one-shot flags.
// Mandatory component emplaced on all NPCs at spawn; use get<> in the main
// NPC loop, try_get<> only when accessing other entities.
struct SocialBehavior {
    // Timers governing how often social interactions can occur.
    struct InteractionCooldowns {
        float gossipCooldown        = 0.f;   // game-hours until NPC can gossip prices again (0 = ready)
        float gossipNudgeTimer      = 0.f;   // game-seconds remaining for gossip drift animation (0 = eligible)
        float chatTimer             = 0.f;   // game-seconds remaining in evening chat stop (0 = free to move)
        float greetCooldown         = 0.f;   // real-seconds until NPC can greet a neighbour again (0 = ready)
        float thankCooldown         = 0.f;   // real-seconds until NPC can thank player again (0 = ready)
        float teachCooldown         = 0.f;   // real-seconds until NPC can teach/learn again (0 = ready)
        float moodContagionCooldown = 0.f;   // game-seconds until NPC can receive mood boost again (0 = ready)
        float comfortCooldown       = 0.f;   // real-seconds until NPC can comfort a grieving neighbour again (0 = ready)
        float begTimer              = 0.f;   // game-hours until NPC can beg from a friend again (0 = ready)
        float skillCelebrateTimer   = 0.f;   // game-hours remaining for skill milestone celebration
        float reconcileGlow         = 0.f;   // game-hours remaining of post-reconciliation productivity boost (+5%)
    } cooldowns;

    // One-shot flags, milestone events, and mood-affecting states.
    // Field categories:
    //   Accumulating timer — counts upward over time; reset on specific events (not periodic decay).
    //   Persistent state   — value set/cleared on discrete events; no time-based decay.
    //   Persistent flag    — set once (true), never reset; gates one-time milestone logic.
    struct MoodState {
        /// [Accumulating timer] Incremented each tick while NPC has a prevSettlement;
        /// triggers homesick return migration when > 72 game-hours and satisfaction < 0.4.
        /// Reset to 0 on migration start and on arrival at new home.
        float        homesickTimer       = 0.f;
        /// [Persistent state] Entity who last gave charity, taught, or helped this NPC.
        /// Set on receiving aid; cleared to entt::null after a gratitude greeting fires.
        /// Also propagated to other NPCs via gossip. No time-based decay.
        entt::entity lastHelper          = entt::null;
        /// [Persistent state] Settlement name where NPC last ate; set in ConsumptionSystem.
        /// Cleared (string::clear) after a gratitude log fires. No time-based decay.
        std::string  lastMealSource;
        /// [Persistent flag] Set true once when elder wisdom transfer fires (age > 70,
        /// skill >= 0.6). Never reset. Guards one-time knowledge transfer to a younger NPC.
        bool         wisdomFired         = false;
        /// [Persistent flag] Set true once when NPC balance crosses 500g. Never reset.
        /// Guards one-time wealth celebration event log.
        bool         wealthCelebrated    = false;
        /// [Persistent flag] Set true once when any skill reaches 0.9. Never reset.
        /// Permanently boosts effective migrateThreshold by 1.5x and enables master-
        /// homecoming / master-departure morale effects on settlements.
        bool         masterSettled       = false;
        /// [Persistent flag] Set true after hauler bankruptcy demotes NPC back to worker.
        /// Never reset. Grants +0.0002 extra skill growth per tick and a higher mentor
        /// bonus (0.15) on second-chance hauler graduation.
        bool         bankruptSurvivor    = false;
    } mood;

    // State for family visits to other settlements.
    struct VisitState {
        float        timer  = 0.f;          // game-minutes remaining on family visit (0 = not visiting)
        entt::entity target = entt::null;   // settlement entity being visited
    } visit;
};

// Bandit-specific timers and state.
// Mandatory component emplaced on all NPCs at spawn; use get<> in the main
// NPC loop, try_get<> only when accessing other entities.
struct BanditState {
    float        banditPovertyTimer    = 0.f;   // game-hours as homeless exile with balance < 2g → bandit at 48h
    float        intimidationCooldown  = 0.f;   // game-seconds until next bandit intimidation log (0 = ready)
    float        fleeTimer             = 0.f;   // real-seconds remaining for post-theft flee (sprint away from settlement)
    float        panicTimer            = 0.f;   // real-seconds remaining of panic flight (skip decisions while > 0)
    std::string  gangName;                      // bandit gang name (set when lurking at a road with other bandits)
};

// Grief timers — mandatory component emplaced on all NPCs at spawn; use get<>
// in the main NPC loop, try_get<> only when accessing other entities.
struct GriefState {
    float griefTimer    = 0.f;   // game-hours remaining of grief (skip social, drain morale)
    float lastGriefDay  = -1.f;  // game-day when grief last started (-1 = never)
};

// Theft tracking — mandatory component emplaced on all NPCs at spawn; use get<>
// in the main NPC loop, try_get<> only when accessing other entities.
struct TheftRecord {
    float stealCooldown = 0.f;   // game-hours until NPC can steal again (0 = can steal)
    int   theftCount    = 0;     // lifetime theft count; >= 3 triggers exile
};

// Personal random event timers (illness, harvest bonus, strike).
// Mandatory component emplaced on all NPCs at spawn; use get<> in the main
// NPC loop, try_get<> only when accessing other entities.
struct PersonalEventState {
    float personalEventTimer = 0.f;   // game-hours until next personal event (randomised per NPC)
    float illnessTimer       = 0.f;   // game-hours remaining for minor illness (2× drain on one need)
    int   illnessNeedIdx     = 0;     // which need index (0=Hunger,1=Thirst,2=Energy) is ill
    float harvestBonusTimer  = 0.f;   // game-hours remaining for good-harvest bonus (1.5× worker contribution)
    float strikeDuration     = 0.f;   // game-hours remaining on work stoppage (0 = can work normally)
};

// Charity giving/receiving state.
// Mandatory component emplaced on all NPCs at spawn; use get<> in the main
// NPC loop, try_get<> only when accessing other entities.
struct CharityState {
    float        charityTimer    = 0.f;          // game-hours until NPC can help a starving neighbour again (0 = ready)
    float        helpedTimer     = 0.f;          // game-hours since receiving charity; > 0 → "recently helped"
    entt::entity gratitudeTarget = entt::null;   // entity to move toward while grateful
    float        gratitudeTimer  = 0.f;          // real-seconds remaining; > 0 → doing gratitude walk
};

// Social standing; accrued by charity & trade deliveries, lost by theft.
struct Reputation {
    float score = 0.f;
};

// ---- Inventory / Transport ----

struct Inventory {
    std::map<int, int> contents;
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
    std::string  lastRoute;                  // "A→B" label of the most recent delivery route
    int          consecutiveRouteCount = 0;  // how many consecutive deliveries on the same route
    float        mentorBonus          = 0.f;  // next-trip efficiency bonus from veteran mentorship
};

// ---- Economy ----

struct Money {
    float balance = 50.f;   // gold coins
};

struct Market {
    std::map<int, float> price;   // mid-market price per unit

    float GetPrice(int t) const {
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
        int   lastVisitedDay = 0;  // day this snapshot was recorded/updated
    };
    // settlement_name → last-known prices (capped to MAX_KNOWN entries)
    std::map<std::string, PriceSnapshot> known;
    static constexpr int MAX_KNOWN = 12;

    void Record(const std::string& name, float food, float water, float wood, int day = 0) {
        if ((int)known.size() >= MAX_KNOWN && known.find(name) == known.end())
            known.erase(known.begin());  // evict one entry to stay bounded
        known[name] = { food, water, wood, day };
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
    int consecutiveWorkHours = 0;  // hours worked without sleep/idle — overwork penalty at ≥10
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

// Season cycle: seasons are defined in WorldSchema and cycle by index.
// SeasonID is an int index into WorldSchema::seasons.

struct TimeManager {
    float gameSeconds  = 0.0f;   // total elapsed game-time seconds
    int   day          = 1;      // current day (1-indexed)
    float hourOfDay    = 6.0f;   // 0.0–24.0, starts at dawn
    int   tickSpeed    = 1;      // multiplier: 1, 2, 4, 16, or 0 (uncapped)
    int   speedIndex   = 1;      // 1-based Paradox-style index (1..5)
    bool  paused       = false;

    // Returns realDt unchanged — tickSpeed is handled by the sub-tick loop in
    // GameState::Update, not by scaling dt. Kept as a pass-through so systems
    // don't need to change their call sites.
    float GameDt(float realDt) const {
        return realDt;
    }

    // Returns the current SeasonID by cycling through schema-defined seasons.
    // Each season has its own durationDays; the cycle length is the sum of all.
    SeasonID CurrentSeason(const std::vector<SeasonDef>& seasons) const {
        if (seasons.empty()) return 0;
        int totalCycleDays = 0;
        for (const auto& s : seasons) totalCycleDays += s.durationDays;
        if (totalCycleDays <= 0) return 0;
        int dayInCycle = (day - 1) % totalCycleDays;
        int cumulative = 0;
        for (int i = 0; i < (int)seasons.size(); ++i) {
            cumulative += seasons[i].durationDays;
            if (dayInCycle < cumulative) return i;
        }
        return (int)seasons.size() - 1;
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
//
// Generic: `levels` is indexed by SkillID (int) from WorldSchema::skills.
// ForResource() and Advance() use schema mappings (SkillDef::forResource).
struct Skills {
    std::vector<float> levels;  // indexed by SkillID; size == schema.skills.size()

    // Construct with a given number of skills, all set to a default value.
    static Skills Make(const WorldSchema& schema, float defaultValue = 0.5f) {
        Skills sk;
        sk.levels.assign(schema.skills.size(), defaultValue);
        return sk;
    }

    // Direct access by SkillID.
    float Get(int skillId) const {
        if (skillId < 0 || skillId >= (int)levels.size()) return 0.f;
        return levels[skillId];
    }
    void Set(int skillId, float v) {
        if (skillId >= 0 && skillId < (int)levels.size())
            levels[skillId] = v;
    }

    // Returns the relevant skill for a given resource output type, using cached flat array.
    float ForResource(int rt, const WorldSchema& schema) const {
        if (rt >= 0 && rt < (int)schema.resourceToSkill.size()) {
            int sid = schema.resourceToSkill[rt];
            if (sid != INVALID_ID) return Get(sid);
        }
        return 0.5f;  // no skill mapped to this resource
    }

    // Returns the SkillID that maps to a given resource, or INVALID_ID.
    static int SkillIdForResource(int rt, const WorldSchema& schema) {
        if (rt >= 0 && rt < (int)schema.resourceToSkill.size())
            return schema.resourceToSkill[rt];
        return INVALID_ID;
    }

    // Advances the relevant skill for a resource by delta (capped at 1).
    // The delta is scaled by the skill's growthRate from the schema.
    void Advance(int rt, float delta, const WorldSchema& schema) {
        int sid = SkillIdForResource(rt, schema);
        if (sid != INVALID_ID && sid >= 0 && sid < (int)levels.size()) {
            levels[sid] = std::min(1.f, levels[sid] + delta * schema.SkillGrowthRate(sid));
        }
    }

    // Returns the SkillID with the highest value, or INVALID_ID if empty.
    int BestSkillId() const {
        if (levels.empty()) return INVALID_ID;
        int best = 0;
        for (int i = 1; i < (int)levels.size(); ++i)
            if (levels[i] > levels[best]) best = i;
        return best;
    }

    // Returns the highest skill value across all skills.
    float BestValue() const {
        float mx = 0.f;
        for (float v : levels) mx = std::max(mx, v);
        return mx;
    }

    // Returns the lowest skill value across all skills.
    float WorstValue() const {
        if (levels.empty()) return 0.f;
        float mn = levels[0];
        for (float v : levels) mn = std::min(mn, v);
        return mn;
    }

    // True if any skill is at or above the threshold.
    bool AnyAbove(float threshold) const {
        for (float v : levels) if (v >= threshold) return true;
        return false;
    }

    // True if all skills are at or above the threshold.
    // Returns false for an empty levels vector (no skills = not above anything).
    bool AllAbove(float threshold) const {
        if (levels.empty()) return false;
        for (float v : levels) if (v < threshold) return false;
        return true;
    }

    // Decay all skills by amount, clamped to floor.
    void DecayAll(float amount, float floor = 0.f) {
        for (float& v : levels)
            v = std::max(floor, v - amount);
    }

    // Grow all skills by amount, clamped to cap.
    void GrowAll(float amount, float cap = 1.f) {
        for (float& v : levels)
            v = std::min(cap, v + amount);
    }

    int Size() const { return (int)levels.size(); }

    float wisdomGriefDays = 0.f;  // days remaining of skill growth penalty after a wise elder dies
    entt::entity wisdomLineage = entt::null;  // deceased elder whose legacy this NPC carries
    std::string wisdomLineageName;             // name of the deceased elder (entity may be destroyed)

    entt::entity elderMentor = entt::null;    // highest-affinity skilled elder of same profession
    std::string  elderMentorName;             // name preserved for logging (entity may be destroyed)
    float        tributeDays = 0.f;           // days remaining of accelerated growth after mentor dies
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
    entt::entity workBestFriend = entt::null;  // coworker with highest cumulative workplace affinity
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
// goal is assigned. Goals influence behaviour via GoalDef::behaviourMod.
// GoalTypeID (int) indexes into WorldSchema::goals.

struct Goal {
    GoalTypeID goalId       = INVALID_ID; // index into WorldSchema::goals
    float    progress      = 0.f;   // current measured value
    float    target        = 100.f; // threshold to complete
    float    celebrateTimer = 0.f;  // game-hours remaining for personal celebration
    bool     halfwayLogged = false; // true once the 50% milestone log has fired
    float    cooldownTimer = 0.f;   // game-minutes remaining before this goal can re-complete (prevents spam)
};

// Helper: data-driven goal label via schema lookup; falls back to "Unknown" if out of range.
inline const char* GoalLabel(GoalTypeID goalId, const WorldSchema& schema) {
    if (goalId >= 0 && goalId < (int)schema.goals.size())
        return schema.goals[goalId].displayName.c_str();
    return "Unknown";
}

// Helper: data-driven goal unit suffix via schema lookup.
inline const char* GoalUnit(GoalTypeID goalId, const WorldSchema& schema) {
    if (goalId >= 0 && goalId < (int)schema.goals.size())
        return schema.goals[goalId].unit.c_str();
    return "";
}
