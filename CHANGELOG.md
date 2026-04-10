# Changelog

All notable changes to ReachingUniversalis are documented here.
Format: `[version/milestone] - date - description`

---

## [Third Settlement] Millhaven + Wood resource + economy fixes — 2026-04-10

Third settlement completes the three-node trade network with a new resource type.

### Added
- **Millhaven** — third settlement at (1200, 200), north of the Greenfield↔Wellsworth road:
  - 2 Lumber Mills producing Wood at 3 units/game-hour each
  - Shelter facility for NPC energy recovery
  - Starting stockpile: 30 Food, 30 Water, 120 Wood
  - Market: Wood=1.5g (surplus), Food=5.0g, Water=5.0g (imported)
  - 20 NPCs + 6 haulers spawned at spawn
- **`ResourceType::Wood`** — new tradeable resource; not a personal need; produced only at Millhaven
  - Haulers automatically detect and trade Wood via generic market price logic (no code changes needed in TransportSystem or PriceSystem)
  - Lumber Mill rendered as brown "L" square on the map
  - Stockpile panel shows "Wood: 120.0 @ 1.50g" in brown text
  - World status bar shows `Wd:xxx@x.x` column for each settlement
- **Three roads**: Greenfield↔Wellsworth (original), Greenfield↔Millhaven, Millhaven↔Wellsworth — creates routing alternatives and richer trade graph
- `SettlementStatus` in `RenderSnapshot` now carries `wood` and `woodPrice` fields

### Fixed
- **Bandits event now blocks one random road** instead of all roads simultaneously — with 3 roads the old behaviour would have cut the entire network at once; now picks a single open road randomly

---

## [Player = NPC] Remove player special powers — 2026-04-10

Player is now fully equivalent to an NPC; the only distinction is human-controlled movement via WASD.

### Removed
- **E key** (manual eat/drink boost): player now consumes passively from home settlement stockpile via `ConsumptionSystem`, same as any NPC
- **R key** (god-mode respawn / restore all needs): player can die of starvation like any NPC; death is permanent

### Changed
- Key hint strip updated: `WASD:Move  B:Road  F:Follow  F1:Debug`
- `InputSnapshot`: removed `playerEat` and `playerRespawn` fields

---

## [Sim Systems] Stockpile alerts, birth system, threading, and performance — 2026-04-10

Autonomous session completing post-WP8 simulation infrastructure.

### Added
- **Stockpile alerts** (`StockpileAlert` component + `ConsumptionSystem` monitoring):
  - Fires "Food low at X (N)" once when a settlement's food drops below 20 units
  - Fires "Food EMPTY at X" once when food drops below 1 unit
  - Flags reset on recovery (above 2× threshold) so alerts re-fire on the next shortage
  - Same logic for Water; all alerts pushed into the `EventLog`
- **Birth system** (`BirthSystem`): settlements spawn one new NPC every 3 game-hours if population < 35 AND food ≥ 30 AND water ≥ 30; costs 10 food + 10 water; logs "Born at X" to EventLog
- **Map boundary clamping** (`MovementSystem`): all agents clamp to MAP_W×MAP_H (2400×720) with 5px margin; velocity zeroed on contact
- **Stockpile cap** (`ProductionSystem`): settlement stockpiles hard-capped at 500 units per resource type
- **Richer hover tooltip**: shows `AgentRole` enum (NPC/Hauler/Player), behavior label, and need percentages; uses role field in snapshot instead of color heuristic
- **Sim/render thread split** (`SimThread`, `InputSnapshot`, `RenderSnapshot`):
  - `SimThread` owns `entt::registry` + all sim systems on a background thread
  - `InputSnapshot` — `std::atomic<>` fields written by main thread, consumed by sim thread via `.exchange(false)`
  - `RenderSnapshot` — mutex-protected POD copy built by sim thread; main thread holds lock briefly to copy vectors, then renders lock-free
  - `SimThread` tracks `simStepsPerSec` (wall-clock accumulator); shown in F1 debug overlay
- **Fixed-timestep sim loop** (`SimThread`): `SIM_STEP_DT = 1/60s`, `MAX_CATCHUP = 8` virtual frames per real frame; each virtual frame runs `tickSpeed` full sim steps so higher speeds do proportionally more work
- **Extended speed levels**: `{1, 2, 4, 8, 16, 32, 64, 128}×` — FPS measurably drops at 64× and 128× confirming sub-tick work is real
- **Uncapped framerate**: `SetTargetFPS(0)` — simulation is dt-based; display rate no longer tied to 60 Hz
- **ms/frame counter**: HUD shows render FPS and frame time in milliseconds alongside sim steps/s

