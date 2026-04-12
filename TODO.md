# ReachingUniversalis — Dev Backlog

**Priority: NPC behaviour depth and inter-NPC interaction — building toward a living world.**

Maintained by Claude Code cron. Each session picks the top `[ ]` task, implements it, commits,
marks it done, then appends 2–3 new concrete tasks to keep the queue full.

---

## In Progress

---

## Done

- [x] **Inter-settlement rivalry** — `Settlement::relations` (std::map<entt::entity,float>)
  updated per hauler delivery in TransportSystem: exporter +0.04, importer -0.04. Drifts toward
  0 at 0.3%/game-hour (RandomEventSystem settlement loop). Trade effects: rival (score < -0.5) →
  30% tax; ally (score > +0.5) → 15% tax. `Hauler::cargoSource` tracks cargo origin for return
  trips. Road tooltip shows colour-coded Relations line (RED=rivals, GREEN=allied, GRAY=scores).

- [x] **Work stoppage event** — `Settlement::strikeCooldown` (24h recharge) + `DeprivationTimer::strikeDuration`
  (6h per strike). In `RandomEventSystem::Update`'s settlement loop: 5% per game-day chance when
  morale < 0.3 and cooldown elapsed. Sets strikeDuration on all Schedule NPCs at that settlement,
  forces Idle. `ScheduleSystem` drains strikeDuration and blocks Idle→Working while > 0. Wages
  not charged automatically (ConsumptionSystem only pays Working NPCs). Logs striker count.

- [x] **Settlement morale** — `Settlement::morale` float (0–1) in Components.h. Rises on birth
  (+0.03), festival (+0.15). Falls on need-death (-0.08), old-age death (-0.02), theft (-0.05),
  drought (-0.10), blight (-0.12), plague (-0.20). Drifts toward 0.5 at ±0.5%/game-hour.
  ProductionSystem applies +10% when >0.7, -15% when <0.3 (unrest). Unrest crossing logged once
  in RandomEventSystem settlement loop. Morale bar replaces stability bar in StockpilePanel
  (RenderSystem); written via SimThread.

- [x] **NPC personal events** — Per-NPC event tier in `RandomEventSystem::Update` fires every
  12–48 game-hours per NPC (staggered at spawn). Events: skill discovery (+0.08–0.12 to a random
  skill), windfall (find 5–15g), minor illness (affected need drains 2× for 6h via `illnessTimer`
  + `illnessNeedIdx` on `DeprivationTimer`, applied in `NeedDrainSystem`), good harvest (1.5×
  worker contribution for 4h via `harvestBonusTimer` in `ProductionSystem`). Interesting events
  logged; illness and harvest timers drained in the per-NPC loop.

- [x] **Charity frequency counter in event log** — `static s_charityCount` map in `AgentDecisionSystem::Update`; pruned for dead entities; appends " (xN)" when N > 1.

- [x] **Charity recipient log detail** — Log now reads "X helped [Name] at [Settlement]." using `try_get<Name>` on recipient and `sett->name`. No new components.

- [x] **Exile on repeat theft** — `theftCount` int on `DeprivationTimer`; incremented each theft. At 3, `HomeSettlement` cleared. Logs "X exiled from Y for repeated theft."

- [x] **Charity radius shown on hover** — `charityReady` field in `AgentEntry`; faint `Fade(LIME, 0.2f)` circle (radius 80) drawn in `GameState::Draw` when hovering NPC with `hungerPct > 0.8`, `balance > 20g`, `charityReady`.

- [x] **Warmth glow shown in tooltip** — `recentWarmthGlow` in `AgentEntry`; set when `htp > 0.9 && charityTimer > 0`. "Warm from giving" in `Fade(ORANGE, 0.75f)` below gratitude line.

- [x] **Gratitude shown in tooltip** — `isGrateful` bool in `AgentEntry`; set from `gratitudeTimer > 0` in SimThread. "Grateful to neighbour" line in `Fade(LIME, 0.55f)` in tooltip, below "Fed by neighbour".

- [x] **Gratitude approach stops at polite distance** — Distance check before `MoveToward` in GRATITUDE block; `vel = 0` when within 25 units of helper.

- [x] **Event modifier label colour** — `ModifierColour()` helper in HUD.cpp; world status bar and settlements panel now tint by event type (Plague→RED, Festival→GOLD, etc.).

- [x] **Settlement anger on theft** — Skills penalty -0.02 per theft in `AgentDecisionSystem`; treasury deduction was already present. No new components.

- [x] **Theft indicator in tooltip** — `recentlyStole` field in `AgentEntry`; set when `stealCooldown > 46f`. Faint red `(thief)` suffix on tooltip line1 in `HUD::DrawHoverTooltip`.

- [x] **Theft from stockpile** — NPCs with `money.balance < 5g` and `stealCooldown == 0` steal
  1 unit of their most-needed resource from their home `Stockpile`. Market price deducted from
  `Settlement::treasury`. `stealCooldown = 48h`. Logs "Mira stole food from Ashford."

## Backlog

### NPC Lifecycle & Identity

### NPC Social Behaviour

### NPC Crime & Consequence

- [x] **Migration memory** — `MigrationMemory` component in Components.h: `std::map<string,
  PriceSnapshot>` capped at 12 entries. Seeded at spawn with home prices. Updated on migration
  departure and arrival. Gossip exchanges now also update both parties' memories. In
  `FindMigrationTarget`: +20% score if destination food cheaper in memory, +10% if water cheaper.

- [x] **Personal goal system** — `GoalType` enum + `Goal` component (progress/target/celebrateTimer)
  in Components.h. Assigned at spawn in `WorldGenerator::SpawnNPCs`. Goal progress checked in
  `AgentDecisionSystem`; on completion: log event, 2h `Celebrating` state, assign new goal.
  SaveGold → doubled purchase interval in `ConsumptionSystem`. BecomeHauler → +10% worker
  contribution in `ProductionSystem`. Celebrating block extended for personal celebrations.

- [x] **Bandit NPCs from desperation** — `BanditTag` struct in Components.h; `banditPovertyTimer`
  in `DeprivationTimer`. Exiles with balance < 2g for 48+ game-hours get `BanditTag`. Bandits lurk
  near nearest Road midpoint and steal 30% of hauler cargo (3g/unit). Player presses E within 80
  units to confront: recovers 50% of bandit gold, +10 rep, removes tag. Dark maroon render color.
  "Bandit (press E to confront)" in tooltip.

### NPC Memory & Goals

