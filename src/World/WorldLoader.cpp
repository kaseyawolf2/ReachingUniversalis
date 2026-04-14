// WorldLoader.cpp — TOML config parser for world definitions.
// Reads worlds/<name>/*.toml and populates a WorldSchema.
// Validates cross-references and reports clear errors.
//
// Non-fatal diagnostics are collected into a std::vector<LoadWarning>
// (if the caller passes one) AND still printed to stderr so existing
// terminal-log behaviour is preserved.

#include "WorldLoader.h"

#include <toml++/toml.hpp>
#include <filesystem>
#include <sstream>
#include <cstdio>
#include <cstdarg>

namespace fs = std::filesystem;

// ---- Structured warning plumbing ----
// The warnings pointer is passed explicitly to every function that needs it.
// No mutable file-scope state.

// Push a warning to the collector AND print to stderr (preserving old behaviour).
// The stored LoadWarning::message contains only the diagnostic text; the
// "[WorldLoader] WARNING:" prefix is added for stderr output only.
static void PushWarning(std::vector<LoadWarning>* warnings,
                        LoadWarningLevel level,
                        const char* category,
                        const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 4, 5)))
#endif
    ;

static void PushWarning(std::vector<LoadWarning>* warnings,
                        LoadWarningLevel level,
                        const char* category,
                        const char* fmt, ...) {
    // First pass: measure the required buffer size.
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    // Allocate exactly the right size and format into it.
    std::string msg;
    if (needed < 0) {
        msg = "<format error>";
    } else if (needed > 0) {
        msg.resize(static_cast<size_t>(needed), '\0');
        vsnprintf(msg.data(), static_cast<size_t>(needed) + 1, fmt, args_copy);
    }
    va_end(args_copy);

    // Always print to stderr with prefix for context
    const char* prefix = (level == LoadWarningLevel::Warning)
                             ? "[WorldLoader] WARNING: "
                             : "[WorldLoader] ";
    fprintf(stderr, "%s%s", prefix, msg.c_str());

    // Collect into the vector if the caller asked for it (no prefix in stored message)
    if (warnings) {
        warnings->push_back({level, category ? category : "", std::move(msg)});
    }
}

// ---- Helpers ----

// Read a required string key, set error if missing.
static std::string ReqStr(const toml::table& tbl, const char* key,
                          const std::string& context, std::string& err) {
    if (auto v = tbl[key].value<std::string>()) return *v;
    if (err.empty()) err = context + ": missing required string field '" + key + "'";
    return "";
}

// Read an optional string key with a default.
static std::string OptStr(const toml::table& tbl, const char* key,
                          const std::string& def = "") {
    if (auto v = tbl[key].value<std::string>()) return *v;
    return def;
}

// Read an optional float with a default.
static float OptFloat(const toml::table& tbl, const char* key, float def = 0.f) {
    if (auto v = tbl[key].value<double>()) return (float)*v;
    return def;
}

// Read an optional int with a default.
static int OptInt(const toml::table& tbl, const char* key, int def = 0) {
    if (auto v = tbl[key].value<int64_t>()) return (int)*v;
    return def;
}

// Read an optional bool with a default.
static bool OptBool(const toml::table& tbl, const char* key, bool def = false) {
    if (auto v = tbl[key].value<bool>()) return *v;
    return def;
}

// Parse a TOML file, returning an empty table + error on failure.
static toml::table ParseFile(const std::string& path, std::string& err) {
    try {
        return toml::parse_file(path);
    } catch (const toml::parse_error& e) {
        std::ostringstream oss;
        oss << path << ": TOML parse error: " << e.description()
            << " (line " << e.source().begin.line << ")";
        err = oss.str();
        return {};
    }
}

// ---- Loaders for each config file ----

static bool LoadWorld(const std::string& path, WorldSchema& schema, std::string& err) {
    auto tbl = ParseFile(path, err);
    if (!err.empty()) return false;

    auto settings = tbl["settings"];
    if (!settings) { err = path + ": missing [settings] table"; return false; }
    auto& s = schema.settings;
    s.worldName         = OptStr(*settings.as_table(), "name", "Unnamed");
    s.mapWidth          = OptFloat(*settings.as_table(), "map_width", 2400.f);
    s.mapHeight         = OptFloat(*settings.as_table(), "map_height", 720.f);
    s.gameMinsPerRealSec = OptFloat(*settings.as_table(), "game_mins_per_real_sec", 1.f);
    s.startPopulation   = OptInt(*settings.as_table(), "start_population", 25);
    s.numSettlements    = OptInt(*settings.as_table(), "num_settlements", 3);
    s.startTreasury     = OptFloat(*settings.as_table(), "start_treasury", 200.f);
    s.defaultPopCap     = OptInt(*settings.as_table(), "default_pop_cap", 35);
    s.roadBuildCost     = OptFloat(*settings.as_table(), "road_build_cost", 400.f);
    s.roadRepairCost    = OptFloat(*settings.as_table(), "road_repair_cost", 30.f);
    s.facilityBuildCost = OptFloat(*settings.as_table(), "facility_build_cost", 200.f);
    s.cartCost          = OptFloat(*settings.as_table(), "cart_cost", 300.f);
    s.settlementFoundCost = OptFloat(*settings.as_table(), "settlement_found_cost", 1500.f);
    s.npcStartGold      = OptFloat(*settings.as_table(), "npc_start_gold", 50.f);
    return true;
}

