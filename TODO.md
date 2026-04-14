# ReachingUniversalis — Dev Backlog

**Priority: Build a generic, data-driven, clusterable simulation engine that can run any world.**

The vision (from Outline.md): an open-world sandbox/life sim where you can set up games in
multiple worlds (Naruto, Battletech, WW2, modern day, Gate: JSDF) and play within them.
NPCs and player share the same tools. Simple graphics. Clusterable, multithreaded, P2P multiplayer.
UI is decoupled from the sim so it stays responsive even when the sim lags.

---

## Phase 0 — Foundation (current)

- [x] **WorldSchema.h** — Runtime data structure for world definitions. Generic ID-indexed definitions for needs, resources, skills, professions, facilities, seasons, events, goals, agent templates. String-to-ID lookup maps built at load time.

- [x] **CommandQueue.h** — Thread-safe command queue replacing InputSnapshot atomic flags. World-agnostic commands with variant payloads. Supports one-shot commands and continuous movement state.

- [x] **Medieval world configs** — Extract all hardcoded values from Components.h, WorldGenerator, and systems into TOML files under `worlds/medieval/`. Proves the config format works by capturing the existing game.

- [x] **WorldLoader.h/.cpp** — TOML config parser that reads `worlds/<name>/*.toml` and populates a WorldSchema. Validates cross-references (e.g., profession references a valid resource). Reports clear errors on missing/invalid config.

- [x] **Wire WorldSchema into SimThread** — SimThread loads a WorldSchema at construction. Systems receive a `const WorldSchema&` reference. No behavior changes yet — just plumbing.

## Phase 1 — Data-Driven Components

- [x] **Generic Needs component** — Replace `enum NeedType` and fixed `std::array<Need, 4>` with a vector of needs sized from WorldSchema. Systems iterate `schema.needs` instead of hardcoded indices.

- [x] **Generic Resources** — Replace `enum ResourceType` with ResourceID (int). Stockpile, Inventory, Market, ProductionFacility all key by ResourceID. Existing switch statements become schema lookups.

- [x] **Generic Skills** — Replace `struct Skills { float farming, water_drawing, woodcutting; }` with `std::vector<float>` indexed by SkillID. `ForResource()` and `Advance()` use schema mappings.

- [x] **Generic Professions** — Replace `enum ProfessionType` with ProfessionID. `ProfessionForResource()` becomes a schema lookup. Display names come from config.

- [x] **Generic Seasons** — Replace `enum Season` and hardcoded modifier functions (`SeasonProductionModifier`, `SeasonHeatDrainMult`, etc.) with schema lookups. TimeManager references season definitions by index.

- [x] **Generic Events** — Replace hardcoded event cases in RandomEventSystem with event definitions from schema. Each event specifies its effect type, value, duration, and spread behavior.

- [x] **Generic Goals** — Replace `enum GoalType` with GoalTypeID. Goal assignment and completion check become data-driven.

- [x] **Slim down DeprivationTimer** — The 40+ fields in DeprivationTimer are medieval-specific social mechanics. Factor out into optional components: SocialBehavior, BanditState, GriefState, etc. Core DeprivationTimer only tracks need-at-zero timers and migration threshold.

- [x] **Schema-driven skill growth/decay rates** — `SkillDef` already has `growthRate` and `decayRate` fields but `Skills::Advance()` and the rust logic in `AgentDecisionSystem.cpp` use hardcoded `SKILL_GROWTH = 0.001f` / `SKILL_RUST = 0.0005f`. Wire the schema values through so each skill can have its own rate.

- [x] **Cache hot-path schema lookups** — `Skills::ForResource()` in `Components.h` does a linear scan of `schema.skills` on every call; `SkillForProfession()` does an `unordered_map::find` per call. Add `resourceToSkill` and `professionToSkill` lookup maps to `WorldSchema`, populated in `BuildMaps()`, and use them in these methods.

- [x] **Named season thresholds** — `RandomEventSystem.cpp` and `ScheduleSystem.cpp` use magic float thresholds (`0.8f`, `0.3f`, `0.15f`) for season property checks (heatDrainMod, productionMod). Define named constants (e.g., `HARSH_COLD_THRESHOLD`, `FLOOD_HEAT_DRAIN_MAX`) in a shared header or as SeasonDef metadata so modders can tune them.

- [x] **Profession bitmask scalability** — All profession diversity checks use `uint32_t` bitmasks with `assert(professions.size() < 32)` in `BuildMaps()`. Replace with `std::bitset` or a variable-width bitfield to support worlds with 32+ professions.

- [x] **Reduce per-frame RenderSnapshot allocations** — `SimThread::WriteSnapshot()` rebuilds `skillNames`, `settlSkillNames`, and nested `std::map<int, SkillAccum>` every frame under the mutex. Store schema-derived names once at construction, and replace `std::map<int, SkillAccum>` with flat `std::vector<SkillAccum>` indexed by SkillID.

- [x] **Remove per-agent skillNames duplication in SettlementEntry** — `SettlementEntry::skillNames` is populated identically for every settlement from schema data. Store it once on `RenderSnapshot` (like `AgentEntry` was already fixed) and reference the shared copy in HUD rendering.

- [x] **Event effect type as enum** — `RandomEventSystem.cpp` compares `ev.effectType` strings (`"production_modifier"`, `"road_block"`, etc.) in per-event hot paths. Add an `enum class EventEffectType` resolved at load time in WorldLoader, and switch on it instead of string comparisons.