### Settlement Social Dynamics

- [ ] **NPC age display in tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), after the role line,
  add an age line: "Age: 23" (integer days). Read `AgentEntry::age` (already in the struct as a
  float). For children, show "Age: 8 (child)". This gives the player immediate lifecycle context
  without opening extra panels.

- [ ] **Deathbed log with age** — In `DeathSystem.cpp`, the death log currently says "Died: Aldric
  Smith (old age) at Ashford". Append the NPC's age: "Died: Aldric Smith at age 72 (old age) at
  Ashford". Find where `maxDays` is checked, read `age->days` cast to int, and include it in the
  `log->Push` call. Also apply to hunger/thirst/heat deaths: "Died: Mira Reed (hunger) age 31 at
  Ashford".

- [ ] **Richest NPC highlighted in stockpile panel** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), the agent list already renders NPC names. After building the list, find the
  agent with the highest `money` field in `StockpilePanel::agents` and render their name in gold
  `GOLD` colour instead of white. No new component needed — use the existing `money` float in
  `AgentEntry`. Makes wealth inequality immediately legible.

- [ ] **Family size in tooltip** — Extend the `(Family: X)` suffix added in `DrawHoverTooltip`
  (HUD.cpp) to include the family size: change to "(Family: X ×N)" where N is the `surnameCount`
  value for that surname. This tells the player at a glance how large a dynasty has grown without
  needing extra UI. Just pass the count integer into the snprintf format string.

- [ ] **NPC mood colour on world dot** — In `GameState.cpp`, agent dots are currently all WHITE.
  Tint the dot colour by the NPC's contentment: `contentment >= 0.7` → `GREEN`, `>= 0.4` →
  `YELLOW`, `< 0.4` → `RED`. Use `AgentEntry::contentment` (already in the snapshot). Children and
  haulers keep their existing colour logic. This makes settlement health instantly readable from the
  overworld view.

- [ ] **Settlement name in event log** — In `RandomEventSystem.cpp`, random events like "Drought at
  Ashford" currently emit to the global EventLog with just the settlement name in the string. Add
  the settlement's current population in brackets: "Drought at Ashford [pop 12]". Read from
  `Settlement::` component and `popCount` computed locally. Helps the player gauge event severity.

- [ ] **Child count in stockpile tooltip** — In `HUD::DrawSettlementTooltip` (HUD.cpp, the tooltip
  shown when hovering a settlement dot), add a "Children: N" line when `childCount > 0`. Read
  `SettlementStatus::childCount` from the `worldStatus` entry that matches the hovered settlement
  name. Display it in faded LIGHTGRAY below the population line. No new components needed.

- [ ] **Hunger crisis indicator in world status** — In `DrawWorldStatus` (HUD.cpp), if any NPC at
  a settlement has `hungerPct < 0.15f` (near starvation), add a small "!" warning after the food
  stock. Track via a new `bool hungerCrisis` field in `SettlementStatus` (RenderSnapshot.h) set
  in SimThread's world-status loop using `m_registry.view<HomeSettlement, Needs>`. Draw the "!"
  in RED tint immediately after the food number.

- [ ] **Population cap shown in stockpile panel header** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), the header currently shows "[12/35 pop, 3 child]" (popCap already wired in).
  Verify `StockpilePanel::popCap` is being written by SimThread (check WriteSnapshot around the
  panel block), and if the cap is being hit (pop >= popCap - 2), tint the pop number in ORANGE to
  signal crowding. No new fields needed — just a colour change based on existing data.

- [ ] **Wandering orphan re-settlement** — Children with `ChildTag` and no `HomeSettlement`
  (orphans) should autonomously seek a new home. In `AgentDecisionSystem`, after the existing
  migration logic, add a check: if the entity has `ChildTag`, `!HomeSettlement` (or
  `hs.settlement == entt::null`), and is within 200 units of any settlement with available pop
  capacity, assign that settlement as their new `HomeSettlement`. Log: "Orphan Aldric found a
  new home at Thornvale."

- [ ] **Collapse cooldown — settlement ruins** — After a settlement collapses (enters
  `m_collapsed` in DeathSystem), set a `Settlement::ruinTimer = 300.f` game-hours. While
  `ruinTimer > 0`, the settlement cannot have new births (check in `BirthSystem`) and the
  settlement dot renders in DARKGRAY in `GameState.cpp`. Drain `ruinTimer` by `gameDt` in
  `DeathSystem`. Gives a natural recovery period before repopulation.

- [ ] **Orphan count in collapse log** — In `DeathSystem`'s orphan-scatter block, include the
  orphan count in both log messages: change "Orphaned children of Ashford scattered." to
  "3 children of Ashford orphaned and scattered." This requires counting orphans before the
  loop (or using the already-counted `orphanCount` to format the string more expressively).

- [ ] **Apprentice tooltip badge** — In `HUD::DrawHoverTooltip` (HUD.cpp), after the role/name
  line, when an NPC has `ChildTag` AND `ageDays >= 12` (check `AgentEntry::ageDays`), append
  " [Apprentice]" in dim yellow to the role label, or as a separate line below the "Child" label.
  No new components or snapshot fields needed — `ageDays` is already in `AgentEntry`.

- [ ] **Graduation announcement shows skill** — In `ScheduleSystem.cpp`'s graduation block,
  after the "came of age" log is pushed, also append the new adult's highest skill and its value:
  "Aldric Smith came of age at Ashford (raised by Brom Cooper) — best skill: Farming 38%".
  Read the `Skills` component just before removing `ChildTag`; find the highest value and its
  label. No new components needed.

- [ ] **Elder count in settlement tooltip** — In `HUD::DrawSettlementTooltip` (HUD.cpp), after
  the population line, add an "Elders: N (+X% prod)" line when any elders are present at the
  settlement. Add `int elderCount = 0` and `float elderBonus = 0.f` to `SettlementStatus` in
  `RenderSnapshot.h`; populate in SimThread's world-status loop using `age->days > 60.f`; display
  in the tooltip in grey with the bonus percentage.

- [ ] **Elder deathbed savings inheritance** — In `DeathSystem.cpp`, when an elder (age > 60)
  dies of old age, increase the inheritance fraction from 0.5 to 0.8 (elders have more time to
  accumulate and bequeath wealth). Add an `isElder` check in the inheritance block and use 0.8f
  instead of `INHERITANCE_FRACTION`. Log: "Aldric Smith left an estate of 45g to Ashford."