static bool LoadNeeds(const std::string& path, WorldSchema& schema, std::string& err,
                      std::vector<LoadWarning>* warnings) {
    if (!fs::exists(path)) { err = path + ": file not found"; return false; }
    auto tbl = ParseFile(path, err);
    if (!err.empty()) return false;

    auto arr = tbl["needs"].as_array();
    if (!arr) { err = path + ": missing [[needs]] array"; return false; }

    for (size_t i = 0; i < arr->size(); ++i) {
        auto* item = arr->get(i)->as_table();
        if (!item) continue;
        std::string ctx = path + " needs[" + std::to_string(i) + "]";
        NeedDef def;
        def.name              = ReqStr(*item, "name", ctx, err);
        if (!err.empty()) return false;
        def.displayName       = OptStr(*item, "display_name", def.name);
        def.drainRate         = OptFloat(*item, "drain_rate", 0.001f);
        def.refillRate        = OptFloat(*item, "refill_rate", 0.005f);
        def.criticalThreshold = OptFloat(*item, "critical_threshold", 0.3f);
        def.startValue        = OptFloat(*item, "start_value", 1.0f);
        def.satisfyBehavior   = OptStr(*item, "satisfy_behavior", "consume");
        if (def.satisfyBehavior != "consume" && def.satisfyBehavior != "sleep" && def.satisfyBehavior != "warmth") {
            PushWarning(warnings, LoadWarningLevel::Warning, "needs",
                    "%s: need '%s' has unknown satisfy_behavior '%s' "
                    "(expected 'consume', 'sleep', or 'warmth')\n",
                    path.c_str(), def.name.c_str(), def.satisfyBehavior.c_str());
        }
        // satisfied_by is resolved after resources are loaded
        std::string satBy = OptStr(*item, "satisfied_by", "");
        // Store temporarily in the name field of satisfiedBy — resolved in cross-ref pass
        // We use the string and resolve to ResourceID after resources load
        def.satisfiedBy = INVALID_ID;  // will be resolved
        schema.needs.push_back(std::move(def));
    }
    return true;
}

static bool LoadResources(const std::string& path, WorldSchema& schema, std::string& err) {
    if (!fs::exists(path)) { err = path + ": file not found"; return false; }
    auto tbl = ParseFile(path, err);
    if (!err.empty()) return false;

    auto arr = tbl["resources"].as_array();
    if (!arr) { err = path + ": missing [[resources]] array"; return false; }

    for (size_t i = 0; i < arr->size(); ++i) {
        auto* item = arr->get(i)->as_table();
        if (!item) continue;
        std::string ctx = path + " resources[" + std::to_string(i) + "]";
        ResourceDef def;
        def.name        = ReqStr(*item, "name", ctx, err);
        if (!err.empty()) return false;
        def.displayName = OptStr(*item, "display_name", def.name);
        def.basePrice   = OptFloat(*item, "base_price", 1.0f);
        def.spoilRate   = OptFloat(*item, "spoil_rate", 0.0f);
        def.weight      = OptFloat(*item, "weight", 1.0f);
        schema.resources.push_back(std::move(def));
    }
    return true;
}

static bool LoadSkills(const std::string& path, WorldSchema& schema, std::string& err) {
    if (!fs::exists(path)) { err = path + ": file not found"; return false; }
    auto tbl = ParseFile(path, err);
    if (!err.empty()) return false;

    auto arr = tbl["skills"].as_array();
    if (!arr) { err = path + ": missing [[skills]] array"; return false; }

    for (size_t i = 0; i < arr->size(); ++i) {
        auto* item = arr->get(i)->as_table();
        if (!item) continue;
        std::string ctx = path + " skills[" + std::to_string(i) + "]";
        SkillDef def;
        def.name        = ReqStr(*item, "name", ctx, err);
        if (!err.empty()) return false;
        def.displayName = OptStr(*item, "display_name", def.name);
        def.startValue  = OptFloat(*item, "start_value", 0.5f);
        def.growthRate  = OptFloat(*item, "growth_rate", 1.0f);
        def.decayRate   = OptFloat(*item, "decay_rate", 0.0f);
        // forResource resolved in cross-ref pass
        def.forResource = INVALID_ID;
        schema.skills.push_back(std::move(def));
    }
    return true;
}

static bool LoadProfessions(const std::string& path, WorldSchema& schema, std::string& err) {
    if (!fs::exists(path)) { err = path + ": file not found"; return false; }
    auto tbl = ParseFile(path, err);
    if (!err.empty()) return false;

    auto arr = tbl["professions"].as_array();
    if (!arr) { err = path + ": missing [[professions]] array"; return false; }

    for (size_t i = 0; i < arr->size(); ++i) {
        auto* item = arr->get(i)->as_table();
        if (!item) continue;
        std::string ctx = path + " professions[" + std::to_string(i) + "]";
        ProfessionDef def;
        def.name        = ReqStr(*item, "name", ctx, err);
        if (!err.empty()) return false;
        def.displayName = OptStr(*item, "display_name", def.name);
        def.isIdle      = OptBool(*item, "is_idle", false);
        def.isHauler    = OptBool(*item, "is_hauler", false);
        // producesResource and primarySkill resolved in cross-ref pass
        def.producesResource = INVALID_ID;
        def.primarySkill     = INVALID_ID;
        schema.professions.push_back(std::move(def));
    }
    return true;
}