- [x] **Multi-spreading event support** — `RandomEventSystem.cpp` spreading logic finds only the first `ev.spreads == true` event. Support multiple spreadable events by storing a vector of spreading event indices and iterating all of them in the spread tick.

- [x] **Goal completion cooldown** — When `goalCount == 1` or fixed-target goals (e.g., FindFamily target=1) are re-assigned, NPCs re-complete immediately every tick, spamming celebration logs. Add a `completionCooldown` field to GoalDef (or a per-NPC cooldown timer) to prevent instant re-completion.

- [x] **Validate TOML at load time** — WorldLoader silently accepts invalid field values (e.g., `chance = 0` for all events causes UB in weighted selection, unknown `check_type` strings default silently). Add validation passes in WorldLoader that log warnings or errors for: zero total weight, unknown enum strings, missing required fields for specific effect/check types.

- [x] **Generic DeprivationTimer need timers** — `DeprivationTimer` still has hardcoded `float hungerTimer, thirstTimer, energyTimer, heatTimer`. Replace with `std::vector<float>` indexed by NeedID, sized from `schema.needs`. Systems iterate generically instead of by named field.

- [x] **Consolidate SocialBehavior fields** — `SocialBehavior` has 18 fields including unrelated concerns (visitTimer, bankruptSurvivor, homesickTimer, reconcileGlow). Split into focused sub-components or at minimum group logically: `VisitState`, `MoodState`, `InteractionCooldowns`.

- [x] **Per-skill growth rate helpers** — Factor the repeated `profSkId >= 0 && profSkId < (int)schema.skills.size() ? schema.skills[profSkId].growthRate : 1.f` ternary in `AgentDecisionSystem.cpp` (lines ~598, ~804, ~1147) and `Components.h` (~503) into a single `WorldSchema::SkillGrowthRate(SkillID)` helper that encapsulates the bounds check and fallback.

- [x] **Symmetric skill fallback defaults** — In `AgentDecisionSystem.cpp`, out-of-range skill indices fall back to `growthRate = 1.f` (normal growth) but `decayRate = 0.f` (no decay). Unify the policy: add `WorldSchema::SkillDecayRate(SkillID)` with a documented default, and add a comment explaining the asymmetry or make both fall back to their `SkillDef` defaults.

- [x] **Flat array resourceToSkill** — `WorldSchema::resourceToSkill` is `unordered_map<int,int>` but `ResourceID` is a dense integer. Replace with `std::vector<SkillID>` sized to `resources.size()` and indexed by `ResourceID`, initialized to `INVALID_ID`. Eliminates hash overhead on the hot path in `Skills::ForResource()` and `Skills::SkillIdForResource()`.

- [x] **Use `inline constexpr` for season thresholds** — Season threshold constants used `static constexpr` which creates separate copies per translation unit. Switched to `inline constexpr` (C++17) for single-definition constants. (Header later deleted when thresholds moved to TOML.)

- [x] **Season threshold config in TOML** — Moved named season threshold constants (`HARSH_COLD`, `MODERATE_COLD`, `MILD_COLD`, `COLD_SEASON`, `HARVEST_SEASON`, `LOW_PRODUCTION`) into `worlds/medieval/seasons.toml` as global threshold fields. `WorldSchema::SeasonThresholds` stores them; systems read from schema instead of compile-time constants. The standalone header was deleted.

- [x] **BuildMaps/ResolveCrossRefs ordering guard** — `WorldSchema::BuildResourceToSkillMap()` must be called after `ResolveCrossRefs()` or the map is empty. Add an `assert(crossRefsResolved)` flag to `WorldSchema` set by `ResolveCrossRefs()` and checked in `BuildResourceToSkillMap()` to catch misordering at dev time.

- [x] **DynBitset unit tests** — `DynBitset` has SBO with inline/heap modes and edge cases at the 64-bit boundary. Add a test file (`tests/DynBitsetTest.cpp`) that validates: singleBit for bits 0-63 (inline) and 64+ (heap), operator& across inline/heap combinations, intersectsAny, containsAll, operator|= mixed modes, and the n==1 edge case in operator&.

- [x] **Rename m_plagueSpreadTimer** — `RandomEventSystem::m_plagueSpreadTimer` still uses plague-specific naming after the multi-spreading generalization. Rename to `m_spreadTimers` and update all references. Also rename `SpreadEntry` comment from "Active plagues" to "Active spreading events".

- [x] **Cache FindNeed results in ConsumptionSystem** — `ConsumptionSystem::Update()` calls `schema.FindNeed("Hunger")` and `schema.FindNeed("Thirst")` (string map lookups) every tick. Cache these as member variables initialized once in the constructor or first `Update()` call.

- [x] **Validate all EventEffectType variants in WorldLoader** — The TOML validation switch covers 7 of 11 effect types. Add validation for `SpawnNpcs`, `RoadBlock`, `Earthquake`, and `MoraleBoost` — checking their required fields (e.g., `roadBlockDuration` for Earthquake, `spawnCount` for SpawnNpcs).

- [x] **DeprivationTimer Make() with migrateThreshold param** — All 9 call sites of `DeprivationTimer::Make(schema)` immediately set `migrateThreshold` afterward. Add an optional `migrateThreshold` parameter to `Make()` to eliminate the construct-then-mutate pattern.

