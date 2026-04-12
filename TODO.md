# ReachingUniversalis — Dev Backlog

**Priority: NPC behaviour depth and inter-NPC interaction — building toward a living world.**

Maintained by Claude Code cron. Each session picks the top `[ ]` task, implements it, commits,
marks it done, then appends 2–3 new concrete tasks to keep the queue full.

---

## In Progress

- [ ] **Rivalry log events** — In `RandomEventSystem::Update`'s settlement loop, after updating
  `relations`, log when a relation crosses a threshold for the first time: when score drops
  below -0.5f add "Rivalry declared: Ashford vs Thornvale" to the event log, when it rises
  above 0.5f add "Alliance formed: Ashford & Thornvale". Guard with a per-pair cooldown using
  an `std::set<pair<entt::entity,entt::entity>> s_loggedRivalries` static set to avoid spam.

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

- [ ] **Population history chart in stockpile panel** — `StockpilePanel::popHistory` (vector<int>)
  is already written by SimThread (up to 30 daily samples). In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), below the population line, draw a small sparkline: 30 thin bars, each
  proportional to the population max across all samples, width 4px, height scaled to 30px.
  Color GREEN when trend is up, RED when down, GRAY flat. Gives an at-a-glance population
  trajectory without opening any extra panel.

- [ ] **Elder knowledge bonus in production** — In `ProductionSystem.cpp`, after the existing
  `moraleBonus` block, add: for each Working NPC at a facility whose `Age::days > 60`, add a
  flat `+0.05` worker contribution (elders provide tacit knowledge). Use `registry.try_get<Age>(e)`.
  Cap contribution per-elder at 2.0x to prevent outliers. Log nothing — the effect is subtle
  and best discovered by the player through observation.

- [ ] **Seasonal migration preference** — In `AgentDecisionSystem`'s `FindMigrationTarget` scoring,
  subtract 0.2 from the score of any destination that is currently in Winter (`Season::Winter`).
  Read the season from `TimeManager` (already in registry view). This creates organic population
  flow away from winter-hit settlements toward warmer-season regions, making seasonal population
  patterns emergent rather than purely price-driven.

- [ ] **Resident wealth tooltip on panel click** — When hovering the "Residents (N):" header line
  in the stockpile panel (detect mouse Y within the section), show a small 2-line tooltip with
  the richest NPC's name and balance, and the poorest's. Use `panel.residents.front()` and
  `panel.residents.back()` already in the snapshot. Draw it in `RenderSystem::DrawStockpilePanel`
  using `GetMousePosition()` comparison against the section Y range. No new fields needed.

- [ ] **Profession distribution in stockpile panel** — Below the residents list header in
  `DrawStockpilePanel` (RenderSystem.cpp), add a single compact line showing profession counts,
  e.g. "Fa:4 Wa:3 Lu:2". Build the counts by iterating `panel.residents` (already populated).
  Render in dim LIGHTGRAY after the header line. Replaces no existing line — just one extra row.
  No new snapshot fields needed.

- [ ] **Richest NPC name in world economy bar** — In `HUD::DrawWorldStatus` (HUD.cpp), the
  economy debug overlay already shows `econRichestName` and `econRichestWealth` (both in
  `RenderSnapshot`). If `debugOverlay` is on, render a line "Richest: [Name] — 123g" in GOLD
  below the existing economy stats. Read directly from `snap.econRichestName` and
  `snap.econRichestWealth`. No new fields or SimThread changes needed.

- [ ] **Family dynasty count in stockpile panel** — In `DrawStockpilePanel` (RenderSystem.cpp),
  after the "Residents (N):" header, count how many distinct `familyName` values appear in
  `panel.residents` and how many surnames appear ≥ 2 times. Add a compact line below the header:
  "Families: 3 dynasties" or "No established families" if all residents have unique surnames.
  Build counts by iterating `panel.residents` — no new snapshot fields needed.

