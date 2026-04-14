// WorldLoader.cpp — TOML config parser for world definitions.
// Reads worlds/<name>/*.toml and populates a WorldSchema.
// Validates cross-references and reports clear errors.

#include "WorldLoader.h"

#include <toml++/toml.hpp>
#include <filesystem>
#include <sstream>
#include <cstdio>

namespace fs = std::filesystem;

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

static bool LoadNeeds(const std::string& path, WorldSchema& schema, std::string& err) {
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

static bool LoadSeasons(const std::string& path, WorldSchema& schema, std::string& err) {
    if (!fs::exists(path)) { err = path + ": file not found"; return false; }
    auto tbl = ParseFile(path, err);
    if (!err.empty()) return false;

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
        schema.seasons.push_back(std::move(def));
    }
    return true;
}

static bool LoadEvents(const std::string& path, WorldSchema& schema, std::string& err) {
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
        def.effectType    = OptStr(*item, "effect_type", "none");
        def.effectValue   = OptFloat(*item, "effect_value", 1.0f);
        def.durationHours = OptFloat(*item, "duration_hours", 0.0f);
        def.chance        = OptFloat(*item, "chance", 0.01f);
        def.minPopulation = OptInt(*item, "min_population", 0);
        def.logMessage    = OptStr(*item, "log_message", "");
        schema.events.push_back(std::move(def));
    }
    return true;
}

static bool LoadGoals(const std::string& path, WorldSchema& schema, std::string& err) {
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
        def.checkType         = OptStr(*item, "check_type", "none");
        def.targetValue       = OptFloat(*item, "target_value", 0.0f);
        def.targetMode        = OptStr(*item, "target_mode", "fixed");
        def.offset            = OptFloat(*item, "offset", 0.0f);
        def.weight            = OptFloat(*item, "weight", 1.0f);
        def.unit              = OptStr(*item, "unit", "");
        def.completionMessage = OptStr(*item, "completion_message", "{name} completed a goal!");
        def.behaviourMod      = OptStr(*item, "behaviour_mod", "");

        // Resolve string enums to int enums at load time (no string comparisons in hot loops)
        if (def.checkType == "balance_gte")       def.checkTypeEnum = GoalCheckType::BalanceGte;
        else if (def.checkType == "age_gte")      def.checkTypeEnum = GoalCheckType::AgeGte;
        else if (def.checkType == "has_family")    def.checkTypeEnum = GoalCheckType::HasFamily;
        else if (def.checkType == "has_profession") def.checkTypeEnum = GoalCheckType::HasProfession;
        else                                       def.checkTypeEnum = GoalCheckType::None;

        if (def.targetMode == "relative_balance")      def.targetModeEnum = GoalTargetMode::RelativeBalance;
        else if (def.targetMode == "relative_age")     def.targetModeEnum = GoalTargetMode::RelativeAge;
        else                                           def.targetModeEnum = GoalTargetMode::Fixed;

        if (def.behaviourMod == "hoard")           def.behaviourModEnum = GoalBehaviourMod::Hoard;
        else if (def.behaviourMod == "ambitious")  def.behaviourModEnum = GoalBehaviourMod::Ambitious;
        else                                       def.behaviourModEnum = GoalBehaviourMod::None;

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

    return true;
}

// ---- Main entry point ----

bool WorldLoader::Load(const std::string& worldDir,
                       WorldSchema& schema,
                       std::string& errorMsg) {
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
        if (!LoadNeeds(dir + "/needs.toml", schema, errorMsg)) return false;
    }

    if (fs::exists(dir + "/skills.toml")) {
        if (!LoadSkills(dir + "/skills.toml", schema, errorMsg)) return false;
    }

    if (fs::exists(dir + "/professions.toml")) {
        if (!LoadProfessions(dir + "/professions.toml", schema, errorMsg)) return false;
    }

    if (fs::exists(dir + "/seasons.toml")) {
        if (!LoadSeasons(dir + "/seasons.toml", schema, errorMsg)) return false;
    }

    if (fs::exists(dir + "/events.toml")) {
        if (!LoadEvents(dir + "/events.toml", schema, errorMsg)) return false;
    }

    if (fs::exists(dir + "/goals.toml")) {
        if (!LoadGoals(dir + "/goals.toml", schema, errorMsg)) return false;
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

    fprintf(stderr, "[WorldLoader] Loaded '%s': %zu needs, %zu resources, "
            "%zu skills, %zu professions, %zu seasons, %zu events, %zu goals, "
            "%zu facilities, %zu agent templates\n",
            schema.settings.worldName.c_str(),
            schema.needs.size(), schema.resources.size(),
            schema.skills.size(), schema.professions.size(),
            schema.seasons.size(), schema.events.size(),
            schema.goals.size(), schema.facilities.size(),
            schema.agentTemplates.size());

    return true;
}