- [x] **Shared skillNames pointer in RenderSnapshot** — `m_cachedSkillNames` is copied 4 times per frame into different snapshot fields. Replace with a `std::shared_ptr<const std::vector<std::string>>` set once at construction, eliminating per-frame string vector copies.

- [x] **EventLog-based diagnostic warnings** — `RandomEventSystem.cpp` and `WorldLoader.cpp` use `fprintf(stderr, ...)` for diagnostic warnings, which is invisible in-game. Route these through `EventLog::Push` or a dedicated diagnostic log channel so they appear in the HUD event log during gameplay.

- [x] **Dead effectType string field cleanup** — `EventDef::effectType` (std::string) is now dead weight at runtime after the `EventEffectType` enum was added. Remove it from `EventDef` in `WorldSchema.h` or mark it `#ifndef NDEBUG` for debug-only retention. Saves 32 bytes per event definition.

- [x] **Flat array resourceToProfession** — `WorldSchema::resourceToProfession` is still an `unordered_map<int,int>` but `ResourceID` is a dense integer (same pattern fixed for `resourceToSkill`). Replace with `std::vector<ProfessionID>` sized to `resources.size()` and indexed by `ResourceID`, initialized to `INVALID_ID`. Eliminates hash overhead in `ProfessionForResource()`.

- [x] **SocialBehavior sub-struct relocation** — `MoodState::skillCelebrateTimer` and `MoodState::reconcileGlow` are functionally cooldown timers gating behavior, not persistent mood flags. Move them to `InteractionCooldowns` for consistency, and update all access sites in `AgentDecisionSystem.cpp`, `ProductionSystem.cpp`, and `ScheduleSystem.cpp`.

- [x] **WorldSchema::SkillGrowthRate INVALID_ID guard** — `AgentDecisionSystem.cpp` line ~598 passes the result of `SkillForProfession()` directly into `SkillGrowthRate()` without checking for `INVALID_ID` (-1). Add an explicit guard (`if (profSkId != INVALID_ID)`) before calling `SkillGrowthRate()`, consistent with the mentor growth call site at line ~1151 which already guards.

- [x] **DynBitset copy/move semantics tests** — `DynBitset` relies on compiler-generated copy/move constructors with a `vector<uint64_t>` heap member. Add tests to `tests/DynBitsetTest.cpp` that copy a heap-mode bitset and mutate the copy (verifying original is untouched), and move a heap-mode bitset (verifying source is empty).

- [x] **Season threshold TOML validation** — `WorldLoader::LoadSeasons()` accepts any float values for season thresholds without sanity checks. Add validation that `mildCold < moderateCold < harshCold`, all values are in `[0.0, 2.0]`, and log warnings for inverted or out-of-range values.

- [x] **Flat array professionToSkill** — `WorldSchema::professionToSkill` is `unordered_map<int,int>` but `ProfessionID` is a dense integer. Replace with `std::vector<SkillID>` sized to `professions.size()` and indexed by `ProfessionID`, initialized to `INVALID_ID`. Same optimization as `resourceToSkill`.

- [x] **DynBitset promotion-path test** — Add a test to `tests/DynBitsetTest.cpp` that calls `set(200)` on a default-constructed (empty inline) `DynBitset`, exercising the `promoteIfNeeded` path from truly empty inline state to heap allocation. Verify `test(200)` returns true and `test(0)` returns false.

- [x] **Remove SeasonThresholds.h references from TODO.md** — Cleaned up stale `SeasonThresholds.h` references in two completed TODO items. The header was deleted when season thresholds moved to TOML.

- [x] **Assert-to-runtime check in BuildResourceToSkillMap** — `BuildResourceToSkillMap()` uses `assert(crossRefsResolved)` which compiles to nothing in release builds. Replace with a runtime check (`if (!crossRefsResolved) { fprintf(stderr, ...); return; }`) since this is a load-time path, not a hot path, and should catch misordering even in release.

- [x] **ConsumptionSystem schema reference** — `ConsumptionSystem` receives `const WorldSchema&` in `Update()` but doesn't store it. Add a `const WorldSchema& m_schema` member initialized in the constructor so `FindNeed()` results can be cached as member variables (prerequisite for the "Cache FindNeed results" task).

- [x] **GameState m_schema member ordering** — `GameState::m_schema` is declared between camera fields and `m_renderSystem`. Move it next to `m_simThread` since both hold references from the same constructor parameter, improving readability of the private section.

- [x] **DynBitset rename n1 test** — Rename `and_n1_both_heap_single_word` in `tests/DynBitsetTest.cpp` to `and_heap_heap_word0_only` since the test exercises the general heap loop (n=2), not the n<=1 branch. The companion `and_n1_edge_case` is the one that actually hits n<=1.

- [x] **Validate season threshold ordering in WorldLoader** — After loading season thresholds from TOML, verify the ordering invariant `mildCold < moderateCold < harshCold` and `lowProduction < harvestSeason`. Log a warning if thresholds are inverted, as this would cause nonsensical sky tints and schedule behavior.

- [x] **Schema-driven PriceSystem floor thresholds** — `PriceSystem::SeasonPriceFloor()` now takes `const SeasonThresholds&` but still uses hardcoded logic for which thresholds trigger price floors. Make the price floor rules data-driven: add a `priceFloorThreshold` field to `SeasonDef` in `WorldSchema.h` so each season can specify its own floor multiplier.