- [ ] **Gossip idle animation** — In `AgentDecisionSystem`, when two NPCs from the same settlement
  are both `Idle` and within 30 units during off-work hours (20–22h), briefly nudge their
  velocity ±5 units toward each other (`vel.x += dx * 0.1f / dist`) for 2–3 game-seconds so
  they visually gravitate together. Track with a `gossipTimer` float on `DeprivationTimer`
  (already exists); set 3.f when gossip fires, skip new gossip while > 0. No new components.

- [ ] **NPC longest-resident badge** — In `SimThread::WriteSnapshot`'s StockpilePanel residents
  loop, also track the NPC whose `Age::days` is highest among residents. Add an `isEldest bool`
  to `StockpilePanel::AgentInfo`. In `DrawStockpilePanel`, suffix the eldest resident's name
  with " [Elder]" in `Fade(ORANGE, 0.8f)`. Represents the settlement patriarch/matriarch.

- [ ] **Contentment shown in world status bar** — Add `float avgContentment = 1.f` to
  `SettlementStatus` in `RenderSnapshot.h`. In SimThread's world-status loop, compute the average
  contentment of homed NPCs (view `Needs, HomeSettlement`, same exclusions as needStability).
  In `HUD::DrawWorldStatus` (HUD.cpp), after the existing pop count, append a small coloured
  "❤XX%" or plain "C:XX%" indicator using GREEN/YELLOW/RED thresholds matching the dot colours.

- [ ] **Mood colour legend overlay** — In `HUD::Draw` (HUD.cpp), when `debugOverlay` is true,
  draw a small 3-row legend in the bottom-right corner: a green dot + "Thriving (>70%)", a yellow
  dot + "Stressed (40-70%)", a red dot + "Suffering (<40%)". Draw using `DrawCircleV` (radius 5)
  + `DrawText` at fixed screen coordinates. Helps the player decode the contentment colour system.

- [ ] **Suffering NPC log event** — In `RandomEventSystem::Update`'s per-NPC loop, when
  `contentment < 0.2f` for an NPC, log "X is desperate at Y" (once per 12 game-hours using the
  existing `personalEventTimer`). Requires computing `contentment` the same way as SimThread's
  snapshot: weighted average of the 4 needs (hunger 30%, thirst 30%, energy 20%, heat 20%). Log
  only if the NPC has a Name and HomeSettlement, and rate-limit per entity.

- [ ] **Event log pop trend** — In `RandomEventSystem::TriggerEvent`, after computing `popCount`,
  also look up the settlement's `popTrend` from `RenderSnapshot::SettlementStatus` — but that's
  render-side. Instead compute it locally: count NPCs at target (already in `popCount`), then
  compare to a rolling previous count stored in a `std::map<entt::entity, int> m_prevPop` member
  on `RandomEventSystem`. Append "(↑)" or "(↓)" to `[pop N]` when trend changes by ≥ 2 between
  samples taken every 24 game-hours. Update sample in `Update()` via a `m_popSampleTimer`.

- [ ] **Plague spread log** — In `RandomEventSystem`'s plague spread block (the section that
  copies plague from one settlement to a neighbour via roads), the current log message is
  "PLAGUE spreads from X to Y — N died". Add `[pop N]` to the destination settlement using
  `popCount` computed the same way as in `TriggerEvent` — quick local count on the destination
  entity before the log push. Keeps spread events as informative as the initial eruption.

- [ ] **Unrest pop context** — In `RandomEventSystem::Update`'s settlement loop, the UNREST log
  currently reads "UNREST in Ashford — morale critical, production suffering". Extend it to
  include `[pop N]` and the current morale percentage: "UNREST in Ashford [pop 8] — morale 22%,
  production suffering". Count pop via the same HomeSettlement view pattern used in TriggerEvent.
  Same for "Tensions ease" recovery log.

