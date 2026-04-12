# ReachingUniversalis — Dev Backlog

**Priority: NPC behaviour depth and inter-NPC interaction — building toward a living world.**

Maintained by Claude Code cron. Each session picks the top `[ ]` task, implements it, commits,
marks it done, then appends 2–3 new concrete tasks to keep the queue full.

---

## In Progress

## Recently Done

- [x] **Scarcity morale penalty** — If any stockpile < 10 units, apply -0.003 morale/game-hour.

- [x] **Stockpile abundance log event** — Log "Prosperity: [settlement] has abundant stores —
  morale rising." on first abundance trigger (all 3 stockpiles >= 80). One-shot per settlement
  per abundance period.

- [x] **Hauler home morale penalty on bankruptcy** — Apply -0.03 morale to home settlement
  when a hauler goes bankrupt in EconomicMobilitySystem.cpp.

- [x] **Trade delivery log with morale** — Logs "Hauler delivered N food to X (morale +1%)".

- [x] **Morale trend arrow in world status bar** — Shows +/- after M:XX% based on 1s sampled morale delta.

- [x] **Morale colour on settlement ring** — Ring tinted green/yellow/red by morale when no event active.

- [x] **Celebrating NPC glow ring** — Pulsating gold ring (radius 12) on celebrating NPCs.

- [x] **Harvest bonus shown in tooltip** — "Good harvest bonus" in faint gold in NPC tooltip.

- [x] **Illness source context in log** — Added "at Greenfield" to illness log messages.

- [x] **Skill discovery location in log** — Added "at Greenfield" to skill insight log messages.

- [x] **Illness contagion between NPCs** — 10% contagion chance in gossip loop, logs spread events.

- [x] **Illness NPC dot tint** — Purple tint on sick NPC/Child dots via RGB averaging.

- [x] **Illness recovery log** — Logs "X recovered from illness at Y" on timer-to-zero transition.

- [x] **GoodHarvest rumour seeding** — New RumourType, seeded on Harvest Bounty, -5% food price on arrival.

- [x] **Rumour immunity after delivery** — 48 game-hour timed immunity per origin+type+settlement.

- [x] **Rumour carrier visible in tooltip** — Shows "(spreading: plague/drought/bandits)" in faint yellow.

- [x] **Profession match indicator on world dot** — Gold ring (radius 5) on working NPCs in vocation.

- [x] **Skill milestone log** — Logs "X reached Journeyman/Master Farming/Water/Woodcutting" on 0.5/0.9 crossings.

- [x] **Profession vocation label in NPC tooltip** — Shows "[vocation]" in gold when profession matches best skill.

- [x] **Idle NPC count in stockpile panel** — Shows "Working: N / Idle: N" in Treasury line.

- [x] **Settlement specialty label in stockpile header** — Specialty label shown in stockpile panel header.

- [x] **Profession colour in residents list** — Fa=green, Wa=skyblue, Lu=brown, Me=gold in
  stockpile panel residents list.

- [x] **Migrant welcome log at destination** — Added "Ashford welcomes X (Farmer) — pop now N"
  log after arrival, counting current HomeSettlement population.

- [x] **Skill reset on profession change** — On profession change during migration, halve old
  skill and boost new by +0.1. Only when both professions are non-Idle.

- [x] **Profession shown in migration log** — Added arrival log "X (Farmer) moved to Y" in
  MIGRATING arrival block. Omits profession suffix when Idle or no Profession component.

- [x] **Estate size shown in settlement tooltip** — Added `pendingEstates` to SettlementStatus,
  populated in SimThread, displayed as "Estates: ~Ng" in Fade(GOLD, 0.5f) in settlement tooltip.

- [x] **Elder will tooltip line** — Added "Will: 80% to treasury" in Fade(GOLD, 0.5f) for
  elders with gold in the NPC hover tooltip.

- [x] **Estate log on need-death too** — Already works: the inheritance block iterates all
  `toRemove` entities (both old-age and need-deprivation deaths). Verified.

- [x] **Contentment milestone log** — Already implemented as "Suffering NPC log event" using
  `s_desperateLogged` static set in RandomEventSystem.cpp (0.2 onset, 0.5 recovery).

- [x] **Elder deathbed savings inheritance** — Elder 0.8 fraction and isElder check were already
  implemented. Added "(elder)" tag to estate log message for visual distinction.

- [x] **Settlement tooltip: pop trend arrow** — In `DrawSettlementTooltip` (HUD.cpp), append the
  popTrend character ('+', '-', '=') to the pop line using `SettlementStatus::popTrend`. Already
  available in `SettlementStatus`. Format: "[12/35 pop +]" or "[12/35 pop -]". Uses plain '+'
  and '-' ASCII.

- [x] **NPC birth log** — Already implemented: BirthSystem.cpp logs "Born: X at Y (to ParentName)"
  using the wealthiest adult at the settlement. ChildTag is an empty struct with no followTarget.

- [x] **Settlement tooltip: specialty and morale** — Extend `DrawSettlementTooltip` (HUD.cpp) to
  show two extra lines: (1) "Specialty: Farming" from `SettlementEntry::specialty` when non-empty;
  (2) "Morale: XX%" from `StockpilePanel::morale` — but that's only available when the settlement
  is selected. Instead add `float morale` to `SettlementStatus` in `RenderSnapshot.h`, populate
  it in SimThread's world-status loop with `s.morale`, and read it in the tooltip. Display it
  in the same green/yellow/red colour scheme as the panel bar.

- [x] **Unrest pop context** — In `RandomEventSystem::Update`'s settlement loop, the UNREST log
  currently reads "UNREST in Ashford — morale critical, production suffering". Extend it to
  include `[pop N]` and the current morale percentage: "UNREST in Ashford [pop 8] — morale 22%,
  production suffering". Count pop via the same HomeSettlement view pattern used in TriggerEvent.
  Same for "Tensions ease" recovery log.

- [x] **Plague spread log** — In `RandomEventSystem`'s plague spread block (the section that
  copies plague from one settlement to a neighbour via roads), the current log message is
  "PLAGUE spreads from X to Y — N died". Add `[pop N]` to the destination settlement using
  `popCount` computed the same way as in `TriggerEvent` — quick local count on the destination
  entity before the log push. Keeps spread events as informative as the initial eruption.

- [x] **Event log pop trend** — In `RandomEventSystem::TriggerEvent`, after computing `popCount`,
  also look up the settlement's `popTrend` from `RenderSnapshot::SettlementStatus` — but that's
  render-side. Instead compute it locally: count NPCs at target (already in `popCount`), then
  compare to a rolling previous count stored in a `std::map<entt::entity, int> m_prevPop` member
  on `RandomEventSystem`. Append "(↑)" or "(↓)" to `[pop N]` when trend changes by ≥ 2 between
  samples taken every 24 game-hours. Update sample in `Update()` via a `m_popSampleTimer`.

- [x] **Suffering NPC log event** — In `RandomEventSystem::Update`'s per-NPC loop, when
  `contentment < 0.2f` for an NPC, log "X is desperate at Y" (once per 12 game-hours using the
  existing `personalEventTimer`). Requires computing `contentment` the same way as SimThread's
  snapshot: weighted average of the 4 needs (hunger 30%, thirst 30%, energy 20%, heat 20%). Log
  only if the NPC has a Name and HomeSettlement, and rate-limit per entity.

- [x] **Mood colour legend overlay** — In `HUD::Draw` (HUD.cpp), when `debugOverlay` is true,
  draw a small 3-row legend in the bottom-right corner: a green dot + "Thriving (>70%)", a yellow
  dot + "Stressed (40-70%)", a red dot + "Suffering (<40%)". Draw using `DrawCircleV` (radius 5)
  + `DrawText` at fixed screen coordinates. Helps the player decode the contentment colour system.

- [x] **Contentment shown in world status bar** — Add `float avgContentment = 1.f` to
  `SettlementStatus` in `RenderSnapshot.h`. In SimThread's world-status loop, compute the average
  contentment of homed NPCs (view `Needs, HomeSettlement`, same exclusions as needStability).
  In `HUD::DrawWorldStatus` (HUD.cpp), after the existing pop count, append a small coloured
  "C:XX%" indicator using GREEN/YELLOW/RED thresholds matching the dot colours.

- [x] **NPC longest-resident badge** — In `SimThread::WriteSnapshot`'s StockpilePanel residents
  loop, also track the NPC whose `Age::days` is highest among residents. Add an `isEldest bool`
  to `StockpilePanel::AgentInfo`. In `DrawStockpilePanel`, suffix the eldest resident's name
  with " [Elder]" in `Fade(ORANGE, 0.8f)`. Represents the settlement patriarch/matriarch.

- [x] **Gossip idle animation** — In `AgentDecisionSystem`, when two NPCs from the same settlement
  are both `Idle` and within 30 units during off-work hours (20–22h), briefly nudge their
  velocity ±5 units toward each other (`vel.x += dx * 0.1f / dist`) for 2–3 game-seconds so
  they visually gravitate together. Track with a `gossipTimer` float on `DeprivationTimer`
  (already exists); set 3.f when gossip fires, skip new gossip while > 0. No new components.

- [x] **Family dynasty count in stockpile panel** — In `DrawStockpilePanel` (RenderSystem.cpp),
  after the "Residents (N):" header, count how many distinct `familyName` values appear in
  `panel.residents` and how many surnames appear ≥ 2 times. Add a compact line below the header:
  "Families: 3 dynasties" or "No established families" if all residents have unique surnames.
  Build counts by iterating `panel.residents` — no new snapshot fields needed.

- [x] **Profession distribution in stockpile panel** — Below the residents list header in
  `DrawStockpilePanel` (RenderSystem.cpp), add a single compact line showing profession counts,
  e.g. "Fa:4 Wa:3 Lu:2". Build the counts by iterating `panel.residents` (already populated).
  Render in dim LIGHTGRAY after the header line. Replaces no existing line — just one extra row.
  No new snapshot fields needed.

- [x] **Resident wealth tooltip on panel click** — When hovering the "Residents (N):" header line
  in the stockpile panel (detect mouse Y within the section), show a small 2-line tooltip with
  the richest NPC's name and balance, and the poorest's. Use `panel.residents.front()` and
  `panel.residents.back()` already in the snapshot. Draw it in `RenderSystem::DrawStockpilePanel`
  using `GetMousePosition()` comparison against the section Y range. No new fields needed.

- [x] **Seasonal migration preference** — In `AgentDecisionSystem`'s `FindMigrationTarget` scoring,
  subtract 0.2 from the score of any destination that is currently in Winter (`Season::Winter`).
  Read the season from `TimeManager` (already in registry view). This creates organic population
  flow away from winter-hit settlements toward warmer-season regions, making seasonal population
  patterns emergent rather than purely price-driven.

- [x] **Elder knowledge bonus in production** — In `ProductionSystem.cpp`, after the existing
  `moraleBonus` block, add: for each Working NPC at a facility whose `Age::days > 60`, add a
  flat `+0.05` worker contribution (elders provide tacit knowledge). Use `registry.try_get<Age>(e)`.
  Cap contribution per-elder at 2.0x to prevent outliers. Log nothing — the effect is subtle
  and best discovered by the player through observation.

- [x] **Rival hauler harassment** — When a hauler from settlement A (home) arrives at rival
  settlement B (where B.relations[A] < -0.5), add a random 20% chance the delivery is "taxed at
  the gate": reduce the hauler's `earned` by an extra 10% and credit B's treasury. Track this in
  `TransportSystem.cpp` right after the `effectiveTax` block. Log "Hauler from X taxed at gate
  in Y (rivalry tariff)." at low probability to avoid log spam (1 in 5 deliveries).

