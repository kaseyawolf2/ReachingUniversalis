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

- [ ] **Schema-driven skill growth/decay rates** — `SkillDef` already has `growthRate` and `decayRate` fields but `Skills::Advance()` and the rust logic in `AgentDecisionSystem.cpp` use hardcoded `SKILL_GROWTH = 0.001f` / `SKILL_RUST = 0.0005f`. Wire the schema values through so each skill can have its own rate.

- [ ] **Cache hot-path schema lookups** — `Skills::ForResource()` in `Components.h` does a linear scan of `schema.skills` on every call; `SkillForProfession()` does an `unordered_map::find` per call. Add `resourceToSkill` and `professionToSkill` lookup maps to `WorldSchema`, populated in `BuildMaps()`, and use them in these methods.

- [ ] **Named season thresholds** — `RandomEventSystem.cpp` and `ScheduleSystem.cpp` use magic float thresholds (`0.8f`, `0.3f`, `0.15f`) for season property checks (heatDrainMod, productionMod). Define named constants (e.g., `HARSH_COLD_THRESHOLD`, `FLOOD_HEAT_DRAIN_MAX`) in a shared header or as SeasonDef metadata so modders can tune them.

- [ ] **Profession bitmask scalability** — All profession diversity checks use `uint32_t` bitmasks with `assert(professions.size() < 32)` in `BuildMaps()`. Replace with `std::bitset` or a variable-width bitfield to support worlds with 32+ professions.

- [ ] **Reduce per-frame RenderSnapshot allocations** — `SimThread::WriteSnapshot()` rebuilds `skillNames`, `settlSkillNames`, and nested `std::map<int, SkillAccum>` every frame under the mutex. Store schema-derived names once at construction, and replace `std::map<int, SkillAccum>` with flat `std::vector<SkillAccum>` indexed by SkillID.

- [ ] **Remove per-agent skillNames duplication in SettlementEntry** — `SettlementEntry::skillNames` is populated identically for every settlement from schema data. Store it once on `RenderSnapshot` (like `AgentEntry` was already fixed) and reference the shared copy in HUD rendering.

- [ ] **Event effect type as enum** — `RandomEventSystem.cpp` compares `ev.effectType` strings (`"production_modifier"`, `"road_block"`, etc.) in per-event hot paths. Add an `enum class EventEffectType` resolved at load time in WorldLoader, and switch on it instead of string comparisons.

- [ ] **Multi-spreading event support** — `RandomEventSystem.cpp` spreading logic finds only the first `ev.spreads == true` event. Support multiple spreadable events by storing a vector of spreading event indices and iterating all of them in the spread tick.

- [ ] **Goal completion cooldown** — When `goalCount == 1` or fixed-target goals (e.g., FindFamily target=1) are re-assigned, NPCs re-complete immediately every tick, spamming celebration logs. Add a `completionCooldown` field to GoalDef (or a per-NPC cooldown timer) to prevent instant re-completion.

- [ ] **Validate TOML at load time** — WorldLoader silently accepts invalid field values (e.g., `chance = 0` for all events causes UB in weighted selection, unknown `check_type` strings default silently). Add validation passes in WorldLoader that log warnings or errors for: zero total weight, unknown enum strings, missing required fields for specific effect/check types.

- [ ] **Generic DeprivationTimer need timers** — `DeprivationTimer` still has hardcoded `float hungerTimer, thirstTimer, energyTimer, heatTimer`. Replace with `std::vector<float>` indexed by NeedID, sized from `schema.needs`. Systems iterate generically instead of by named field.

- [ ] **Consolidate SocialBehavior fields** — `SocialBehavior` has 18 fields including unrelated concerns (visitTimer, bankruptSurvivor, homesickTimer, reconcileGlow). Split into focused sub-components or at minimum group logically: `VisitState`, `MoodState`, `InteractionCooldowns`.

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