static bool LoadSeasons(const std::string& path, WorldSchema& schema, std::string& err,
                        std::vector<LoadWarning>* warnings) {
    if (!fs::exists(path)) { err = path + ": file not found"; return false; }
    auto tbl = ParseFile(path, err);
    if (!err.empty()) return false;

    // ---- Global season thresholds (top-level keys) ----
    // Defaults come from the SeasonThresholds struct's in-class initializers,
    // which reference the DEFAULT_* constants in WorldSchema.h.  If you need
    // to change a default, update the constant there (single source of truth).
    {
        SeasonThresholds& st = schema.seasonThresholds;
        st.harshCold     = OptFloat(tbl, "harsh_cold",     st.harshCold);
        st.moderateCold  = OptFloat(tbl, "moderate_cold",  st.moderateCold);
        st.coldSeason    = OptFloat(tbl, "cold_season",    st.coldSeason);
        st.mildCold      = OptFloat(tbl, "mild_cold",      st.mildCold);
        st.harvestSeason = OptFloat(tbl, "harvest_season", st.harvestSeason);
        st.lowProduction = OptFloat(tbl, "low_production", st.lowProduction);

        // --- Validate season threshold values ---
        auto rangeCheck = [&](float val, const char* name) {
            if (val < 0.0f || val > 2.0f)
                PushWarning(warnings, LoadWarningLevel::Warning, "seasons",
                        "%s: '%s' = %.3f is out of range [0.0, 2.0]\n",
                        path.c_str(), name, val);
        };
        rangeCheck(st.harshCold,     "harsh_cold");
        rangeCheck(st.moderateCold,  "moderate_cold");
        rangeCheck(st.coldSeason,    "cold_season");
        rangeCheck(st.mildCold,      "mild_cold");
        rangeCheck(st.harvestSeason, "harvest_season");
        rangeCheck(st.lowProduction, "low_production");

        // Pairwise ordering: mildCold < coldSeason < moderateCold < harshCold
        if (!(st.mildCold < st.coldSeason))
            PushWarning(warnings, LoadWarningLevel::Warning, "seasons",
                    "%s: mild_cold (%.3f) should be less than cold_season (%.3f)\n",
                    path.c_str(), st.mildCold, st.coldSeason);
        if (!(st.coldSeason < st.moderateCold))
            PushWarning(warnings, LoadWarningLevel::Warning, "seasons",
                    "%s: cold_season (%.3f) should be less than moderate_cold (%.3f)\n",
                    path.c_str(), st.coldSeason, st.moderateCold);
        if (!(st.moderateCold < st.harshCold))
            PushWarning(warnings, LoadWarningLevel::Warning, "seasons",
                    "%s: moderate_cold (%.3f) should be less than harsh_cold (%.3f)\n",
                    path.c_str(), st.moderateCold, st.harshCold);

        // Top-level ordering invariant: mildCold < moderateCold < harshCold.
        // Inverted ordering causes nonsensical sky tints and schedule behavior.
        if (!(st.mildCold < st.moderateCold))
            PushWarning(warnings, LoadWarningLevel::Warning, "seasons",
                    "%s: cold threshold ordering violated: "
                    "mild_cold (%.3f) must be less than moderate_cold (%.3f)\n",
                    path.c_str(), st.mildCold, st.moderateCold);

        // Production threshold ordering: lowProduction < harvestSeason.
        // Inverted ordering causes nonsensical schedule behavior.
        if (!(st.lowProduction < st.harvestSeason))
            PushWarning(warnings, LoadWarningLevel::Warning, "seasons",
                    "%s: production threshold ordering violated: "
                    "low_production (%.3f) must be less than harvest_season (%.3f)\n",
                    path.c_str(), st.lowProduction, st.harvestSeason);
    }

    auto arr = tbl["seasons"].as_array();
    if (!arr) { err = path + ": missing [[seasons]] array"; return false; }

    for (size_t i = 0; i < arr->size(); ++i) {
        auto* item = arr->get(i)->as_table();
        if (!item) continue;
        std::string ctx = path + " seasons[" + std::to_string(i) + "]";
        SeasonDef def;
        def.name            = ReqStr(*item, "name", ctx, err);
        if (!err.empty()) return false;
        def.displayName     = OptStr(*item, "display_name", def.name);
        def.durationDays    = OptInt(*item, "duration_days", 30);
        def.productionMod   = OptFloat(*item, "production_mod", 1.0f);
        def.energyDrainMod  = OptFloat(*item, "energy_drain_mod", 1.0f);
        def.heatDrainMod    = OptFloat(*item, "heat_drain_mod", 0.0f);
        def.baseTemperature = OptFloat(*item, "base_temperature", 20.0f);
        def.tempSwing       = OptFloat(*item, "temp_swing", 8.0f);

        // Per-resource price floor multipliers: [seasons.price_floors] sub-table.
        // Each key is a resource name (e.g. "Wood"), value is a float multiplier.
        // Unspecified resources default to 1.0f.
        def.priceFloorMult.assign(schema.resources.size(), 1.0f);
        if (auto* pf = item->get("price_floors")) {
            if (auto* pfTbl = pf->as_table()) {
                for (auto& [key, val] : *pfTbl) {
                    // Look up resource name in already-loaded schema.resources
                    int resId = -1;
                    for (size_t r = 0; r < schema.resources.size(); ++r) {
                        if (schema.resources[r].name == key) { resId = (int)r; break; }
                    }
                    if (resId < 0) {
                        PushWarning(warnings, LoadWarningLevel::Warning, "seasons",
                                "%s: price_floors references unknown resource '%s'\n",
                                ctx.c_str(), std::string(key).c_str());
                        continue;
                    }
                    if (auto fv = val.value<double>()) {
                        def.priceFloorMult[resId] = (float)*fv;
                    }
                }
            }
        }
        schema.seasons.push_back(std::move(def));
    }
    return true;
}