- [x] **Alliance bonus shown in road tooltip** — When two settlements are allied, also boost the
  road's arbitrage rate in `PriceSystem.cpp`. In the per-road arbitrage loop, check
  `sA->relations.find(road.to)` and `sB->relations.find(road.from)`; if both scores > 0.5, multiply
  `convergeFrac` by 1.5 (prices converge 50% faster on allied trade routes). Add a tooltip note
  "Allied: faster price convergence" in `HUD.cpp DrawRoadTooltip` when the alliance line is shown.

- [x] **Rivalry log events**

- [x] **Relationship pair memory** — Add a lightweight `Relations` component: `struct Relations {
  std::map<entt::entity, float> affinity; }`. In `AgentDecisionSystem`, when two idle same-settlement
  NPCs are within 25 units (evening gathering), increment their mutual affinity by 0.02 per tick
  (capped at 1.0). Affinity above 0.5 means "friend": friends share food charity (threshold reduced
  from 5g to 1g for the helping-neighbour check), and when one migrates, the other has a 30% chance
  to follow. Log "Aldric and Mira left Ashford together." Decay affinity by 0.001/game-hour when
  apart. No UI needed yet — the effects on migration and charity are the observable outcome.

---

## Done

- [x] **Child count in stockpile tooltip** — New `HUD::DrawSettlementTooltip` (HUD.cpp + HUD.h)
  triggered when mouse hovers inside a settlement's world-radius. Shows name/pop/cap, resource
  stocks, treasury+haulers, and a faded-LIGHTGRAY "Children: N" line when `childCount > 0`.
  Matches `SettlementEntry` to `SettlementStatus` by name. Called from `HUD::Draw` between
  `DrawFacilityTooltip` and `DrawRoadTooltip`.

- [x] **Settlement name in event log** — `TriggerEvent` in `RandomEventSystem.cpp`: computes
  `popCount` once (HomeSettlement view, excluding player/haulers) after picking the target
  settlement. Added `[pop N]` to all 13 settlement-specific cases (Drought, Blight, Plague,
  Trade Boom, Migration, Spring Flood, Harvest Bounty, Convoy, Festival, Fire, Heat Wave,
  Lumber Windfall, Skilled Immigrant, Market Crisis, Earthquake). Migration shows post-arrival
  pop; Skilled Immigrant shows post-arrival pop too.

- [x] **NPC mood colour on world dot** — `GameState.cpp` agent draw loop: added contentment-based
  `drawColor` for `AgentRole::NPC`. Green (≥ 0.7), yellow (≥ 0.4), red (< 0.4). Celebrating
  overrides to gold for all roles. Children, haulers, and player use `a.color` unchanged.

- [x] **Family size in tooltip** — `DrawHoverTooltip` (HUD.cpp): added `familyNameCount` map
  built alongside `surnameCount` in the same agent loop. FamilyTag path looks up the count;
  surname-heuristic path uses `it->second`. Both format strings changed to
  `"  (Family: %s x%d)"`. Shows e.g. "(Family: Smith x3)".

- [x] **Richest NPC highlighted in stockpile panel** — Added `StockpilePanel::AgentInfo` struct
  (name, balance, profession) and `residents` vector to `RenderSnapshot.h`. SimThread collects
  homed NPCs (excluding player/haulers) in the selected-settlement block, sorts by balance
  descending, caps at 12. `DrawStockpilePanel` renders a "Residents (N):" section — richest
  first entry is drawn in `GOLD`, others in `Fade(LIGHTGRAY, 0.85f)`. Panel height updated.

- [x] **Deathbed log with age** — `DeathSystem.cpp`: old-age path uses `age.days` already in scope;
  need-death path adds `try_get<Age>`. Both look up `HomeSettlement→Settlement` for location.
  Formats: "Aldric Smith died at age 72 (old age) at Ashford" / "Mira Reed died of hunger, age 31,
  at Ashford". `printf` debug line also updated.

- [x] **NPC age display in tooltip** — `lineAge[32]` in `DrawHoverTooltip` (HUD.cpp). For named
  NPCs, inserted immediately after the name/role line (line1). Format: "Age: 8 (child)" for
  `ageDays < 15`, "Age: 63 (elder)" for `ageDays > 60`, "Age: 32" otherwise. Colour-coded:
  SKYBLUE for children, ORANGE for elders, LIGHTGRAY for adults. lineCount bumped from 4→5 for
  named NPCs; width calculation updated.

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

- [x] **Population cap shown in stockpile panel header** — `popCap` was already wired. Split header
  into prefix + pop-number + suffix `DrawText` calls. Pop drawn in ORANGE when `pop >= popCap - 2`.

- [x] **Wandering orphan re-settlement** — Separate pass in `AgentDecisionSystem::Update` after
  the main NPC loop. Orphans (ChildTag + null HomeSettlement) seek nearest settlement within
  200 units with available capacity. Moves via `Migrating` state; logs on arrival.

- [x] **Collapse cooldown — settlement ruins** — `Settlement::ruinTimer = 300.f` set on collapse
  in DeathSystem (also drains timer there). BirthSystem checks `ruinTimer <= 0`. Ring renders in
  GRAY (lighter than DARKGRAY) while ruin timer > 0. `SettlementEntry::ruinTimer` carries it.

- [x] **Orphan count in collapse log** — `DeathSystem.cpp`: log now reads "N children of X
  orphaned and scattered." using `snprintf` with `orphanCount` (already computed in the loop).

- [x] **Apprentice tooltip badge** — `HUD::DrawHoverTooltip`: appends " [Apprentice]" in
  `Fade(YELLOW, 0.6f)` after the age line text when `role == Child && ageDays >= 12`.

- [x] **Graduation announcement shows skill** — `ScheduleSystem.cpp` graduation block: appends
  " — best skill: Farming 38%" using `try_get<Skills>`, comparing the three skill floats.

- [x] **Elder count in settlement tooltip** — `elderCount`/`elderBonus` added to `SettlementStatus`.
  SimThread counts via `try_get<Age>`. Tooltip shows "Elders: N (+X% prod)" in orange.

- [x] **Elder deathbed savings inheritance** — `DeathSystem.cpp` inheritance block: `try_get<Age>`
  in the per-death loop; if `age->days > 60`, uses 0.8f fraction instead of 0.5f. Logs
  "X left an estate of Ng to Settlement." for estates ≥ 10g.

- [x] **Profession change on migration** — `AgentDecisionSystem.cpp` MIGRATING arrival block:
  after memory update, views `ProductionFacility` to find the settlement's primary facility
  (highest `baseRate`), then sets `Profession::type = ProfessionForResource(pf.output)`.
  Guarded by `try_get<Profession>` so NPCs without the component are skipped.

- [x] **Profession shown in stockpile panel NPC list** — `RenderSystem::DrawStockpilePanel`:
  replaced single `snprintf`/`DrawText` per resident with three calls: name in NPC color,
  " [Fa]"/" [Wa]"/" [Lu]"/" [Me]" in `Fade(GRAY, 0.75f)`, then gold in NPC color.
  Profession mapped from full string to 2-letter abbr; no abbr shown for unmapped professions.

- [x] **Profession-based work speed bonus** — `ScheduleSystem.cpp` skill-at-worksite block:
  `try_get<Profession>` then compare `prof->type == ProfessionForResource(facType)`.
  `gainMult = 1.1f` when matched, else 1.0f. Multiplied into `SKILL_GAIN_PER_GAME_HOUR`.

- [x] **NPC rumour propagation via gossip** — Extend the existing gossip system
  (`AgentDecisionSystem.cpp`). Add a `Rumour` component: `enum RumourType { PlagueNearby,
  GoodHarvest, BanditRoads }` with a `hops` int (decrements per gossip exchange) and an `origin`
  settlement entity. When `RandomEventSystem` fires a plague or drought, attach a `Rumour` to 1–2
  NPCs at that settlement. During gossip exchanges, spread the rumour to the other NPC (if their
  settlement doesn't already have it and `hops > 0`). When a rumour arrives at a settlement,
  nudge that settlement's relevant stockpile fear: plague → food hoarding (+10% Food price at
  Market), drought → water scarcity (+15% Water price). Log "Rumour of plague reached Thornvale."

- [x] **Illness visible in tooltip** — When an NPC has `depTimer->illnessTimer > 0`, added
  `bool ill` and `int illNeedIdx` to `AgentEntry` in `RenderSnapshot.h`, populated from
  `DeprivationTimer` in SimThread's agent snapshot loop. `HUD::DrawHoverTooltip` draws a faint
  red "(ill: hunger)" / "(ill: thirst)" / "(ill: fatigue)" suffix inline on the needs line.
  Width calculation updated so the tooltip box fits the extended line.

- [x] **Windfall source context in log** — In `RandomEventSystem`'s per-NPC event loop (case 1),
  windfall log now reads "Aldric Smith found 12g on the road near Greenfield" using
  `try_get<HomeSettlement>` → `try_get<Settlement>` to get the name. Falls back to the original
  format if the NPC has no home settlement.

- [x] **Harvest bonus glow on worker dot** — Added `bool harvestBonus` to `AgentEntry` in
  `RenderSnapshot.h`, populated from `DeprivationTimer::harvestBonusTimer > 0` in SimThread's
  WriteSnapshot. `GameState.cpp`'s agent render loop draws a faint `Fade(GOLD, 0.4f)` ring
  (radius 10) around any agent with the bonus active.

- [x] **Morale shown in world status bar** — Added `float morale` to `SettlementStatus` in
  `RenderSnapshot.h`; populated from `Settlement::morale` in SimThread's world-status loop.
  `HUD::DrawWorldStatus` renders "M:XX%" label per settlement: green (≥70%), yellow (≥30%),
  red (<30%). Width calculation updated so the status bar expands to fit.

- [x] **Morale impact from hauler trade success** — In `TransportSystem.cpp`'s GoingToDeposit
  arrival block, after the sale completes and before return-trip opportunism, bumps
  `destSettl->morale` by +0.01 (capped at 1.0). Active trade routes gradually lift morale.

- [x] **Morale recovery from full stockpiles** — In `RandomEventSystem::Update`'s per-settlement
  loop, when all three stockpiles (food, water, wood) exceed 80 units, applies +0.002 morale per
  game-hour. Changed from population-relative threshold to fixed 80-unit minimum per the spec.

- [x] **Work stoppage morale recovery** — In `ScheduleSystem.cpp`'s strike drain block, when
  `strikeDuration` transitions from > 0 to 0, bumps home settlement morale by +0.05 (capped at
  1.0). Models grievances being aired and partially resolved.

- [x] **NPC age display in tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), after the role line,
  add an age line: "Age: 23" (integer days). Read `AgentEntry::ageDays` cast to int. For children
  (`ageDays < 15`), show "Age: 8 (child)". For elders (`ageDays > 60`), show "Age: 63 (elder)".
  No new snapshot fields needed — `ageDays` is already in `AgentEntry`.

- [x] **Rivalry log events** — In `RandomEventSystem::Update`'s settlement loop (where relations
  drift already runs), add threshold-crossing logs. When `A.relations[B]` crosses below -0.5 for
  the first time, log "RIVALRY: X and Y relations deteriorate — tariffs imposed (+10%)". When it
  rises above -0.3 (recovery), log "Relations improving between X and Y". Use a similar `bool`
  crossing approach as `Settlement::unrest`. This makes rivalry formation a visible story beat.

- [x] **Alliance bonus shown in road tooltip** — When two settlements are allied, also boost the
  road's arbitrage rate in `PriceSystem.cpp`. In the per-road arbitrage loop, check
  `sA->relations.find(road.to)` and `sB->relations.find(road.from)`; if both scores > 0.5, multiply
  `convergeFrac` by 1.5 (prices converge 50% faster on allied trade routes). Add a tooltip note
  "Allied: faster price convergence" in `HUD.cpp DrawRoadTooltip` when the alliance line is shown.

