# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
bash build.sh          # Debug build (default)
bash build.sh release  # Release build with optimisations
bash build.sh clean    # Clean build directory
```

Build output: `build/ReachingUniversalis`

## Running and Testing

```bash
./build/ReachingUniversalis        # Interactive run (requires display)
bash test.sh [seconds]             # Headless smoke test via Xvfb (default: 5s)
                                   # Passes if the process doesn't crash
```

**SSH display note:** The server requires `ssh -X` from WSL, and `xauth` must be installed. The test script handles this automatically via Xvfb.

No linter or unit test suite; `test.sh` is the only automated validation.

## Architecture Overview

### Threading Model

Two threads communicate through two shared structs:

- **`InputSnapshot`** (`src/Threading/InputSnapshot.h`) — main thread writes, sim thread reads. All fields are `std::atomic`. Contains one-shot action flags (set true by main, cleared by sim after processing) and continuous movement values.
- **`RenderSnapshot`** (`src/Threading/RenderSnapshot.h`) — sim thread writes, main thread reads. Protected by `RenderSnapshot::mutex`. Sim thread locks briefly at the end of each step to swap all drawable state; main thread locks briefly at render time.

The main thread (`GameState`, `src/GameState.h`) does three things: poll Raylib input → write `InputSnapshot`, update the camera, render from `RenderSnapshot`. It never touches the ECS registry.

### Simulation Thread

`SimThread` (`src/Threading/SimThread.h` / `.cpp`) owns the `entt::registry` and all ECS systems. It runs a tight loop: `ProcessInput()` → `RunSimStep(dt)` → `WriteSnapshot()`.

`WriteSnapshot()` walks the registry and populates every field of `RenderSnapshot` under the mutex. Anything that needs to reach the HUD must be explicitly written here — there is no automatic sync.

**Tick speed** (1×/2×/4×): `GameState::Update` runs the sim step loop multiple times per frame rather than scaling `dt`. Systems receive `realDt` unchanged and convert it to game-time via `TimeManager::GameDt()`.

### ECS Systems (execution order in `RunSimStep`)

1. **TimeSystem** — advances `TimeManager` (day, hour, season, temperature)
2. **NeedDrainSystem** — drains hunger/thirst/energy/heat per NPC and player
3. **ConsumptionSystem** — NPCs buy from stockpile or pay wages; emergency market purchases
4. **ScheduleSystem** — assigns work/sleep/idle based on `Schedule` and time of day
5. **AgentDecisionSystem** — migration, skill-affinity job seeking, need-satisfaction behaviour
6. **MovementSystem** — `Position += Velocity * dt`, clamped to world bounds
7. **ProductionSystem** — workers produce resources; yield cycle, spoilage, seasonal modifiers
8. **TransportSystem** — hauler state machine: Idle → GoingToDeposit → GoingHome
9. **DeathSystem** — need-deprivation death, old-age death, inheritance, cargo recovery
10. **BirthSystem** — prosperity-based NPC births, dynamic pop cap
11. **PriceSystem** — supply/demand price adjustment; arbitrage convergence weighted by road condition
12. **RandomEventSystem** — fires stochastic events (plague, drought, festival, migration, etc.)
13. **EconomicMobilitySystem** — NPC→Hauler graduation and hauler bankruptcy
14. **ConstructionSystem** — road repair/degradation, facility building, settlement founding

### Key Components

- **`Settlement`** — has a `treasury` (gold) that pays NPC wages and is credited on trade tax. Gold flow invariant: every `balance -=` must credit a treasury unless it's a legitimate infrastructure sink.
- **`Hauler`** — uses `buyPrice` to record the per-unit cost paid at pickup, enabling exact refunds on road-abort or bankruptcy.
- **`TimeManager`** — `GAME_MINS_PER_REAL_SEC = 1.0`, `SIM_STEP_DT = 1/60`. At 1× speed: 1 real second = 1 game-minute; 1 real day = 24 game-minutes.
- **`Needs`** — array of 4 needs (Hunger, Thirst, Energy, Heat). Indexed by int in several systems; order matters.
- **`Skills`** — three floats (farming, water, woodcutting), 0–1. Not added by default to all NPCs; check `try_get` before use.

### Player Input Keys

| Key | Action |
|-----|--------|
| WASD / Arrows | Move |
| T | Auto-buy cheapest resource at nearest settlement |
| Q | Buy 1 unit of cheapest resource |
| E | Work at nearest facility |
| V | Buy cart (+10 carry capacity, 300g) |
| C | Build production facility (200g) |
| P | Found new settlement (1500g) |
| R | Repair nearest blocked road (50g) |
| N (×2) | Build new road (400g) |
| H | Set nearest settlement as home |
| Z | Toggle sleep |
| Space | Pause |
| `[` / `]` | Speed down / up |

### World Generation

`WorldGenerator` (`src/World/WorldGenerator.h`) creates the initial world: settlements, roads, production facilities, NPCs with needs/schedule/money, and the player entity. Founding a new settlement (P key) auto-connects it to the nearest existing settlement via road.

### Gold Flow Rule

Every gold deduction from a `Money` balance must be accompanied by a credit to a `Settlement::treasury` (or another `Money` balance), unless it is a legitimate infrastructure sink:
- Road maintenance, road repair, road building, facility construction, and housing = gold burns with no recipient (materials/labour in the broader world economy).
- All trade, wages, taxes, and purchases must balance to a recipient treasury or balance.
