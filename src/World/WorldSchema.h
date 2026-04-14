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

struct EventDef {
    EventID     id            = INVALID_ID;
    std::string name;                         // "Drought", "Plague", ...
    std::string displayName;
    std::string effectType;                   // "production_modifier", "stockpile_destroy", etc.
    float       effectValue   = 1.0f;         // magnitude of the effect
    float       durationHours = 0.0f;         // game-hours the effect lasts (0 = instant)
    float       chance        = 0.01f;        // probability per check (per settlement per hour)
    int         minPopulation = 0;            // minimum pop to trigger
    std::string logMessage;                   // template with {settlement}, {npc}, etc.
};

struct GoalDef {
    GoalTypeID  id          = INVALID_ID;
    std::string name;                         // "SaveGold", "ReachAge", ...
    std::string displayName;                  // "Save Gold", "Reach Age", ...
    std::string checkType;                    // "balance_gte", "age_gte", "has_relation", etc.
    float       targetValue = 0.0f;           // threshold for completion
    float       weight      = 1.0f;           // selection weight when assigning goals
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

    // ---- Lookup helpers (safe, return INVALID_ID on miss) ----

    NeedID       FindNeed(const std::string& name) const       { auto it = needsByName.find(name);       return it != needsByName.end()       ? it->second : INVALID_ID; }
    ResourceID   FindResource(const std::string& name) const   { auto it = resourcesByName.find(name);   return it != resourcesByName.end()   ? it->second : INVALID_ID; }
    SkillID      FindSkill(const std::string& name) const      { auto it = skillsByName.find(name);      return it != skillsByName.end()      ? it->second : INVALID_ID; }
    ProfessionID FindProfession(const std::string& name) const { auto it = professionsByName.find(name); return it != professionsByName.end() ? it->second : INVALID_ID; }
    SeasonID     FindSeason(const std::string& name) const     { auto it = seasonsByName.find(name);     return it != seasonsByName.end()     ? it->second : INVALID_ID; }
    EventID      FindEvent(const std::string& name) const      { auto it = eventsByName.find(name);      return it != eventsByName.end()      ? it->second : INVALID_ID; }
    GoalTypeID   FindGoal(const std::string& name) const       { auto it = goalsByName.find(name);       return it != goalsByName.end()       ? it->second : INVALID_ID; }

    // ---- Build lookup maps from definition vectors ----
    void BuildMaps() {
        needsByName.clear();
        for (auto& d : needs)        { d.id = (NeedID)(&d - needs.data());               needsByName[d.name] = d.id; }
        resourcesByName.clear();
        for (auto& d : resources)    { d.id = (ResourceID)(&d - resources.data());        resourcesByName[d.name] = d.id; }
        skillsByName.clear();
        for (auto& d : skills)       { d.id = (SkillID)(&d - skills.data());              skillsByName[d.name] = d.id; }
        professionsByName.clear();
        for (auto& d : professions)  { d.id = (ProfessionID)(&d - professions.data());    professionsByName[d.name] = d.id; }
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
};