static bool LoadEvents(const std::string& path, WorldSchema& schema, std::string& err,
                       std::vector<LoadWarning>* warnings) {
    if (!fs::exists(path)) { err = path + ": file not found"; return false; }
    auto tbl = ParseFile(path, err);
    if (!err.empty()) return false;

    auto arr = tbl["events"].as_array();
    if (!arr) { err = path + ": missing [[events]] array"; return false; }

    for (size_t i = 0; i < arr->size(); ++i) {
        auto* item = arr->get(i)->as_table();
        if (!item) continue;
        std::string ctx = path + " events[" + std::to_string(i) + "]";
        EventDef def;
        def.name          = ReqStr(*item, "name", ctx, err);
        if (!err.empty()) return false;
        def.displayName   = OptStr(*item, "display_name", def.name);
        // Resolve effect_type string to enum at load time (string is not stored on EventDef)
        std::string effectType = OptStr(*item, "effect_type", "none");
        if      (effectType == "production_modifier") def.effectEnum = EventEffectType::ProductionModifier;
        else if (effectType == "stockpile_destroy")   def.effectEnum = EventEffectType::StockpileDestroy;
        else if (effectType == "road_block")          def.effectEnum = EventEffectType::RoadBlock;
        else if (effectType == "treasury_boost")      def.effectEnum = EventEffectType::TreasuryBoost;
        else if (effectType == "spawn_npcs")          def.effectEnum = EventEffectType::SpawnNpcs;
        else if (effectType == "stockpile_add")       def.effectEnum = EventEffectType::StockpileAdd;
        else if (effectType == "convoy")              def.effectEnum = EventEffectType::Convoy;
        else if (effectType == "earthquake")          def.effectEnum = EventEffectType::Earthquake;
        else if (effectType == "fire")                def.effectEnum = EventEffectType::Fire;
        else if (effectType == "price_spike")         def.effectEnum = EventEffectType::PriceSpike;
        else if (effectType == "morale_boost")        def.effectEnum = EventEffectType::MoraleBoost;
        else if (effectType == "none")                def.effectEnum = EventEffectType::None;
        else {
            PushWarning(warnings, LoadWarningLevel::Warning, "events",
                    "%s: event '%s' has unknown effect_type '%s', defaulting to None\n",
                    path.c_str(), def.name.c_str(), effectType.c_str());
            def.effectEnum = EventEffectType::None;
        }

        def.effectValue   = OptFloat(*item, "effect_value", 1.0f);
        def.durationHours = OptFloat(*item, "duration_hours", 0.0f);
        def.chance        = OptFloat(*item, "chance", 0.01f);
        def.minPopulation = OptInt(*item, "min_population", 0);

        // Extended fields
        def.moraleImpact          = OptFloat(*item, "morale_impact", 0.0f);
        def.killFraction          = OptFloat(*item, "kill_fraction", 0.0f);
        def.targetResource        = OptStr(*item, "target_resource", "");
        def.destroyResource       = OptStr(*item, "destroy_resource", "");
        def.destroyFraction       = OptFloat(*item, "destroy_fraction", 0.0f);
        // destroy_resources = [{resource = "Food", fraction = 0.45}, ...]
        if (auto* drArr = item->get("destroy_resources")) {
            if (auto* darr = drArr->as_array()) {
                for (size_t di = 0; di < darr->size(); ++di) {
                    auto* ditem = darr->get(di)->as_table();
                    if (!ditem) continue;
                    std::string drName = OptStr(*ditem, "resource", "");
                    float drFrac = OptFloat(*ditem, "fraction", 0.0f);
                    if (!drName.empty() && drFrac > 0.f)
                        def.destroyResourceNames.emplace_back(drName, drFrac);
                }
            }
        }
        def.addResource           = OptStr(*item, "add_resource", "");
        def.addAmount             = OptFloat(*item, "add_amount", 0.0f);
        def.treasuryChange        = OptFloat(*item, "treasury_change", 0.0f);
        def.blockAllRoads         = OptBool(*item, "block_all_roads", false);
        def.roadBlockDuration     = OptFloat(*item, "road_block_duration", 0.0f);
        def.roadDamage            = OptFloat(*item, "road_damage", 0.0f);
        def.spreads               = OptBool(*item, "spreads", false);
        def.spreadInterval        = OptFloat(*item, "spread_interval", 0.0f);
        def.spreadChance          = OptFloat(*item, "spread_chance", 0.0f);
        def.spreadKillFraction    = OptFloat(*item, "spread_kill_fraction", 0.0f);
        def.facilityDestroyChance = OptFloat(*item, "facility_destroy_chance", 0.0f);
        def.seasonMinHeatDrain    = OptFloat(*item, "season_min_heat_drain", -1.0f);
        def.seasonMaxHeatDrain    = OptFloat(*item, "season_max_heat_drain", 999.0f);
        def.seasonMinTemp         = OptFloat(*item, "season_min_temp", -999.0f);
        def.seasonMinProdMod      = OptFloat(*item, "season_min_prod_mod", -1.0f);
        def.rumourType            = OptStr(*item, "rumour_type", "");
        def.rumourSeeds           = OptInt(*item, "rumour_seeds", 0);
        def.triggersSolidarity    = OptBool(*item, "triggers_solidarity", false);
        def.solidarityBoost       = OptFloat(*item, "solidarity_boost", 0.0f);
        def.triggersCelebration   = OptBool(*item, "triggers_celebration", false);
        def.priceSpikeMultiplier  = OptFloat(*item, "price_spike_multiplier", 0.0f);
        def.spawnSkilled          = OptBool(*item, "spawn_skilled", false);
        def.affectsAllSettlements = OptBool(*item, "affects_all_settlements", false);
        def.breaksDrought         = OptBool(*item, "breaks_drought", false);
        def.convoyAmount          = OptFloat(*item, "convoy_amount", 0.0f);
        def.convoyMinPrice        = OptFloat(*item, "convoy_min_price", 0.0f);

        // targetResourceId and addResourceId resolved in cross-ref pass
        def.targetResourceId = INVALID_ID;
        def.addResourceId    = INVALID_ID;

        schema.events.push_back(std::move(def));
    }
    return true;
}

