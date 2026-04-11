# Changelog

All notable changes to ReachingUniversalis are documented here.
Format: `[version/milestone] - date - description`

---

## [Roads & Disease] Player road building/repair, plague spreading — 2026-04-11

### Added
- **Player road repair (R key)**: Press R near a blocked road (within 80px) to pay 50g and
  clear it immediately. Gives the player a direct role in keeping trade flowing after bandit
  raids or blizzards. Logs a failure message if no blocked road is nearby or funds insufficient.
- **Player road building (N key, two-press)**: Press N near one settlement, walk to another,
  press N again to build a new road connection between them for 400g. A dashed orange preview
  line shows the pending start-point during selection. ESC cancels. Validates that the two
  endpoints resolve to different settlements and that no road already exists between them.
- **Plague spreading system**: Disease outbreaks now propagate to neighbouring settlements via
  open roads. Each infected settlement attempts to spread every 20 game-hours (60% success
  chance); if a road is blocked, disease cannot cross it. Infected settlements get a 55%
  production penalty for 72 game-hours. Plague visually shows as a purple double-ring on the
  map. Blocking a road mid-plague can contain the outbreak — blocking roads now has strategic
  defensive value.
- **Visual road-build pending line**: While in road-build mode (after first N press), a dashed
  orange line is drawn from the selected source position to the player's current world position,
  giving clear real-time feedback about what will be connected.

### Fixed
- **KEY_C duplicate binding**: Camera-reset action (center map, zoom out) was bound to C,
  conflicting with the Player Build action. Moved camera reset to Home key.

### Changed
- Controls hint updated: R:Repair, N:Road added; Home replaces C for camera reset.
- Disease outbreak (random event 3) now sets `modifierName = "Plague"` and enters the
  spreading pipeline rather than being a one-time NPC kill with no further consequences.

---

## [Observability & Growth] Trade hints, economy stats, dynamic population caps — 2026-04-10

### Added
- **Trade opportunity hint**: Player HUD shows the best buy→sell margin for goods at the player's
  current settlement (e.g. "Food: buy Greenfield 1.2g → sell Wellsworth 7.8g (+6.6g)"). Updates
  dynamically as prices change. Falls back to global best margin when not near a settlement.
- **Cargo capacity display**: HUD cargo row now shows "Cargo: X/15" with a red indicator when
  full. Items always shown (shows "(empty)" rather than hiding the row).
- **Economy-wide statistics in F1 debug overlay**: Adds an "Economy" section showing total gold
  in the world (all NPCs + player + treasuries), average NPC wealth, richest individual NPC
  (name + balance), and total hauler count.
- **Dynamic population caps**: Settlements can now expand their population cap (default 35) by
  building housing. ConstructionSystem triggers housing when pop >= 80% of cap and treasury
  >= 300g; each housing built adds +5 cap up to a maximum of 70. BirthSystem uses the dynamic
  cap; StockpilePanel shows the actual cap rather than a hardcoded constant.
- **Location-aware trade hint**: Replaces the naive global-spread calculation with one that
  prioritizes the best margin *from the player's current location*. Shows what to buy here and
  where to carry it, not just the globally cheapest vs most expensive.

### Fixed
- Player respawn incorrectly gave 5-unit carry capacity (component default) instead of 15
  units matching the starting inventory given to new characters.

---

## [Economy & Self-Organization] Settlement construction, skill specialisation, player trading — 2026-04-10

### Added
- **ConstructionSystem**: Settlements autonomously build new production facilities when thriving.
  Trigger: treasury > 200g AND a resource price > 7g AND stock < 20 units. Costs 200g from
  treasury. Max 4 facilities per resource type per settlement. Logged green in event log.
- **Skill aptitude at birth**: Newborns have a random aptitude (farming/water/wood). Aptitude
  skill starts at 0.15 vs 0.08 for others, and grows to cap 0.42 during childhood (vs 0.35).
  Creates visible natural specialisation by workforce entry age.
- **Aptitude-seeking work behavior**: Working NPCs now prefer the facility matching their
  strongest skill (farmers to farms, water carriers to wells, woodcutters to mills), only
  falling back to nearest facility if no matching one exists at their settlement.
- **Skill-aware migration**: `FindMigrationTarget` adds a 20% affinity bonus for destinations
  whose primary facility matches the NPC's strongest skill; skilled workers self-sort over time.
- **Skill-based wages**: `wage = 0.3 × (0.5 + bestSkill)` → range 0.15–0.45 g/hr. Master
  craftspersons earn 3× a beginner, creating economic reward for skill specialisation.
