# Changelog

All notable changes to ReachingUniversalis are documented here.
Format: `[version/milestone] - date - description`

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
