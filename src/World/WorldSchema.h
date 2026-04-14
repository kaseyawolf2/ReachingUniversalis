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

// Resolved enum for EventDef::effectType (no string comparisons in hot loops)
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
    std::string effectType;                   // "production_modifier", "stockpile_destroy", etc.
    EventEffectType effectEnum = EventEffectType::None;  // resolved at load time
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

// Resolved enum for GoalDef::checkType (no string comparisons in hot loops)
enum class GoalCheckType { None, BalanceGte, AgeGte, HasFamily, HasProfession };

// Resolved enum for GoalDef::behaviourMod
enum class GoalBehaviourMod { None, Hoard, Ambitious };

// Resolved enum for GoalDef::targetMode
enum class GoalTargetMode { Fixed, RelativeBalance, RelativeAge };

struct GoalDef {
    GoalTypeID  id          = INVALID_ID;
    std::string name;                         // "SaveGold", "ReachAge", ...
    std::string displayName;                  // "Save Gold", "Reach Age", ...
    std::string checkType;                    // original string; prefer checkTypeEnum in systems
    GoalCheckType checkTypeEnum = GoalCheckType::None;  // resolved at load time
    float       targetValue = 0.0f;           // threshold for completion (or base target)
    std::string targetMode  = "fixed";        // original string; prefer targetModeEnum in systems
    GoalTargetMode targetModeEnum = GoalTargetMode::Fixed;  // resolved at load time
    float       offset      = 0.0f;           // added to current value for relative targets
    float       weight      = 1.0f;           // selection weight when assigning goals
    std::string unit;                         // display unit suffix ("g", "d", "")
    std::string completionMessage;            // "{name} reached their savings goal!" — {name} is replaced at runtime
    std::string behaviourMod;                 // original string; prefer behaviourModEnum in systems
    GoalBehaviourMod behaviourModEnum = GoalBehaviourMod::None;  // resolved at load time
    ProfessionID targetProfessionId = INVALID_ID;  // for has_profession: which profession to check
    float       completionCooldown = 5.0f;        // game-minutes before this goal can re-complete (prevents spam)
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

// ---- Season thresholds (loaded from seasons.toml, with compile-time defaults) ----

struct SeasonThresholds {
    // Heat-drain thresholds (compared against SeasonDef::heatDrainMod)
    float harshCold    = 0.8f;   // winter-like: schedule contraction, migration penalty, icy-blue sky tint
    float moderateCold = 0.3f;   // autumn-like: amber/orange sky tint
    float coldSeason   = 0.2f;   // wood becomes essential heating fuel
    float mildCold     = 0.05f;  // spring-like: slight green sky tint

    // Production-mod thresholds (compared against SeasonDef::productionMod)
    float harvestSeason = 1.1f;  // high-production: more frequent work shanties
    float lowProduction = 0.5f;  // scarce-output: food price floor doubles
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
    std::unordered_map<ResourceID, ProfessionID> resourceToProfession;

    // Resource → Skill reverse lookup (built by BuildResourceToSkillMap after cross-refs resolved)
    // Flat vector indexed by ResourceID; value is SkillID (INVALID_ID if none).
    std::vector<int> resourceToSkill;

    // Cached special profession IDs (populated by BuildMaps; INVALID_ID if absent)
    ProfessionID idleProfessionId   = INVALID_ID;
    ProfessionID haulerProfessionId = INVALID_ID;

    // Ordering guard: set true by ResolveCrossRefs(), checked by BuildResourceToSkillMap().
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
        auto it = resourceToProfession.find(res);
        return it != resourceToProfession.end() ? it->second : INVALID_ID;
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
        resourceToProfession.clear();
        idleProfessionId = INVALID_ID;
        haulerProfessionId = INVALID_ID;
        for (auto& d : professions)  {
            d.id = (ProfessionID)(&d - professions.data());
            professionsByName[d.name] = d.id;
            if (d.producesResource != INVALID_ID)
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

    // Build the resourceToSkill reverse lookup from SkillDef::forResource.
    // Must be called AFTER ResolveCrossRefs populates forResource fields.
    void BuildResourceToSkillMap() {
        assert(crossRefsResolved && "BuildResourceToSkillMap() must be called after ResolveCrossRefs()");
        resourceToSkill.assign(resources.size(), INVALID_ID);
        for (const auto& d : skills) {
            if (d.forResource >= 0 && d.forResource < (int)resources.size())
                resourceToSkill[d.forResource] = d.id;
        }
    }
};
