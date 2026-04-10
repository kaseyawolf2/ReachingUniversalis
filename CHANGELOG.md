# Changelog

All notable changes to ReachingUniversalis are documented here.
Format: `[version/milestone] - date - description`

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