- **Profession from skill not settlement**: Hover tooltip profession label now reflects the NPC's
  actual strongest skill (only falls back to settlement primary if skills are too uniform).
- **Life-stage visual sizing**: Children are rendered at 60% of adult size, youth at 80%,
  elderly at 105%; makes demographic composition instantly visible on the map.
- **Production/consumption breakdown in settlement panel**: Each resource now shows two lines:
  `Food: 50u @2.50g  net:+3.5/hr` (summary) plus `prod:+6.0  cons:-2.5  /hr` (detail sub-line).
  Settlement panel also shows current worker count beside treasury.
- **Q-key buy action**: Player can press Q near a settlement to buy 1 unit of the cheapest
  available resource at market price. Enables the merchant playstyle: earn gold via E-key work,
  buy cheap at surplus settlements, sell at premium settlements via T-key.
- Event log: construction and hauler graduation events now colored green in both HUD log and
  settlement panel recent-events list.

### Changed
- Controls hint updated to include Q:Buy; removed M:Market (key not bound).
- `StockpilePanel` gains `prodRatePerHour`, `consRatePerHour`, and `workers` fields.
- Settlement panel width increased from 265px to 280px to accommodate breakdown lines.

---

## [Skills & Observability] Worker skills, player work action, and world tooltips — 2026-04-10

### Added
- **NPC Skills system**: Each NPC has three skills: `farming`, `water_drawing`, `woodcutting` (range 0–1).
  Skills advance through practice (working at the relevant facility) and decay through disuse.
  Skill formula: `skillMult = 0.5 + avgSkill` → [0.5, 1.5] multiplier on production output.
- **Child skill lifecycle**: Children (<15 game-days) passively grow all skills from 0.1 toward
  0.35 by observing community work. Capped at 0.35 (Novice) on entering the workforce.
  Adults not Working slowly decay skills at 0.005/day to model disuse.
- **Player Skills component**: Player spawns with 0.4 skill in all categories. Player contributes
  to production when Working near a facility (same skill-gain rate as NPCs).
- **E-key Work action**: Player can press E near a production facility to toggle Working state.
  Stops automatically when player moves (WASD). Skill advances while stationary at work site.
- **Settlement specialty labels**: Below each settlement name on the map, the primary production
  type is shown ("Farming" in green, "Water" in blue, "Lumber" in brown). Inferred from the
  highest-baseRate facility at that settlement.
- **Facility hover tooltip**: Hovering within 20 world units of a facility square shows:
  type, settlement name, worker count, average worker skill, base rate, and estimated output.
  Output bar colored green/yellow/red relative to base-rate performance.
- **Road hover tooltip**: Hovering within 25 world units of a road segment shows price
  differentials between the two endpoints. Arrow direction (→/←/=) indicates which settlement
  offers better buying/selling prices per resource type. Blocked roads show [BLOCKED].
- **NPC skill line in hover tooltip**: Hovering an NPC now shows their skill levels (Farm/Water/Wood)
  with a rank label: Novice (<0.3), Trained (0.3–0.6), Expert (0.6–0.85), Master (>0.85).
- **Player skill panel**: Player HUD shows "Skills: Farm:X% Wtr:Y% Wood:Z%" line below needs.
- **Desperate theft**: NPCs near death (6+ game-hours needsAtZero) with no money (< 1g) steal
  2 units from their home stockpile. Cooldown: 4 game-hours between thefts. Logged as event:
  "Alice stole food at Greenfield (desperate)". Creates tragedy-of-the-commons dynamics during crises.
- **Population cap display**: Settlement stockpile panel header now shows `[X/35 pop]`.
- **Controls hint updated**: HUD controls hint includes "E:Work".

