#pragma once
// WorldSchema.h — Runtime data structure for world definitions.
//
// Generic, ID-indexed definitions for needs, resources, skills, professions,
// facilities, seasons, events, goals, and agent templates.  Populated at
// load time by WorldLoader from TOML config files; consumed (read-only) by
// every ECS system via `const WorldSchema&`.
//
// All IDs are plain ints.  String-to-ID lookup maps are built at load time
// so systems never do string comparisons in the hot loop.

#include <string>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "SeasonDef.h"

// ---- ID types (strong-typedef via wrapper) ----
// We use plain ints for speed but wrap them so call sites stay readable.

using NeedID       = int;
using ResourceID   = int;
using SkillID      = int;
using ProfessionID = int;
// SeasonID is defined in SeasonDef.h
using EventID      = int;
using GoalTypeID   = int;

static constexpr int INVALID_ID = -1;

// ---- Definition structs ----

struct NeedDef {
    NeedID      id          = INVALID_ID;
    std::string name;                         // "Hunger", "Thirst", ...
    std::string displayName;                  // user-facing label
    float       drainRate          = 0.001f;  // units per real-second at 1× speed
    float       refillRate         = 0.005f;  // units per real-second while satisfying
    float       criticalThreshold  = 0.3f;    // below this = urgent
    float       startValue         = 1.0f;    // initial value at spawn
    ResourceID  satisfiedBy        = INVALID_ID;  // resource that refills this need (-1 = special, e.g. sleep)
    std::string satisfyBehavior;              // "consume", "sleep", "warmth" — how the need is met
};

struct ResourceDef {
    ResourceID  id          = INVALID_ID;
    std::string name;                         // "Food", "Water", ...
    std::string displayName;
    float       basePrice   = 1.0f;           // starting market price
    float       spoilRate   = 0.0f;           // units lost per game-hour in stockpile (0 = no spoilage)
    float       weight      = 1.0f;           // inventory slots per unit
};

struct SkillDef {
    SkillID     id          = INVALID_ID;
    std::string name;                         // "farming", "water_drawing", ...
    std::string displayName;
    ResourceID  forResource = INVALID_ID;     // which resource this skill boosts (-1 = general)
    float       startValue  = 0.5f;           // initial skill level (0–1)
    float       growthRate  = 1.0f;           // multiplier on practice-based growth
    float       decayRate   = 0.0f;           // skill loss per game-day when not practicing
};

struct ProfessionDef {
    ProfessionID id         = INVALID_ID;
    std::string  name;                        // "Farmer", "WaterCarrier", ...
    std::string  displayName;
    ResourceID   producesResource = INVALID_ID; // primary output (-1 = none, e.g. Hauler)
    SkillID      primarySkill     = INVALID_ID; // skill used when working
    bool         isIdle           = false;       // true for the "Idle" pseudo-profession
    bool         isHauler         = false;       // true for hauler/merchant role
};

// SeasonDef is defined in SeasonDef.h

// Enum for EventDef effect type (no string comparisons in hot loops)
enum class EventEffectType {
    None,
    ProductionModifier,
    StockpileDestroy,
    RoadBlock,
    TreasuryBoost,
    SpawnNpcs,
    StockpileAdd,
    Convoy,
    Earthquake,
    Fire,
    PriceSpike,
    MoraleBoost,
};

struct EventDef {
    EventID     id            = INVALID_ID;
    std::string name;                         // "Drought", "Plague", ...
    std::string displayName;
    EventEffectType effectEnum = EventEffectType::None;  // resolved at load time from TOML "effect_type"
    float       effectValue   = 1.0f;         // magnitude of the effect
    float       durationHours = 0.0f;         // game-hours the effect lasts (0 = instant)
    float       chance        = 0.01f;        // probability per check (per settlement per hour)
    int         minPopulation = 0;            // minimum pop to trigger
    // --- Extended fields for data-driven event effects ---

    // Morale impact when event fires (negative = demoralising, positive = uplifting)
    float       moraleImpact  = 0.0f;

    // Kill a fraction of the settlement population (0 = no deaths)
    float       killFraction  = 0.0f;

