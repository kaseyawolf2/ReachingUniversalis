# ReachingUniversalis — Dev Backlog

**Priority: NPC behaviour depth and inter-NPC interaction — building toward a living world.**

Maintained by Claude Code cron. Each session picks the top `[ ]` task, implements it, commits,
marks it done, then appends 2–3 new concrete tasks to keep the queue full.

---

## In Progress

_(none)_

---

## Backlog

### NPC Lifecycle & Identity

- [ ] **Child work apprenticeship** — At age 12–14 (not full adult but near), allow children to
  enter a light `Working` state for 2 hours per day (10:00–12:00). They produce at 20% of adult
  rate and gain skill at 2× the normal child passive rate. Implement in `ScheduleSystem` by adding
  an apprentice work window when `age->days >= 12.f && age->days < 15.f`.

- [ ] **Elder retirement** — NPCs over age 60 stop working (`AgentBehavior::Working` → `Idle` for
  elders in `ScheduleSystem`). They no longer drain from the settlement treasury (no wages). Instead
  they slowly drain their own `Money` balance for subsistence. Add a settlement "elder bonus": each
  elder alive at home = +0.5% production modifier on the `Settlement` (experience/wisdom effect),
  capped at +5%. Apply in `ProductionSystem`.

- [ ] **Profession identity** — Add a `Profession` component with an enum: `Farmer`, `WaterCarrier`,
  `Lumberjack`, `Hauler`, `Idle`. Assigned at spawn based on home settlement's primary output.
  When an NPC migrates in `AgentDecisionSystem`, they prefer settlements that match their profession.
  Show profession in NPC tooltip (already have `profession` string in `AgentEntry`, just populate it
  from the component rather than inferring it).

- [ ] **Named families** — When two adult NPCs of the same `HomeSettlement` are both over age 18 and
  have no `FamilyTag`, give them a shared `FamilyTag{ familyName }` (combine their surnames or pick
  a new one). Children born via `BirthSystem` inherit the `FamilyTag` of their "parents" (the two
  most recently paired adults). Show family name in NPC tooltip.

### NPC Social Behaviour

- [ ] **Evening gathering** — Between hours 18–21, NPCs without an active task (not `Working`,
  `SeekingFood`, etc.) should move toward their home settlement's centre position instead of
  wandering. This is purely a `Velocity` target change in `AgentDecisionSystem` — no new component
  needed. Makes the world look alive: NPCs cluster at dusk.

- [ ] **Gossip / price sharing** — When two NPCs from *different* settlements are within 30 units of
  each other (check positions in `AgentDecisionSystem`), they exchange price knowledge: the visiting
  NPC's home settlement `Market` prices nudge 5% toward the local settlement's prices. This simulates
  word-of-mouth market information spreading without roads. Run once per pair per 6 game-hours
  (use a cooldown on `DeprivationTimer` or a new lightweight component).

- [ ] **NPC helping starving neighbours** — If an NPC has `Hunger > 0.8` and is near another NPC
  with `Hunger < 0.2` and `Money.balance > 20g`, the wealthy NPC "gifts" 5g and the starving NPC
  uses it to immediately buy food (trigger the emergency purchase path in `ConsumptionSystem`).
  Log the event: "Aldric helped a starving neighbour." Happens at most once per 24 game-hours per
  helper (track with a float timer).

- [ ] **Celebration behaviour** — When a Festival event fires in `RandomEventSystem`, affected NPCs
  (home settlement matches) should enter a new `AgentBehavior::Celebrating` state for the event
  duration. During Celebrating: movement speed halved, needs drain 50% slower, they move toward
  the settlement centre. Add `Celebrating` to the `AgentBehavior` enum and handle it in
  `AgentDecisionSystem` and `NeedDrainSystem`.

### NPC Crime & Consequence