### Changed
- `DeprivationTimer` gains `stealCooldown = 0.f` field for theft rate limiting.
- `ProductionSystem` now uses skill-weighted worker average; production scales with worker expertise.
- `ScheduleSystem` wage block removed (duplicate of `ConsumptionSystem`'s `WAGE_RATE = 0.3f`).
- Player now excluded from `ProductionSystem`'s worker filter — player can contribute to production.
- Event log bad-event color filter extended with "stole" and "BANDITS".

---

## [Simulation Polish] Economy, observability, and NPC lifecycle improvements — 2026-04-10

### Added
- **Player sleep key (Z)**: Player can press Z to toggle Sleeping state, restoring Energy need.
  WASD movement while sleeping auto-wakes. Fixes bug where player could never restore Energy.
- **Off-map trade convoy** (random event type 9): When a settlement has a critically scarce
  resource (price > 5g), an external convoy delivers 50 units. Simulates off-map trade pressure.
  Shows GREEN in event log as "OFF-MAP CONVOY at Greenfield +50 food (price was 8g)".
- **Hauler adaptive patience**: Haulers track consecutive idle evaluations (`waitCycles`).
  After 5 failed evaluations, they accept any positive-margin route (floor: 0.5g). Resets on
  successful delivery or home arrival — addresses Q9 "willing to wait for better margin."
- **Treasury alerts**: Settlement treasury low (<50g) and empty (<1g) events logged via the
  StockpileAlert system. `StockpileAlert` gains `treasuryLow` / `treasuryEmpty` flags.
- **Food spoilage**: Stockpile food decays at 0.5%/game-hour (Summer: 2×, Winter: 0.5×).
  Prevents indefinite accumulation and keeps markets dynamic.
- **Seasonal price floors**: `PriceSystem` now maintains minimum prices by season — Wood price
  floor rises to 5g in Winter (demand surge); Food floor rises to 2g in Winter. Prices can't
  decay below these floors via abundance alone.
- **NPC work-site movement**: During work shifts, NPCs walk toward the nearest production
  facility at their home settlement (80% speed). Makes work visible and legible.
- **Leisure wandering**: Idle NPCs between work-end and sleep-time pick random wander destinations
  within 80px of home settlement. They amble at 40% speed, making the settlement feel alive.
- **Behavior breakdown in debug overlay (F1)**: Shows Working/Sleeping/Idle/Seeking/Migrating
  counts and hauler count alongside existing diagnostics.
- **Settlement ring includes wood shortage in winter**: When season is Autumn/Winter and
  woodStock < 20, the settlement health ring factors in wood (turns red/yellow). Added
  `woodStock` and `season` to `SettlementEntry` in RenderSnapshot.
- **BirthSystem fixes**:
  - Newborns now get a Money component (5g starting purse) — can participate in emergency market
  - Newborns get ±20% personality variation on need drain rates
  - Winter birth check: requires ≥10 wood in stockpile before spawning a child in cold seasons
- **Migration wave NPC fixes**: RandomEventSystem case 5 now applies ±20% personality variation
  to all arriving migrants (drain rates randomised per-NPC).
- **Player respawn Money fix**: RespawnPlayer now gives 10g starting purse (was missing entirely).
- **Player sell trade tax**: Player T-key sells now pay same 20% trade tax as haulers.
  Tax revenue flows to destination settlement treasury. Log shows tax amount.

### Changed
- `PRICE_MAX` raised from 20g to 25g to accommodate seasonal spike potential.
- Debug overlay expanded from 10 to 14 rows; reorganised into sections with behavior breakdown.

---

## [Seasons, Heat & Economy] Season cycle, Heat need, NPC market purchasing — 2026-04-10

### Added
- **Seasons** (`Season` enum in `TimeManager`): 4-season cycle (Spring/Summer/Autumn/Winter),
  each 30 game-days. `CurrentSeason()` computed from `day` field.
- **Season production modifiers**: Spring 0.8×, Summer 1.0×, Autumn 1.2× (harvest), Winter 0.2×
  applied on top of settlement/drought modifiers in ProductionSystem
- **Winter energy drain boost**: Winter raises Energy drain to 1.8× normal (harder to rest warm)
- **Season transition logging**: TimeSystem logs "--- Winter begins ---" etc. when season changes
- **Season display in HUD**: Top-right clock panel shows current season name (green/yellow/orange/skyblue)
- **Heat need** (`NeedType::Heat`, index 3): 4th need for all NPCs, player, haulers
  - Drains based on season: Summer=0×, Spring=0.15×, Autumn=0.4×, Winter=1.0×
  - Satisfied passively by Wood stockpile: ConsumptionSystem burns Wood when available
  - 0.03 Wood per NPC per game-hour consumed in Winter (scaled by season)
  - If no Wood available, Heat drains → death by cold or migration
  - Summer passively restores Heat to full (no burning needed)
- **Heat bar in HUD**: 4th orange bar in player left panel below Energy
- **Wood stockpile alerts**: Log once on Wood low/empty events (same threshold system as food/water)
- **DeprivationTimer expanded**: 4 needsAtZero slots (was 3); `stockpileEmpty` now also triggers
  on winter heat deprivation (no Wood fuel)
- **Death by cold**: DeathSystem handles Heat need at index 3; logs "Alice died of cold"

### Changed
- `Needs` array size: 3 → 4 (added Heat at index 3)
- `DeprivationTimer::needsAtZero` array: 3 → 4 slots; added `purchaseTimer` field
- `StockpileAlert` gained `woodLow`/`woodEmpty` fields
- Player left panel height increased to accommodate 4th bar

### Added (visual & behavioral polish)
- **Season sky tint**: Background color shifts per season — Winter is blue-shifted (icy),
  Autumn is warm orange-shifted, Spring has a slight green tint, Summer unchanged
- **Season event log color**: Season transition messages ("--- Winter begins ---") shown in SKYBLUE
- **Death by cold**: Event log highlights "cold" deaths in RED
- **NPC market purchasing**: When settlement stockpile runs out, NPCs with money
  automatically buy 1 unit of food/water from the local market every 2 game-hours.
  Gold flows to settlement treasury; creates economic feedback loop (wages → purchases → treasury)
- **Wider migration stagger**: NPC migration threshold widened from 2-5 hours to 1-10 hours,
  giving much more spread in when NPCs decide to leave (addresses wishlist: mass simultaneous exodus)
- **Heat in NPC tooltip**: Hover tooltip now shows `Ht:XX%` alongside H/T/E percentages
- **AgentEntry heatPct**: RenderSnapshot now carries heat% for each agent
- **Seasonal random events**: 3 new season-locked events:
  - *Blizzard* (Winter only): blocks ALL roads for 4 game-hours
  - *Spring Flood* (Spring only): destroys 40% of food at a settlement
  - *Harvest Bounty* (Autumn only): +50% production for 12 game-hours
- **Seasonal world status bar**: In Autumn/Winter, status bar shows `Wd:XX` (wood stock)
  instead of prices; wood stock shown in RED when < 20 in Winter
- **Temperature display**: Top-right panel shows ambient temperature (°C) — computed from
  season baseline + diurnal swing (colder at night, warmer midday). Color: icy blue when <0°C
- **NPC personality variation**: ±20% random variance on each NPC's need drain rates —
  some NPCs are naturally sturdier (drain slower), others more fragile (drain faster)
- **Debug overlay improvements**: Now shows Season and Temperature; overlay moved down to
  avoid overlap with Heat bar; panel widened
- **BLIZZARD/FLOOD in event log**: Colored RED; HARVEST BOUNTY colored GREEN

---

## [Simulation Depth] NPC names, aging, economy, player trading — 2026-04-10

Autonomous session adding significant simulation depth across multiple systems.

### Added
- **NPC names** (`Name` component): All NPCs, haulers, and player have names drawn from
  a pool of 30 first + 20 last names; births log "Born: Alice Smith at Greenfield";
  deaths log "Alice Smith died of hunger"; hover tooltip shows name prominently
- **NPC aging** (`Age` component): All agents have `days` and `maxDays` (60–100 game-days);
  DeathSystem advances age each tick and destroys entities on reaching max age;
  NPCs at spawn get randomised starting age (0–30 days) for immediate age distribution
- **Player aging**: Player has Age component, age shown in HUD panel (green→yellow→red);
  hover tooltip shows age with same color coding
- **Settlement collapse detection**: DeathSystem logs "X has COLLAPSED — population zero"
  once per collapse episode; fires recovery erase when pop > 0 again
- **Auto-respawn on player death**: After each sim batch, SimThread checks for missing
  PlayerTag; spawns new character at settlement with most food+water and logs the event
- **Player trading** (`T` key): Player can buy/sell goods at nearest settlement (140px range):
  - Empty inventory: auto-buys highest-profit tradeable good at market price, paying from wallet
  - Loaded inventory: sells all cargo at destination market price
- **Player money**: Player spawns with 50g (`Money` component) and 15-unit inventory
- **Gold + inventory HUD**: Player HUD left panel shows gold balance and cargo contents
  (color-coded: food=green, water=skyblue, wood=brown)
- **Settlement health ring**: Settlement circles color-coded by min(food, water) stock:
  green (>30), yellow (>10), red (<10), grey (pop=0/collapsed)
- **Production input requirements**: Farms consume 0.15 water per food unit produced,
  creating a supply-chain dependency; Greenfield must import water to sustain food production

### Fixed
- **Migration destination** (3-settlement bug): `FindMigrationTarget` now picks the
  reachable settlement with the highest combined food+water, not the first road endpoint
- **Probabilistic births** (per Q7): 25% chance roll per birth interval so population
  grows slowly and only when "NPCs decide to have a child"
- **Bandits event** now blocks one random open road, not all roads simultaneously

### Changed
- Greenfield starting water raised 20→80 to give haulers time to establish supply routes
- Key hint updated: `WASD:Move  B:Road  T:Trade  F:Follow  F1:Debug`
- Birth log: "Born: Alice Smith at Greenfield" instead of "Birth at Greenfield (pop N)"

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