- [x] **WorldLoader OptFloat default source-of-truth** — `LoadSeasons()` uses `OptFloat(tbl, key, struct_default)` where the default comes from the struct's in-class initializer. If someone changes the struct default without updating the TOML comment, they silently diverge. Add a comment warning about this coupling, or define named constants in `WorldSchema.h` used by both the struct initializer and the TOML comment.

- [x] **Dead GoalDef string fields cleanup** — `GoalDef` still carries dead `std::string checkType`, `targetMode`, and `behaviourMod` fields alongside their resolved enums — the same pattern just removed from `EventDef`. Remove them from `GoalDef` in `WorldSchema.h` and use local variables in `WorldLoader.cpp` during parsing.

- [x] **Flat array professionToSkill** — `WorldSchema::professionToSkill` is `unordered_map<int,int>` but `ProfessionID` is a dense integer. Replace with `std::vector<SkillID>` sized to `professions.size()` and indexed by `ProfessionID`, initialized to `INVALID_ID`. Same optimization as `resourceToSkill` and `resourceToProfession`.

- [x] **Cache FindNeed in NeedDrainSystem** — `NeedDrainSystem::Update()` likely calls `schema.FindNeed()` by string every tick, same pattern fixed in `ConsumptionSystem`. Add cached member variables for frequently-used NeedIDs, initialized on first `Update()` call.

- [x] **Cache FindNeed in DeathSystem** — `DeathSystem::Update()` may call `schema.FindNeed()` by string for death cause determination. Cache the NeedIDs as member variables to eliminate per-tick string lookups.

- [x] **Spread timer comment cleanup** — `RandomEventSystem.cpp` still has `(drought/spreading event recovery)` as an awkward half-rename. Either go fully generic (`(event recovery)`) or use two concrete examples consistently.

- [x] **Duplicate static const emptyNames in HUD.cpp** — `HUD.cpp` declares three separate `static const std::vector<std::string> emptyNames;` in different methods. Consolidate into a single file-scope `static const` to eliminate duplication.

- [x] **DeprivationTimer Make() default constant** — `DeprivationTimer::Make()` default parameter `2.f * 60.f` duplicates the member initializer `migrateThreshold = 2.f * 60.f`. Define a named constant (e.g., `DEFAULT_MIGRATE_THRESHOLD`) used by both to prevent drift.

- [x] **DeprivationTimer units comment fix** — INVALID: Original "game-min" was correct (1 real sec = 1 game-min per GAME_MINS_PER_REAL_SEC=1.0). PR #66 rejected.

- [x] **RenderSnapshot immutable field comment** — `RenderSnapshot::skillNames` shared_ptr is written once at construction and never mutated, unlike every other field which is written per frame under mutex. Add a comment `// Immutable after construction; not protected by mutex` to prevent future maintainers from moving it into WriteSnapshot.

- [x] **EventLog system-name prefix** — `RandomEventSystem.cpp` diagnostic warnings use `[WARNING]` prefix, dropping the system name. Restore `[RandomEventSystem]` prefix so stderr grep can identify the source system.

- [x] **SpawnNpcs effectValue runtime clamp** — `RandomEventSystem.cpp` line ~1297 casts `effectValue` to `int` for `maxArrivals` without clamping, so values < 1 produce UB in `uniform_int_distribution`. Add `maxArrivals = std::max(1, (int)ev.effectValue)` as a runtime safety net alongside the load-time validation.

- [x] **WorldLoader stderr to structured logging** — All 26+ `fprintf(stderr, ...)` calls in `WorldLoader.cpp` output unstructured text. Define a `LoadWarning` struct or use a vector to collect warnings during loading, then dump them after load completes. Enables future UI display of load diagnostics.

- [x] **ConsumptionSystem remove redundant bool** — `ConsumptionSystem::m_needsCached` bool is redundant since `m_hungerNeedId` is initialized to `-1` (INVALID_ID). Remove the bool and use `m_hungerNeedId < 0` as the cache-miss sentinel.

- [x] **Shared skillNames null-check consistency** — `GameState.cpp` null-checks `sharedSkillNames` before use, but the three HUD methods use `emptyNames` fallback. Pick one pattern: either always null-check with early return, or always fallback to empty. Apply consistently across all 4 consumer sites.

- [x] **ProfessionForResource callers audit** — After `resourceToProfession` changed to flat vector, audit all callers of `ProfessionForResource()` to ensure none pass ResourceIDs that could exceed `resources.size()`. Add an assert in `ProfessionForResource()` for debug builds.

- [x] **SocialBehavior InteractionCooldowns doc comment** — `InteractionCooldowns` in `Components.h` now holds `skillCelebrateTimer` and `reconcileGlow` after the sub-struct relocation, but has no doc comment explaining that these are behavior-gating cooldowns (not mood state). Add a brief `///` comment above the struct listing each timer's purpose and units (game-seconds).

- [x] **SocialBehavior MoodState dead field audit** — After relocating `skillCelebrateTimer` and `reconcileGlow` out of `MoodState`, audit whether any remaining `MoodState` fields are also functionally cooldowns. Candidates: `griefLevel` decays over time and gates visit behavior. Document each field's category (persistent state vs. decaying timer) with inline comments.

- [x] **SkillGrowthRate bounds-check unit test** — Add a test to `tests/` that constructs a minimal `WorldSchema` with 2 skills, then calls `SkillGrowthRate(INVALID_ID)`, `SkillGrowthRate(-1)`, and `SkillGrowthRate(999)` to verify the fallback returns 1.0f for all out-of-range inputs. Same for `SkillDecayRate`.