- [x] **Rival hauler harassment** — When a hauler from settlement A (home) arrives at rival
  settlement B (where B.relations[A] < -0.5), add a random 20% chance the delivery is "taxed at
  the gate": reduce the hauler's `earned` by an extra 10% and credit B's treasury. Track this in
  `TransportSystem.cpp` right after the `effectiveTax` block. Log "Hauler from X taxed at gate
  in Y (rivalry tariff)." at low probability to avoid log spam (1 in 5 deliveries).

- [x] **Population history chart in stockpile panel** — `StockpilePanel::popHistory` (vector<int>)
  is already written by SimThread (up to 30 daily samples). In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), below the population line, draw a small sparkline: 30 thin bars, each
  proportional to the population max across all samples, width 4px, height scaled to 30px.
  Color GREEN when trend is up, RED when down, GRAY flat. Gives an at-a-glance population
  trajectory without opening any extra panel.

- [x] **Elder knowledge bonus in production** — In `ProductionSystem.cpp`, after the existing
  `moraleBonus` block, add: for each Working NPC at a facility whose `Age::days > 60`, add a
  flat `+0.05` worker contribution (elders provide tacit knowledge). Use `registry.try_get<Age>(e)`.
  Cap contribution per-elder at 2.0x to prevent outliers. Log nothing — the effect is subtle
  and best discovered by the player through observation.

- [x] **Seasonal migration preference** — In `AgentDecisionSystem`'s `FindMigrationTarget` scoring,
  subtract 0.2 from the score of any destination that is currently in Winter (`Season::Winter`).
  Read the season from `TimeManager` (already in registry view). This creates organic population
  flow away from winter-hit settlements toward warmer-season regions, making seasonal population
  patterns emergent rather than purely price-driven.

- [x] **Resident wealth tooltip on panel click** — When hovering the "Residents (N):" header line
  in the stockpile panel (detect mouse Y within the section), show a small 2-line tooltip with
  the richest NPC's name and balance, and the poorest's. Use `panel.residents.front()` and
  `panel.residents.back()` already in the snapshot. Draw it in `RenderSystem::DrawStockpilePanel`
  using `GetMousePosition()` comparison against the section Y range. No new fields needed.

- [x] **Profession distribution in stockpile panel** — Below the residents list header in
  `DrawStockpilePanel` (RenderSystem.cpp), add a single compact line showing profession counts,
  e.g. "Fa:4 Wa:3 Lu:2". Build the counts by iterating `panel.residents` (already populated).
  Render in dim LIGHTGRAY after the header line. Replaces no existing line — just one extra row.
  No new snapshot fields needed.

- [x] **Richest NPC name in world economy bar** — In `HUD::DrawWorldStatus` (HUD.cpp), the
  economy debug overlay already shows `econRichestName` and `econRichestWealth` (both in
  `RenderSnapshot`). If `debugOverlay` is on, render a line "Richest: [Name] — 123g" in GOLD
  below the existing economy stats. Read directly from `snap.econRichestName` and
  `snap.econRichestWealth`. No new fields or SimThread changes needed.

- [x] **Family dynasty count in stockpile panel** — In `DrawStockpilePanel` (RenderSystem.cpp),
  after the "Residents (N):" header, count how many distinct `familyName` values appear in
  `panel.residents` and how many surnames appear ≥ 2 times. Add a compact line below the header:
  "Families: 3 dynasties" or "No established families" if all residents have unique surnames.
  Build counts by iterating `panel.residents` — no new snapshot fields needed.

- [x] **Gossip idle animation** — In `AgentDecisionSystem`, when two NPCs from the same settlement
  are both `Idle` and within 30 units during off-work hours (20–22h), briefly nudge their
  velocity ±5 units toward each other (`vel.x += dx * 0.1f / dist`) for 2–3 game-seconds so
  they visually gravitate together. Track with a `gossipTimer` float on `DeprivationTimer`
  (already exists); set 3.f when gossip fires, skip new gossip while > 0. No new components.

- [x] **NPC longest-resident badge** — In `SimThread::WriteSnapshot`'s StockpilePanel residents
  loop, also track the NPC whose `Age::days` is highest among residents. Add an `isEldest bool`
  to `StockpilePanel::AgentInfo`. In `DrawStockpilePanel`, suffix the eldest resident's name
  with " [Elder]" in `Fade(ORANGE, 0.8f)`. Represents the settlement patriarch/matriarch.

- [x] **Contentment shown in world status bar** — Add `float avgContentment = 1.f` to
  `SettlementStatus` in `RenderSnapshot.h`. In SimThread's world-status loop, compute the average
  contentment of homed NPCs (view `Needs, HomeSettlement`, same exclusions as needStability).
  In `HUD::DrawWorldStatus` (HUD.cpp), after the existing pop count, append a small coloured
  "❤XX%" or plain "C:XX%" indicator using GREEN/YELLOW/RED thresholds matching the dot colours.

- [x] **Mood colour legend overlay** — In `HUD::Draw` (HUD.cpp), when `debugOverlay` is true,
  draw a small 3-row legend in the bottom-right corner: a green dot + "Thriving (>70%)", a yellow
  dot + "Stressed (40-70%)", a red dot + "Suffering (<40%)". Draw using `DrawCircleV` (radius 5)
  + `DrawText` at fixed screen coordinates. Helps the player decode the contentment colour system.

- [x] **Suffering NPC log event** — In `RandomEventSystem::Update`'s per-NPC loop, when
  `contentment < 0.2f` for an NPC, log "X is desperate at Y" (once per 12 game-hours using the
  existing `personalEventTimer`). Requires computing `contentment` the same way as SimThread's
  snapshot: weighted average of the 4 needs (hunger 30%, thirst 30%, energy 20%, heat 20%). Log
  only if the NPC has a Name and HomeSettlement, and rate-limit per entity.

- [x] **Event log pop trend** — In `RandomEventSystem::TriggerEvent`, after computing `popCount`,
  also look up the settlement's `popTrend` from `RenderSnapshot::SettlementStatus` — but that's
  render-side. Instead compute it locally: count NPCs at target (already in `popCount`), then
  compare to a rolling previous count stored in a `std::map<entt::entity, int> m_prevPop` member
  on `RandomEventSystem`. Append "(↑)" or "(↓)" to `[pop N]` when trend changes by ≥ 2 between
  samples taken every 24 game-hours. Update sample in `Update()` via a `m_popSampleTimer`.

- [x] **Plague spread log** — In `RandomEventSystem`'s plague spread block (the section that
  copies plague from one settlement to a neighbour via roads), the current log message is
  "PLAGUE spreads from X to Y — N died". Add `[pop N]` to the destination settlement using
  `popCount` computed the same way as in `TriggerEvent` — quick local count on the destination
  entity before the log push. Keeps spread events as informative as the initial eruption.

- [x] **Unrest pop context** — In `RandomEventSystem::Update`'s settlement loop, the UNREST log
  currently reads "UNREST in Ashford — morale critical, production suffering". Extend it to
  include `[pop N]` and the current morale percentage: "UNREST in Ashford [pop 8] — morale 22%,
  production suffering". Count pop via the same HomeSettlement view pattern used in TriggerEvent.
  Same for "Tensions ease" recovery log.

- [x] **Settlement tooltip: specialty and morale** — Extend `DrawSettlementTooltip` (HUD.cpp) to
  show two extra lines: (1) "Specialty: Farming" from `SettlementEntry::specialty` when non-empty;
  (2) "Morale: XX%" from `StockpilePanel::morale` — but that's only available when the settlement
  is selected. Instead add `float morale` to `SettlementStatus` in `RenderSnapshot.h`, populate
  it in SimThread's world-status loop with `s.morale`, and read it in the tooltip. Display it
  in the same green/yellow/red colour scheme as the panel bar.

- [x] **NPC birth log** — Already implemented: BirthSystem.cpp logs "Born: X at Y (to ParentName)"
  using the wealthiest adult at the settlement. ChildTag is an empty struct with no followTarget.

- [x] **Settlement tooltip: pop trend arrow** — In `DrawSettlementTooltip` (HUD.cpp), append the
  popTrend character ('+', '-', '=') to the pop line using `SettlementStatus::popTrend`. Already
  available in `SettlementStatus`. Format: "[12/35 pop +]" or "[12/35 pop -]". Uses plain '+'
  and '-' ASCII.

- [x] **Elder deathbed savings inheritance** — Elder 0.8 fraction and isElder check were already
  implemented. Added "(elder)" tag to estate log message for visual distinction.

- [x] **Contentment milestone log** — Already implemented as "Suffering NPC log event".

- [x] **Estate log on need-death too** — Already works: inheritance block iterates all `toRemove`.

- [x] **Elder will tooltip line** — Added "Will: 80% to treasury" in Fade(GOLD, 0.5f) for
  elders with gold in the NPC hover tooltip.

- [x] **Estate size shown in settlement tooltip** — Added `pendingEstates` to SettlementStatus,
  populated in SimThread, displayed as "Estates: ~Ng" in settlement tooltip.

- [x] **Profession shown in migration log** — Added arrival log with profession in MIGRATING block.

- [x] **Skill reset on profession change** — On profession change during migration, halve old
  skill and boost new by +0.1.

- [x] **Migrant welcome log at destination** — Added welcome log with pop count on arrival.

- [x] **Profession colour in residents list** — Fa=green, Wa=skyblue, Lu=brown, Me=gold.

- [x] **Settlement specialty label in stockpile header** — Specialty label shown in stockpile panel header.

- [x] **Idle NPC count in stockpile panel** — Shows "Working: N / Idle: N" in Treasury line.

- [x] **Profession vocation label in NPC tooltip** — Shows "[vocation]" in gold when profession matches best skill.

- [x] **Skill milestone log** — Logs Journeyman/Master crossings at worksite.

- [x] **Profession match indicator on world dot** — Gold ring on working NPCs in vocation.

- [x] **Rumour carrier visible in tooltip** — Shows "(spreading: plague/drought/bandits)" in faint yellow.

- [x] **Rumour immunity after delivery** — 48 game-hour timed immunity replaces permanent flag.

- [x] **GoodHarvest rumour seeding** — New RumourType, seeded on Harvest Bounty, -5% food on arrival.

- [x] **Illness recovery log** — Logs recovery on illnessTimer transition to zero.

- [x] **Illness NPC dot tint** — Purple tint on sick NPC/Child dots.

- [x] **Illness contagion between NPCs** — 10% contagion in gossip loop, same illness type.

- [x] **Skill discovery location in log** — Added settlement name to skill insight log.

- [x] **Illness source context in log** — Added settlement name to illness log.

- [x] **Harvest bonus shown in tooltip** — "Good harvest bonus" line in faint gold.

- [x] **Celebrating NPC glow ring** — Pulsating gold ring on celebrating NPCs.

- [x] **Morale colour on settlement ring** — Ring tinted by morale when no event active.

- [x] **Morale trend arrow in world status bar** — +/- appended to M:XX% label.

- [x] **Trade delivery log with morale** — Logs delivery with cargo summary and morale bump.

- [x] **Hauler home morale penalty on bankruptcy** — In `EconomicMobilitySystem.cpp`'s hauler
  bankruptcy block (where `BanditTag` or demotion happens), apply a morale penalty of -0.03 to
  the hauler's home settlement. A merchant going bankrupt is demoralising for the community. Use
  `registry.try_get<Settlement>(home.settlement)->morale -= 0.03f` with a `std::max(0.f, ...)`
  clamp. No new components needed.