    // Stockpile effects: which resource to affect and by how much
    // "target_resource" names (e.g. "Food", "Water", "Wood"); empty = first/most-expensive
    std::string targetResource;               // resource name for stockpile effects
    int         targetResourceId = INVALID_ID; // resolved at load time

    // Destroy a specific resource by fraction (e.g. HeatWave destroys water)
    std::string destroyResource;              // resource name to destroy (empty = none)
    int         destroyResourceId = INVALID_ID; // resolved at load time
    float       destroyFraction   = 0.0f;     // fraction of stockpile to destroy (0 = none)

    // Multiple resource destruction (e.g. fire destroys food + wood)
    // Each pair is (resourceId, fraction); resolved at load time.
    std::vector<std::pair<std::string, float>> destroyResourceNames; // raw names from TOML
    std::vector<std::pair<int, float>>         destroyResources;     // resolved IDs

    // Additional stockpile additions (e.g. rainstorm adds water)
    std::string addResource;                  // resource name to add
    int         addResourceId = INVALID_ID;   // resolved at load time
    float       addAmount     = 0.0f;         // units to add

    // Treasury effects (gold added to settlement treasury, can be negative)
    float       treasuryChange = 0.0f;

    // Road effects
    bool        blockAllRoads       = false;  // block ALL roads in the world (e.g. blizzard)
    float       roadBlockDuration   = 0.0f;   // game-hours roads stay blocked
    float       roadDamage          = 0.0f;   // condition damage to roads (0-1)

    // Spread behavior (plague-like: event can spread along roads to neighbours)
    bool        spreads           = false;
    float       spreadInterval    = 0.0f;     // game-hours between spread attempts
    float       spreadChance      = 0.0f;     // probability per spread attempt (0-1)
    float       spreadKillFraction = 0.0f;    // kill fraction when spreading

    // Facility destruction chance (earthquake-like)
    float       facilityDestroyChance = 0.0f;

    // Season constraints: event only fires when season matches
    float       seasonMinHeatDrain = -1.0f;   // min heatDrainMod for season (-1 = no constraint)
    float       seasonMaxHeatDrain = 999.0f;  // max heatDrainMod for season
    float       seasonMinTemp      = -999.0f; // min baseTemperature for season
    float       seasonMinProdMod   = -1.0f;   // min productionMod for season (-1 = no constraint)

    // Rumour seeding: which rumour type to inject (empty = none)
    std::string rumourType;                   // "PlagueNearby", "DroughtNearby", "GoodHarvest", etc.
    int         rumourSeeds    = 0;            // how many NPCs get the rumour

    // Solidarity: whether the crisis triggers affinity boosts among residents
    bool        triggersSolidarity = false;
    float       solidarityBoost    = 0.0f;    // affinity boost amount

    // Festival-like: NPCs switch to Celebrating behavior
    bool        triggersCelebration = false;

    // Price spike multiplier (0 = no price effect)
    float       priceSpikeMultiplier = 0.0f;

    // NPC spawning (migration wave): effectValue is the count
    // Skilled immigrant: spawns one high-skill NPC
    bool        spawnSkilled = false;

    // Applies to all settlements (e.g. rainstorm adds water everywhere)
    bool        affectsAllSettlements = false;

    // Breaks drought at the target settlement if active
    bool        breaksDrought = false;

    // Convoy: buy most-expensive resource from off-map
    float       convoyAmount   = 0.0f;
    float       convoyMinPrice = 0.0f;        // only dispatch when scarcest price >= this
};

// Resolved enums for GoalDef (no string comparisons in hot loops)
enum class GoalCheckType { None, BalanceGte, AgeGte, HasFamily, HasProfession };
enum class GoalBehaviourMod { None, Hoard, Ambitious };
enum class GoalTargetMode { Fixed, RelativeBalance, RelativeAge };

struct GoalDef {
    /// Unique index into WorldSchema::goals (== position in the vector).
    /// Assigned automatically by WorldSchema::BuildMaps(); not set from TOML.
    /// Consumed by: WorldSchema::BuildMaps, WorldSchema::goalsByName.
    GoalTypeID  id          = INVALID_ID;