- [ ] **Profession change on migration** — When an NPC arrives at a new settlement (migration
  complete in `AgentDecisionSystem`), update their `Profession` component to match the new
  settlement's primary output facility. Use the same `ProfessionForResource` helper from
  Components.h. This reflects NPCs adapting to their new community's trade over time.

- [ ] **Profession shown in stockpile panel NPC list** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), the NPC list currently shows name, state, and gold. After the name, append
  a short profession abbreviation in grey: "Fa" = Farmer, "Wa" = Water Carrier, "Lu" = Lumberjack,
  "Me" = Merchant. Read from `AgentEntry::profession` (already populated from the Profession
  component). No new fields needed.

- [ ] **Profession-based work speed bonus** — In `ScheduleSystem.cpp`, when a Working NPC is
  at their skill-matched facility, check if their `Profession` type matches the facility output
  (via `ProfessionForResource`). If so, apply a 10% skill gain bonus: multiply
  `SKILL_GAIN_PER_GAME_HOUR` by 1.1f for that tick. This rewards NPCs who are both skilled
  AND identify with their profession.

- [ ] **Morning departure scatter** — Between hours 7–8 (work start), NPCs leaving sleep should
  scatter slightly from the settlement centre rather than all heading to the same facility at once.
  In `AgentDecisionSystem` (or `ScheduleSystem`), when transitioning from Sleeping to Idle at
  wake time, nudge the NPC's position by a small random offset (±30 units) so they don't
  all path-find from the exact same spot. No new component needed.

- [ ] **NPC idle chat radius** — When two NPCs of the same `HomeSettlement` are both `Idle` and
  within 25 units of each other during hours 18–21 (evening gathering), briefly stop both
  (`vel = 0`) for 30–60 game-seconds to simulate chatting. Track with a `chatTimer` float on
  `DeprivationTimer` (already has spare fields). After the timer expires, resume normal gathering
  movement. Implement in `AgentDecisionSystem` after the evening gathering block.

- [ ] **Family dissolution on death** — In `DeathSystem.cpp`, when an NPC with `FamilyTag` dies,
  check if their partner (the other `FamilyTag`-holder with the same name at the same settlement)
  is still alive. If not (both partners gone), remove `FamilyTag` from all surviving children
  (age < 15 `ChildTag` entities with the same family name at that settlement) so they can form
  new families later. Log "The [name] family line has ended at [settlement]." when the last adult
  member dies. Implement in the existing death-cleanup block.

- [ ] **Family size in HUD stockpile list** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), after drawing each NPC name, if their `AgentEntry::familyName` is non-empty,
  count how many other agents at that settlement share the same `familyName` and append " ×N" in
  dim color (DARKGRAY) when N ≥ 2. E.g. "Aldric Smith ×3" shows there are 3 members of the Smith
  family. Requires adding `familyName` to `StockpilePanel::AgentInfo` — check the existing struct
  and populate it in SimThread's WriteSnapshot near the StockpilePanel block.

- [ ] **Skill degradation with age** — In `NeedDrainSystem.cpp` (or `ScheduleSystem.cpp`), when
  an NPC's age passes 65, slowly degrade all three `Skills` values at 0.0002 per game-hour
  (`farming`, `water_drawing`, `woodcutting`). Cap the decay so skills can't fall below 0.1
  (elders retain some tacit knowledge). No new component needed — `Skills` and `Age` already
  exist. This creates an economic lifecycle: NPCs peak in middle age, then their output contribution
  falls as the elder bonus fades and their skill degrades.

- [ ] **Relationship pair memory** — Add a lightweight `Relations` component: `struct Relations {
  std::map<entt::entity, float> affinity; }`. In `AgentDecisionSystem`, when two idle same-settlement
  NPCs are within 25 units (evening gathering), increment their mutual affinity by 0.02 per tick
  (capped at 1.0). Affinity above 0.5 means "friend": friends share food charity (threshold reduced
  from 5g to 1g for the helping-neighbour check), and when one migrates, the other has a 30% chance
  to follow. Log "Aldric and Mira left Ashford together." Decay affinity by 0.001/game-hour when
  apart. No UI needed yet — the effects on migration and charity are the observable outcome.

- [ ] **NPC rumour propagation via gossip** — Extend the existing gossip system
  (`AgentDecisionSystem.cpp`). Add a `Rumour` component: `enum RumourType { PlagueNearby,
  GoodHarvest, BanditRoads }` with a `hops` int (decrements per gossip exchange) and an `origin`
  settlement entity. When `RandomEventSystem` fires a plague or drought, attach a `Rumour` to 1–2
  NPCs at that settlement. During gossip exchanges, spread the rumour to the other NPC (if their
  settlement doesn't already have it and `hops > 0`). When a rumour arrives at a settlement,
  nudge that settlement's relevant stockpile fear: plague → food hoarding (+10% Food price at
  Market), drought → water scarcity (+15% Water price). Log "Rumour of plague reached Thornvale."

- [ ] **Illness visible in tooltip** — When an NPC has `depTimer->illnessTimer > 0`, add
  `bool ill = true` and `int illNeedIdx` to `AgentEntry` in `RenderSnapshot.h`, populated in
  SimThread's agent snapshot loop. In `HUD::DrawHoverTooltip`, draw a faint red "(ill: hunger)"
  / "(ill: thirst)" / "(ill: fatigue)" suffix on the needs line. This makes personal events
  player-visible without requiring any new components.

- [ ] **Windfall source context in log** — In `RandomEventSystem`'s per-NPC event loop, when a
  windfall fires (case 1), also log the NPC's current `HomeSettlement` name so the log reads
  "Aldric Smith found 12g on the road near Greenfield" instead of without context. Use
  `registry.try_get<HomeSettlement>(e)` and `registry.try_get<Settlement>(hs->settlement)` to
  get the name. Requires no new components.

- [ ] **Harvest bonus glow on worker dot** — When an NPC has `harvestBonusTimer > 0`, set a
  `bool harvestBonus` flag in `AgentEntry` (RenderSnapshot.h), populated in SimThread's snapshot
  loop via `try_get<DeprivationTimer>`. In `GameState.cpp`'s agent render loop, draw a small
  `Fade(GOLD, 0.4f)` ring (radius 10) around workers with the bonus active. This makes good
  personal events legible in the overworld view.

- [ ] **Morale shown in world status bar** — Add `float morale` to `SettlementStatus` in
  `RenderSnapshot.h`; populate it in SimThread's world-status loop with `s.morale`. In
  `HUD::DrawWorldStatus` (HUD.cpp), after the food/water/wood numbers, render a small coloured
  "M:XX%" label using the same green/yellow/red colour logic as the panel bar. Gives a per-
  settlement morale glance without opening the stockpile panel.