- [x] **Stockpile abundance log event** — In `RandomEventSystem::Update`'s per-settlement loop,
  when the abundance condition fires (all three stockpiles ≥ 80) for the first time, log
  "Prosperity: [settlement] has abundant stores — morale rising." Use a `static
  std::set<entt::entity> s_loggedAbundance`; insert on first trigger, erase when any stockpile
  drops below 40. One-shot per settlement per abundance period to avoid log spam.

- [x] **Scarcity morale penalty** — In `RandomEventSystem::Update`'s per-settlement loop, after
  the abundance check, add a scarcity check: if any stockpile (food, water, wood) is below 10
  units, apply -0.003 morale per game-hour. This makes shortages actively harmful to morale
  rather than just neutral. Use the same `registry.try_get<Stockpile>(e)` already accessed.
  No new components needed.

- [ ] **Hauler profit margin in tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), when hovering
  a hauler with cargo, show "Profit: ~Ng" calculated as `money.balance - buyPrice * cargoQty`
  (approximate margin from last trade). Read `Hauler::buyPrice` and `Inventory::contents` from
  `AgentEntry` fields (add `float haulerBuyPrice` and `int haulerCargoQty` to `AgentEntry` in
  `RenderSnapshot.h`, populate in SimThread). Display in green if positive, red if negative.

- [ ] **Bankrupt hauler flash before demotion** — In `EconomicMobilitySystem.cpp`, when a
  hauler's `s_bankruptTimer` exceeds `BANKRUPTCY_HOURS * 0.75` (18h), set a `bool nearBankrupt`
  flag on a new `AgentEntry` field. In `GameState.cpp`'s agent render loop, draw a faint red
  pulsating ring (like celebrating but red) around near-bankrupt haulers. Gives visual warning
  before demotion. Add `bool nearBankrupt` to `AgentEntry` in `RenderSnapshot.h`.

- [ ] **Abundance end log** — In `RandomEventSystem::Update`'s abundance block, when a
  settlement exits abundance (was in `s_loggedAbundance` but `abundant` is now false and not yet
  below the scarcity reset), log "Abundance fading at [settlement] — stores declining." before
  erasing from the set. Gives a narrative arc: prosperity → warning → scarcity.

- [ ] **Hauler graduation celebration** — In `EconomicMobilitySystem.cpp`'s NPC→Hauler
  graduation block, set the new hauler's `AgentState::behavior = Celebrating` for 2 game-hours
  (set `celebrateTimer = 2.f` on `DeprivationTimer`). Also bump home settlement morale by +0.02.
  Becoming a hauler is a proud moment for the community.

- [ ] **Scarcity log event** — In `RandomEventSystem::Update`'s scarcity check, log "Shortage:
  [settlement] running low on [resource]" once per scarcity period per resource. Use a
  `static std::map<entt::entity, int> s_loggedScarcity` keyed by entity, value is a bitmask
  (bit 0=food, 1=water, 2=wood). Set bit on first trigger, clear when that resource rises above
  20. Keeps scarcity visible in the event log without spam.

- [ ] **Morale-dependent work speed** — In `ProductionSystem.cpp`'s per-worker contribution
  block, multiply worker output by a morale factor: `1.0 + 0.3 * (morale - 0.5)`. At morale
  0.8 workers produce +9%, at 0.2 they produce -9%. Read morale from `try_get<Settlement>` via
  worker's `HomeSettlement`. Makes morale mechanically important beyond cosmetic display.

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

- [ ] **Strike duration shown in tooltip** — Extend the "On strike" line in `HUD::DrawHoverTooltip`
  to show the remaining duration: `"On strike (%.0f h left)"` using `DeprivationTimer::strikeDuration`
  exposed as a new `float strikeHoursLeft` field in `AgentEntry`. Populate it in `SimThread::WriteSnapshot`
  alongside `onStrike`. Divide `strikeDuration` by 60 to convert sim-seconds to game-hours.

- [ ] **Settlement morale shown in NPC tooltip** — Add the home settlement's morale as a faint
  line at the bottom of the hover tooltip: `"Home morale: 72%"` (skip if NPC has no home).
  Expose it via a new `float homeMorale` field in `AgentEntry` (default -1 = no home).
  Set it in `SimThread::WriteSnapshot` via `registry.try_get<Settlement>(hs->settlement)->morale`.
  In `HUD::DrawHoverTooltip`, if `homeMorale >= 0`, add the line coloured GREEN/YELLOW/RED
  using the same thresholds as the morale bar in `DrawStockpilePanel`.

- [ ] **NPC reputation score** — Add a `Reputation` component (float `score`, initially 0) to
  each NPC. Increase it when the NPC gives charity (`charityTimer` resets), decrease it when
  they steal. Haulers gain +0.05 rep per completed delivery. In `HUD::DrawHoverTooltip`, expose
  the score as a `float reputation` field in `AgentEntry` and display `"Rep: +3.2"` (or negative
  in RED) when non-zero. This creates visible social hierarchy without requiring new UI panels.

- [ ] **Abundant-supply notification** — When the morale surplus bonus first activates for a
  settlement (i.e., was NOT abundant last tick, now IS abundant), log a one-time message to
  `EventLog`: `"Greenfield: plentiful supplies boost morale"`. Track this with a
  `bool abundantLastTick` field on `Settlement` (Components.h). Prevents silent changes and
  gives the player feedback that their supply investment is paying off.

- [ ] **Worker fatigue accumulation** — In `NeedDrainSystem.cpp`, when an NPC with `Schedule` is
  in `Working` state and energy need falls below 0.2, apply a 20% production penalty via a new
  `bool fatigued` bool on the `Schedule` component. In `ProductionSystem.cpp`, multiply yield by
  `0.8f` if `sched->fatigued`. `fatigued` is cleared when energy recovers above 0.5 (also in
  `NeedDrainSystem`). This makes sleep deprivation have visible production consequences.

- [ ] **Settlement founding event log** — In `SimThread::ProcessInput` (SimThread.cpp), when the
  player successfully founds a settlement (P key, cost 1500g), log it to `EventLog`: `"Player
  founded [settlement name] at (x, y)"`. Look up the newly created Settlement entity immediately
  after `WorldGenerator::FoundSettlement` returns. This makes founding visible in the event log
  alongside other major events.

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

- [ ] **Relation score shown in road tooltip** — The road hover tooltip (`HUD::DrawRoadTooltip` in
  HUD.cpp) already shows connected settlement names. Extend it to also show the current relation
  score between those two settlements: `"Relations: -0.62 (Rivals)"` / `"Relations: +0.71 (Allies)"`.
  Read from `SettlementEntry::relations` (expose as `std::map<std::string,float>` keyed by
  settlement name) populated in `SimThread::WriteSnapshot`'s settlement loop from `s.relations`.

- [ ] **Alliance trade bonus log** — When a hauler completes a delivery between allied settlements
  (relation score > 0.5), log it to `EventLog` as `"Ally trade: Greenfield → Wellsworth (+5g bonus)"`.
  In `TransportSystem.cpp`, after the existing ally tax reduction block, check the relation score
  and push a log entry using the hauler's carry quantity and the bonus gold saved. This makes the
  alliance benefit tangible and visible.

- [ ] **Rival settlement tax UI indicator** — In `HUD::DrawRoadTooltip` (HUD.cpp), when the road
  connects rival settlements (relation < -0.5), append a red `"(+30% rival surcharge)"` note to
  the road condition line. Read from the same `SettlementEntry` relation data. Helps player
  understand why haulers on that route are less profitable.

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

- [ ] **Sleep arrival indicator** — When an NPC is walking toward their settlement to sleep
  (`state.behavior == AgentBehavior::Sleeping` and not yet at `SLEEP_ARRIVE` distance),
  draw a faint dim ring around their world dot in `GameState::Draw`. After the main
  `DrawCircleV`, add: if `a.behavior == AgentBehavior::Sleeping && !a.atHome`, call
  `DrawCircleLinesV({a.x, a.y}, a.size + 2.f, Fade(BLUE, 0.35f))`. Add `bool atHome = false`
  to `AgentEntry` (RenderSnapshot.h); set it in `SimThread::WriteSnapshot` when sleeping and
  within `SLEEP_ARRIVE` of home. Makes it easy to spot NPCs still commuting at night.

- [ ] **Evening gathering scatter** — Currently all Idle NPCs wander independently. In
  `ScheduleSystem.cpp`'s leisure-wander block (lines ~378–385), when `hour >= 18 && hour < 22`,
  bias the wander target toward the settlement centre rather than the full `LEISURE_RADIUS`.
  Replace `s_radius(s_rng)` with `s_radius(s_rng) * 0.4f` during evening hours so NPCs
  cluster near home in the evening — visible as a loose gathering on the world map.

- [ ] **Facility crowding log** — When 4+ NPCs arrive at the same `ProductionFacility` in
  the same tick, log a flavour event: `"Greenfield farm is crowded — 5 workers competing."`.
  In `ProductionSystem.cpp`, count workers per facility (already iterated in
  `registry.view<Position, AgentState, HomeSettlement>`). When the worker count for a facility
  exceeds 3 for the first time this game-day, push the log. Track last-logged day with a
  `static std::map<entt::entity, int> s_lastCrowdLog` keyed by facility entity.

- [ ] **Chat indicator on world dot** — When an NPC has `chatTimer > 0`, draw a small pulsing
  ring around their world-map dot to signal that they're in conversation. In `GameState::Draw`,
  after the main `DrawCircleV`, add: if `a.chatting && a.role == RenderSnapshot::AgentRole::NPC`,
  call `DrawCircleLinesV({a.x, a.y}, a.size + 3.f, Fade(YELLOW, 0.45f))`. Add `bool chatting = false`
  to `AgentEntry` in `RenderSnapshot.h`; set in `SimThread::WriteSnapshot` when
  `dt->chatTimer > 0.f`. Makes evening social clusters visible on the map.

- [ ] **Chat log entry** — When a chat pair forms (chatTimer is set > 0 in `AgentDecisionSystem`),
  log a brief flavour message: `"Aldric and Mira chat near Greenfield."`. After setting both
  `timer.chatTimer` and `oTimer.chatTimer`, push to `EventLog` using names from `registry.try_get<Name>`.
  Only log ~10% of chats (gate with `std::uniform_real_distribution<float>(0,1)(s_chatRng) < 0.1f`)
  to avoid log spam. Gives the event log some social flavour.

- [ ] **Evening gathering density ring** — During hours 18–21, draw a faint translucent circle
  around each settlement on the world map showing how many NPCs are gathered there. In
  `GameState::Draw` (after drawing settlement dots), iterate `snap.settlements` and count how many
  agents have matching `homeSettlName`. Draw `DrawCircleLinesV` with radius scaled by `pop / popCap`
  in `Fade(SKYBLUE, 0.15f)`. Requires no new snapshot fields — use existing agent data.

- [ ] **Family reunion log on founding** — When a new settlement is founded via the P key in
  `SimThread::ProcessInput` (SimThread.cpp), scan existing NPCs for any with `FamilyTag` whose
  `HomeSettlement` is the founding player's nearest settlement. If two or more share the same
  `FamilyTag::name`, log "The [name] family helped found [settlement name]." immediately after
  the founding log. Pure flavour; no new components.

- [ ] **Family size shown in stockpile residents panel** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), after the existing resident name+profession+gold line, append " ×N" in
  `Fade(GRAY, 0.7f)` when N ≥ 2 members of the same family are resident. Requires adding
  `familyName` (std::string) to `StockpilePanel::AgentInfo` in `RenderSnapshot.h`; populate
  it in `SimThread::WriteSnapshot` near the `StockpilePanel` residents block via
  `registry.try_get<FamilyTag>(npcEntity)->name`.