- [x] **SkillForProfession INVALID_ID propagation audit** — After the INVALID_ID guard was added at line ~598 of `AgentDecisionSystem.cpp`, audit all other call sites of `SkillForProfession()` across all systems to ensure they also guard against INVALID_ID before passing to `SkillGrowthRate()` or `SkillDecayRate()`.

- [x] **DynBitset copy assignment operator test** — `tests/DynBitsetTest.cpp` tests copy/move constructors but not copy/move assignment operators. Add tests for `DynBitset a; a = b;` (copy assign) and `DynBitset a; a = std::move(b);` (move assign) for both inline and heap modes, verifying independence and source state.

- [x] **DynBitset clear() method** — `DynBitset` has no `clear()` or `reset()` method to zero all bits without reallocating. Add `void clear()` that zeros all words (inline and heap) without changing capacity, and `void reset()` that returns to default-constructed empty state. Add corresponding tests.

- [ ] **Season threshold TOML test config** — Add a `tests/test_seasons_invalid.toml` with intentionally inverted thresholds (`harshCold < mildCold`) and a test or script that runs `WorldLoader::LoadSeasons()` against it, verifying the warning is logged. Documents the validation behavior for future modders.

- [ ] **Season threshold HUD display** — `GameState.cpp` uses season thresholds for sky tint but the HUD doesn't show the current threshold regime. Add a `seasonRegime` string field to `RenderSnapshot` (e.g., "Harsh Cold", "Mild", "Harvest") set by `TimeSystem` based on current `heatDrainMod` vs thresholds, displayed in the time panel.

- [ ] **Flat professionToSkill bounds-check assert** — `WorldSchema::SkillForProfession(ProfessionID)` indexes into `professionToSkill` vector but has no bounds check. Add `assert(pid >= 0 && pid < (int)professionToSkill.size())` for debug builds, consistent with `SkillIdForResource()`.

- [ ] **BuildProfessionToSkillMap ordering test** — Add a test or assert that calling `BuildProfessionToSkillMap()` before `ResolveCrossRefs()` triggers the `assert(crossRefsResolved)` failure. Currently only tested implicitly by the load path. An explicit test documents the ordering contract.

- [ ] **GoalDef resolved enum validation** — After removing dead string fields from `GoalDef`, the resolved enums (`checkEnum`, `targetEnum`, `behaviourEnum`) are the only source of truth. Add validation in `WorldLoader.cpp` that warns if any resolved enum is the default/unknown value after parsing, indicating the TOML had an unrecognized string that was silently defaulted.

- [ ] **GoalDef field documentation** — `GoalDef` in `WorldSchema.h` has no doc comments on its remaining fields after the dead string removal. Add `///` comments for each field explaining its purpose, valid range, and which system consumes it (e.g., `completionCooldown` is consumed by `AgentDecisionSystem`).

- [ ] **DynBitset promoteIfNeeded capacity test** — Add a test to `tests/DynBitsetTest.cpp` that sets bits 0 (inline), then 65 (promoting to heap), then 200 (growing heap), verifying all three bits remain set after each promotion step. Tests that promotion preserves existing bits across multiple resizes.

- [ ] **DynBitset count() method** — `DynBitset` has `none()` but no `count()` to return the number of set bits (population count). Add `size_t count() const` using `__builtin_popcountll` for each word (inline and heap). Add tests for empty, single-bit, full-word, and multi-word cases.

- [ ] **BuildResourceToSkillMap abort test** — The assert-to-runtime change replaced `assert(crossRefsResolved)` with `fprintf + std::abort()`. Add a comment in `WorldSchema.h` documenting that `BuildResourceToSkillMap()` and `BuildProfessionToSkillMap()` will abort if called before `ResolveCrossRefs()`, and reference the call ordering in `WorldLoader.cpp`.

- [ ] **WorldSchema InitDerivedData() method** — `WorldLoader.cpp` calls `ResolveCrossRefs()`, `BuildResourceToSkillMap()`, `BuildProfessionToSkillMap()` in sequence. Bundle these into a single `WorldSchema::InitDerivedData()` method that enforces the correct call order internally, eliminating the class of ordering bugs that the runtime abort guards against.

- [ ] **NeedDrainSystem schema reference** — Same pattern as ConsumptionSystem: `NeedDrainSystem::Update()` receives `const WorldSchema&` as a parameter. Store it as `const WorldSchema& m_schema` member, remove the parameter from `Update()`, and update `SimThread.cpp` caller. Prerequisite for caching FindNeed results in NeedDrainSystem.

- [ ] **DeathSystem schema reference** — Same pattern as ConsumptionSystem: `DeathSystem::Update()` receives `const WorldSchema&` as a parameter. Store it as `const WorldSchema& m_schema` member, remove the parameter from `Update()`, and update `SimThread.cpp` caller. Prerequisite for caching FindNeed results in DeathSystem.

- [ ] **GameState camera member grouping** — After moving `m_schema` next to `m_simThread`, the camera-related fields (`m_camera`, `m_camX`, `m_camY`, `m_camZoom`) are scattered between system references. Group all camera fields together with a `// Camera state` comment block for readability.

- [ ] **GameState constructor init-list comment** — `GameState.cpp` constructor initializer list initializes `m_simThread` then `m_schema` from the same parameter but has no comment. Add a `// Both bind schema from constructor arg` comment to the init list to document the lifetime dependency.