static bool LoadGoals(const std::string& path, WorldSchema& schema, std::string& err,
                      std::vector<LoadWarning>* warnings) {
    if (!fs::exists(path)) { err = path + ": file not found"; return false; }
    auto tbl = ParseFile(path, err);
    if (!err.empty()) return false;

    auto arr = tbl["goals"].as_array();
    if (!arr) { err = path + ": missing [[goals]] array"; return false; }

    for (size_t i = 0; i < arr->size(); ++i) {
        auto* item = arr->get(i)->as_table();
        if (!item) continue;
        std::string ctx = path + " goals[" + std::to_string(i) + "]";
        GoalDef def;
        def.name        = ReqStr(*item, "name", ctx, err);
        if (!err.empty()) return false;
        def.displayName       = OptStr(*item, "display_name", def.name);
        def.targetValue       = OptFloat(*item, "target_value", 0.0f);
        def.offset            = OptFloat(*item, "offset", 0.0f);
        def.weight            = OptFloat(*item, "weight", 1.0f);
        def.unit              = OptStr(*item, "unit", "");
        def.completionMessage = OptStr(*item, "completion_message", "{name} completed a goal!");
        def.completionCooldown = OptFloat(*item, "completion_cooldown", 5.0f);

        // Resolve string enums to int enums at load time (no string comparisons in hot loops)
        std::string checkType    = OptStr(*item, "check_type", "none");
        std::string targetMode   = OptStr(*item, "target_mode", "fixed");
        std::string behaviourMod = OptStr(*item, "behaviour_mod", "");

        if (checkType == "balance_gte")         def.checkTypeEnum = GoalCheckType::BalanceGte;
        else if (checkType == "age_gte")        def.checkTypeEnum = GoalCheckType::AgeGte;
        else if (checkType == "has_family")     def.checkTypeEnum = GoalCheckType::HasFamily;
        else if (checkType == "has_profession") def.checkTypeEnum = GoalCheckType::HasProfession;
        else if (checkType == "none")           def.checkTypeEnum = GoalCheckType::None;
        else {
            PushWarning(warnings, LoadWarningLevel::Warning, "goals",
                    "%s: goal '%s' has unknown check_type '%s', defaulting to None\n",
                    path.c_str(), def.name.c_str(), checkType.c_str());
            def.checkTypeEnum = GoalCheckType::None;
        }

        if (targetMode == "fixed")                  def.targetModeEnum = GoalTargetMode::Fixed;
        else if (targetMode == "relative_balance")  def.targetModeEnum = GoalTargetMode::RelativeBalance;
        else if (targetMode == "relative_age")      def.targetModeEnum = GoalTargetMode::RelativeAge;
        else {
            PushWarning(warnings, LoadWarningLevel::Warning, "goals",
                    "%s: goal '%s' has unknown target_mode '%s', defaulting to Fixed\n",
                    path.c_str(), def.name.c_str(), targetMode.c_str());
            def.targetModeEnum = GoalTargetMode::Fixed;
        }

        if (behaviourMod.empty() || behaviourMod == "none")
            def.behaviourModEnum = GoalBehaviourMod::None;
        else if (behaviourMod == "hoard")      def.behaviourModEnum = GoalBehaviourMod::Hoard;
        else if (behaviourMod == "ambitious")  def.behaviourModEnum = GoalBehaviourMod::Ambitious;
        else {
            PushWarning(warnings, LoadWarningLevel::Warning, "goals",
                    "%s: goal '%s' has unknown behaviour_mod '%s', defaulting to None\n",
                    path.c_str(), def.name.c_str(), behaviourMod.c_str());
            def.behaviourModEnum = GoalBehaviourMod::None;
        }

        schema.goals.push_back(std::move(def));
    }
    return true;
}

static bool LoadFacilities(const std::string& path, WorldSchema& schema, std::string& err) {
    if (!fs::exists(path)) { err = path + ": file not found"; return false; }
    auto tbl = ParseFile(path, err);
    if (!err.empty()) return false;

    auto arr = tbl["facilities"].as_array();
    if (!arr) { err = path + ": missing [[facilities]] array"; return false; }

    for (size_t i = 0; i < arr->size(); ++i) {
        auto* item = arr->get(i)->as_table();
        if (!item) continue;
        std::string ctx = path + " facilities[" + std::to_string(i) + "]";
        FacilityDef def;
        def.name        = ReqStr(*item, "name", ctx, err);
        if (!err.empty()) return false;
        def.displayName = OptStr(*item, "display_name", def.name);
        def.baseRate    = OptFloat(*item, "base_rate", 1.0f);
        def.buildCost   = OptFloat(*item, "build_cost", 200.0f);
        // outputResource resolved in cross-ref pass
        def.outputResource = INVALID_ID;
        // inputs resolved in cross-ref pass (array of {resource, amount})
        schema.facilities.push_back(std::move(def));
    }
    return true;
}

static bool LoadAgents(const std::string& path, WorldSchema& schema, std::string& err) {
    if (!fs::exists(path)) { err = path + ": file not found"; return false; }
    auto tbl = ParseFile(path, err);
    if (!err.empty()) return false;

    auto arr = tbl["agents"].as_array();
    if (!arr) { err = path + ": missing [[agents]] array"; return false; }

    for (size_t i = 0; i < arr->size(); ++i) {
        auto* item = arr->get(i)->as_table();
        if (!item) continue;
        std::string ctx = path + " agents[" + std::to_string(i) + "]";
        AgentTemplateDef def;
        def.name              = ReqStr(*item, "name", ctx, err);
        if (!err.empty()) return false;
        def.displayName       = OptStr(*item, "display_name", def.name);
        def.startGold         = OptFloat(*item, "start_gold", 50.0f);
        def.moveSpeed         = OptFloat(*item, "move_speed", 80.0f);
        def.inventoryCapacity = OptInt(*item, "inventory_capacity", 5);
        def.minAge            = OptFloat(*item, "min_age", 18.0f);
        def.maxAge            = OptFloat(*item, "max_age", 30.0f);
        def.minLifespan       = OptFloat(*item, "min_lifespan", 60.0f);
        def.maxLifespan       = OptFloat(*item, "max_lifespan", 100.0f);
        // startProfession resolved in cross-ref pass
        def.startProfession   = INVALID_ID;
        schema.agentTemplates.push_back(std::move(def));
    }
    return true;
}

// ---- Cross-reference resolution ----
// After all files are loaded and BuildMaps() is called, resolve string
// references (e.g. "Food" → ResourceID 0) into integer IDs.

