# Changelog

All notable changes to ReachingUniversalis are documented here.
Format: `[version/milestone] - date - description`

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