    /// Internal identifier string used as the TOML key and for name-to-ID lookups.
    /// Must be unique across all goals.  Examples: "SaveGold", "ReachAge".
    /// Consumed by: WorldLoader (parsing, cross-ref resolution, validation warnings),
    ///              WorldSchema::BuildMaps (populates goalsByName).
    std::string name;

    /// User-facing label shown in HUD tooltips and event-log messages.
    /// Defaults to `name` if omitted in TOML.
    /// Consumed by: GoalLabel() helper (Components.h), SimThread::WriteSnapshot (tooltip text).
    std::string displayName;

    /// Determines how goal progress is measured each tick.  Resolved at load
    /// time from the TOML "check_type" string to avoid hot-loop string comparisons.
    ///   - BalanceGte : progress = NPC's gold balance (Money::balance).
    ///   - AgeGte     : progress = NPC's age in days (Age::days).
    ///   - HasFamily  : progress = 1 if NPC has FamilyTag, else 0.
    ///   - HasProfession : progress = 1 if NPC's Profession::type matches targetProfessionId.
    ///   - None       : progress is never updated (goal cannot complete on its own).
    /// Consumed by: AgentDecisionSystem (progress evaluation), EconomicMobilitySystem
    ///              (marks hauler-profession goals complete on graduation),
    ///              WorldLoader (validation of required companion fields).
    GoalCheckType checkTypeEnum = GoalCheckType::None;

    /// The completion threshold: the goal is met when progress >= targetValue.
    /// For Fixed target mode this is the literal threshold.  For relative modes
    /// it acts as a minimum floor (see targetModeEnum).
    /// Valid range: >= 0.  A value of 0 with Fixed mode triggers a load-time
    /// validation warning for BalanceGte / AgeGte goals (trivially complete).
    /// Consumed by: AgentDecisionSystem (target computation), WorldGenerator
    ///              (initial goal assignment), WorldLoader (validation).
    float       targetValue = 0.0f;

    /// Controls how the per-NPC target is derived from targetValue.
    /// Resolved at load time from the TOML "target_mode" string.
    ///   - Fixed           : target = targetValue.
    ///   - RelativeBalance : target = max(targetValue, currentBalance + offset).
    ///   - RelativeAge     : target = currentAge + offset.
    /// Consumed by: AgentDecisionSystem (target computation on goal assignment
    ///              and re-assignment), WorldGenerator (initial goal assignment),
    ///              WorldLoader (validation).
    GoalTargetMode targetModeEnum = GoalTargetMode::Fixed;

    /// Additive term used by relative target modes to shift the per-NPC target
    /// above the NPC's current value.  For RelativeBalance, the target becomes
    /// max(targetValue, balance + offset).  For RelativeAge, target = age + offset.
    /// Ignored when targetModeEnum is Fixed.
    /// Valid range: any float (typically positive).
    /// Consumed by: AgentDecisionSystem (target computation), WorldGenerator
    ///              (initial goal assignment).
    float       offset      = 0.0f;

    /// Relative probability weight for random goal selection.  When a new goal
    /// is assigned (at spawn or after completion), a weighted random draw selects
    /// among all GoalDefs proportional to their weight values.
    /// Valid range: > 0.  Higher values mean more frequent selection.
    /// Consumed by: AgentDecisionSystem (weighted random goal pick),
    ///              WorldGenerator (initial goal assignment).
    float       weight      = 1.0f;

    /// Display-only unit suffix appended to progress/target numbers in HUD
    /// tooltips and event-log messages (e.g. "g" for gold, "d" for days, "" for none).
    /// Consumed by: GoalUnit() helper (Components.h), SimThread::WriteSnapshot
    ///              (tooltip text), AgentDecisionSystem (halfway milestone log).
    std::string unit;

    /// Template string logged when the goal completes.  The placeholder "{name}"
    /// is replaced with the NPC's Name::value at runtime.
    /// Defaults to "{name} completed a goal!" if omitted in TOML.
    /// Consumed by: AgentDecisionSystem (goal completion event-log entry).
    std::string completionMessage;