- [ ] **Morale impact from hauler trade success** — In `TransportSystem.cpp`, when a hauler
  successfully delivers cargo to the destination settlement (the `GoingToDeposit → GoingHome`
  transition), call `settl->morale = std::min(1.f, settl->morale + 0.01f)` on the *destination*
  settlement. Trade arriving boosts community confidence. Use `registry.try_get<Settlement>` on
  `hauler.targetSettlement`. Small per-delivery tick so active trade routes gradually lift morale.

- [ ] **Morale recovery from full stockpiles** — In `RandomEventSystem::Update`'s per-settlement
  loop (where morale drift already runs), add: if all three stockpiles (food, water, wood) are
  above 80 units, apply an extra +0.002 morale per game-hour. This rewards players who maintain
  supply surpluses and gives morale a meaningful second recovery path beyond just waiting for
  the drift. Read `registry.try_get<Stockpile>(e)` in the same settlement loop.

- [ ] **Strike indicator in tooltip** — When an NPC has `strikeDuration > 0` (from `DeprivationTimer`),
  set a `bool onStrike` field in `AgentEntry` (RenderSnapshot.h), populated in SimThread's agent
  snapshot loop. In `HUD::DrawHoverTooltip` (HUD.cpp), add a faint orange "(on strike)" suffix
  after the behavior/state line. This lets the player see which workers are currently refusing
  to work without digging into the event log.

- [ ] **Work stoppage morale recovery** — After a work stoppage completes (strikeDuration drains
  to 0 in `ScheduleSystem.cpp`), give a small morale nudge: `+0.05` to the home settlement's
  morale. This models grievances being aired and partially resolved — the act of striking itself
  slightly relieves tension. Check `dt->strikeDuration` transitioning from > 0 to 0 in the drain
  block and call `registry.try_get<Settlement>(home.settlement)->morale += 0.05f`.

- [ ] **NPC age display in tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), after the role line,
  add an age line: "Age: 23" (integer days). Read `AgentEntry::ageDays` cast to int. For children
  (`ageDays < 15`), show "Age: 8 (child)". For elders (`ageDays > 60`), show "Age: 63 (elder)".
  No new snapshot fields needed — `ageDays` is already in `AgentEntry`.

- [ ] **Rivalry log events** — In `RandomEventSystem::Update`'s settlement loop (where relations
  drift already runs), add threshold-crossing logs. When `A.relations[B]` crosses below -0.5 for
  the first time, log "RIVALRY: X and Y relations deteriorate — tariffs imposed (+10%)". When it
  rises above -0.3 (recovery), log "Relations improving between X and Y". Use a similar `bool`
  crossing approach as `Settlement::unrest`. This makes rivalry formation a visible story beat.

- [ ] **Alliance bonus shown in road tooltip** — When two settlements are allied, also boost the
  road's arbitrage rate in `PriceSystem.cpp`. In the per-road arbitrage loop, check
  `sA->relations.find(road.to)` and `sB->relations.find(road.from)`; if both scores > 0.5, multiply
  `convergeFrac` by 1.5 (prices converge 50% faster on allied trade routes). Add a tooltip note
  "Allied: faster price convergence" in `HUD.cpp DrawRoadTooltip` when the alliance line is shown.

- [ ] **Rival hauler harassment** — When a hauler from settlement A (home) arrives at rival
  settlement B (where B.relations[A] < -0.5), add a random 20% chance the delivery is "taxed at
  the gate": reduce the hauler's `earned` by an extra 10% and credit B's treasury. Track this in
  `TransportSystem.cpp` right after the `effectiveTax` block. Log "Hauler from X taxed at gate
  in Y (rivalry tariff)." at low probability to avoid log spam (1 in 5 deliveries).

---

## Done

- [x] **Festival NPC count in event log** — Added `celebrantCount` int incremented in the
  existing Celebrating-setter loop in `RandomEventSystem`'s festival case. Log message updated
  to "FESTIVAL at Ashford — 12 celebrating, treasury +120g, production +35% (16h)". Buffer
  widened to 128.

- [x] **Festival dot colour** — In `GameState.cpp`'s settlement render loop, added Festival
  override after the Plague override: `modifierName == "Festival"` → ring = `Fade(GOLD, 0.85f)`
  (selected stays YELLOW). Same 2-line pattern as Plague. No new fields.

- [x] **Festival NPC colour** — In `GameState.cpp`'s agent draw loop, compute `drawColor` at
  render time: if `AgentEntry::behavior == Celebrating`, use `Fade(GOLD, 0.85f)` instead of
  `a.color`. No new snapshot fields. Reverts automatically when festival ends.

- [x] **Festival: interrupt critical needs** — Inside CELEBRATING's festival-active branch in
  `AgentDecisionSystem`, added critical-need check (same as WORKING block). If any need is below
  `criticalThreshold`, sets `behavior = Idle` and falls through; NPC re-enters Celebrating next
  tick automatically when needs recover (festival modifier still active on settlement).

- [x] **Celebration behaviour** — Added `AgentBehavior::Celebrating` to enum and `BehaviorLabel`.
  `RandomEventSystem` sets all non-hauler/player NPCs at a festival settlement to Celebrating.
  `AgentDecisionSystem` CELEBRATING block moves them toward centre at 50% speed; auto-reverts
  to Idle when `Settlement::modifierName` is no longer "Festival". `NeedDrainSystem` applies 0.5×
  drain multiplier while celebrating.

- [x] **Charity warmth modifier** — In `AgentDecisionSystem`'s charity block, after setting the
  helper's cooldown, reads `Needs` component and bumps `needs.list[(int)NeedType::Heat].value`
  by `+0.15`, capped at 1.0. Single write, no new component or field.

- [x] **Helped-NPC gratitude follow** — Added `gratitudeTarget` (entt::entity) and
  `gratitudeTimer` (float, real-seconds) to `DeprivationTimer`. Charity block sets random 30–60s
  timer and stores helper entity. New GRATITUDE block in main behaviour loop (before migration
  check): if active, NPC moves toward helper at 70% speed and skips all other decisions. Clears
  on expiry or if helper is destroyed. Added `<random>` include to AgentDecisionSystem.