- [ ] **DynBitset test section comment cleanup** — After renaming `and_n1_both_heap_single_word` to `and_heap_heap_word0_only`, the section comment `// operator& n==1 edge case` in `tests/DynBitsetTest.cpp` is stale. Update to `// operator& edge cases` or split the renamed test into the `heap/heap` group above.

- [ ] **DynBitset test boundary exhaustive** — Add tests to `tests/DynBitsetTest.cpp` for the exact boundary between inline and heap: `set(63)` (last inline bit), `set(64)` (first heap bit), and `set(63)` followed by `set(64)` (promoting from inline to heap while preserving bit 63). Verifies the boundary transition is correct.

- [ ] **Season threshold warning consistency** — `WorldLoader.cpp` cold threshold warnings say "should be less than" (soft) but the production threshold warning says "must be less than" (hard). Neither is enforced (no abort/return). Pick consistent wording — use "should" for all non-fatal warnings.

- [ ] **Season threshold range-check constants** — `WorldLoader.cpp` uses magic `0.0f` and `2.0f` bounds for season threshold range validation. Define named constants (e.g., `MIN_THRESHOLD = 0.0f`, `MAX_THRESHOLD = 2.0f`) at file scope or in `WorldSchema.h` so the bounds are documented and reusable.

- [ ] **PriceSystem per-resource floor validation** — `WorldLoader.cpp` parses `[seasons.price_floors]` per-resource multipliers but does not validate values. Add a `std::max(0.0f, ...)` clamp or warning for negative/zero multipliers to prevent nonsensical negative price floors.

- [ ] **PriceSystem price_floors non-table warning** — If a modder writes `price_floors = 2.0` (scalar instead of table) in `seasons.toml`, the loader silently ignores it. Add a `fprintf(stderr, "[WorldLoader] WARNING: ...")` when `price_floors` exists but is not a TOML table.

- [ ] **SeasonThresholds constants into struct scope** — The `DEFAULT_HARSH_COLD` etc. constants in `WorldSchema.h` are at file scope, polluting the namespace of every TU. Move them inside `SeasonThresholds` as `static constexpr` members (e.g., `SeasonThresholds::DEFAULT_HARSH_COLD`), and update the struct initializers and any references.

- [ ] **NeedDrainSystem FindNeed validation for ConsumptionSystem** — `ConsumptionSystem` caches `FindNeed("Hunger")` and `FindNeed("Thirst")` but has no INVALID_ID warning (same gap just fixed in NeedDrainSystem). Add `fprintf(stderr, "[ConsumptionSystem] WARNING: ...")` when cached IDs are INVALID_ID.

- [ ] **PriceSystem resource name lookup optimization comment** — `WorldLoader.cpp` does O(n) linear scan over `schema.resources` for each price_floors key. Add a comment explaining why `resourcesByName` map isn't used (BuildMaps hasn't been called yet at this point in load sequence) to prevent someone "optimizing" into uninitialized data.

- [ ] **Remove lowProduction dead threshold** — `SeasonThresholds::lowProduction` is loaded and validated for ordering but no longer consumed by any ECS system after PriceSystem was made data-driven. Audit all consumers; if truly dead, remove the field, its TOML key, its DEFAULT constant, and its ordering validation.

- [ ] **DeathSystem lazy-init to constructor-init** — `DeathSystem` caches need names on first `Update()` call via `m_needsCached` flag. The schema is fully built at construction time. Move the caching into the constructor body and remove the `m_needsCached` flag for simpler per-tick code.

- [ ] **DeathSystem drop unused m_schema** — After caching `m_needNames` in the constructor, `DeathSystem::m_schema` is never read again. Remove the member to avoid holding a dead reference and reduce class size.

- [ ] **RandomEventSystem generic modifier break** — `RandomEventSystem.cpp` line ~1333 only breaks modifiers whose name contains "Drought". Generalize to break any active negative modifier at the target settlement when `ev.breaksDrought` is true, or rename the flag to `breaksModifier` and update the TOML schema.

- [ ] **HUD emptyNames avoid global constructor** — The file-scope `static const std::vector<std::string> emptyNames;` in `HUD.cpp` triggers a global constructor for a heap-allocated empty vector. Replace with `static const std::vector<std::string>* emptyNames` pointing to a function-local static, or use an empty `std::span<const std::string>` to avoid the global init overhead.

- [ ] **NeedDrainSystem lazy-init to constructor-init** — Same pattern as DeathSystem: `NeedDrainSystem` caches NeedIDs on first `Update()` via `m_needsCached`. Move caching into the constructor and remove the flag.

- [ ] **ConsumptionSystem lazy-init to constructor-init** — Same pattern: `ConsumptionSystem` caches NeedIDs on first `Update()` via `m_needsCached`. Move caching into the constructor and remove the flag.

- [ ] **DeprivationTimer Make() callers audit** — After introducing `DEFAULT_MIGRATE_THRESHOLD`, audit all call sites of `DeprivationTimer::Make()` to verify none pass a hardcoded `2.f * 60.f` instead of using the constant.

- [ ] **DeprivationTimer time model doc comment** — The `DeprivationTimer` struct comments mix "game-min", "game-hours", and "seconds" without explaining the time model. Add a doc comment at the struct level explaining: 1 real second = 1 game-minute (per GAME_MINS_PER_REAL_SEC=1.0), so 120.0f = 2 game-hours.

- [ ] **RenderSnapshot WriteSnapshot mutex scope comment** — `RenderSnapshot` has an `// Immutable after construction` comment on `skillNames` but no comment on the mutable fields explaining they ARE protected by mutex. Add a `// Written per-frame under mutex` section comment above the mutable field block.