    /// Modifies NPC economic behaviour while this goal is active.  Resolved at
    /// load time from the TOML "behaviour_mod" string.
    ///   - None     : no behaviour change.
    ///   - Hoard    : NPC doubles its emergency purchase interval (buys less
    ///                frequently), conserving gold toward a savings goal.
    ///   - Ambitious: NPC produces 10% more output at their work facility.
    /// Consumed by: ConsumptionSystem (Hoard: doubles purchase interval),
    ///              ProductionSystem (Ambitious: 1.1x worker contribution).
    GoalBehaviourMod behaviourModEnum = GoalBehaviourMod::None;

    /// For HasProfession goals: the ProfessionID that the NPC's Profession::type
    /// must match for the goal to complete.  Resolved at load time from the TOML
    /// "target_profession" string via WorldSchema::FindProfession(); defaults to
    /// the hauler profession if omitted.  INVALID_ID triggers a load-time warning.
    /// Consumed by: AgentDecisionSystem (HasProfession progress check),
    ///              EconomicMobilitySystem (marks hauler goals complete on graduation),
    ///              WorldLoader (cross-ref resolution and validation).
    ProfessionID targetProfessionId = INVALID_ID;

    /// Cooldown in game-minutes after a goal completes before progress is checked
    /// again.  Prevents immediate re-completion when the same or a similar goal
    /// is re-assigned (e.g. a fixed-target goal that is already satisfied).
    /// Valid range: >= 0.  Default: 5.0 game-minutes.
    /// Consumed by: AgentDecisionSystem (sets Goal::cooldownTimer on completion;
    ///              drains the timer each tick and skips progress checks until it expires).
    float       completionCooldown = 5.0f;
};

struct FacilityDef {
    int         id          = INVALID_ID;
    std::string name;                         // "Farm", "Well", "LumberCamp"
    std::string displayName;
    ResourceID  outputResource = INVALID_ID;
    float       baseRate       = 1.0f;        // units per game-hour per worker
    // Inputs consumed per 1 unit of output (empty = none)
    std::vector<std::pair<ResourceID, float>> inputs;
    float       buildCost      = 200.0f;      // gold to construct
};

struct AgentTemplateDef {
    int         id          = INVALID_ID;
    std::string name;                         // "Villager", "Hauler", "Player"
    std::string displayName;
    ProfessionID startProfession = INVALID_ID;
    float       startGold       = 50.0f;
    float       moveSpeed       = 80.0f;
    int         inventoryCapacity = 5;
    float       minAge          = 18.0f;      // starting age range
    float       maxAge          = 30.0f;
    float       minLifespan     = 60.0f;      // life expectancy range
    float       maxLifespan     = 100.0f;
    // Per-skill starting value overrides (SkillID → value)
    std::vector<std::pair<SkillID, float>> startSkills;
};

// ---- Season threshold defaults (single source of truth) ----
// These constants are used by SeasonThresholds in-class initializers AND as the
// fallback defaults in WorldLoader::LoadSeasons()'s OptFloat calls.  If you
// change a value here, also update the matching TOML comment in seasons.toml
// so modders see the correct default.

static constexpr float DEFAULT_HARSH_COLD     = 0.8f;   // winter-like: schedule contraction, migration penalty, icy-blue sky tint
static constexpr float DEFAULT_MODERATE_COLD  = 0.3f;   // autumn-like: amber/orange sky tint
static constexpr float DEFAULT_COLD_SEASON    = 0.2f;   // wood becomes essential heating fuel
static constexpr float DEFAULT_MILD_COLD      = 0.05f;  // spring-like: slight green sky tint
static constexpr float DEFAULT_HARVEST_SEASON = 1.1f;   // high-production: more frequent work shanties
static constexpr float DEFAULT_LOW_PRODUCTION = 0.5f;   // scarce-output: reduced yields

// Valid range for season threshold values loaded from TOML.  Both heat-drain
// and production-mod thresholds must lie within [MIN, MAX].
static constexpr float SEASON_THRESHOLD_MIN = 0.0f;
static constexpr float SEASON_THRESHOLD_MAX = 2.0f;