- [x] **Charity shown in tooltip** — Added `helpedTimer` float to `DeprivationTimer`. Set to 1.f
  (game-hour) on charity receiver in `AgentDecisionSystem`; drained each frame. `SimThread` reads
  `helpedTimer > 0` → `AgentEntry::recentlyHelped`. `HUD` tooltip shows a dim lime "Fed by
  neighbour" extra line when set; disappears automatically after 1 game-hour.

- [x] **NPC helping starving neighbours** — Added `charityTimer` to `DeprivationTimer`. In
  `AgentDecisionSystem`, well-fed NPCs (Hunger > 0.8, Money > 20g, cooldown 0) scan within 80
  units for starving NPCs (Hunger < 0.2). On match: 5g transfers peer-to-peer, then immediate
  market food purchase (gold → treasury, food to stockpile). purchaseTimer reset. Cooldown 24
  game-hours per helper. Logs "X helped a starving neighbour."

- [x] **Named families** — Added `FamilyTag { std::string name; }` to Components.h. Every 12
  game-hours, AgentDecisionSystem pairs unpaired adults (age ≥ 18, same settlement) two-by-two,
  assigning the most common settlement surname as the family name. BirthSystem inherits the dominant
  FamilyTag onto newborns and twins. HUD tooltip shows "(Family: X)" from FamilyTag if set,
  falling back to the surname-count heuristic for untagged NPCs.

- [x] **Gossip / price sharing** — AgentDecisionSystem.cpp: after main NPC loop, builds a
  `GossipEntry` snapshot of all non-hauler/player NPCs with valid home settlements. O(N²) pair check
  within 30 units; different-settlement pairs only. On match: each home `Market` nudges 5% toward
  the other's prices. Cooldown 6 game-hours on `DeprivationTimer::gossipCooldown`. Each NPC gossips
  with at most one stranger per cooldown window.

- [x] **Parent–child naming** — ScheduleSystem graduation block extracts parent's last name from
  `raisedBy` (rfind ' ') and replaces the NPC's last name with it before the log reads `who`.
  Log now reads "Aldric Cooper came of age at Ashford (raised by Brom Cooper)".

- [x] **Graduation log improvement** — ScheduleSystem graduation block now reads
  `AgentState::target → Name` before removing ChildTag and appends "(raised by X)" when
  a followed adult is found. Log now reads e.g. "Aldric Smith came of age at Ashford (raised by Brom Cooper)".

- [x] **Child follow indicator in tooltip** — `followingName` added to `AgentEntry`. SimThread
  WriteSnapshot resolves `AgentState::target → Name` for Child entities. HUD tooltip shows
  "Following: Aldric Smith" in sky-blue when present.

- [x] **Child HUD visibility** — `AgentRole::Child` added to enum. WriteSnapshot sets it for
  `ChildTag` entities. GameState skips ring draw for children (plain small dot). Stockpile panel
  header shows child count when > 0. HUD tooltip shows "Child" role label.

- [x] **Evening gathering** — AgentDecisionSystem: extracted `currentHour` from TimeManager at
  top of Update(). In the no-critical-needs branch (critIdx==-1), hours 18–21 NPCs with a valid
  home settlement move toward its centre at 60% speed (stop within 40 units). No new component.

- [x] **Profession identity** — Added `ProfessionType` enum and `Profession` component to
  Components.h with helpers. WorldGenerator assigns profession at spawn per settlement type.
  BirthSystem detects settlement primary facility and emplaces Profession on newborns/twins.
  AgentDecisionSystem: +15% migration score bonus when profession matches destination output
  (stacks with existing +20% skill affinity). EconomicMobilitySystem sets Profession::Hauler
  on graduation. SimThread reads from component instead of inferring each frame.

- [x] **Elder retirement** — ScheduleSystem: age > 60 sets `workEligible=false` (full retirement).
  ConsumptionSystem: `isElder` guard blocks wages; elders drain own `Money` at 0.1g/game-hour.
  ProductionSystem: `elderCount` map; each elder at home = +0.5% production bonus, capped +5%.

- [x] **Child work apprenticeship** — ScheduleSystem: age 12–14 children get `isApprentice` flag;
  they enter Working state during hours 10–12 only, still follow adults at leisure. Skill passive
  growth is 2× when Working. ProductionSystem: apprentices contribute 0.2 worker-equivalents
  (`workers` map changed from `int` to `float`). ConsumptionSystem wage guard unchanged.

- [x] **Child abandonment on settlement collapse** — DeathSystem settlement-collapse block now
  iterates over ChildTag entities at the collapsed settlement, clears their HomeSettlement and
  nulls AgentState::target so they become wanderers. Logs "Orphaned children of X scattered."

- [x] **Child count in world status bar** — Added `childCount` to `SettlementStatus` in
  `RenderSnapshot.h`. SimThread passes `childPop` (from its existing pop-counting loop) into
  `worldStatus`. `DrawWorldStatus` draws a faded `(Nc)` suffix after each settlement's status.

- [x] **Surname tooltip** — HUD::DrawHoverTooltip builds a surname→count map from the agents
  snapshot once before the hit-test loop. If the hovered NPC's surname appears on 2+ agents,
  appends "  (Family: Surname)" to line1. line1 expanded to 128 chars to fit the suffix.

- [x] **Birth announcement names parent** — BirthSystem.cpp finds the wealthiest adult (highest
  `Money.balance`) at the settlement at birth time and appends "(to Name)" to the birth log message.
  Applied to both single births and twins. Uses a simple view loop over `HomeSettlement, Money, Name`.

- [x] **Family surname on birth** — BirthSystem.cpp scans adult residents' surnames at each
  settlement, picks the most common one, and 50% of the time assigns it as the newborn's last name.
  Twins share the same familySurname as their sibling. Builds visible family clusters over time.

- [x] **Child → Adult lifecycle** — `ChildTag` added; BirthSystem emits it for newborns/twins.
  ScheduleSystem: children follow nearest adult at home settlement during leisure hours (target
  cached on `AgentState::target`). At age 15: `ChildTag` removed, skill boosted toward home
  settlement's primary production, "came of age" logged. ConsumptionSystem: age < 15 wage guard.

---

## Session Rules (read every cron run)

1. Pick the **top** `[ ]` task. Move it under "In Progress" before starting.
2. Read `CLAUDE.md` for architecture context before editing. Follow the Gold Flow Rule.
3. Read the relevant source files before writing any code.
4. `bash build.sh && bash test.sh 10` before committing. Fix build errors before moving on.
5. Commit with a clear message referencing the task name.
6. Mark done `[x]`, move to Done section, commit `TODO.md`.
7. Append **2–3 new concrete NPC/living-world tasks** to Backlog (name the file/struct/system).
8. Use `AskUserQuestion` only if genuinely blocked. Don't ask about minor design choices — make
   a reasonable call and note it in the commit message.