- [ ] **Theft from stockpile** — NPCs with `money.balance < 5g` and `stealCooldown == 0` (field
  already exists in `DeprivationTimer`) can steal 1 unit of their most-needed resource from their
  home `Stockpile`. Deduct the market price from `Settlement::treasury` (the settlement "loses" the
  good). Set `stealCooldown = 48` game-hours. Log: "Mira stole food from Ashford."
  Implement in `AgentDecisionSystem` after the emergency purchase check.

- [ ] **Exile on repeat theft** — Track a `theftCount` int on `DeprivationTimer`. After 3 thefts,
  the NPC is "exiled": their `HomeSettlement` is cleared, they become a wanderer (no home, no wages,
  no schedule). They can re-settle at a new settlement by spending 30g (implement as an extended
  version of the existing H-key settle logic, triggered automatically for wanderers in
  `AgentDecisionSystem` when they have enough gold).

- [ ] **Bandit NPCs from desperation** — Wandering exiles with `money.balance < 2g` for more than
  48 game-hours become `BanditTag` entities. Bandits move toward roads (target the midpoint of the
  nearest `Road`) and intercept haulers passing within 40 units — stealing 30% of cargo and
  fleeing. Player can "confront" a bandit (E key within range) to recover the loot for 10 rep.
  Remove `BanditTag` if their balance rises above 20g (they go straight).

### NPC Memory & Goals

- [ ] **Personal goal system** — Add a `Goal` component: `enum GoalType { SaveGold, ReachAge,
  FindFamily, BecomeHauler }` with a `progress` float and `target` float. Each NPC gets one goal
  at spawn. When met, log a small event ("Aldric reached his savings goal!"), give a 2-hour
  `Celebrating` state boost, then assign a new goal. Goals affect behaviour: SaveGold NPCs spend
  less on emergency purchases; BecomeHauler NPCs work harder (small production bonus).

- [ ] **Migration memory** — Add a `MigrationMemory` component: a small map of
  `{ settlement_name → last_known_price_snapshot }`. When an NPC migrates, they carry their old
  settlement's prices. In `AgentDecisionSystem`, migrating NPCs prefer destinations where their
  remembered prices suggest a better life (food cheaper, wages higher). Update memory on arrival.
  Also used by the gossip system above.

- [ ] **NPC personal events** — In `RandomEventSystem`, add a per-NPC event tier that fires every
  12–48 game-hours per NPC (jittered per entity). Small events: skill discovery (+0.1 to a random
  skill), windfall (find 5–15g on the road), minor illness (one need drains 2× for 6 hours),
  good harvest (worker produces 1.5× for 4 hours). Log the interesting ones.

### Settlement Social Dynamics

- [ ] **Settlement morale** — Add a `morale` float (0–1) to `Settlement`. Morale rises when:
  stockpiles are full, festivals fire, births happen. Falls when: NPCs die of hunger, thefts occur,
  population drops. Morale above 0.7 gives +10% production. Below 0.3 triggers a "Unrest" modifier
  (random chance of a work stoppage event). Update in relevant systems, display in stockpile panel
  (replace or augment `stability`).

- [ ] **Work stoppage event** — New random event type in `RandomEventSystem`: when settlement morale
  < 0.3, there is a 5% chance per day of a Work Stoppage. All `Schedule`-following NPCs at that
  settlement enter `AgentBehavior::Idle` for 6 game-hours, refusing to work. Settlement treasury
  is not charged wages during this period. Log: "Workers in Ashford downed tools."

- [ ] **Inter-settlement rivalry** — Track a `rivalry` map in each `Settlement`:
  `std::map<entt::entity, float>` where negative = rival, positive = ally. Rivals form when one
  settlement's haulers consistently undercut another's prices (detected in `PriceSystem`). Rival
  settlements have a 10% trade tax surcharge between them. Allied settlements (formed by prolonged
  fair trade) get a 5% discount. Show rivalry/alliance in the settlement tooltip.

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

---

## Done

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
