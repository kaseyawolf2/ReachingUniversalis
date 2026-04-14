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

- [x] **Use `inline constexpr` in SeasonThresholds.h** — `SeasonThresholds.h` uses `static constexpr` which creates separate copies per translation unit. Switch to `inline constexpr` (C++17) for single-definition constants across all TUs that include the header.

- [x] **Season threshold config in TOML** — Move the named constants in `SeasonThresholds.h` (`HARSH_COLD`, `MODERATE_COLD`, `MILD_COLD`, `COLD_SEASON`, `HARVEST_SEASON`, `LOW_PRODUCTION`) into `worlds/medieval/seasons.toml` as global threshold fields. `WorldSchema` stores them; systems read from schema instead of compile-time constants. Enables modders to tune season breakpoints per world.

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

- [ ] **DynBitset promotion-path test** — Add a test to `tests/DynBitsetTest.cpp` that calls `set(200)` on a default-constructed (empty inline) `DynBitset`, exercising the `promoteIfNeeded` path from truly empty inline state to heap allocation. Verify `test(200)` returns true and `test(0)` returns false.

- [ ] **Remove SeasonThresholds.h references from TODO.md** — Two completed TODO items (lines ~72, ~74) still reference `SeasonThresholds.h` as if it exists. The header was deleted when season thresholds moved to TOML. Clean up stale references.

- [ ] **Assert-to-runtime check in BuildResourceToSkillMap** — `BuildResourceToSkillMap()` uses `assert(crossRefsResolved)` which compiles to nothing in release builds. Replace with a runtime check (`if (!crossRefsResolved) { fprintf(stderr, ...); return; }`) since this is a load-time path, not a hot path, and should catch misordering even in release.

- [ ] **ConsumptionSystem schema reference** — `ConsumptionSystem` receives `const WorldSchema&` in `Update()` but doesn't store it. Add a `const WorldSchema& m_schema` member initialized in the constructor so `FindNeed()` results can be cached as member variables (prerequisite for the "Cache FindNeed results" task).

- [ ] **GameState m_schema member ordering** — `GameState::m_schema` is declared between camera fields and `m_renderSystem`. Move it next to `m_simThread` since both hold references from the same constructor parameter, improving readability of the private section.

- [ ] **DynBitset rename n1 test** — Rename `and_n1_both_heap_single_word` in `tests/DynBitsetTest.cpp` to `and_heap_heap_word0_only` since the test exercises the general heap loop (n=2), not the n<=1 branch. The companion `and_n1_edge_case` is the one that actually hits n<=1.

- [ ] **Validate season threshold ordering in WorldLoader** — After loading season thresholds from TOML, verify the ordering invariant `mildCold < moderateCold < harshCold` and `lowProduction < harvestSeason`. Log a warning if thresholds are inverted, as this would cause nonsensical sky tints and schedule behavior.

- [ ] **Schema-driven PriceSystem floor thresholds** — `PriceSystem::SeasonPriceFloor()` now takes `const SeasonThresholds&` but still uses hardcoded logic for which thresholds trigger price floors. Make the price floor rules data-driven: add a `priceFloorThreshold` field to `SeasonDef` in `WorldSchema.h` so each season can specify its own floor multiplier.

- [ ] **WorldLoader OptFloat default source-of-truth** — `LoadSeasons()` uses `OptFloat(tbl, key, struct_default)` where the default comes from the struct's in-class initializer. If someone changes the struct default without updating the TOML comment, they silently diverge. Add a comment warning about this coupling, or define named constants in `WorldSchema.h` used by both the struct initializer and the TOML comment.

- [x] **Dead GoalDef string fields cleanup** — `GoalDef` still carries dead `std::string checkType`, `targetMode`, and `behaviourMod` fields alongside their resolved enums — the same pattern just removed from `EventDef`. Remove them from `GoalDef` in `WorldSchema.h` and use local variables in `WorldLoader.cpp` during parsing.