### Changed
- `HUD` rewritten to read from `RenderSnapshot` (no registry access on main thread)
- `RenderSystem` stripped to `DrawStockpilePanel` only; all world/agent rendering moved to `GameState::Draw()`
- `GameState` rewritten: constructor starts `SimThread`; `Update()` polls input + lerps camera; `Draw()` renders from snapshot
- Stockpile panel moved from PY=580 to PY=200 to avoid overlapping the event log panel

---

## [Balance + QoL] Simulation stability and wishlist — 2026-04-10

Addresses Q1/Q2 balance feedback and both wishlist items.

### Balance
- Starting stockpiles raised: Greenfield Food 30→120, Water 5→20; Wellsworth Water 30→120, Food 5→20 — gives several game-hours of observation time before any shortage
- Farm/well `baseRate` raised 1→4 Food(Water)/hr — at full workforce (scale 2.0) each settlement produces ~16/hr against ~10–16/hr consumption; shortage only kicks in when workforce or haulers degrade
- Haulers per settlement raised 4→6; hauler carry capacity raised 5→15 — cross-settlement flow now meaningful

### Added (Wishlist)
- **Click-and-drag pan**: hold middle mouse button (or right mouse) and drag to pan the camera; disables player follow mode, same as arrow keys
- **FPS counter** in top-right HUD panel (always visible, below pop/deaths row)

---

## [WP8] HUD and Observation Tools — 2026-04-10

Full observability layer: event log, world status bar, NPC hover tooltip, and F1 debug overlay.

### Added
- **Event log panel** (bottom-right): renders last 8 `EventLog` entries; scroll with mouse wheel; color-coded (red=blockade, orange=death, green=cleared)
- **World status bar** (top-centre): shows each settlement's name, Food stock, Water stock, and population count; auto-centres between left and right HUD panels
- **NPC hover tooltip**: hover any agent in world-space to see role, behavior state, and need percentages; tooltip auto-repositions to stay on screen
- **F1 debug overlay**: entity count, FPS, NPC/hauler counts, sleeping/working counts, event log size
- `HUD::HandleInput` — handles F1 toggle and mouse-wheel log scrolling; called from `GameState::Update`
- Death events now pushed into `EventLog` by `DeathSystem` (day, hour, cause)

### Changed
- `HUD::Draw` split into sub-methods: `DrawWorldStatus`, `DrawEventLog`, `DrawHoverTooltip`, `DrawDebugOverlay`
- Key hint strip updated to include `F1:Debug`

---

## [WP7] Road Blockade and Event Log Foundation — 2026-04-10

Press **B** to toggle the road, watch cascades begin; all events are recorded.

### Added
- `EventLog` singleton component — `deque<Entry>` (max 50) of `{day, hour, message}` records
- **B key** in `TimeSystem`: toggles `Road.blocked` on all roads; pushes a blockade/clear event to `EventLog` with current in-game day and hour
- HUD road status row: shows **!! ROAD BLOCKED !!** in red or "Road: open" in green
- Updated key hint strip to include `B:Road`

### Notes
- Haulers already respected `Road.blocked` from WP4 — blockade immediately stops all trade
- `EventLog` is populated here; WP8 renders its contents in a scrollable bottom panel

---

## [WP6] Day/Night Schedules and Sleep — 2026-04-10

NPCs follow a daily schedule: sleep at night, work during the day; production scales with active workforce.

### Added
- `ScheduleSystem` — runs each frame before `AgentDecisionSystem`:
  - **Sleep** (`sleepHour`–`wakeHour`, default 22:00–06:00): sets `AgentBehavior::Sleeping`, NPC walks at 60% speed toward settlement centre and stops there; no other system overrides sleep
  - **Wake**: transitions `Sleeping → Idle` at `wakeHour`
  - **Work** (`workStart`–`workEnd`, default 07:00–17:00): promotes `Idle → Working` so non-critical NPCs look busy; clears `Working → Idle` outside work hours
- Worker-scaled production in `ProductionSystem`:
  - Counts `Working`-state NPCs per settlement each tick
  - Scale factor = `clamp(workers / BASE_WORKERS, 0.1, 2.0)` where `BASE_WORKERS = 5`
  - Creates cascade: deaths → fewer workers → lower production → more deaths

### Changed
- `AgentDecisionSystem` skips entities in `Sleeping` state; interrupts `Working` only when a need is below `criticalThreshold`

---

## [WP5] Player Controls and Camera Follow — 2026-04-10