- [ ] **Orphan adoption** — When a child has `ChildTag` but no valid `HomeSettlement` (orphaned
  by family dissolution or settlement collapse), any adult NPC at a settlement with `pop < popCap - 1`
  who has `charityTimer == 0` can adopt them. In `AgentDecisionSystem`, after the charity block,
  add: if the adult spots a nearby orphan (ChildTag, no home, within 60 units), set the orphan's
  `HomeSettlement` to the adult's home, assign the adult's `FamilyTag::name` to the orphan (or
  emplace a new FamilyTag), and log "X took in orphan Y at Z."

- [ ] **Largest family in settlement header** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), in the header section after the treasury/workers line, add a one-liner
  showing the most populous family at this settlement: `"Largest family: Smith ×4"`. Build the
  count by iterating `panel.residents` and finding the `familyName` with the highest count.
  Only show when at least one family has ≥ 2 members. No new snapshot fields needed.

- [ ] **Family wealth total in stockpile panel** — Extend the family `×N` display to also show
  the combined gold of all family members in the visible residents list. After `×N` add
  `(total Xg)` in `Fade(GOLD, 0.6f)`. Sum `r.balance` for all residents sharing that
  `familyName`. This makes family economic power visible at a glance.

- [ ] **Skill degradation with age** — In `ScheduleSystem.cpp`'s skill decay block (the adult
  `!Working` decay path, lines ~310–318), add an additional age-based multiplier: when
  `age.days > 65`, multiply the decay rate by 2 so elders lose skills twice as fast.
  This creates a visible lifecycle — peak working years mid-life, gradual decline as elders.
  No new components; uses the existing `SKILL_DECAY_PER_HOUR` constant and `age2->days` check.

- [ ] **Elder mentor bonus** — In `ScheduleSystem.cpp`'s skill-at-worksite block (where
  `SKILL_GAIN_PER_GAME_HOUR` is applied), if there is an elder NPC (age > 60) currently `Working`
  at the same `ProductionFacility`, give all other younger workers at that facility a +20% skill
  gain multiplier (`gainMult *= 1.2f`). Check for nearby elders via a small inner loop over the
  same facility view, capped to avoid O(n²) blow-up by breaking after finding one elder. Log
  `"[Name] mentored workers at [settlement]."` once per game-day per facility.

- [ ] **Peak-age production bonus** — In `ProductionSystem.cpp`, when a worker NPC's age is
  between 25 and 55 (prime working years), apply a small +10% production bonus to their
  contribution (`workerContrib *= 1.1f`). Read via `registry.try_get<Age>(workerEntity)`.
  This completes the lifecycle arc: gradual skill growth in youth, peak output in prime years,
  decline in old age — all without new components.

- [ ] **Elder wisdom event** — In `RandomEventSystem`'s per-NPC event tier (personal event
  system), add a new case: when a NPC's age > 70 and has a skill ≥ 0.6, fire a rare one-time
  "wisdom transfer" event — boost one random co-settled younger NPC's skill by 0.1, capped at
  0.8. Log `"Elder [Name] passed their knowledge to [Name2] at [settlement]."` Guard with a
  `wisdomFired` bool on `DeprivationTimer` (reuse `illnessNeedIdx` as a flag or add a dedicated
  bool) to prevent repeated firings.

- [ ] **Friendship shown in NPC tooltip** — Surface the strongest friendship in the hover tooltip.
  Add `std::string bestFriendName` and `float bestFriendAffinity` to `AgentEntry` in
  `RenderSnapshot.h`. In `SimThread::WriteSnapshot`, iterate the NPC's `Relations::affinity` map,
  find the highest-affinity valid entity, and set both fields. In `HUD::DrawHoverTooltip`, when
  `bestFriendAffinity >= 0.5`, show `"Friend: Aldric (82%)"` in `Fade(LIME, 0.75f)`. Same
  `lineCount`/`pw` pattern as existing flag lines.

- [ ] **Friend grief on death** — In `DeathSystem.cpp`'s inheritance loop, after the family
  dissolution check, scan all NPCs who have the dead entity in their `Relations::affinity` map
  with affinity ≥ 0.5. For each friend, lower their home settlement's morale by -0.03 and
  set their `DeprivationTimer::helpedTimer = 0.f` (clears "recently helped" so the grief resets
  social warmth). Log `"[Name] mourns the loss of [dead Name] at [settlement]."` Only log
  for the 2 closest friends (highest affinity) to avoid log spam.

- [ ] **Friendship bonus on birth** — In `BirthSystem.cpp`, when a birth occurs, check if the
  parent NPC has any friends (Relations::affinity ≥ 0.5) at the same settlement. If so, give
  those friends a small morale boost: `settl->morale = std::min(1.f, settl->morale + 0.01f)`
  per friend (capped at 2 boosts). Log `"[FriendName] celebrates [Name]'s new child."` This
  makes social bonds visible in the community's response to new births.

- [ ] **Strike grievance log** — In `ScheduleSystem.cpp`, when `strikeDuration` transitions from
  0 to >0 (strike begins), log "Workers strike at [Settlement] — [N] workers walk out (morale:
  XX%)." Count affected NPCs by iterating the same HomeSettlement view used elsewhere. When the
  strike ends (transition >0 to 0, already handled for morale recovery), log "[Settlement] strike
  ends — morale recovering." This makes strikes a proper story beat rather than a silent mechanic.

- [ ] **Morale-driven migration push** — In `AgentDecisionSystem.cpp`'s migration scoring, add a
  morale push factor. When the NPC's home settlement morale < 0.25, add +0.3 to migration score
  (making them more likely to leave). When home morale > 0.7, add -0.2 (making them more likely
  to stay). Read morale from `registry.get<Settlement>(home.settlement).morale`. This creates a
  feedback loop: low morale → strikes → people leave → smaller settlement, which either recovers
  or collapses organically.

- [ ] **Rivalry tariff shown in hauler tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), when the
  hovered entity is a hauler with a `cargoSource` set, check if the destination settlement has
  `relations[cargoSource] < -0.5`. If so, show a red "Rivalry tariff (+30%)" line below the cargo
  info. Read relations from `SettlementStatus` or add a `bool rivalryTariff` to `AgentEntry` and
  set it in SimThread's hauler snapshot block. This surfaces the rivalry mechanic to the player.

- [ ] **Alliance trade log** — In `TransportSystem.cpp`'s delivery block, when both source and
  destination settlements have `relations[other] > 0.5` (allied), log "Allied trade: [Hauler]
  delivers [N] [resource] from [Source] to [Dest] (boosted)." at 1-in-3 frequency to avoid spam.
  Use a static counter per hauler entity. This complements the rivalry log by making positive
  relations also visible as story beats.

- [ ] **Rivalry decay on successful trade** — In `TransportSystem.cpp`'s delivery completion block,
  after crediting the destination treasury, nudge `dest.relations[source]` by +0.02 per delivery.
  This creates a natural path out of rivalry: continued trade slowly rebuilds relations. Combined
  with the -0.04 per hauler delivery on the exporter side, net effect is that importing settlements
  slowly warm to their trade partners while exporters cool — modelling the asymmetry of trade.

- [ ] **Alliance production bonus** — In `ProductionSystem.cpp`, after the existing morale modifier
  block, check if the worker's home settlement has any ally (any `relations` entry > 0.5). If so,
  apply a +5% production bonus (multiply `contribution` by 1.05f). This models allied settlements
  sharing techniques/tools. Cap at one bonus regardless of number of allies. Log nothing — the
  effect is visible through faster resource accumulation.

- [ ] **Rivalry morale drain** — In `RandomEventSystem::Update`'s settlement loop, after the morale
  drift block, check if the settlement has any rival (any `relations` entry < -0.5). If so, apply
  a slow morale drain of -0.001 per game-hour per rival (uncapped, stacks). This creates pressure:
  prolonged rivalry grinds morale down, increasing strike risk, which can push NPCs to migrate away.
  Combined with morale-driven migration push, this creates organic settlement decline under rivalry.

- [ ] **Hauler gate tax shown in tooltip** — Add `bool gateTaxed = false` to `AgentEntry` in
  `RenderSnapshot.h`. In `SimThread::WriteSnapshot`'s hauler snapshot block, set it from a new
  `Hauler::lastGateTaxed` bool (set true in TransportSystem when gate tax fires, cleared on next
  delivery). In `HUD::DrawHoverTooltip`, when `gateTaxed` is true, show a red "Gate-taxed at
  last delivery" line. This surfaces the rivalry harassment mechanic to the player via the UI.

- [ ] **Hauler profit tracking** — Add `float lastTripProfit = 0.f` and `float totalProfit = 0.f`
  to the `Hauler` component in `Components.h`. Set `lastTripProfit` in `TransportSystem.cpp`'s
  delivery block (earned minus buy cost). Accumulate into `totalProfit`. Add both to `AgentEntry`
  in `RenderSnapshot.h` and display in `HUD::DrawHoverTooltip` as "Last trip: +Xg" and
  "Lifetime: +Xg". No economy impact — purely observability for the player.

- [ ] **Elder mentorship skill transfer** — In `ProductionSystem.cpp`'s worker counting loop,
  when a working elder (age > 60) is at the same settlement as non-elder workers, boost those
  workers' relevant `Skills` field by +0.002 per game-hour (mentorship). Use `registry.try_get<Skills>`
  on each non-elder worker in a second pass after counting. Cap skills at 1.0 as usual. This
  creates emergent value in elder presence — settlements with working elders upskill faster.

- [ ] **Elder retirement event** — In `RandomEventSystem::Update`'s per-NPC loop, when an NPC
  reaches age 55 (check `Age::days` crossing 55 with a static `std::set<entt::entity>` to fire
  once), log "X retires from active work at Y." and set `AgentState::behavior` to Idle permanently
  by adding a `bool retired = false` flag to `DeprivationTimer`. `ScheduleSystem` skips work
  assignment for retired NPCs. They still contribute the settlement elder bonus but no longer
  as active workers — creating a natural workforce lifecycle.

- [ ] **Working elder shown in tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), for named NPCs
  with `ageDays > 60` who are Working (check `AgentEntry::behavior`), show "Elder worker (+5%
  knowledge)" in `Fade(GOLD, 0.6f)` below the age line. No new snapshot fields needed — `ageDays`
  and behavior are already in `AgentEntry`. Surfaces the elder knowledge bonus to the player.

- [ ] **Spring migration surge** — In `AgentDecisionSystem::Update`'s migration trigger block,
  add a Spring bonus: when `Season::Spring`, multiply the migration chance by 1.5 (NPCs are more
  likely to seek new settlements after winter). This pairs with the Winter penalty to create a
  seasonal migration rhythm: hunker down in Winter, move in Spring. Read season from `TimeManager`
  already in scope at the top of `Update`.

- [ ] **Migration log with season context** — In `AgentDecisionSystem::Update`'s MIGRATING arrival
  block (where the NPC settles at the new home), append the current season to the existing migration
  log message. Change from "Mira moved to Thornvale" to "Mira moved to Thornvale (Spring)". Read
  season from `TimeManager` already accessible in the Update scope. No new components needed —
  purely enriches the event log narrative.

- [ ] **Wealth inequality indicator in stockpile panel** — In `RenderSystem::DrawStockpilePanel`,
  after the residents section, compute a Gini-like ratio: `(richest.balance - poorest.balance) /
  max(1, richest.balance)` from `panel.residents.front()` and `.back()`. Show "Inequality: XX%"
  in dim text. Color RED when > 0.8 (high inequality), YELLOW > 0.5, GREEN otherwise. No new
  snapshot fields — computed from existing residents data at render time.