- [ ] **Settlement tooltip: specialty and morale** — Extend `DrawSettlementTooltip` (HUD.cpp) to
  show two extra lines: (1) "Specialty: Farming" from `SettlementEntry::specialty` when non-empty;
  (2) "Morale: XX%" from `StockpilePanel::morale` — but that's only available when the settlement
  is selected. Instead add `float morale` to `SettlementStatus` in `RenderSnapshot.h`, populate
  it in SimThread's world-status loop with `s.morale`, and read it in the tooltip. Display it
  in the same green/yellow/red colour scheme as the panel bar.

- [ ] **NPC birth log** — In `BirthSystem.cpp`, the birth event currently only logs if there's an
  `EventLog`. Extend the log message from "Born: Aldric Smith at Ashford" to also include the
  parent's name if the `ChildTag` has a `followTarget` (the entity following at birth). Use
  `registry.try_get<Name>(childTag.followTarget)` to get the parent's name and append
  "raised by Brom Cooper". Requires no new components.

- [ ] **Settlement tooltip: pop trend arrow** — In `DrawSettlementTooltip` (HUD.cpp), append the
  popTrend character ('+', '-', '=') to the pop line using `SettlementStatus::popTrend`. Already
  available in `SettlementStatus`. Format: "[12/35 pop ↑]" or "[12/35 pop ↓]". Use plain '+'
  and '-' ASCII since raylib's default font may not render arrow glyphs.

- [ ] **Elder deathbed savings inheritance** — In `DeathSystem.cpp`, when an elder (age > 60)
  dies of old age, increase the inheritance fraction from the default 0.5 to 0.8 (elders have had
  more time to save). Add an `isElder` check before the `INHERITANCE_FRACTION` constant usage in
  the old-age death block and use 0.8f when true. Log: "Aldric Smith (elder) left an estate of
  45g to Ashford." Requires no new components.

- [ ] **Contentment milestone log** — In `RandomEventSystem`'s per-NPC event loop, track a
  `contentmentMilestone` bool in a static per-entity `std::set<entt::entity> s_lowLogged`. When
  NPC contentment drops below 0.2 for the first time (not in set), log "X is desperate at Y"
  and insert into set. When contentment recovers above 0.5, remove from set (so the message can
  fire again later). This avoids log spam while ensuring desperate NPCs are surfaced once.

- [ ] **Estate log on need-death too** — `DeathSystem.cpp`: the estate log ("X left an estate of
  Ng to Y") currently only fires in the inheritance block (after the morale/cargo blocks), but
  the name retrieval uses `try_get<Name>` which works for both old-age and need-death. Verify the
  log fires for need-death by confirming the inheritance block is reached for all `toRemove`
  entities, not just old-age ones. If `money->balance >= MIN_INHERITANCE` is always evaluated,
  no code change is needed — just a test/verification pass with a log trace.

- [ ] **Elder will tooltip line** — In `HUD::DrawHoverTooltip` (HUD.cpp), for elders (ageDays >
  60 and hasName), append a faint line "Will: 80% to treasury" in `Fade(GOLD, 0.5f)` below the
  gold balance line. This surfaces the inheritance mechanic to the player via the UI. Read
  `ageDays` from `AgentEntry` (already present). No new snapshot fields needed.

- [ ] **Estate size shown in settlement tooltip** — Add `float pendingEstates = 0.f` to
  `SettlementStatus` in `RenderSnapshot.h`. In SimThread's world-status loop, sum `money->balance`
  for all elders (age > 60) homed at each settlement multiplied by 0.8f. In
  `DrawSettlementTooltip` (HUD.cpp), show "Estates: ~Ng" in dim gold when > 0. Gives the player
  a forward-looking economic signal — how much will flow into treasury when elders die.

- [ ] **Profession shown in migration log** — In `AgentDecisionSystem.cpp`'s MIGRATING arrival
  block, after setting the new profession, append it to the existing migration log message.
  Currently the log reads "Mira moved to Thornvale". Change it to "Mira (Farmer) moved to
  Thornvale" by reading `ProfessionLabel(prof->type)` after updating `prof->type`. If the NPC
  has no Profession component, omit the suffix. No new fields or components needed.

- [ ] **Skill reset on profession change** — In `AgentDecisionSystem.cpp`'s MIGRATING arrival
  block, when a profession change occurs (new type differs from old), halve the old primary
  skill and boost the new primary skill by 10% (capped at 1.0). For example, an ex-Farmer who
  becomes a Lumberjack loses half their farming skill and gets +0.1 woodcutting. Use
  `try_get<Skills>` and the old/new `ProfessionForResource` to identify which skill to adjust.
  Models the cost of retraining in a new trade.

- [ ] **Migrant welcome log at destination** — In `AgentDecisionSystem.cpp`'s MIGRATING arrival
  block, after the profession update, push a second log entry from the destination settlement's
  perspective: "Ashford welcomes Mira Reed (Farmer) — pop now 14." Read the new pop count by
  iterating `HomeSettlement` views (as in other systems) or use the settlement's existing
  `popCap`/current-pop from prior computation. Use `registry.view<EventLog>()` the same way
  as the departure log.

- [ ] **Profession colour in residents list** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), colour the profession abbreviation by type instead of uniform grey:
  Fa = `Fade(GREEN, 0.6f)` (farming), Wa = `Fade(SKYBLUE, 0.6f)` (water), Lu = `Fade(BROWN, 0.7f)`
  (wood), Me = `Fade(GOLD, 0.5f)` (merchant). Change the single `Fade(GRAY, 0.75f)` colour to
  a per-abbreviation lookup. No new fields or components needed.