static bool ResolveCrossRefs(const std::string& worldDir,
                             WorldSchema& schema, std::string& err) {
    // Re-parse needs.toml to get satisfied_by strings
    {
        auto tbl = ParseFile(worldDir + "/needs.toml", err);
        if (!err.empty()) return false;
        auto arr = tbl["needs"].as_array();
        if (arr) {
            for (size_t i = 0; i < arr->size() && i < schema.needs.size(); ++i) {
                auto* item = arr->get(i)->as_table();
                if (!item) continue;
                std::string satBy = OptStr(*item, "satisfied_by", "");
                if (!satBy.empty()) {
                    auto id = schema.FindResource(satBy);
                    if (id == INVALID_ID) {
                        err = worldDir + "/needs.toml: need '" + schema.needs[i].name
                            + "' references unknown resource '" + satBy + "'";
                        return false;
                    }
                    schema.needs[i].satisfiedBy = id;
                }
            }
        }
    }

    // Resolve skills.for_resource
    {
        auto tbl = ParseFile(worldDir + "/skills.toml", err);
        if (!err.empty()) return false;
        auto arr = tbl["skills"].as_array();
        if (arr) {
            for (size_t i = 0; i < arr->size() && i < schema.skills.size(); ++i) {
                auto* item = arr->get(i)->as_table();
                if (!item) continue;
                std::string res = OptStr(*item, "for_resource", "");
                if (!res.empty()) {
                    auto id = schema.FindResource(res);
                    if (id == INVALID_ID) {
                        err = worldDir + "/skills.toml: skill '" + schema.skills[i].name
                            + "' references unknown resource '" + res + "'";
                        return false;
                    }
                    schema.skills[i].forResource = id;
                }
            }
        }
    }

    // Resolve professions.produces_resource and primary_skill
    {
        auto tbl = ParseFile(worldDir + "/professions.toml", err);
        if (!err.empty()) return false;
        auto arr = tbl["professions"].as_array();
        if (arr) {
            for (size_t i = 0; i < arr->size() && i < schema.professions.size(); ++i) {
                auto* item = arr->get(i)->as_table();
                if (!item) continue;
                std::string res = OptStr(*item, "produces_resource", "");
                if (!res.empty()) {
                    auto id = schema.FindResource(res);
                    if (id == INVALID_ID) {
                        err = worldDir + "/professions.toml: profession '" + schema.professions[i].name
                            + "' references unknown resource '" + res + "'";
                        return false;
                    }
                    schema.professions[i].producesResource = id;
                }
                std::string sk = OptStr(*item, "primary_skill", "");
                if (!sk.empty()) {
                    auto id = schema.FindSkill(sk);
                    if (id == INVALID_ID) {
                        err = worldDir + "/professions.toml: profession '" + schema.professions[i].name
                            + "' references unknown skill '" + sk + "'";
                        return false;
                    }
                    schema.professions[i].primarySkill = id;
                }
            }
        }
    }

    // Resolve facilities.output_resource
    {
        auto tbl = ParseFile(worldDir + "/facilities.toml", err);
        if (!err.empty()) return false;
        auto arr = tbl["facilities"].as_array();
        if (arr) {
            for (size_t i = 0; i < arr->size() && i < schema.facilities.size(); ++i) {
                auto* item = arr->get(i)->as_table();
                if (!item) continue;
                std::string res = OptStr(*item, "output_resource", "");
                if (!res.empty()) {
                    auto id = schema.FindResource(res);
                    if (id == INVALID_ID) {
                        err = worldDir + "/facilities.toml: facility '" + schema.facilities[i].name
                            + "' references unknown resource '" + res + "'";
                        return false;
                    }
                    schema.facilities[i].outputResource = id;
                }
                // Resolve input resources
                auto inputs = (*item)["inputs"].as_array();
                if (inputs) {
                    for (size_t j = 0; j < inputs->size(); ++j) {
                        auto* inp = inputs->get(j)->as_table();
                        if (!inp) continue;
                        std::string inRes = OptStr(*inp, "resource", "");
                        float amount = OptFloat(*inp, "amount", 1.0f);
                        if (!inRes.empty()) {
                            auto id = schema.FindResource(inRes);
                            if (id == INVALID_ID) {
                                err = worldDir + "/facilities.toml: facility '" + schema.facilities[i].name
                                    + "' input references unknown resource '" + inRes + "'";
                                return false;
                            }
                            schema.facilities[i].inputs.push_back({id, amount});
                        }
                    }
                }
            }
        }
    }

    // Resolve agents.start_profession
    {
        auto tbl = ParseFile(worldDir + "/agents.toml", err);
        if (!err.empty()) return false;
        auto arr = tbl["agents"].as_array();
        if (arr) {
            for (size_t i = 0; i < arr->size() && i < schema.agentTemplates.size(); ++i) {
                auto* item = arr->get(i)->as_table();
                if (!item) continue;
                std::string prof = OptStr(*item, "start_profession", "");
                if (!prof.empty()) {
                    auto id = schema.FindProfession(prof);
                    if (id == INVALID_ID) {
                        err = worldDir + "/agents.toml: agent '" + schema.agentTemplates[i].name
                            + "' references unknown profession '" + prof + "'";
                        return false;
                    }
                    schema.agentTemplates[i].startProfession = id;
                }
            }
        }
    }

    // Resolve goals.target_profession for has_profession checks
    {
        auto tbl = ParseFile(worldDir + "/goals.toml", err);
        if (!err.empty()) return false;
        auto arr = tbl["goals"].as_array();
        if (arr) {
            for (size_t i = 0; i < arr->size() && i < schema.goals.size(); ++i) {
                auto* item = arr->get(i)->as_table();
                if (!item) continue;
                // For has_profession goals, resolve optional target_profession string
                if (schema.goals[i].checkTypeEnum == GoalCheckType::HasProfession) {
                    std::string prof = OptStr(*item, "target_profession", "");
                    if (!prof.empty()) {
                        auto id = schema.FindProfession(prof);
                        if (id == INVALID_ID) {
                            err = worldDir + "/goals.toml: goal '" + schema.goals[i].name
                                + "' references unknown profession '" + prof + "'";
                            return false;
                        }
                        schema.goals[i].targetProfessionId = id;
                    } else {
                        // Default: check for hauler profession if no explicit target
                        schema.goals[i].targetProfessionId = schema.haulerProfessionId;
                    }
                }
            }
        }
    }

    // Resolve events.target_resource and events.add_resource
    for (auto& ev : schema.events) {
        if (!ev.targetResource.empty()) {
            auto id = schema.FindResource(ev.targetResource);
            if (id == INVALID_ID) {
                err = worldDir + "/events.toml: event '" + ev.name
                    + "' references unknown target_resource '" + ev.targetResource + "'";
                return false;
            }
            ev.targetResourceId = id;
        }
        if (!ev.addResource.empty()) {
            auto id = schema.FindResource(ev.addResource);
            if (id == INVALID_ID) {
                err = worldDir + "/events.toml: event '" + ev.name
                    + "' references unknown add_resource '" + ev.addResource + "'";
                return false;
            }
            ev.addResourceId = id;
        }
        if (!ev.destroyResource.empty()) {
            auto id = schema.FindResource(ev.destroyResource);
            if (id == INVALID_ID) {
                err = worldDir + "/events.toml: event '" + ev.name
                    + "' references unknown destroy_resource '" + ev.destroyResource + "'";
                return false;
            }
            ev.destroyResourceId = id;
        }
        for (const auto& [resName, frac] : ev.destroyResourceNames) {
            auto id = schema.FindResource(resName);
            if (id == INVALID_ID) {
                err = worldDir + "/events.toml: event '" + ev.name
                    + "' references unknown destroy_resources resource '" + resName + "'";
                return false;
            }
            ev.destroyResources.emplace_back(id, frac);
        }
    }

    schema.crossRefsResolved = true;
    return true;
}