- [ ] **Poorest NPC charity priority** — In `AgentDecisionSystem.cpp`'s charity block, when
  selecting a charity recipient, prefer the poorest NPC at the settlement rather than the first
  found. Iterate the `HomeSettlement` view, track the NPC with lowest `Money::balance` that meets
  the need threshold, and give to them instead. This makes charity targeted rather than random,
  creating more meaningful wealth redistribution among NPCs.

- [ ] **Profession change on skill discovery** — In `RandomEventSystem.cpp`'s skill discovery
  personal event block, after boosting a random skill by +0.08–0.12, check if the boosted skill
  now exceeds the NPC's current profession skill by > 0.15. If so, change `Profession::type` to
  match the new dominant skill (e.g., farming skill > water_drawing → become Farmer). Log
  "X discovers talent for farming, changes profession at Y." This makes skill discovery a career
  turning point rather than just a stat bump.

- [ ] **Profession count in world status bar** — In `HUD::DrawWorldStatus` (HUD.cpp), after the
  morale label for each settlement, add a compact "F/W/L" count showing farmers/water carriers/
  lumberjacks. Read from a new `int farmers, waterCarriers, lumberjacks` triple in `SettlementStatus`
  (RenderSnapshot.h), populated in SimThread's world-status loop by counting `Profession::type`
  for NPCs homed at each settlement. Gives at-a-glance workforce composition per settlement.

- [ ] **Dynasty wealth tooltip** — In `RenderSystem::DrawStockpilePanel`'s dynasty line, when
  hovering the "Families: N dynasties" text (mouse Y detection like the residents header tooltip),
  show a tooltip listing each dynasty name and combined wealth. Iterate `panel.residents`, group
  by `familyName`, sum `balance` per family. Show top 3 families sorted by total wealth in GOLD.
  No new snapshot fields needed — computed from existing residents data at render time.

- [ ] **Dynasty founding log** — In `BirthSystem.cpp`, when a birth results in a second NPC
  sharing the same `FamilyTag::name` at a settlement (checking the `FamilyTag` view), log
  "[Family] dynasty established at [Settlement] — 2 members." Use a static
  `std::set<std::string>` to fire once per family name. This makes family growth a visible
  narrative event, connecting births to the dynasty mechanic.