- [ ] **Settlement specialty label in stockpile header** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), after the existing header line, add a small `specialty` label if non-empty.
  `StockpilePanel` doesn't currently carry specialty — add `std::string specialty` to it in
  `RenderSnapshot.h` and populate it in SimThread's WriteSnapshot (same as `SettlementEntry`
  specialty). Draw "Specialty: Farming" in dim colour under the header line. Occupies one extra
  row (adjust `totalLines` accordingly).

- [ ] **Idle NPC count in stockpile panel** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), extend the "Treasury/Workers" line to also show idle NPCs:
  "Treasury: 230g   Working: 4 / Idle: 3". Add `int idle = 0` to `StockpilePanel` in
  `RenderSnapshot.h` and populate it in SimThread's WriteSnapshot by counting homed NPCs with
  `AgentBehavior::Idle` (excluding Haulers, player). No visual layout change beyond the extended
  string.

- [ ] **Profession vocation label in NPC tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp),
  when an NPC's `Profession::type` matches their highest-skill resource (i.e. they are working
  in their vocation), append " [vocation]" in `Fade(GOLD, 0.6f)` to the role line. The match
  check mirrors the ScheduleSystem bonus: `ProfessionForResource` of the aptitude resource ==
  `prof->type`. Add `bool inVocation = false` to `AgentEntry` in `RenderSnapshot.h`; populate
  in SimThread's agent snapshot loop via `try_get<Profession>` + `try_get<Skills>`.

- [ ] **Skill milestone log** — In `ScheduleSystem.cpp`'s skill-at-worksite block, after
  `skills->Advance(...)`, check if the skill just crossed 0.5 (journeyman) or 0.9 (master)
  for the first time. Use a `static std::set<std::pair<entt::entity,int>> s_milestones` to
  prevent repeat fires. Log "Aldric Smith reached Journeyman Farming." or "Master Water." via
  `registry.view<EventLog>()`. Levels: 0.5 = Journeyman, 0.9 = Master.

- [ ] **Profession match indicator on world dot** — In `GameState.cpp`'s agent draw loop,
  when an NPC is Working and their `AgentEntry::inVocation` is true (to be added per task above),
  draw a small additional ring dot (radius 5, `Fade(GOLD, 0.5f)`) centred on the NPC. This makes
  vocation-aligned workers visually distinct on the map. Requires the `inVocation` field from the
  tooltip vocation task above.

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