- [x] **Flat array professionToSkill** — `WorldSchema::professionToSkill` is `unordered_map<int,int>` but `ProfessionID` is a dense integer. Replace with `std::vector<SkillID>` sized to `professions.size()` and indexed by `ProfessionID`, initialized to `INVALID_ID`. Same optimization as `resourceToSkill` and `resourceToProfession`.

- [ ] **Cache FindNeed in NeedDrainSystem** — `NeedDrainSystem::Update()` likely calls `schema.FindNeed()` by string every tick, same pattern fixed in `ConsumptionSystem`. Add cached member variables for frequently-used NeedIDs, initialized on first `Update()` call.

- [ ] **Cache FindNeed in DeathSystem** — `DeathSystem::Update()` may call `schema.FindNeed()` by string for death cause determination. Cache the NeedIDs as member variables to eliminate per-tick string lookups.

- [ ] **Spread timer comment cleanup** — `RandomEventSystem.cpp` still has `(drought/spreading event recovery)` as an awkward half-rename. Either go fully generic (`(event recovery)`) or use two concrete examples consistently.

- [ ] **Duplicate static const emptyNames in HUD.cpp** — `HUD.cpp` declares three separate `static const std::vector<std::string> emptyNames;` in different methods. Consolidate into a single file-scope `static const` to eliminate duplication.

- [ ] **DeprivationTimer Make() default constant** — `DeprivationTimer::Make()` default parameter `2.f * 60.f` duplicates the member initializer `migrateThreshold = 2.f * 60.f`. Define a named constant (e.g., `DEFAULT_MIGRATE_THRESHOLD`) used by both to prevent drift.

- [ ] **DeprivationTimer units comment fix** — `DeprivationTimer::migrateThreshold` comment says "game-min" but the sim ticks in game-seconds. Fix the comment to say "game-seconds" to match the actual units.

- [ ] **RenderSnapshot immutable field comment** — `RenderSnapshot::skillNames` shared_ptr is written once at construction and never mutated, unlike every other field which is written per frame under mutex. Add a comment `// Immutable after construction; not protected by mutex` to prevent future maintainers from moving it into WriteSnapshot.

- [ ] **EventLog system-name prefix** — `RandomEventSystem.cpp` diagnostic warnings use `[WARNING]` prefix, dropping the system name. Restore `[RandomEventSystem]` prefix so stderr grep can identify the source system.

- [ ] **SpawnNpcs effectValue runtime clamp** — `RandomEventSystem.cpp` line ~1297 casts `effectValue` to `int` for `maxArrivals` without clamping, so values < 1 produce UB in `uniform_int_distribution`. Add `maxArrivals = std::max(1, (int)ev.effectValue)` as a runtime safety net alongside the load-time validation.

- [ ] **WorldLoader stderr to structured logging** — All 26+ `fprintf(stderr, ...)` calls in `WorldLoader.cpp` output unstructured text. Define a `LoadWarning` struct or use a vector to collect warnings during loading, then dump them after load completes. Enables future UI display of load diagnostics.

- [ ] **ConsumptionSystem remove redundant bool** — `ConsumptionSystem::m_needsCached` bool is redundant since `m_hungerNeedId` is initialized to `-1` (INVALID_ID). Remove the bool and use `m_hungerNeedId < 0` as the cache-miss sentinel.

- [ ] **Shared skillNames null-check consistency** — `GameState.cpp` null-checks `sharedSkillNames` before use, but the three HUD methods use `emptyNames` fallback. Pick one pattern: either always null-check with early return, or always fallback to empty. Apply consistently across all 4 consumer sites.

- [ ] **ProfessionForResource callers audit** — After `resourceToProfession` changed to flat vector, audit all callers of `ProfessionForResource()` to ensure none pass ResourceIDs that could exceed `resources.size()`. Add an assert in `ProfessionForResource()` for debug builds.