- [ ] **Gossip visual indicator** — In `GameState.cpp`'s agent render loop, when an NPC's
  `gossipNudgeTimer > 0` (currently drifting toward another NPC), draw a faint speech-bubble
  indicator: a small `DrawCircle` at (a.x + 6, a.y - 8, 3, Fade(WHITE, 0.3f))`. Add
  `bool gossiping = false` to `AgentEntry` in `RenderSnapshot.h`, set from
  `dt->gossipNudgeTimer > 0.f` in SimThread's agent snapshot block. This makes the gossip
  mechanic visible without being intrusive.

- [ ] **Gossip affinity boost** — In `AgentDecisionSystem.cpp`'s gossip nudge block, after
  setting `gossipNudgeTimer = 3.f`, also increment `Relations::affinity` between the two NPCs
  by +0.005 (much smaller than the chat boost of +0.02, since gossip drift is more casual).
  Use `registry.try_get<Relations>` on both entities. This makes the visual gossip drift
  also functionally meaningful — frequent proximity builds weak bonds over time.

- [ ] **Elder death succession log** — In `DeathSystem.cpp`, when the eldest NPC at a settlement
  dies (old-age death, `Age::days` is highest among NPCs homed there), log "[Name] the elder of
  [Settlement] passes. [NextEldest] is now the eldest." Find the next-eldest by iterating the
  `HomeSettlement` + `Age` view for the same settlement. This makes elder succession a narrative
  beat, connecting the [Elder] badge to the death system.

- [ ] **Elder wisdom tooltip line** — In `HUD::DrawHoverTooltip` (HUD.cpp), for NPCs with
  `ageDays > 60` who are the eldest at their settlement (add `bool isSettlementEldest = false`
  to `AgentEntry` in RenderSnapshot.h, set in SimThread by comparing against the eldest tracked
  per settlement), show "Settlement elder — respected" in `Fade(ORANGE, 0.7f)` below the age
  line. Surfaces the eldest mechanic to the hover tooltip without cluttering the panel.

- [ ] **Contentment trend arrow in status bar** — In `HUD::DrawWorldStatus` (HUD.cpp), after the
  "C:XX%" contentment label, append a '+' or '-' trend character when contentment changes >5%
  between snapshots. Add `float prevAvgContentment = 1.f` to `SettlementStatus` in RenderSnapshot.h.
  Set it in SimThread's world-status loop from a `std::map<entt::entity, float> m_contentPrev`
  member (same pattern as `m_popPrev`). Draw '+' in GREEN, '-' in RED after the percentage.

- [ ] **Low contentment event trigger** — In `RandomEventSystem::Update`'s settlement loop, when
  average contentment drops below 0.25 (compute the same way as SimThread: avg of 4 needs across
  homed NPCs), trigger a "Desperation" event: 30% chance per game-day that a random NPC steals
  from the stockpile (same logic as existing theft in `AgentDecisionSystem` but triggered by
  settlement-wide suffering rather than individual need). Log "Desperation theft at [Settlement]."
  This connects the contentment metric to emergent behavior.

- [ ] **Celebration visual on high contentment** — In `GameState.cpp`'s agent render loop, when
  an NPC's contentment (average of 4 needs from `AgentEntry`) exceeds 0.9, draw a small gold
  sparkle: `DrawCircleV({ a.x + 4, a.y - 6 }, 2.f, Fade(GOLD, 0.5f + 0.3f * sinf(time * 5)))`.
  Use `GetTime()` for the sine pulse. No new snapshot fields needed — compute contentment inline
  from existing `hungerPct`, `thirstPct`, `energyPct`, `heatPct` on `AgentEntry`.

- [ ] **Contentment-based idle behavior** — In `AgentDecisionSystem.cpp`'s idle block (the
  `critIdx == -1` branch), when NPC contentment (avg of 4 needs) > 0.85, instead of standing
  still, apply a gentle random walk: `vel.vx = speed * 0.2f * (rng() % 3 - 1)`. This makes
  content NPCs visually "wander happily" while stressed NPCs stay still. Use the existing
  `s_chatRng` for randomness. Check once per 5 game-seconds using `chatTimer` as guard.

- [ ] **NPC recovery log event** — In `RandomEventSystem::Update`'s per-NPC loop, complement
  the suffering log: when an NPC's contentment rises back above 0.5 after being desperate
  (tracked in the existing `s_desperateLogged` static set), log "X recovers at Y" before
  erasing from the set. Gives the player closure on suffering events and shows the world healing.

- [ ] **Settlement specialisation tooltip** — In `HUD.cpp`'s settlement hover tooltip (the
  `DrawSettlementTooltip` section), after the existing modifier line, append a line showing
  the settlement's `specialty` from `SettlementEntry` (already populated). Format: "Specialty:
  Farming" in WHITE. If `specialty` is empty, skip the line. No new snapshot fields needed.

- [ ] **Hauler profit/loss tracker** — Add a `float lifetimeProfit = 0.f` field to the `Hauler`
  component in `Components.h`. In `TransportSystem.cpp`, when a hauler sells cargo at the
  destination gate, accumulate `lifetimeProfit += saleRevenue - buyCost` (where buyCost comes
  from `buyPrice * qty`). Expose this in `AgentEntry` as `haulerProfit` and populate it in
  `SimThread::WriteSnapshot()`. Display in the agent hover tooltip in `GameState.cpp`'s hauler
  tooltip section as "Profit: +X.Xg" (GREEN) or "Loss: -X.Xg" (RED).

- [ ] **NPC memory of past settlements** — Add a `std::vector<std::string> pastHomes` (max 3) to
  `DeprivationTimer` in `Components.h`. In `AgentDecisionSystem.cpp`'s migration block, when an
  NPC changes `HomeSettlement`, push the old settlement name onto `pastHomes` (pop front if > 3).
  In `SimThread::WriteSnapshot()`, expose as `std::string migrationHistory` on `AgentEntry`
  (comma-separated). Display in the NPC hover tooltip in `GameState.cpp` as "Formerly: A, B".

- [ ] **Plague survivor immunity** — In `RandomEventSystem::KillFraction`, NPCs that survive a
  plague roll get a `bool plagueImmune = false` flag on `DeprivationTimer` in `Components.h`.
  Set it true after surviving. In `KillFraction`, skip immune NPCs from the kill list entirely.
  In `SimThread::WriteSnapshot()`, expose as `bool plagueImmune` on `AgentEntry` and render a
  small blue circle behind immune NPCs in `GameState.cpp`'s agent draw loop.

- [ ] **Pop trend in world status bar** — In `HUD.cpp`'s `DrawWorldStatus`, after the existing
  population count for each settlement, append the `popTrend` char from `SettlementStatus`
  (already populated as '+', '=', or '-'). Render '+' in GREEN, '-' in RED, '=' omitted. Uses
  existing snapshot data — no sim changes needed, purely a HUD display addition.

- [ ] **Plague death log names victims** — In `RandomEventSystem::KillFraction`, before
  destroying each NPC, check if they have a `Name` component and collect up to 3 victim names.
  Return them via a new `std::vector<std::string>` out-parameter (or change return type to a
  struct). In both the initial plague eruption (case 3) and spread block log messages, append
  "victims: Alice, Bob, ..." after the death count. Makes plague events feel personal.

- [x] **Morale recovery log** — In `RandomEventSystem::Update`'s settlement loop, the unrest
  recovery message currently says "Tensions ease in X — morale recovering". Add the current
  morale percentage: "Tensions ease in X — morale recovering (42%)". Use
  `(int)(s.morale * 100)` inline in the existing `log->Push` call. One-line change, no new
  fields needed. *(Implemented as part of Unrest pop context task.)*

- [ ] **NPC flee from plague** — In `AgentDecisionSystem.cpp`, when an NPC's home settlement has
  `modifierName == "Plague"` and the NPC's contentment < 0.4, trigger migration to the nearest
  non-plague settlement. Use the existing migration logic but bypass the normal migration cooldown.
  Log "X flees plague at Y" via `EventLog`. Makes plague events create visible refugee movement.

- [ ] **Settlement founding log with founder name** — In `ConstructionSystem.cpp`'s settlement
  founding block (the P-key handler), the log currently says "New settlement founded: X". Add the
  player's name from the `Name` component: "X founds new settlement: Y". If the founder has no
  Name component, fall back to "New settlement founded: Y". One-line snprintf change.

- [ ] **Hauler route tooltip in HUD** — In `GameState.cpp`'s hauler hover tooltip section, when
  a hauler has `hasRouteDest == true` in `AgentEntry`, append a line "Route: → Wellsworth (3 food,
  2 wood)" showing `destSettlName` and cargo contents from `AgentEntry::cargo`. Format each
  resource type as "N type" comma-separated. No new snapshot fields needed — all data already
  in `AgentEntry`.

- [ ] **Settlement tooltip: price summary** — In `DrawSettlementTooltip` (HUD.cpp), add a line
  showing market prices: "Prices: F:1.2 W:0.8 L:2.1" using `SettlementStatus::foodPrice`,
  `waterPrice`, `woodPrice` (already available via the `status` pointer). Color each price
  GREEN if < 2.0, YELLOW if 2.0-5.0, RED if > 5.0. Helps players spot trade opportunities
  without opening the market overlay.

- [ ] **NPC grudge after theft** — In `ConsumptionSystem.cpp`, when an NPC steals from a
  stockpile (the `recentlyStole` flag path), record the victim settlement entity on a new
  `entt::entity grudgeTarget = entt::null` field in `DeprivationTimer` (Components.h). In
  `AgentDecisionSystem.cpp`'s migration scoring, apply a -0.3 penalty to the grudge target
  settlement. Clear grudge after 48 game-hours via a `float grudgeTimer = 0.f` on
  `DeprivationTimer`. Makes theft have social consequences — thieves avoid returning.

- [ ] **Settlement tooltip: active workers count** — In `DrawSettlementTooltip` (HUD.cpp), add
  a line "Workers: N" showing how many NPCs at this settlement currently have
  `AgentBehavior::Working`. Add `int workerCount = 0` to `SettlementStatus` in
  `RenderSnapshot.h`. Populate in SimThread's world-status loop by counting NPCs with
  `HomeSettlement` matching this settlement AND `AgentBehavior::Working` in their `Velocity`
  component's behavior field. Display in LIGHTGRAY after the elders line.

- [ ] **NPC wealth-based clothing color** — In `SimThread::WriteSnapshot()`'s agent loop, set
  `AgentEntry::color` based on `Money::balance`: < 10g = GRAY, 10-50g = BEIGE, 50-200g = SKYBLUE,
  > 200g = GOLD. Currently all NPCs share a single color per role. This visual distinction lets
  the player see wealth distribution at a glance. Only apply to NPCs (not Player, Hauler, Child).

- [ ] **Elder mentorship skill boost** — In `ProductionSystem.cpp`'s worker loop, when an elder
  (age > 60, from `Age` component) is working at a facility alongside younger NPCs, grant a
  +5% skill gain bonus to all non-elder workers at the same facility. Track by checking if any
  worker in the facility's NPC list has `age.days > 60`. Apply to the `Skills` component's
  relevant skill (farming/water/woodcutting matching facility output). Small per-tick increment
  `+= 0.0001f * gameHoursDt` when elder is present.

- [ ] **Death cause in event log** — In `DeathSystem.cpp`'s need-deprivation death block, the
  log currently says "X died at Y". Extend to include the specific need that killed them:
  "X died of hunger at Y" / "X died of thirst at Y" / "X died of exposure at Y". Check which
  need in `Needs::list` is at 0 (the lethal one) and map index 0→hunger, 1→thirst, 2→exhaustion,
  3→exposure. If multiple are at 0, pick the first.

- [ ] **NPC aging speed tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), for NPCs with
  `ageDays > 0`, show remaining lifespan estimate: "~N days left" calculated as
  `(int)(best->maxDays - best->ageDays)`. Display in Fade(LIGHTGRAY, 0.6f) after the age line.
  For elders (ageDays > 60), color it Fade(RED, 0.5f) to signal urgency. No new snapshot fields.

- [ ] **Hauler idle timer log** — In `TransportSystem.cpp`, track how many consecutive game-hours
  a hauler has been in `HaulerState::Idle` without finding a route. Add `float idleHours = 0.f`
  to `Hauler` component in `Components.h`. Increment by `gameHoursDt` when idle, reset to 0 on
  state transition. When `idleHours > 8.f`, log "Hauler X idle for 8h — no profitable routes"
  once (use a bool guard `idleLogged`). Reset guard on state change.

- [ ] **Settlement wealth inequality metric** — Add `float giniCoeff = 0.f` to `SettlementStatus`
  in `RenderSnapshot.h`. In SimThread's world-status loop, compute a simplified Gini coefficient
  from the `Money::balance` of all homed NPCs (exclude Player/Hauler). Sort balances, compute
  `sum(i * balance[i]) / (n * totalBalance)` scaled to 0-1. Display in settlement tooltip as
  "Inequality: XX%" in LIGHTGRAY. High inequality (> 60%) could trigger social events later.

- [ ] **NPC gratitude memory** — Add `std::string lastHelper` and `float gratitudeTimer = 0.f`
  to `DeprivationTimer` in `Components.h`. In `ConsumptionSystem.cpp`'s charity block, when an
  NPC receives charity, set `lastHelper` to the giver's `Name::value` and `gratitudeTimer = 24.f`
  (game-hours). In `AgentDecisionSystem.cpp`'s migration scoring, while `gratitudeTimer > 0`,
  apply a +0.3 bonus to the helper's home settlement score. Tick down in `NeedDrainSystem`.
  Creates emergent loyalty — helped NPCs prefer to stay near their benefactors.

- [ ] **Migration departure log with origin** — In `AgentDecisionSystem.cpp`'s migration decision
  block (where `state.behavior = AgentBehavior::Migrating` is set), the existing log says
  "X migrating A → B". Extend to include reason: append "— low food" / "— low water" /
  "— seeking work" based on which need or stockpile condition triggered migration. Check
  `timer.stockpileEmpty > 0` and `needs.list[i].value` to determine the primary driver.

- [ ] **Friend-follows-friend migration log** — In `AgentDecisionSystem.cpp`'s friend-follow
  block (where friends copy the migration target), log "Y follows X to Z" when a friend decides
  to migrate along. Use `registry.try_get<Name>` on both the original migrant and the follower,
  and `registry.try_get<Settlement>(dest)` for the destination name. Currently friend-following
  is silent — this surfaces a key social mechanic.

- [ ] **Skill decay for idle NPCs** — In `NeedDrainSystem.cpp`, when an NPC's `AgentBehavior`
  is `Idle` (from `AgentState`), slowly decay all three skills by `-0.0002f * gameHoursDt`
  (floored at 0.05f). Check `try_get<Skills>` and `try_get<AgentState>`. This creates pressure
  for NPCs to keep working and makes prolonged unemployment meaningful.

- [ ] **Profession change log event** — In `AgentDecisionSystem.cpp`'s MIGRATING arrival block,
  when the skill reset fires (old profession != new), log "X retrained: Farmer → Woodcutter
  (farming 45%→22%, woodcutting 38%→48%)" showing both old and new skill values. Use the
  skill values captured before and after the adjustment. Surfaces the retraining mechanic.

- [ ] **Settlement population cap warning** — In `RandomEventSystem::Update`'s settlement loop,
  when a settlement's pop reaches 90% of `popCap` (from `Settlement::popCap`), log once:
  "Ashford approaching capacity (32/35)". Track with a `std::set<entt::entity> m_capWarned`
  member on `RandomEventSystem`. Clear when pop drops below 80%. Count pop via the same
  `HomeSettlement` view pattern. No new components needed.

- [ ] **NPC nostalgia for birthplace** — In `AgentDecisionSystem.cpp`'s migration scoring
  (`FindMigrationTarget`), if an NPC's birthplace (stored as `entt::entity birthSettlement` on
  a new field in `DeprivationTimer` in `Components.h`) differs from current home, apply a
  +0.15 bonus to the birthplace score. Set `birthSettlement` in `BirthSystem.cpp` at NPC
  creation to `settl` (the settlement entity). Creates pull toward hometown after migrating away.

- [ ] **Profession distribution colour in stockpile header** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), the compact "Fa:4 Wa:3 Lu:2" profession distribution line (around line 188)
  is drawn in uniform `Fade(LIGHTGRAY, 0.7f)`. Split it into per-segment draws: render "Fa:4"
  in `Fade(GREEN, 0.6f)`, "Wa:3" in `Fade(SKYBLUE, 0.6f)`, "Lu:2" in `Fade(BROWN, 0.7f)`.
  Use individual `DrawText` + `MeasureText` calls instead of a single buffer. Matches the new
  per-resident profession colours.

- [ ] **NPC age-based work speed** — In `ProductionSystem.cpp`'s worker contribution calculation,
  scale `workerContrib` by age bracket: youth (age < 20) get 0.7x, prime (20-50) get 1.0x,
  mature (50-60) get 0.9x, elder (> 60) already has special handling. Read `Age::days` via
  `try_get<Age>` on the worker entity. Models physical capability varying with life stage.

- [ ] **Seasonal work schedule shift** — In `ScheduleSystem.cpp`, adjust NPC work-start and work-end
  hours by season: Summer has longer work hours (5:00–20:00), Winter has shorter (7:00–17:00),
  Spring/Autumn keep the current default. Read `TimeManager::season` and modify the `workStart`
  / `workEnd` thresholds accordingly. This makes seasonal daylight affect productivity naturally.

- [ ] **NPC mood emoji in tooltip** — In `HUD.cpp`'s NPC tooltip section, add a small text emoji
  or symbol reflecting the NPC's contentment level: "☺" (>0.7), "😐" (0.3-0.7), "☹" (<0.3).
  Draw it next to the NPC's name line using the existing `contentment` field from `AgentEntry`.
  Color-code: green for happy, yellow for neutral, red for unhappy.

- [ ] **Settlement resource deficit warning** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), after the per-resource lines, if any resource's `consRatePerHour` exceeds
  `prodRatePerHour` by more than 50%, draw a small warning like "⚠ Food deficit" in `Fade(RED, 0.7f)`.
  Uses existing `StockpilePanel::prodRatePerHour` and `consRatePerHour` maps — no new snapshot
  data needed. Helps the player spot settlements heading toward shortage.

- [ ] **NPC grudge after theft** — In `AgentDecisionSystem.cpp`, when an NPC steals from a
  stockpile (the `recentlyStole` flag path), record the victim settlement entity in a new
  `Grudge` component (`entt::entity target; float timer;`). While the grudge is active (e.g. 48
  game-hours), the NPC avoids migrating to that settlement. In `Components.h`, add the `Grudge`
  struct. Drain `timer` in `AgentDecisionSystem::Update` and remove the component when expired.

- [ ] **Vocation bonus production boost** — In `ProductionSystem.cpp`, when calculating per-worker
  contribution, check if the worker has `inVocation` status (profession matches highest skill).
  Workers in their vocation get a 10% production bonus on top of the normal skill multiplier.
  Use `try_get<Profession>` + `try_get<Skills>` to compute the vocation match inline, mirroring
  the same logic used in SimThread's snapshot loop.

- [ ] **NPC friendship from shared workplace** — In `AgentDecisionSystem.cpp`, when two NPCs
  are both Working at the same facility (same `ProductionFacility::settlement` and same
  `output`), increment a `Friendship` component between them. Add `struct Friendship { entt::entity
  friend_; float bond = 0.f; }` to `Components.h`. Bond grows by 0.01 per game-hour of shared
  work, capped at 1.0. When an NPC considers migrating, penalise destinations that would separate
  them from friends with bond > 0.5.

- [ ] **Master skill production bonus** — In `ProductionSystem.cpp`'s per-worker contribution
  calculation, if `try_get<Skills>` returns a skill >= 0.9 for the facility's output resource,
  apply a 15% production bonus to that worker's contribution. This makes reaching Master rank
  mechanically meaningful beyond the log message. Check via `skills->ForResource(fac.output)`.

- [ ] **Skill regression log on profession change** — In `AgentDecisionSystem.cpp`'s MIGRATING
  arrival block, where skill reset halves the old skill (the `sk->Advance(oldRes, -(oldVal * 0.5f))`
  line), if `oldVal >= 0.5` (was at least Journeyman), log "X lost Journeyman Farming (career
  change to Water Carrier)." via `registry.view<EventLog>()`. Also remove the entity from
  `ScheduleSystem`'s `s_milestones` static set — but since it's static, instead just note in the
  log that the milestone was lost; the static set prevents re-firing anyway when the NPC re-earns it.

- [ ] **Sleeping NPC visual dim** — In `GameState.cpp`'s agent draw loop (line ~332), when
  `a.behavior == AgentBehavior::Sleeping`, apply `Fade(drawColor, 0.4f)` to make sleeping NPCs
  visually muted on the map. Also skip the outer ring draw for sleeping NPCs (like children).
  This creates a visual day/night rhythm as NPCs dim and brighten.

- [ ] **NPC mentorship at worksite** — In `ScheduleSystem.cpp`'s skill-at-worksite block
  (around line 279), when two NPCs are working at the same facility and one has skill >= 0.8
  while the other has skill < 0.4, boost the learner's `SKILL_GAIN_PER_GAME_HOUR` by 50%.
  Requires iterating nearby Working NPCs at the same `ProductionFacility` entity. Add a log
  via `EventLog` when mentorship begins: "Master X is mentoring Y in Farming."

- [ ] **Rumour visual on world dot** — In `GameState.cpp`'s agent draw loop (line ~332), when
  `AgentEntry::hasRumour` is true, draw a small pulsing yellow ring (radius 7, alpha
  `0.3 + 0.2*sin(time*4)`) around the NPC dot. This makes rumour carriers visually distinct on
  the world map without needing to hover each NPC individually.

- [ ] **Rumour hop count in tooltip** — Add `int rumourHops = 0` to `AgentEntry` in
  `RenderSnapshot.h`. Populate from `Rumour::hops` in SimThread's agent snapshot loop. In
  `HUD::DrawHoverTooltip`, extend the rumour line to show "(spreading: plague, 2 hops left)".
  This lets the player gauge how far the rumour can still travel.

- [ ] **Rumour-driven migration bias** — In `AgentDecisionSystem.cpp`'s migration scoring
  (where NPCs evaluate destination settlements), if the NPC carries a `Rumour` with
  `RumourType::PlagueNearby`, penalise settlements near the rumour's `origin` by -30% in the
  migration attractiveness score. Check distance between candidate settlement and `rum->origin`
  via `Position` components; apply penalty if within 300 world units. Makes NPCs flee plague areas.

- [ ] **Rumour decay visual in tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), when showing
  the rumour line "(spreading: plague)", colour-fade the text based on remaining hops: 3 hops =
  bright yellow, 2 = dim yellow, 1 = faint grey. Use `Fade(YELLOW, 0.3f + 0.23f * rumourHops)`
  (requires `rumourHops` from the "Rumour hop count in tooltip" task). Visually conveys rumour
  freshness at a glance.

- [ ] **GoodHarvest rumour migration pull** — In `AgentDecisionSystem.cpp`'s migration scoring
  (where NPCs evaluate destination settlements), if the NPC carries a `Rumour` with
  `RumourType::GoodHarvest`, boost the score of the rumour's `origin` settlement by +20%.
  Makes NPCs hearing about a bountiful harvest more likely to migrate toward it.

- [ ] **Rumour origin name in tooltip** — Add `std::string rumourOrigin` to `AgentEntry` in
  `RenderSnapshot.h`. Populate from the `Rumour::origin` entity's `Settlement::name` in
  SimThread's snapshot loop. Extend the rumour tooltip line in `HUD::DrawHoverTooltip` to
  show "(spreading: good harvest from Ashford)" instead of just "(spreading: good harvest)".
  Gives the player information about where the rumour originated.

- [ ] **Illness spread between coworkers** — In `ScheduleSystem.cpp`'s Working state block
  (around line 274), when an NPC is at a worksite and has `DeprivationTimer::illnessTimer > 0`,
  check other Working NPCs at the same `ProductionFacility` entity. If a healthy coworker
  (illnessTimer <= 0) is within 30 units, roll a 2% per-game-hour chance to infect them with the
  same `illnessNeedIdx`. Log "X caught illness from Y at Z" via `EventLog`. Cap at 1 spread per
  ill NPC per game-hour using a cooldown on `DeprivationTimer`.

- [ ] **Illness affects work output** — In `ProductionSystem.cpp`'s per-worker contribution,
  if the worker has `DeprivationTimer::illnessTimer > 0` (via `try_get<DeprivationTimer>`),
  reduce their production contribution by 40%. This makes illness a tangible economic cost
  beyond just accelerated need drain, creating pressure to keep NPCs healthy.

- [ ] **Illness count in settlement stockpile panel** — Add `int illCount = 0` to `StockpilePanel`
  in `RenderSnapshot.h`. In SimThread's WriteSnapshot, count homed NPCs with
  `DeprivationTimer::illnessTimer > 0` for the selected settlement. In `RenderSystem::DrawStockpilePanel`,
  if `illCount > 0`, append " | Sick: N" to the Treasury/Working/Idle line in `Fade(PURPLE, 0.7f)`.

- [ ] **NPC avoids plague settlement** — In `AgentDecisionSystem.cpp`'s migration destination
  scoring, if a candidate settlement has `Settlement::modifierName == "Plague"`, apply a -50%
  penalty to that settlement's attractiveness score. NPCs with `Rumour{PlagueNearby}` referencing
  that settlement apply an additional -20%. This makes plague a real population driver as healthy
  NPCs flee afflicted areas.

- [ ] **Contagion chain length tracking** — Add `int contagionGen = 0` to `DeprivationTimer`
  in `Components.h`. When illness spreads via contagion in `AgentDecisionSystem.cpp`, set the
  new patient's `contagionGen = source.contagionGen + 1`. When the random event creates illness
  (case 2 in `RandomEventSystem.cpp`), reset `contagionGen = 0`. Log the generation in the
  contagion message: "X caught illness from Y (gen 3)". Add `int contagionGen = 0` to
  `AgentEntry` in `RenderSnapshot.h` and show it in the illness tooltip line in `HUD.cpp`.

- [ ] **Quarantine behaviour for ill NPCs** — In `AgentDecisionSystem.cpp`'s migration and
  gossip sections, when an NPC has `illnessTimer > 0`, reduce their gossip radius by 50%
  (use `GOSSIP_RADIUS * 0.5f` for sick NPCs) and block migration initiation entirely. This
  simulates self-quarantine behaviour, slowing contagion spread while keeping the sick NPC
  at their home settlement. Check `DeprivationTimer::illnessTimer` in the existing gossip
  and migration code paths.

- [ ] **Windfall location in log** — In `RandomEventSystem.cpp`'s per-NPC event loop (case 1:
  windfall), extend "X found Ng on the road" to "X found Ng on the road near Greenfield" using
  the same `try_get<HomeSettlement>` → `try_get<Settlement>` pattern. Same one-line change as
  the skill discovery location task.

- [ ] **NPC gratitude strengthens affinity** — In `AgentDecisionSystem.cpp`'s charity section,
  when an NPC receives charity (the `recentlyHelped` path), if both NPCs have `Relations`
  components, boost the recipient's affinity toward the helper by +0.15 (capped at 1.0).
  Use `registry.get_or_emplace<Relations>(recipient)` to ensure the component exists. This
  makes charity create lasting social bonds that influence future migration and gossip.

- [ ] **Harvest bonus location in log** — In `RandomEventSystem.cpp`'s per-NPC event loop
  (case 3: good harvest bonus), extend "X has a good harvest bonus" log to include "at
  Greenfield" using the same `try_get<HomeSettlement>` → `try_get<Settlement>` pattern.

- [ ] **NPC social memory of illness source** — In `AgentDecisionSystem.cpp`'s illness
  contagion block (the `trySpread` lambda), when an NPC catches illness from another, record the
  source entity in a new `IllnessSource` component (`entt::entity source; std::string sourceName;`)
  in `Components.h`. Display "Caught from: X" in the illness tooltip line in `HUD.cpp` by adding
  `std::string illnessSource` to `AgentEntry` in `RenderSnapshot.h` and populating it in SimThread.

- [ ] **Harvest bonus remaining time in tooltip** — Extend the "Good harvest bonus" tooltip
  line in `HUD.cpp` to show remaining hours: "Good harvest bonus (2h left)". Add `float
  harvestBonusHours = 0.f` to `AgentEntry` in `RenderSnapshot.h`. Populate from
  `DeprivationTimer::harvestBonusTimer` in SimThread's agent snapshot loop. Use snprintf to
  format the line with the remaining hours.

- [ ] **NPC celebration triggers affinity boost** — In `AgentDecisionSystem.cpp`, when an NPC
  transitions to `AgentBehavior::Celebrating` (goal completion), boost affinity by +0.1 toward
  all NPCs within 50 world units who share the same `HomeSettlement`. This simulates shared
  joy strengthening community bonds. Use `registry.view<Relations, Position, HomeSettlement>`
  to find nearby same-settlement NPCs.

- [ ] **Celebration log with goal details** — In the goal completion block of
  `AgentDecisionSystem.cpp` (where `AgentBehavior::Celebrating` is set), extend the existing
  celebration log to include the completed goal type: "X is celebrating (saved 100g)!" or
  "X is celebrating (reached age 50)!". Read the `Goal` component's `type` and `target` fields
  to format the message. No new components needed.

- [ ] **Nearby NPCs join celebrations** — In `AgentDecisionSystem.cpp`, when an NPC begins
  celebrating (behavior transitions to Celebrating), check for other NPCs within 40 world
  units at the same `HomeSettlement`. With 30% probability each, set their behavior to
  `Celebrating` for a shorter duration (half the normal celebration time). Log "X joined Y's
  celebration at Z." This creates spontaneous community festivities from individual achievements.

- [ ] **Low morale triggers emigration** — In `AgentDecisionSystem.cpp`'s migration evaluation,
  when an NPC's home settlement has `Settlement::morale < 0.2`, increase migration probability
  by 2× (double the base migration chance). Read morale via `try_get<Settlement>` on the NPC's
  `HomeSettlement::settlement` entity. This creates a feedback loop where low morale causes
  population loss, making morale a key settlement health indicator.

- [ ] **Morale boost from trade delivery** — In `TransportSystem.cpp`'s GoingToDeposit arrival
  block (where hauler cargo is deposited into a settlement's `Stockpile`), add a small morale
  boost: `settl->morale = std::min(1.f, settl->morale + 0.02f)` per delivery. This rewards
  well-connected trade networks with happier settlements, reinforcing the value of hauler routes.

- [ ] **Contentment trend arrow in world status bar** — In `HUD::DrawWorldStatus` (HUD.cpp),
  apply the same trend tracking pattern used for morale to the contentment label (C:XX%). Use
  a `static std::map<std::string, float> s_prevContent` sampled once per second. Append "+"
  if contentment rose by > 0.03, "-" if fell by > 0.03. Gives players visibility into whether
  NPC wellbeing is improving or declining at each settlement.

- [ ] **Morale event log on threshold crossing** — In `RandomEventSystem.cpp`'s settlement
  event loop (where `s.morale` is already read), add a static set tracking settlements that
  crossed below 0.25 morale. When a settlement's morale drops below 0.25 for the first time,
  log "Morale crisis at X — settlers may leave!" via `EventLog`. Clear the entry when morale
  recovers above 0.4. Mirrors the existing unrest/recovery log pattern.

- [ ] **Return trip delivery log** — In `TransportSystem.cpp`'s return-trip section (around
  line 420, where the hauler picks up goods at the destination for a return trip), add a similar
  delivery log when the hauler arrives back home with return-trip cargo. Format: "Hauler returned
  with N food to Settlement (return trip)". Uses the same EventLog pattern as the forward delivery.

- [ ] **Hauler profit summary in tooltip** — Add `float lastProfit = 0.f` to `AgentEntry` in
  `RenderSnapshot.h`. In SimThread's agent snapshot loop, for haulers, populate from
  `Money::balance` delta since last delivery (requires adding `float prevBalance = 0.f` to the
  `Hauler` component in `Components.h`, updated on each delivery in `TransportSystem.cpp`).
  Show "Last trip: +N.Ng" in `HUD::DrawHoverTooltip` for hauler tooltips.