Direct player movement, camera tracking, stockpile interaction, and respawn.

### Added
- `PlayerInputSystem` — handles all direct player input each frame:
  - **WASD**: move player (normalised diagonal, respects gameDt / pause)
  - **E**: consume 1 Food + 1 Water from nearest settlement stockpile (within 140px); instantly refills corresponding needs by 30× refillRate
  - **R**: teleport player to home settlement, restore all needs to full
  - **F**: toggle camera follow mode on/off
- Camera follow mode (`CameraState::followPlayer = true` by default) — camera target smoothly lerps to player position each frame (lerp factor 5×realDt)
- Key hint strip at bottom of player HUD panel: `WASD:Move  E:Eat/Drink  R:Respawn  F:Follow`

### Changed
- `CameraSystem`: arrow key pan now sets `followPlayer = false`; **C** key recenters map (zoom 0.5) and disables follow
- `HUD::BehaviorLabel` extended to cover `Migrating`, `Sleeping`, `Working` states
- HUD left panel height expanded to fit key hint row

---

## [WP4] Transport and Logistics — 2026-04-10

Haulers shuttle surplus resources between settlements along the road network.

### Added
- `HaulerState { Idle, GoingToDeposit, GoingHome }` — hauler state machine enum
- `Hauler` component — holds `HaulerState` and `targetSettlement` entity
- `Inventory` component — `map<ResourceType,int>` contents, `maxCapacity=5`, `TotalItems()`
- `TransportSystem` — three-state hauler loop:
  - **Idle**: checks road open; finds resource type with most surplus (min 2 units); takes up to 50% of surplus (max `maxCapacity`); deducts from home stockpile; sets `GoingToDeposit`
  - **GoingToDeposit**: walks to destination; aborts home if road is blocked mid-trip; deposits all inventory into dest stockpile on arrival → `GoingHome`
  - **GoingHome**: walks home; resets to `Idle` on arrival
- `WorldGenerator` spawns 4 haulers per settlement (SKYBLUE dots, size 7)
- `RenderSystem` shows haulers with DARKBLUE ring + colored cargo dot (green=food, blue=water)

### Changed
- `GameState` wires `TransportSystem` into update loop (after `ProductionSystem`, before `DeathSystem`)

---

## [WP3] Multi-NPC Population — 2026-04-10

40 NPCs living across both settlements, consuming resources, seeking facilities, migrating, and dying.

### Added
- `DeprivationTimer` component — tracks per-need time-at-zero and stockpile empty time (gameDt seconds)
- `AgentBehavior::Migrating` — new state for NPCs walking between settlements
- `ConsumptionSystem` — each NPC passively draws Food (0.5/hr) and Water (0.8/hr) from home stockpile; if stockpile has supply, the corresponding need drain is cancelled keeping the need stable; increments `stockpileEmpty` timer when deprived
- `DeathSystem` — destroys any entity whose need stays at 0 for 12 game-hours (720 gameDt seconds); logs cause to console; exposes `totalDeaths` counter
- `AgentDecisionSystem` rewritten:
  - Seeks nearest production facility of needed type within home settlement (replaces old resource node seeking)
  - Migration: when `stockpileEmpty >= 2 game-hours`, NPC sets target to the other settlement via Road and enters Migrating state; adopts new HomeSettlement on arrival
- NPC color encoding: white = satisfied, yellow = below 0.55, orange = below 0.30, red = below 0.15
- HUD top-right panel now shows population count and cumulative death count
- `WorldGenerator` spawns 20 NPCs per settlement (40 total), plus distinct yellow player NPC; stockpiles seeded with 20 Food (Greenfield) and 20 Water (Wellsworth)

### Changed
- Drain rates tuned for simulation timescale: Hunger ~20 game-hours, Thirst ~13, Energy ~33
- `GameState` wires ConsumptionSystem and DeathSystem into update loop

---

## [WP2] Spatial World Structure — 2026-04-10

Two settlements, a road, production facilities, stockpiles, and a pan/zoom camera.

### Added
- `Settlement` component — name and radius; rendered as a labeled circle
- `Stockpile` component — `map<ResourceType, float>` quantities per settlement
- `ProductionFacility` component — output type, base rate (units/game-hour), owning settlement
- `Road` component — connects two settlement entities; `blocked` flag for WP7
- `HomeSettlement` component — records which settlement an NPC belongs to
- `CameraState` component (singleton) — wraps Raylib `Camera2D`
- `CameraSystem` — arrow key pan, scroll wheel zoom, `C` to re-center; target clamped to map bounds
- `ProductionSystem` — adds `baseRate × gameHoursDt` to settlement stockpile each tick
- `RenderSystem` — now wraps world draw in `BeginMode2D`/`EndMode2D`; draws settlements,
  road (red + X when blocked), and production facility icons (F=Farm, W=Well)