- [ ] **SocialBehavior InteractionCooldowns doc comment** — `InteractionCooldowns` in `Components.h` now holds `skillCelebrateTimer` and `reconcileGlow` after the sub-struct relocation, but has no doc comment explaining that these are behavior-gating cooldowns (not mood state). Add a brief `///` comment above the struct listing each timer's purpose and units (game-seconds).

- [ ] **SocialBehavior MoodState dead field audit** — After relocating `skillCelebrateTimer` and `reconcileGlow` out of `MoodState`, audit whether any remaining `MoodState` fields are also functionally cooldowns. Candidates: `griefLevel` decays over time and gates visit behavior. Document each field's category (persistent state vs. decaying timer) with inline comments.

- [ ] **SkillGrowthRate bounds-check unit test** — Add a test to `tests/` that constructs a minimal `WorldSchema` with 2 skills, then calls `SkillGrowthRate(INVALID_ID)`, `SkillGrowthRate(-1)`, and `SkillGrowthRate(999)` to verify the fallback returns 1.0f for all out-of-range inputs. Same for `SkillDecayRate`.

- [ ] **SkillForProfession INVALID_ID propagation audit** — After the INVALID_ID guard was added at line ~598 of `AgentDecisionSystem.cpp`, audit all other call sites of `SkillForProfession()` across all systems to ensure they also guard against INVALID_ID before passing to `SkillGrowthRate()` or `SkillDecayRate()`.

- [ ] **DynBitset copy assignment operator test** — `tests/DynBitsetTest.cpp` tests copy/move constructors but not copy/move assignment operators. Add tests for `DynBitset a; a = b;` (copy assign) and `DynBitset a; a = std::move(b);` (move assign) for both inline and heap modes, verifying independence and source state.

- [ ] **DynBitset clear() method** — `DynBitset` has no `clear()` or `reset()` method to zero all bits without reallocating. Add `void clear()` that zeros all words (inline and heap) without changing capacity, and `void reset()` that returns to default-constructed empty state. Add corresponding tests.

- [ ] **Season threshold TOML test config** — Add a `tests/test_seasons_invalid.toml` with intentionally inverted thresholds (`harshCold < mildCold`) and a test or script that runs `WorldLoader::LoadSeasons()` against it, verifying the warning is logged. Documents the validation behavior for future modders.

- [ ] **Season threshold HUD display** — `GameState.cpp` uses season thresholds for sky tint but the HUD doesn't show the current threshold regime. Add a `seasonRegime` string field to `RenderSnapshot` (e.g., "Harsh Cold", "Mild", "Harvest") set by `TimeSystem` based on current `heatDrainMod` vs thresholds, displayed in the time panel.

- [ ] **Flat professionToSkill bounds-check assert** — `WorldSchema::SkillForProfession(ProfessionID)` indexes into `professionToSkill` vector but has no bounds check. Add `assert(pid >= 0 && pid < (int)professionToSkill.size())` for debug builds, consistent with `SkillIdForResource()`.

- [ ] **BuildProfessionToSkillMap ordering test** — Add a test or assert that calling `BuildProfessionToSkillMap()` before `ResolveCrossRefs()` triggers the `assert(crossRefsResolved)` failure. Currently only tested implicitly by the load path. An explicit test documents the ordering contract.

- [ ] **GoalDef resolved enum validation** — After removing dead string fields from `GoalDef`, the resolved enums (`checkEnum`, `targetEnum`, `behaviourEnum`) are the only source of truth. Add validation in `WorldLoader.cpp` that warns if any resolved enum is the default/unknown value after parsing, indicating the TOML had an unrecognized string that was silently defaulted.

- [ ] **GoalDef field documentation** — `GoalDef` in `WorldSchema.h` has no doc comments on its remaining fields after the dead string removal. Add `///` comments for each field explaining its purpose, valid range, and which system consumes it (e.g., `completionCooldown` is consumed by `AgentDecisionSystem`).

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