// ---- Season thresholds (loaded from seasons.toml, with compile-time defaults) ----

struct SeasonThresholds {
    // Heat-drain thresholds (compared against SeasonDef::heatDrainMod)
    float harshCold    = DEFAULT_HARSH_COLD;
    float moderateCold = DEFAULT_MODERATE_COLD;
    float coldSeason   = DEFAULT_COLD_SEASON;
    float mildCold     = DEFAULT_MILD_COLD;

    // Production-mod thresholds (compared against SeasonDef::productionMod)
    float harvestSeason = DEFAULT_HARVEST_SEASON;
    float lowProduction = DEFAULT_LOW_PRODUCTION;
};

// ---- World settings (map, timing, economy) ----

struct WorldSettings {
    std::string worldName = "Unnamed World";
    float mapWidth        = 2400.0f;
    float mapHeight       = 720.0f;
    float gameMinsPerRealSec = 1.0f;  // game-time speed
    int   startPopulation = 25;       // per settlement
    int   numSettlements  = 3;
    float startTreasury   = 200.0f;
    int   defaultPopCap   = 35;
    float roadBuildCost   = 400.0f;
    float roadRepairCost  = 30.0f;
    float facilityBuildCost = 200.0f;
    float cartCost        = 300.0f;
    float settlementFoundCost = 1500.0f;
    float npcStartGold    = 50.0f;
};

// ---- The main schema ----

struct WorldSchema {
    WorldSettings settings;
    SeasonThresholds seasonThresholds;

    // Indexed definition arrays — ID == index into the vector
    std::vector<NeedDef>          needs;
    std::vector<ResourceDef>      resources;
    std::vector<SkillDef>         skills;
    std::vector<ProfessionDef>    professions;
    std::vector<SeasonDef>        seasons;
    std::vector<EventDef>         events;
    std::vector<GoalDef>          goals;
    std::vector<FacilityDef>      facilities;
    std::vector<AgentTemplateDef> agentTemplates;

    // String-to-ID lookup maps (built at load time)
    std::unordered_map<std::string, NeedID>       needsByName;
    std::unordered_map<std::string, ResourceID>   resourcesByName;
    std::unordered_map<std::string, SkillID>      skillsByName;
    std::unordered_map<std::string, ProfessionID> professionsByName;
    std::unordered_map<std::string, SeasonID>     seasonsByName;
    std::unordered_map<std::string, EventID>      eventsByName;
    std::unordered_map<std::string, GoalTypeID>   goalsByName;
    std::unordered_map<std::string, int>          facilitiesByName;
    std::unordered_map<std::string, int>          agentTemplatesByName;

    // Resource → Profession reverse lookup (built by BuildMaps from ProfessionDef::producesResource)
    // Flat vector indexed by ResourceID; value is ProfessionID (INVALID_ID if none).
    std::vector<ProfessionID> resourceToProfession;

    // Resource → Skill reverse lookup (built by InitDerivedData after cross-refs resolved)
    // Flat vector indexed by ResourceID; value is SkillID (INVALID_ID if none).
    std::vector<int> resourceToSkill;

    // Profession → Skill reverse lookup (built by InitDerivedData after cross-refs resolved)
    // Flat vector indexed by ProfessionID; value is SkillID (INVALID_ID if none).
    std::vector<SkillID> professionToSkill;

    // Cached special profession IDs (populated by BuildMaps; INVALID_ID if absent)
    ProfessionID idleProfessionId   = INVALID_ID;
    ProfessionID haulerProfessionId = INVALID_ID;

    // Ordering guard: set true by ResolveCrossRefs(), checked by InitDerivedData().
    // Violation aborts the process.
    bool crossRefsResolved = false;

    // ---- Lookup helpers (safe, return INVALID_ID on miss) ----