- [ ] **Theft indicator in tooltip** — Surface recent theft in the NPC tooltip. Add `bool recentlyStole = false`
  to `AgentEntry` in `RenderSnapshot.h`; set it when `stealCooldown > 46.f` (within 2 game-hours of a theft).
  In `SimThread::WriteSnapshot`, populate from `DeprivationTimer::stealCooldown`. In `HUD::DrawHoverTooltip`,
  when `recentlyStole`, append a faint RED " (thief)" suffix to line1. Mirror the `familyName` pattern already there.

- [ ] **Skills penalty on theft** — When a theft fires in `AgentDecisionSystem`, read the thief's `Skills`
  component via `registry.try_get<Skills>`, and reduce all three skill floats (farming, water_drawing,
  woodcutting) by 0.02 each, clamped at 0. This models social ostracism without new components.

- [ ] **Thief flees home after stealing** — After a successful theft, set the NPC's velocity away from the
  settlement centre for 3–5 real seconds (use a new `fleeTimer` float in `DeprivationTimer`, or reuse
  `helpedTimer` as a flee flag). In `AgentDecisionSystem`, when `fleeTimer > 0`, move away from home pos
  at full speed. This makes theft visible: a dot sprinting away from the settlement dot.

- [ ] **Thief dot colour on world map** — When `recentlyStole` is true, tint the NPC's world-map
  dot with a dark red: `Fade(MAROON, 0.9f)`. In `GameState.cpp`, the agent draw loop already
  applies `Fade(GOLD, 0.85f)` for Celebrating; add an `else if (a.recentlyStole)` branch before
  the default color assignment. Use `AgentEntry::recentlyStole` (already in the snapshot). Makes
  thieves visible on the map for ~2 game-hours after stealing.

- [ ] **Settlement theft count in stockpile panel** — Track how many times NPCs have stolen from a
  settlement. Add `int theftCount = 0` to `Settlement` component in `Components.h`; increment it
  in `AgentDecisionSystem`'s theft block each time a theft succeeds. In `SimThread::WriteSnapshot`,
  populate a new `int theftCount = 0` in `StockpilePanel`. In `RenderSystem::DrawStockpilePanel`,
  show "Thefts: N" below the treasury line in faint RED when `theftCount > 0`.