// ---- Main entry point ----

bool WorldLoader::Load(const std::string& worldDir,
                       WorldSchema& schema,
                       std::string& errorMsg,
                       std::vector<LoadWarning>* warnings) {
    if (warnings) warnings->clear();

    schema = WorldSchema{};  // reset

    if (!fs::is_directory(worldDir)) {
        errorMsg = "World directory not found: " + worldDir;
        return false;
    }

    // Load each config file.  Order matters: resources before needs/skills/etc.
    // so cross-references can be resolved.
    std::string dir = worldDir;

    // world.toml (settings)
    if (fs::exists(dir + "/world.toml")) {
        if (!LoadWorld(dir + "/world.toml", schema, errorMsg)) return false;
    }

    // resources first (other things reference them)
    if (fs::exists(dir + "/resources.toml")) {
        if (!LoadResources(dir + "/resources.toml", schema, errorMsg)) return false;
    }

    if (fs::exists(dir + "/needs.toml")) {
        if (!LoadNeeds(dir + "/needs.toml", schema, errorMsg, warnings)) return false;
    }

    if (fs::exists(dir + "/skills.toml")) {
        if (!LoadSkills(dir + "/skills.toml", schema, errorMsg)) return false;
    }

    if (fs::exists(dir + "/professions.toml")) {
        if (!LoadProfessions(dir + "/professions.toml", schema, errorMsg)) return false;
    }

    if (fs::exists(dir + "/seasons.toml")) {
        if (!LoadSeasons(dir + "/seasons.toml", schema, errorMsg, warnings)) return false;
    }

    if (fs::exists(dir + "/events.toml")) {
        if (!LoadEvents(dir + "/events.toml", schema, errorMsg, warnings)) return false;
    }

    if (fs::exists(dir + "/goals.toml")) {
        if (!LoadGoals(dir + "/goals.toml", schema, errorMsg, warnings)) return false;
    }

    if (fs::exists(dir + "/facilities.toml")) {
        if (!LoadFacilities(dir + "/facilities.toml", schema, errorMsg)) return false;
    }

    if (fs::exists(dir + "/agents.toml")) {
        if (!LoadAgents(dir + "/agents.toml", schema, errorMsg)) return false;
    }

    // Build string→ID lookup maps
    schema.BuildMaps();

    // Resolve cross-references (string names → integer IDs)
    if (!ResolveCrossRefs(dir, schema, errorMsg)) return false;

    // Build cached lookup maps that depend on resolved cross-references
    schema.BuildResourceToSkillMap();
    schema.BuildProfessionToSkillMap();

    // ---- Validation passes ----

    // 1. Check total event weight: if all chance values sum to zero,
    //    weighted selection in RandomEventSystem produces UB.
    if (!schema.events.empty()) {
        float totalChance = 0.f;
        for (const auto& ev : schema.events)
            totalChance += ev.chance;
        if (totalChance <= 0.f) {
            PushWarning(warnings, LoadWarningLevel::Warning, "events",
                    "%s/events.toml: total event chance is zero "
                    "(sum of all 'chance' fields = 0); weighted selection will be undefined\n",
                    worldDir.c_str());
        }
    }

    // 2. Validate required fields for specific event effect types.
    for (const auto& ev : schema.events) {
        switch (ev.effectEnum) {
        case EventEffectType::ProductionModifier:
            if (ev.durationHours <= 0.f)
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': production_modifier effect has "
                        "duration_hours <= 0 (will be treated as instant, no modifier applied)\n",
                        worldDir.c_str(), ev.name.c_str());
            break;
        case EventEffectType::StockpileDestroy:
            if (ev.targetResource.empty() && ev.destroyResource.empty() && ev.destroyResourceNames.empty())
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': stockpile_destroy effect has no "
                        "target_resource, destroy_resource, or destroy_resources specified\n",
                        worldDir.c_str(), ev.name.c_str());
            break;
        case EventEffectType::StockpileAdd:
            if (ev.addResource.empty())
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': stockpile_add effect has no "
                        "add_resource specified\n", worldDir.c_str(), ev.name.c_str());
            if (ev.addAmount <= 0.f)
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': stockpile_add effect has "
                        "add_amount <= 0\n", worldDir.c_str(), ev.name.c_str());
            break;
        case EventEffectType::TreasuryBoost:
            if (ev.treasuryChange == 0.f)
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': treasury_boost effect has "
                        "treasury_change = 0 (no gold will be added)\n", worldDir.c_str(), ev.name.c_str());
            break;
        case EventEffectType::Convoy:
            if (ev.convoyAmount <= 0.f)
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': convoy effect has "
                        "convoy_amount <= 0\n", worldDir.c_str(), ev.name.c_str());
            break;
        case EventEffectType::PriceSpike:
            if (ev.priceSpikeMultiplier <= 0.f)
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': price_spike effect has "
                        "price_spike_multiplier <= 0\n", worldDir.c_str(), ev.name.c_str());
            break;
        case EventEffectType::Fire:
            if (ev.destroyResourceNames.empty() && ev.destroyResource.empty())
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': fire effect has no "
                        "destroy_resources or destroy_resource specified\n", worldDir.c_str(), ev.name.c_str());
            break;
        case EventEffectType::SpawnNpcs:
            if (ev.effectValue < 1.f && !ev.spawnSkilled)
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': SpawnNpcs event has "
                        "effectValue < 1 (truncates to 0), which causes undefined behavior in spawn count distribution\n",
                        worldDir.c_str(), ev.name.c_str());
            break;
        case EventEffectType::RoadBlock:
            if (ev.roadBlockDuration <= 0.f)
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': road_block effect has "
                        "road_block_duration <= 0 (will fall back to hardcoded default)\n",
                        worldDir.c_str(), ev.name.c_str());
            break;
        case EventEffectType::Earthquake:
            if (ev.roadBlockDuration <= 0.f)
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': earthquake effect has "
                        "road_block_duration <= 0 (will fall back to hardcoded default)\n",
                        worldDir.c_str(), ev.name.c_str());
            if (ev.facilityDestroyChance <= 0.f)
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': earthquake effect has "
                        "facility_destroy_chance <= 0 (will fall back to hardcoded default)\n",
                        worldDir.c_str(), ev.name.c_str());
            break;
        case EventEffectType::MoraleBoost:
            if (ev.moraleImpact == 0.f && ev.treasuryChange == 0.f)
                PushWarning(warnings, LoadWarningLevel::Warning, "events",
                        "%s/events.toml: event '%s': morale_boost effect has "
                        "morale_impact = 0 and treasury_change = 0 (event will have no effect)\n",
                        worldDir.c_str(), ev.name.c_str());
            break;
        default:
            break;
        }

        // Warn if chance is negative (always invalid)
        if (ev.chance < 0.f)
            PushWarning(warnings, LoadWarningLevel::Warning, "events",
                    "%s/events.toml: event '%s': has negative chance value %f\n",
                    worldDir.c_str(), ev.name.c_str(), ev.chance);
    }

    // 3. Validate required fields for specific goal check types.
    for (const auto& goal : schema.goals) {
        switch (goal.checkTypeEnum) {
        case GoalCheckType::BalanceGte:
            if (goal.targetValue <= 0.f && goal.targetModeEnum == GoalTargetMode::Fixed)
                PushWarning(warnings, LoadWarningLevel::Warning, "goals",
                        "%s/goals.toml: goal '%s': balance_gte check has "
                        "target_value <= 0 with fixed mode (goal is trivially complete)\n",
                        worldDir.c_str(), goal.name.c_str());
            break;
        case GoalCheckType::AgeGte:
            if (goal.targetValue <= 0.f && goal.targetModeEnum == GoalTargetMode::Fixed)
                PushWarning(warnings, LoadWarningLevel::Warning, "goals",
                        "%s/goals.toml: goal '%s': age_gte check has "
                        "target_value <= 0 with fixed mode (goal is trivially complete)\n",
                        worldDir.c_str(), goal.name.c_str());
            break;
        case GoalCheckType::HasProfession:
            if (goal.targetProfessionId == INVALID_ID)
                PushWarning(warnings, LoadWarningLevel::Warning, "goals",
                        "%s/goals.toml: goal '%s': has_profession check has no "
                        "target_profession resolved (will default to hauler if available)\n",
                        worldDir.c_str(), goal.name.c_str());
            break;
        default:
            break;
        }

        // Warn if weight is zero or negative (will never be selected)
        if (goal.weight <= 0.f)
            PushWarning(warnings, LoadWarningLevel::Warning, "goals",
                    "%s/goals.toml: goal '%s': has weight <= 0 "
                    "(goal will never be assigned to NPCs)\n", worldDir.c_str(), goal.name.c_str());
    }

    // 4. Check total goal weight: if all weights sum to zero, goal assignment may malfunction.
    if (!schema.goals.empty()) {
        float totalGoalWeight = 0.f;
        for (const auto& g : schema.goals)
            totalGoalWeight += g.weight;
        if (totalGoalWeight <= 0.f) {
            PushWarning(warnings, LoadWarningLevel::Warning, "goals",
                    "%s/goals.toml: total goal weight is zero "
                    "(sum of all 'weight' fields = 0); goal assignment will not work\n",
                    worldDir.c_str());
        }
    }

    // Final validation
    if (schema.needs.empty()) {
        errorMsg = worldDir + ": no needs defined (need at least one)";
        return false;
    }
    if (schema.resources.empty()) {
        errorMsg = worldDir + ": no resources defined (need at least one)";
        return false;
    }
    if (schema.seasons.empty()) {
        errorMsg = worldDir + ": no seasons defined (need at least one)";
        return false;
    }

    PushWarning(warnings, LoadWarningLevel::Info, "summary",
            "Loaded '%s': %zu needs, %zu resources, "
            "%zu skills, %zu professions, %zu seasons, %zu events, %zu goals, "
            "%zu facilities, %zu agent templates\n",
            schema.settings.worldName.c_str(),
            schema.needs.size(), schema.resources.size(),
            schema.skills.size(), schema.professions.size(),
            schema.seasons.size(), schema.events.size(),
            schema.goals.size(), schema.facilities.size(),
            schema.agentTemplates.size());

    // ---- Summary dump ----
    // Print a summary block to stderr so load diagnostics are easy to spot
    // in a scrolling terminal log.
    if (warnings && !warnings->empty()) {
        int warnCount = 0;
        for (const auto& w : *warnings)
            if (w.level == LoadWarningLevel::Warning) ++warnCount;
        if (warnCount > 0) {
            fprintf(stderr, "[WorldLoader] === Load complete: %d warning(s) ===\n", warnCount);
        }
    }

    return true;
}
