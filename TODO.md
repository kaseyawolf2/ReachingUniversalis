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

- [ ] **Family surname on birth** — In `BirthSystem.cpp`, newborns currently get a random first
  and last name. If the settlement has any adult NPC with a `Name` containing a space (i.e., a
  surname), use the most common surname among adult residents as the newborn's last name (50%
  chance), otherwise keep the random last name. This ties newborns to existing family lines.
  Count surnames with a `std::map<std::string, int>` inside the birth block, pick the most
  frequent, apply it to both `npc` and `npc2` (twin).

- [ ] **Birth announcement names parent** — In `BirthSystem.cpp`, when a new NPC is born,
  the log currently says "Born: Aldric Smith at Ashford". Find the adult with the highest
  `Money.balance` at that settlement (as a proxy for "most established parent") and append
  their name: "Born: Aldric Smith at Ashford (to Brom Cooper)". Use a simple loop over
  `HomeSettlement, Money` filtered to the current settlement inside the birth block.

- [ ] **Surname tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), when an NPC's name contains a
  space, show their surname in a subtle way: after the main name line, if two or more agents in
  `agentCopy` share the same last name (check `npcName.rfind(' ')` to extract surname), append
  "(Family: <surname>)" to `line1`. This makes family clusters visible just by hovering. Compute
  a surname→count map from `agentCopy` once before the hit-test loop.

- [ ] **Child count in world status bar** — `RenderSnapshot::SettlementStatus` (shown in
  `HUD::DrawWorldStatus`) currently shows only `pop` and `haulers`. Add `int childCount = 0`
  to `SettlementStatus` in `RenderSnapshot.h`, populate it in SimThread's world-status loop
  (around line 1486 where `hCount` is computed — use `m_registry.all_of<ChildTag>(ne)` while
  counting `HomeSettlement`), and display it in `DrawWorldStatus` as a greyed "(Nc)" suffix
  after the population number.

- [ ] **Child abandonment on settlement collapse** — When a settlement collapses (pop hits 0,
  logged in `DeathSystem`), any remaining `ChildTag` entities with that `HomeSettlement` should
  have their `HomeSettlement` cleared and `AgentState::target` set to `entt::null` so they become
  wanderers. Add this cleanup to `DeathSystem.cpp` in the settlement-collapse check block (after
  line 130). Log: "Orphaned children of Ashford scattered."

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