- [ ] **NPC grudge after being stolen from** — When an NPC's charity gift or help is followed by a
  theft from the same entity (helper's `helpedTimer > 0` and the helped entity steals), log a
  social event: "Aldric saw through Mira's gratitude." Implement in `AgentDecisionSystem`'s theft
  block: after confirming a theft, check all nearby NPCs (within 80 units) who have
  `gratitudeTarget == thief entity`; clear their `gratitudeTimer` and log the message. Purely
  social flavour — no new components.

- [ ] **Theft log includes skill level** — Extend the theft log message in `AgentDecisionSystem`'s
  theft block to include the thief's relevant skill after the penalty: change "Mira stole food from
  Ashford." to "Mira stole food from Ashford (skill −0.02 → 23%)." Read `sk->farming` after
  applying the penalty, pick the skill matching `stealRes` (farming→Food, water→Water,
  woodcutting→Wood), format as integer percent. Only append the skill suffix when the NPC has a
  `Skills` component.

- [ ] **Skill degradation with age** — In `NeedDrainSystem.cpp`, after the need-drain loop, add
  a second loop over `registry.view<Skills, Age>()`. When `age.days > 65.f`, reduce all three
  skills by `0.0002f * gameDt` per tick, clamped at `0.1f` minimum. This creates an economic
  lifecycle: NPCs peak mid-life, then gradually reduce output as elders. No new components needed.

- [ ] **NPC idle chat radius** — When two NPCs of the same `HomeSettlement` are both `Idle` and
  within 25 units during hours 18–21, briefly stop both (`vel = 0`) for 30–60 game-seconds.
  Track with a `chatTimer` float on `DeprivationTimer`. After the timer expires, resume normal
  gathering movement. Implement in `AgentDecisionSystem` after the evening gathering block.

- [ ] **Event colour in minimap ring** — The minimap (`HUD::DrawMinimap` in HUD.cpp) draws a
  `Fade(YELLOW, 0.8f)` ring around settlements with active modifiers (line ~1087). Use
  `ModifierColour(s.modifierName)` instead so the ring colour matches the event type
  (Plague→RED ring, Festival→GOLD ring, etc.). One-line change; requires including the already-
  defined `ModifierColour` helper (already in the same file).

- [ ] **Event log entry colour coding** — In `HUD::DrawEventLog` (HUD.cpp), all log entries are
  currently drawn in `WHITE`. Scan each entry's `text` field for known modifier keywords and tint
  accordingly: entries containing "plague" or "PLAGUE" → `Fade(RED, 0.9f)`, "festival" or
  "FESTIVAL" → `Fade(GOLD, 0.85f)`, "drought" or "DROUGHT" → `Fade(ORANGE, 0.85f)`, "stole" →
  `Fade(RED, 0.7f)`, "died" or "Died" → `Fade(GRAY, 0.8f)`, others → WHITE. No new fields; use
  `entry.text` which is already in `EventLog::Entry`.

- [ ] **Gratitude approach stops at polite distance** — In `AgentDecisionSystem`'s GRATITUDE
  block, after computing the target position, if the NPC is within 25 units of the gratitude
  target set `vel.vx = vel.vy = 0.f` (polite stop) but still decrement `gratitudeTimer` and
  `continue`. This prevents NPCs clipping through their helper — they stand nearby for the
  remainder of the gratitude window.

- [ ] **Gratitude shown in tooltip** — When `gratitudeTimer > 0`, surface it in the NPC tooltip.
  In `SimThread::WriteSnapshot`, add `bool isGrateful = false` to `AgentEntry` alongside
  `recentlyHelped`; set from `dt->gratitudeTimer > 0.f`. In `HUD::DrawHoverTooltip`, when
  `isGrateful`, show a "Grateful to neighbour" line in `Fade(LIME, 0.75f)` (below "Fed by
  neighbour" if also present). Add it to `lineCount` and the `pw` max-width calculation —
  mirrors the existing `showHelped` pattern exactly.

- [ ] **Charity cooldown shown in tooltip** — When an NPC has `charityTimer > 0` (recently gave
  charity), add a "Gave charity (Xh ago)" line to the hover tooltip in `HUD::DrawHoverTooltip`.
  Add `float charityTimerLeft = 0.f` to `AgentEntry` in `RenderSnapshot.h`; populate from
  `dt->charityTimer` in `SimThread::WriteSnapshot`. In the tooltip, show the line only when
  `charityTimerLeft > 0`; format hours remaining. Same `lineCount`/`pw` pattern as `showHelped`.

- [ ] **Gratitude shown in world dot** — When `gratitudeTimer > 0`, give the NPC a subtle visual
  cue on the world map. In `GameState.cpp`'s agent draw loop, add a small faint LIME ring around
  the dot: after drawing the main dot, if `a.isGrateful` (add this field to `AgentEntry` via
  the Gratitude tooltip task), call `DrawCircleLines` with radius `a.size + 2` in
  `Fade(LIME, 0.5f)`. Only draw when role is NPC (not hauler, player, child).

- [ ] **Gratitude shown in world dot** — While `isGrateful` is true, draw a faint LIME ring
  around the NPC's world-map dot. In `GameState.cpp`'s agent draw loop (where `Fade(GOLD, 0.85f)`
  is used for Celebrating), after drawing the main dot, add: if `a.isGrateful && a.role ==
  RenderSnapshot::AgentRole::NPC`, call `DrawCircleLines((int)wx, (int)wy, a.size + 2.f,
  Fade(LIME, 0.5f))`. `isGrateful` is already in `AgentEntry` from this task.

- [ ] **Warmth glow shown in tooltip** — Mirrors "Fed by neighbour". Add `bool recentWarmthGlow`
  to `AgentEntry` in `RenderSnapshot.h`; set in `SimThread::WriteSnapshot` when
  `needs.list[(int)NeedType::Heat].value > 0.9f` AND `dt->charityTimer > 0.f`. In
  `HUD::DrawHoverTooltip`, when `recentWarmthGlow`, add a "Warm from giving" line in
  `Fade(ORANGE, 0.75f)`. Add to `lineCount` and `pw` max — same pattern as `showHelped`.

- [ ] **Charity recipient log detail** — Extend the charity log in `AgentDecisionSystem`'s
  charity block: change "X helped a starving neighbour." to "X helped [Name] at [Settlement]."
  After finding the starving NPC, call `registry.try_get<Name>` on `starving.entity` for their
  name and `registry.try_get<Settlement>` on the starving NPC's `HomeSettlement::settlement`
  for the settlement name. No new components — just expand the `charityLog->Push` format string.

- [ ] **Charity radius shown on hover** — When the player hovers an NPC who `canHelp`
  (hungerPct > 0.8, balance > 20g, charityTimer == 0 i.e. `recentWarmthGlow` is false and
  `recentlyHelped` is false as a proxy), draw a faint dim circle of radius 80 around them
  in `GameState.cpp`'s agent draw loop. Use `DrawCircleLinesV` in `Fade(LIME, 0.2f)`. Only
  draw for the hovered NPC (compare world-mouse position to agent position within `a.size + 8`).

- [ ] **Charity recipient log detail** — In `AgentDecisionSystem`'s charity block, after finding
  the starving NPC, extend the log: change "X helped a starving neighbour." to "X helped [Name]
  at [Settlement]." Read `registry.try_get<Name>` on `starving.entity` and
  `registry.try_get<Settlement>` on `starving.homeSettl`. No new components needed.

- [ ] **NPC mood colour on world dot** — In `GameState.cpp`'s agent draw loop, tint NPC dots by
  contentment: `contentment >= 0.7` → `WHITE` (current), `>= 0.4` → `YELLOW`, `< 0.4` → `RED`.
  Use `AgentEntry::contentment` (already in snapshot). Only apply when role is NPC and the dot
  isn't already overridden by Celebrating (GOLD) or distress color logic in SimThread. The
  contentment tint replaces the need-distress color that SimThread already computes, so verify
  the two don't conflict — SimThread sets drawColor based on worst need; the render loop in
  GameState can layer a contentment tint on top via `ColorAlphaBlend` or just gate on `a.contentment`.

- [ ] **Charity recipient log detail** — In `AgentDecisionSystem`'s charity block, after
  identifying `starving.entity`, expand the log from "X helped a starving neighbour." to
  "X helped [Name] at [Settlement]." Read `registry.try_get<Name>(starving.entity)` for the
  recipient name and `registry.try_get<Settlement>` on `starving.homeSettl` for the settlement.
  No new components. The charity block is in the large NPC loop near the bottom of
  `AgentDecisionSystem::Update`.

- [ ] **Gratitude shown in world dot** — While `isGrateful` is true, draw a faint LIME ring
  around the NPC's world dot in `GameState::Draw`. After the `DrawCircleV` for the agent dot,
  add: if `a.isGrateful && a.role == RenderSnapshot::AgentRole::NPC`, call
  `DrawCircleLinesV({a.x, a.y}, a.size + 3.f, Fade(LIME, 0.45f))`. `isGrateful` is already in
  `AgentEntry`. Keeps the visual footprint small (just one extra ring draw per grateful NPC).

- [ ] **Wanderer re-settlement** — Exiled NPCs (those with `HomeSettlement::settlement == entt::null`
  and `theftCount >= 3` in `DeprivationTimer`) can earn a fresh start. In `AgentDecisionSystem`'s
  IDLE/SEEKING block, when `home.settlement == entt::null`, check if `balance >= 30.f`. If so,
  find the nearest settlement with `pop < popCap - 2` and set it as the new `home.settlement`,
  deduct 30g from `Money::balance`, credit to that settlement's `treasury`, reset `theftCount = 0`.
  Log "X settled at Y (fresh start)."

- [ ] **Exile indicator in tooltip** — Surface exile state in the NPC hover tooltip. Add
  `bool isExiled = false` to `AgentEntry` in `RenderSnapshot.h`; set when
  `home.settlement == entt::null && dt->theftCount >= 3` in `SimThread::WriteSnapshot`. In
  `HUD::DrawHoverTooltip`, when `isExiled`, append " [Exiled]" in `Fade(RED, 0.8f)` to line2
  (the behavior line), or show it as a separate faint red line below the role line.

- [ ] **Wanderer re-settlement** — Exiled NPCs (`home.settlement == entt::null`,
  `theftCount >= 3`) with `balance >= 30g` can re-settle. In `AgentDecisionSystem`'s
  IDLE/SEEKING block, when `home.settlement == entt::null`, find nearest settlement with
  `pop < popCap - 2`, deduct 30g from `Money::balance`, credit to that `Settlement::treasury`,
  set as new `home.settlement`, reset `theftCount = 0`. Log "X settled at Y (fresh start)."

- [ ] **Exile indicator in tooltip** — Add `bool isExiled = false` to `AgentEntry` in
  `RenderSnapshot.h`; set in `SimThread::WriteSnapshot` when home entity is `entt::null` AND
  `dt->theftCount >= 3`. In `HUD::DrawHoverTooltip`, when `isExiled`, show a faint red
  "[Exiled]" line below the role/behavior lines — same `lineCount`/`pw` pattern as `showHelped`.

- [ ] **Gratitude world dot ring** — While `isGrateful` is true, draw a faint LIME ring
  around the NPC's world dot in `GameState::Draw`. After `DrawCircleV` for the agent dot,
  add: if `a.isGrateful && a.role == RenderSnapshot::AgentRole::NPC`, call
  `DrawCircleLinesV({a.x, a.y}, a.size + 3.f, Fade(LIME, 0.45f))`. `isGrateful` is already
  in `AgentEntry`.

- [ ] **Charity giver count in settlement tooltip** — In `SimThread::WriteSnapshot`, when
  building `worldStatus`, count how many NPCs at this settlement have `charityTimer > 0`
  (i.e. gave charity recently). Add `int recentGivers = 0` to `SettlementStatus` in
  `RenderSnapshot.h`. In `HUD`'s settlement list panel (`DrawWorldStatus`), append
  a faint `(Ng)` suffix (in LIME) when `recentGivers > 0` — like the existing `(Nc)` child suffix.

- [ ] **Theft frequency shown in stockpile panel** — Add `int theftCount = 0` (total thefts
  since settlement founding) to `Settlement` in `Components.h`. Increment in
  `AgentDecisionSystem`'s theft block alongside `timer.theftCount`. In `SimThread::WriteSnapshot`,
  populate `StockpilePanel::theftCount` (add field). In `RenderSystem::DrawStockpilePanel`,
  show "Thefts: N" in faint RED below the treasury line when `theftCount > 0`.

- [ ] **Exile indicator in tooltip** — Add `bool isExiled = false` to `AgentEntry` in
  `RenderSnapshot.h`. Set it in `SimThread::WriteSnapshot` when `hs.settlement == entt::null` and
  the entity is not a bandit (bandit state supersedes exile). In `HUD::DrawHoverTooltip`, show
  "(exile)" in faded ORANGE below the profession line when `isExiled`. Lets the player identify
  wandering exiles before they turn bandit.

- [ ] **Bandit density cap per road** — Prevent more than 3 bandits from lurking at the same
  road midpoint. In `AgentDecisionSystem`'s bandit block, build a `std::map<entt::entity, int>`
  counting how many bandits target each Road entity. If the count for the nearest road is ≥ 3,
  pick the second-nearest road instead. Stops visual clumping of bandits on a single road.

- [ ] **Road safety indicator in road tooltip** — Add `int banditCount = 0` to
  `RenderSnapshot::RoadEntry`. In `WriteSnapshot`, count `BanditTag` entities whose nearest road
  is this road (use a simple proximity check: within 80 units of the midpoint). In
  `HUD::DrawRoadTooltip`, append "⚠ Bandits: N" in RED when `banditCount > 0`. Gives the player
  meaningful route-safety information.

- [ ] **Goal shown in NPC tooltip** — Add `std::string goalDescription` to `RenderSnapshot::AgentEntry`
  (RenderSnapshot.h). In `SimThread::WriteSnapshot`, if the entity has a `Goal` component, set it to
  e.g. "Goal: Save Gold (42/100g)" using the `GoalLabel()` helper and `goal.progress`/`goal.target`.
  In `HUD::DrawHoverTooltip` (HUD.cpp), render it as an extra line in dim SKYBLUE below the skill
  line. Lets the player see at a glance what each NPC is striving for.

- [ ] **Goal progress milestone log** — In `AgentDecisionSystem`'s goal system section, when
  `progress` crosses 50% of `target` for the first time (add a `bool halfwayLogged` field to `Goal`
  in Components.h), push a brief log: "Aldric is halfway to their savings goal (50/100g)." Set
  `halfwayLogged = true` after firing. Reset it to `false` when a new goal is assigned. Gives
  players a mid-goal feedback signal.

- [ ] **BecomeHauler goal auto-completes on graduation** — In `EconomicMobilitySystem.cpp`,
  when an NPC graduates to Hauler (`registry.emplace<Hauler>`), check if they have a `Goal` with
  `type == GoalType::BecomeHauler`. If so, set `goal.progress = goal.target` immediately so the
  goal system picks it up next tick and triggers the celebration + new goal assignment. Currently
  graduation happens but the goal completion fires only on the next frame via the registry check —
  making it explicit here ensures the log fires reliably.

- [ ] **Migration memory shown in tooltip** — Add `std::string migrationMemorySummary` to
  `RenderSnapshot::AgentEntry` (RenderSnapshot.h). In `SimThread::WriteSnapshot`, if the entity
  has a `MigrationMemory` with ≥ 2 entries, set it to e.g. "Knows: Wellsworth (food 2g), Millhaven
  (wood 1g)". In `HUD::DrawHoverTooltip`, render it as an extra dim GRAY line. Gives the player
  insight into what an NPC knows about the world.

- [ ] **Stale memory decay** — In `AgentDecisionSystem`'s migration trigger section, add a
  `lastVisitedDay` int field to `MigrationMemory::PriceSnapshot` (Components.h). When recording
  a snapshot, set it to the current `tm.day`. When scoring destinations in `FindMigrationTarget`,
  if `tm.day - snap.lastVisitedDay > 30` (more than 30 days old), reduce the memory bonus to 50%
  — stale knowledge is less reliable. This creates realistic information decay over time.

- [ ] **NPC personal events** — In `RandomEventSystem`, add a per-NPC event tier that fires every
  12–48 game-hours per NPC (jittered by entity ID). Small events: skill discovery (+0.1 to a
  random skill), windfall (find 5–15g — no gold source needed, treat as lucky find), minor illness
  (one need drains 2× for 6 game-hours via a `illnessTimer` float on `DeprivationTimer`), good
  harvest (working NPC produces 1.5× for 4 hours via a `harvestBonus` float). Log the notable
  ones. Use `entt::to_integral(e) % period` for deterministic per-entity jitter.