- [ ] **SimThread WriteSnapshot skillNames skip** — `SimThread::WriteSnapshot()` should skip writing `skillNames` since it's immutable. Verify it doesn't re-assign the shared_ptr each frame. If it does, remove the redundant write.

- [ ] **RandomEventSystem diagnostic severity levels** — `RandomEventSystem.cpp` uses `[RandomEventSystem]` prefix but drops the severity level (WARNING/ERROR/INFO). Add severity to all diagnostic messages following the pattern `[RandomEventSystem] WARNING: ...` consistent with `WorldLoader.cpp` diagnostics, enabling severity-level grep across all stderr output.

- [ ] **EventLog source-system filtering** — `EventLog::Push()` stores plain message strings with no metadata. Add an optional `sourceSystem` field to log entries so the HUD event log can filter by system (e.g., show only RandomEventSystem events, or hide diagnostic messages from gameplay events).

- [ ] **SpawnNpcs count distribution comment fix** — `RandomEventSystem.cpp` line ~1302 comment says "Migration wave: spawn 3-5 NPCs" but the actual count is driven by `ev.effectValue` from schema. Update comment to reflect the data-driven behavior, e.g., "Migration wave: spawn (effectValue-2) to effectValue NPCs".

- [ ] **RandomEventSystem effectValue audit** — After `SpawnNpcs` was clamped with `std::max(1, ...)`, audit all other `ev.effectValue` usages in `RandomEventSystem.cpp` (production_modifier, morale_boost, etc.) for similar unclamped casts or potential division-by-zero. Add defensive clamps where effectValue reaches arithmetic operators.

- [ ] **WorldLoader LoadWarning thread-through for all loaders** — `LoadSkills`, `LoadProfessions`, `LoadFacilities`, and `LoadAgents` in `WorldLoader.cpp` do not accept a `std::vector<LoadWarning>*` parameter. Thread the warnings pointer through these four functions for consistency, so any future diagnostics added to them automatically collect into the structured warnings vector.

- [ ] **WorldLoader LoadWarning UI display** — `main.cpp` collects `loadWarnings` but only retains them for future use. Wire the warnings vector into `GameState` or `RenderSnapshot` so the HUD can display load-time diagnostics (e.g., a "Load Warnings" panel showing misconfigured TOML entries) during gameplay.

- [ ] **NeedDrainSystem remove redundant bool** — `NeedDrainSystem::m_needsCached` uses the same old bool pattern just removed from `ConsumptionSystem`. Apply the same `NOT_CACHED = -2` sentinel approach: remove the bool, initialize cached NeedIDs to `NOT_CACHED`, and check `== NOT_CACHED` as the cache-miss gate.

- [ ] **Shared NOT_CACHED constant** — `ConsumptionSystem` defines a private `NOT_CACHED = -2` sentinel. If `NeedDrainSystem` and `DeathSystem` adopt the same pattern, each will have its own copy. Define a shared `static constexpr int NOT_CACHED_ID = -2` next to `INVALID_ID` in `WorldSchema.h` so all systems use one source of truth.

- [ ] **Shared emptyNames constant** — `GameState.cpp` and `HUD.cpp` each define their own `static const std::vector<std::string> emptyNames` for sharedSkillNames fallback. Extract into a shared file-scope constant in a common header (e.g., `RenderSnapshot.h` near the skillNames shared_ptr) so both translation units reference the same empty vector.

- [ ] **GameState skillNames variable naming** — `GameState.cpp` uses `skillNames` as the local variable name after the emptyNames fallback, while `HUD.cpp` uses `sharedSkillNames`. Unify to the same name across all 4 consumer sites for grep-ability and consistency.

- [ ] **SkillForProfession bounds-check assert** — `WorldSchema::SkillForProfession()` has the same bounds-check-and-fallback pattern as `ProfessionForResource()` but lacks the debug assert added in PR #72. Add `assert(pid >= 0 && pid < (int)professionToSkill.size())` for consistency.

- [ ] **SkillIdForResource bounds-check assert** — `WorldSchema::SkillIdForResource()` indexes `resourceToSkill` vector without a debug assert. Add `assert(res >= 0 && res < (int)resourceToSkill.size())` consistent with `ProfessionForResource()`.

- [ ] **InteractionCooldowns greetCooldown comment fix** — `AgentDecisionSystem.cpp` line ~2432 has a pre-existing wrong comment "120 game-seconds cooldown = 2 real-seconds" for `greetCooldown`. The cooldown value is `2.f` decremented by `dt`. Fix the inline comment to accurately describe the 2-second cooldown.

- [ ] **InteractionCooldowns GameDt passthrough note** — The `InteractionCooldowns` doc comment distinguishes "game-seconds" vs "real-seconds" but `GameDt()` is currently a passthrough (returns `realDt` unchanged). Add a one-line note to the doc block: "Note: GameDt currently returns realDt unchanged, so game-seconds == real-seconds at 1x speed."

- [ ] **MoodState wisdomFired age unit clarification** — `MoodState::wisdomFired` comment says "age > 70" but the code checks `age->days > 70.f` which is game-days. Clarify the comment to say "age > 70 game-days" to avoid confusion with real-world years.

- [ ] **MoodState homesickTimer reset condition precision** — `MoodState::homesickTimer` doc comment says "Reset to 0 on migration start" but only homesick return migration resets it, not forward migration. Clarify to "Reset on homesick return trigger and on arrival at a new settlement."