    NeedID       FindNeed(const std::string& name) const       { auto it = needsByName.find(name);       return it != needsByName.end()       ? it->second : INVALID_ID; }
    ResourceID   FindResource(const std::string& name) const   { auto it = resourcesByName.find(name);   return it != resourcesByName.end()   ? it->second : INVALID_ID; }
    SkillID      FindSkill(const std::string& name) const      { auto it = skillsByName.find(name);      return it != skillsByName.end()      ? it->second : INVALID_ID; }
    ProfessionID FindProfession(const std::string& name) const { auto it = professionsByName.find(name); return it != professionsByName.end() ? it->second : INVALID_ID; }
    SeasonID     FindSeason(const std::string& name) const     { auto it = seasonsByName.find(name);     return it != seasonsByName.end()     ? it->second : INVALID_ID; }
    EventID      FindEvent(const std::string& name) const      { auto it = eventsByName.find(name);      return it != eventsByName.end()      ? it->second : INVALID_ID; }
    GoalTypeID   FindGoal(const std::string& name) const       { auto it = goalsByName.find(name);       return it != goalsByName.end()       ? it->second : INVALID_ID; }

    // Map a resource ID to the profession that produces it (INVALID_ID if none).
    ProfessionID ProfessionForResource(ResourceID res) const {
        assert(res >= 0 && res < (int)resourceToProfession.size()
               && "ProfessionForResource: ResourceID out of range");
        if (res >= 0 && res < (int)resourceToProfession.size())
            return resourceToProfession[res];
        return INVALID_ID;
    }

    // Map a profession ID to its primary skill (INVALID_ID if none).
    SkillID SkillForProfession(ProfessionID prof) const {
        assert(prof >= 0 && prof < (int)professionToSkill.size()
               && "SkillForProfession: ProfessionID out of range");
        if (prof >= 0 && prof < (int)professionToSkill.size())
            return professionToSkill[prof];
        return INVALID_ID;
    }

    // Get the display name for a profession ID (empty string if out of range).
    const char* ProfessionLabel(ProfessionID id) const {
        if (id >= 0 && id < (int)professions.size())
            return professions[id].displayName.c_str();
        return "Idle";
    }

    // Find the "Idle" profession ID (O(1) via cached field set by BuildMaps).
    ProfessionID FindIdleProfession() const { return idleProfessionId; }

    // Find the "Hauler" profession ID (O(1) via cached field set by BuildMaps).
    ProfessionID FindHaulerProfession() const { return haulerProfessionId; }

    // ---- Skill rate helpers (safe fallback on out-of-range) ----

    // Returns the growth-rate multiplier for the given skill.
    // Out-of-range IDs fall back to 1.0f, matching SkillDef::growthRate's default.
    float SkillGrowthRate(SkillID id) const {
        if (id >= 0 && id < (int)skills.size())
            return skills[id].growthRate;
        return 1.0f;  // SkillDef::growthRate default
    }

    // Returns the per-game-day decay rate for the given skill.
    // Out-of-range IDs fall back to 0.0005f, which comes from the legacy
    // SKILL_RUST constant -- NOT from SkillDef::decayRate (which defaults
    // to 0.0f).  This is intentional: out-of-range skills still decay
    // slightly rather than being immortal.
    float SkillDecayRate(SkillID id) const {
        if (id >= 0 && id < (int)skills.size())
            return skills[id].decayRate;
        return 0.0005f;  // SKILL_RUST legacy constant, not SkillDef default
    }

    // Check if a profession produces a resource (i.e., is not Idle/Hauler).
    bool ProfessionProduces(ProfessionID id) const {
        if (id >= 0 && id < (int)professions.size())
            return professions[id].producesResource != INVALID_ID;
        return false;
    }