- Settlement click-to-select — click a settlement circle to open its stockpile panel
  showing current Food/Water quantities; click again to close
- `WorldGenerator` — replaced 3 infinite resource nodes with:
  - Greenfield (x=400) with 2 Farms producing Food (1 unit/game-hour each)
  - Wellsworth (x=2000) with 2 Wells producing Water (1 unit/game-hour each)
  - One road connecting them

### Fixed
- NPC now freezes when paused and speeds up with tick multiplier (`MovementSystem`
  was using raw `realDt`; now uses `gameDt` from `TimeManager`)

### Changed
- Map width expanded to 2400px to fit both settlements
- `AgentDecisionSystem` stubs out resource node seeking (WP3 will wire stockpile consumption)
- `GameState` wires `CameraSystem`, `ProductionSystem`, and `RenderSystem.HandleInput`

---

## [WP1] Time System — 2026-04-10

Game clock, day/night sky, pause and speed controls.

### Added
- `TimeManager` component (singleton entity) — tracks `gameSeconds`, `day`,
  `hourOfDay` (0–24), `tickSpeed` (1/2/4x), and `paused` flag
- `TimeSystem` — advances the clock each frame; handles input:
  `Space` = pause/unpause, `+`/`=` = speed up, `-` = slow down
- Day/night background color — sky smoothly interpolates through midnight
  navy → dawn orange → morning blue → noon sky → dusk orange → night
- HUD time panel (top-right) — shows `Day N  HH:MM` and current speed / PAUSED
- `NeedDrainSystem` and `AgentDecisionSystem` now use `TimeManager.GameDt()`
  so needs drain at consistent game-time rates regardless of tick speed;
  pausing freezes all need drain and NPC refilling

### Changed
- `main.cpp` — `ClearBackground` now reads from `GameState::SkyColor()` instead
  of a hardcoded colour
- `HUD` panel height expanded to accommodate the state label row correctly

---

## [WP0] ECS Architecture Migration — 2026-04-10

Complete rewrite from monolithic OOP to EnTT-based Entity Component System.
Visual output is identical to the previous build — 1 NPC, 3 resource nodes, HUD.

### Added
- `EnTT v3.13.2` via CMake FetchContent — ECS registry and view system
- `src/ECS/Components.h` — all POD component structs: `Position`, `Velocity`,
  `MoveSpeed`, `Need`, `Needs`, `AgentState`, `ResourceNode`, `Renderable`, `PlayerTag`
- `src/ECS/Systems/NeedDrainSystem` — drains each need by `drainRate × dt` per tick
- `src/ECS/Systems/AgentDecisionSystem` — seek/satisfy state machine: finds most
  critical need, locates nearest resource node, moves toward it, refills need on arrival
- `src/ECS/Systems/MovementSystem` — applies `Velocity` to `Position` each tick
- `src/ECS/Systems/RenderSystem` — draws resource nodes (colored squares + radius ring)
  and agents (white circles) from registry components
- `src/World/WorldGenerator` — populates registry with initial scene entities
- `src/UI/HUD` — queries registry for `PlayerTag` entity, draws 3 need bars + state label
- `install-deps.sh` — one-time apt install of cmake, g++, make, X11/GL/audio headers
- `build.sh` — cmake configure + parallel build; supports debug / release / clean modes
- `test.sh` — headless smoke test via `xvfb-run`; PASS if game runs 5s without crashing
- `.vscode/tasks.json` — VS Code tasks: Install Deps, Build Debug/Release, Clean, Test

### Removed
- `src/NPC.h / NPC.cpp` — replaced by ECS components + AgentDecisionSystem
- `src/ResourceNode.h / ResourceNode.cpp` — replaced by `ResourceNode` component
- `src/Need.h` — merged into `ECS/Components.h`
- `src/World.h / World.cpp` — replaced by `WorldGenerator`
- `src/HUD.h / HUD.cpp` — replaced by `src/UI/HUD`

---

## [Base] Initial commit — 2026-04-10

### Added
- Raylib 5.5 window, 60 FPS game loop (1280×720)
- Single NPC with Hunger / Thirst / Energy needs and seek-satisfy behaviour
- 3 infinite resource nodes (Food, Water, Shelter) at fixed positions
- HUD with 3 need bars and current state label
- CMake build system with Raylib via FetchContent