- [ ] **SkillRate boundary test** — `tests/SkillRateTest.cpp` tests `SkillGrowthRate(999)` but not the exact boundary `SkillGrowthRate(2)` (first out-of-range for a 2-skill schema). Add boundary-exact tests for both `SkillGrowthRate` and `SkillDecayRate` at `id == skills.size()`, where off-by-one bugs live.

- [ ] **SkillRate empty schema test** — `tests/SkillRateTest.cpp` doesn't test behavior when `skills` vector is empty (default-constructed `WorldSchema` without `BuildMaps()`). Add a test calling `SkillGrowthRate(0)` and `SkillDecayRate(0)` on an empty schema to verify the fallback path handles zero-size vectors.

- [ ] **DynBitset clear/reset naming review** — `DynBitset::clear()` zeros bits (like `std::bitset::reset()`) while `DynBitset::reset()` releases memory (like `std::vector::clear()`). The names are swapped relative to STL conventions. Consider renaming to `clearBits()` and `releaseMemory()`, or add comments explaining the naming choice to prevent confusion.

- [ ] **DynBitset clear/reset sequencing test** — `tests/DynBitsetTest.cpp` tests `clear()` and `reset()` independently but not in combination. Add tests for `clear()` after `reset()` and `reset()` after `clear()` to verify no interaction bugs between the two operations.

- [ ] **DynBitset move-assign self test** — `tests/DynBitsetTest.cpp` tests copy-assign self for both inline and heap modes but not move-assign self (`x = std::move(x)`). Add move-self-assignment tests for both modes to verify the compiler-generated move-assign handles this edge case.

- [ ] **DynBitset moved-from state documentation** — `tests/DynBitsetTest.cpp` move tests assert specific moved-from state (e.g., `b.none()` for heap, `b.test(bit)` for inline) which depends on compiler-generated move semantics. Add comments on each moved-from assertion noting this is implementation-detail verification, not a behavioral contract.

- [ ] **AgentDecisionSystem jealousy guard ordering** — `AgentDecisionSystem.cpp` line ~762 jealousy block checks `myProfFlag.any() && hs.settlement != entt::null && profSkId != INVALID_ID`. The cheapest check (`profSkId != INVALID_ID`, integer comparison) should be first, before the bitset `.any()` call. Reorder for micro-optimization.

- [ ] **SkillForProfession callers audit documentation** — The PR #78 audit of all 18 `SkillForProfession()` call sites lives only in the closed PR description. Add a brief `///` comment above `SkillForProfession()` in `WorldSchema.h` documenting that callers must guard against INVALID_ID before passing to `SkillGrowthRate()`/`SkillDecayRate()`.

## Phase 2 — UI Decoupling

- [ ] **UI State layer** — Create a UIState struct that owns all input handling, panel state, selection, pending actions, scroll positions. Lives on the main thread, never blocks on sim.

- [ ] **Replace InputSnapshot with CommandQueue** — Main thread pushes Commands instead of setting atomic flags. SimThread drains the queue. Remove InputSnapshot.h.

- [ ] **Generic RenderSnapshot** — Replace the 400-line game-specific RenderSnapshot with a generic entity list + metadata map. Sim publishes raw world state; UI layer formats it for display based on world config.

- [ ] **Immediate UI feedback** — Player actions show pending state in UI immediately (e.g., "buying..." indicator) without waiting for sim round-trip.

- [ ] **Data-driven HUD** — HUD reads WorldSchema to know what needs/resources/stats to display. No hardcoded "hunger bar" — it shows whatever needs the current world defines.

- [ ] **Data-driven keybindings** — Player action keys loaded from world config. No hardcoded KEY_T = trade.

## Phase 3 — Registry Partitioning & Multi-Threading

- [ ] **Region concept** — Define regions (settlement + surrounding area). Each region could own its own entity group.

- [ ] **Per-region system execution** — Systems that are region-local (NeedDrain, Production, Schedule, Birth, Death) run independently per region. Only cross-region systems (Transport, Migration, Events) need coordination.

- [ ] **Thread pool for regions** — Each region ticks on its own thread from a pool. Coordinator synchronizes cross-region interactions via message queues.

- [ ] **Spatial partitioning** — Add a spatial index (grid or quadtree) for distance queries. Replace O(n) scans in AgentDecision, Transport, etc.

## Phase 4 — Serialization & Persistence

- [ ] **Component serialization** — Save/load all ECS components to/from a binary or JSON format. Required for checkpointing, save games, and clustering handoff.

- [ ] **World state snapshot** — Full world state can be serialized for transfer between nodes or for save/load.

- [ ] **Deterministic replay** — Command log + initial state = reproducible simulation. Foundation for networking.

## Phase 5 — Networking & Multiplayer

- [ ] **Command serialization** — Commands can be serialized over the network. Same format used by CommandQueue.

- [ ] **State sync protocol** — Define how regions synchronize state between nodes. Delta compression for bandwidth.

- [ ] **P2P authority model** — Each player's local region is authoritative. Cross-region interactions are negotiated.

## Phase 6 — Second World (validation)

- [ ] **Create a second world definition** — Build a non-medieval world (e.g., sci-fi, post-apocalyptic, or modern) using only config files. No C++ changes. This validates that the engine is truly generic.

- [ ] **World selection at startup** — Menu or CLI flag to choose which world to load.