    // ---- Build lookup maps from definition vectors ----
    void BuildMaps() {
        needsByName.clear();
        for (auto& d : needs)        { d.id = (NeedID)(&d - needs.data());               needsByName[d.name] = d.id; }
        resourcesByName.clear();
        for (auto& d : resources)    { d.id = (ResourceID)(&d - resources.data());        resourcesByName[d.name] = d.id; }
        skillsByName.clear();
        for (auto& d : skills)       { d.id = (SkillID)(&d - skills.data());              skillsByName[d.name] = d.id; }
        professionsByName.clear();
        resourceToProfession.assign(resources.size(), INVALID_ID);
        idleProfessionId = INVALID_ID;
        haulerProfessionId = INVALID_ID;
        for (auto& d : professions)  {
            d.id = (ProfessionID)(&d - professions.data());
            professionsByName[d.name] = d.id;
            if (d.producesResource >= 0 && d.producesResource < (int)resources.size())
                resourceToProfession[d.producesResource] = d.id;
            if (d.isIdle)   idleProfessionId   = d.id;
            if (d.isHauler) haulerProfessionId = d.id;
        }
        seasonsByName.clear();
        for (auto& d : seasons)      { d.id = (SeasonID)(&d - seasons.data());            seasonsByName[d.name] = d.id; }
        eventsByName.clear();
        for (auto& d : events)       { d.id = (EventID)(&d - events.data());              eventsByName[d.name] = d.id; }
        goalsByName.clear();
        for (auto& d : goals)        { d.id = (GoalTypeID)(&d - goals.data());            goalsByName[d.name] = d.id; }
        facilitiesByName.clear();
        for (auto& d : facilities)   { d.id = (int)(&d - facilities.data());              facilitiesByName[d.name] = d.id; }
        agentTemplatesByName.clear();
        for (auto& d : agentTemplates) { d.id = (int)(&d - agentTemplates.data());        agentTemplatesByName[d.name] = d.id; }
    }

    /// Build the resourceToSkill reverse lookup from SkillDef::forResource.
    ///
    /// Precondition: ResolveCrossRefs() must have been called first (sets
    /// crossRefsResolved = true).  Violation aborts the process.
    ///
    /// Prefer calling InitDerivedData() instead — it bundles this method
    /// and BuildProfessionToSkillMap() in the correct order.
    void BuildResourceToSkillMap() {
        if (!crossRefsResolved) {
            fprintf(stderr, "[WorldSchema] FATAL: %s called before ResolveCrossRefs() — aborting\n", __func__);
            std::abort();
        }
        resourceToSkill.assign(resources.size(), INVALID_ID);
        for (const auto& d : skills) {
            if (d.forResource >= 0 && d.forResource < (int)resources.size())
                resourceToSkill[d.forResource] = d.id;
        }
    }

    /// Build the professionToSkill reverse lookup from ProfessionDef::primarySkill.
    ///
    /// Precondition: ResolveCrossRefs() must have been called first (sets
    /// crossRefsResolved = true).  Violation aborts the process.
    ///
    /// Prefer calling InitDerivedData() instead — it bundles this method
    /// and BuildResourceToSkillMap() in the correct order.
    void BuildProfessionToSkillMap() {
        if (!crossRefsResolved) {
            fprintf(stderr, "[WorldSchema] FATAL: %s called before ResolveCrossRefs() — aborting\n", __func__);
            std::abort();
        }
        professionToSkill.assign(professions.size(), INVALID_ID);
        for (const auto& d : professions) {
            if (d.id >= 0 && d.id < (int)professionToSkill.size())
                professionToSkill[d.id] = d.primarySkill;
        }
    }

    /// Single entry point that builds every derived / reverse-lookup table.
    /// Must be called exactly once, after BuildMaps() and ResolveCrossRefs()
    /// have populated the definition vectors and resolved all string-to-ID
    /// cross-references (crossRefsResolved == true).
    ///
    /// Internally calls BuildResourceToSkillMap() then BuildProfessionToSkillMap()
    /// in the required order, eliminating the class of ordering bugs that
    /// callers would otherwise have to get right manually.
    ///
    /// Precondition violation (crossRefsResolved == false) is a programmer
    /// error and aborts the process — the game cannot run with empty lookup
    /// tables.
    ///
    /// Required call ordering in WorldLoader.cpp LoadWorld():
    ///   1. BuildMaps()
    ///   2. ResolveCrossRefs()
    ///   3. InitDerivedData()   <-- this function
    void InitDerivedData() {
        if (!crossRefsResolved) {
            fprintf(stderr, "[WorldSchema] FATAL: %s called before ResolveCrossRefs() — aborting\n", __func__);
            std::abort();
        }
        BuildResourceToSkillMap();
        BuildProfessionToSkillMap();
    }
};
