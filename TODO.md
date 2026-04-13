# ReachingUniversalis — Dev Backlog

**Priority: NPC behaviour depth and inter-NPC interaction — building toward a living world.**

Maintained by Claude Code cron. Each session picks the top `[ ]` task, implements it, commits,
marks it done, then appends 2–3 new concrete tasks to keep the queue full.

---

## In Progress





## Recently Done

- [x] **Hauler profit shown in stockpile panel** — lifetimeProfit on HaulerInfo, rendered as "(+Xg)" GREEN or "(-Xg)" RED inline after route text.

- [x] **NPC comforts grieving neighbour** — Non-grieving idle NPCs within 25u with affinity >= 0.3 reduce griefTimer by 0.5h. 180s comfortCooldown on DeprivationTimer.

- [x] **Family greeting clears grief early** — Family reunion halves griefTimer on both NPCs. Logs "[Name] finds comfort in [Other]'s company."

- [x] **Grief reduces work output** — Grieving workers (griefTimer > 0) produce at 0.5× rate in ProductionSystem. Reads existing DeprivationTimer field.

- [x] **Grief shown in NPC tooltip** — `isGrieving` + `griefHoursLeft` piped through AgentEntry. Shows "Grieving (X.Xh left)" in faint PURPLE.

- [x] **Skill decay warning in tooltip** — Non-working NPCs with any skill ≥ 0.5 show "Skills rusting" in faint ORANGE in HUD tooltip. Pure display check on existing piped data.

- [x] **Settlement specialisation bonus** — In ProductionSystem, +15% output when ≥3 masters in a settlement match a facility's resource type. One-time log per settlement+type.

- [x] **Skill training builds mentor bond** — After successful teach, learner's `lastHelper = teacher entity`. Enables existing gratitude greeting for mentor-student bond.

- [x] **Teaching shown in NPC tooltip** — `recentlyTaught` bool piped through AgentEntry from teachCooldown > 0. Shows "Recently taught/learned" in faint SKYBLUE in HUD tooltip.

- [x] **NPC waves at player when happy** — Content NPCs (avg needs > 0.8) within 50u wave at player with 1%/s chance. Gated by thankCooldown (60s). Logs "[NPC] waves at you cheerfully."

- [x] **NPC avoids player with bad reputation** — NPCs with Reputation < -0.5 within 30u flee at 0.8× speed for 2s via panicTimer. Logs "[NPC] hurries away from you nervously."

- [x] **NPC gossips about confrontation** — In greeting block, NPCs with `lastHelper == playerEntity` spread the memory to greeted NPCs. Logs "[NPC] tells [Other] about the player's bravery."

- [x] **Confrontation witness remembers player** — Witnesses in SimThread's confrontation loop get `lastHelper = playerEntity` via `get_or_emplace<DeprivationTimer>`, enabling gratitude greetings later.

- [x] **Milestone celebration boosts settlement morale** — Master milestone (idx 1) in ScheduleSystem::checkMilestone boosts home Settlement::morale by +0.03 (capped at 1.0).

- [x] **Nearby NPCs join skill celebration** — Idle friends (Relations::affinity >= 0.2) within 30u join skill celebrations with 0.25 game-hour timer. Logs "[Friend] joins [Celebrant]'s celebration."

- [x] **NPC mood contagion in idle** — Happy NPCs (avg needs > 0.8) cheer up struggling NPCs (avg needs < 0.4) within 25u, boosting home settlement morale +0.01/game-hour. 120s cooldown via moodContagionCooldown.

- [x] **Hauler route shown in stockpile panel** — HaulerInfo struct + haulerRoutes on StockpilePanel. Up to 3 haulers with "Home→Dest" routes in SKYBLUE (RED if struggling).




- [x] **Family reunion greeting** — NPCs sharing FamilyTag::name get "[Name] embraces [Other] warmly." log and +0.08 mutual affinity (8× normal) in greeting block.




- [x] **Family grief on death** — griefTimer on DeprivationTimer set to 4 game-hours when family member dies. Skips greeting/visiting, drains settlement morale -0.05/game-hour. Log: "[Name] mourns the loss of [Dead]."




- [x] **Settlement skill summary in stockpile panel** — "Top skill: [Type] (N masters, M journeymen)" in faint GOLD. masterCount[3]/journeymanCount[3] on StockpilePanel piped from WriteSnapshot.




- [x] **Skill training between NPCs** — Idle NPCs with skill ≥ 0.6 teach nearby (30u) NPCs with that skill < 0.3. +0.005/game-hour gain, +0.02 mutual affinity, 120s cooldown via teachCooldown.




- [x] **NPC thanks player after witnessing** — Idle NPCs with Reputation > 0.3 within 40u of the player log "[NPC] nods respectfully at you." 60 real-second cooldown via thankCooldown on DeprivationTimer.




- [x] **Witness count shown in confrontation log** — Replaced per-witness log spam with single "Player confronted [bandit] (N witnesses)." summary after the witness loop. Witnesses still get +0.1 rep each.




- [x] **NPC celebrates skill milestone** — Journeyman/Master milestones in ScheduleSystem trigger Celebrating behavior for 0.5 game-hours via skillCelebrateTimer on DeprivationTimer. Existing gold glow renders automatically.




- [x] **Hauler bankruptcy shown in settlement stockpile** — "Struggling haulers: N" in faint RED in stockpile panel. Piped via `strugglingHaulers` on `StockpilePanel`, counted in `WriteSnapshot`.




- [x] **NPC shares food with family first** — Family members prioritized in charity via two-pass loop. Family helpers need only ≥1g, bypass reputation check. Log: "[Name] feeds family member [Other]."




- [x] **Skill milestone shown in NPC tooltip** — "Journeyman/Master [Type]" line in HUD tooltip for skills ≥0.5/≥0.9.


- [x] **Bandit intimidation aura** — Idle NPCs within 50u of bandits drain home settlement morale at -0.02/game-hour. Log with 60s cooldown via intimidationCooldown.


- [x] **NPC remembers who helped them** — lastHelper on DeprivationTimer. Gratitude greeting replaces plain greeting with +0.05 mutual affinity (5x normal). Clears after expression.


- [x] **Hauler avoids recently unprofitable routes** — worstRoute/worstLoss/worstRouteTimer on Hauler. -20% score penalty in FindBestRoute for 24 game-hours after a loss. Timer auto-clears.


- [x] **Player reputation affects trade prices** — Rep ≥100 = 10% discount, ≥50 = 5%, ≤-20 = 10% markup on T/Q key purchases. Gold flow to treasury preserved.


- [x] **NPC contentment affects work output** — Worker contribution in ProductionSystem multiplied by contentment factor from avg Needs: ≥0.7 = 1.0×, 0.4–0.7 = 0.85×, <0.4 = 0.65×.


- [x] **Mood score shown in settlement tooltip** — "Mood: X%" line in settlement tooltip, color-coded GREEN/YELLOW/RED. Uses SettlementEntry::moodScore.


- [x] **NPC age affects move speed** — Children (< 15 days) 80%, elders (> 55 days) 70%, prime adults 100%. Applied in MovementSystem as velocity multiplier, excludes haulers/player.


- [x] **Hauler preferred route bias** — +10% score bonus in FindBestRoute when candidate route matches hauler.bestRoute string. Experienced haulers specialize on profitable corridors.


- [x] **NPC complains about need in greeting** — Greeting log appends " (complains about hunger/thirst/fatigue/the cold)" when any need < 0.3. Priority order, first low need only.


- [x] **Greeting builds affinity** — After logging a greeting in AgentDecisionSystem, +0.01 mutual affinity via Relations. Casual greetings slowly build familiarity into friendship.


- [x] **Settlement rivalry events** — Adjacent settlements with morale > 0.7 and pop > 15 trigger rivalry (2%/game-hour). 20% trade score penalty in FindBestRoute, 24 game-hour duration. rivalWith+rivalryTimer+rivalEntity on Settlement.


- [x] **NPC family visit behaviour** — Idle NPCs with FamilyTag visit family at other settlements. 5% chance/game-hour, 30 game-min visit, logged to EventLog. visitTimer+visitTarget on DeprivationTimer.


- [x] **Bandit threat radius visual** — DrawCircleLinesV at 80u in Fade(RED, 0.2f) when hovering a bandit in GameState::Draw.


- [x] **NPC morale comment in tooltip** — "Mood: Content/Getting by/Struggling/Desperate" line in HUD tooltip, color-coded GREEN/YELLOW/ORANGE/RED by contentment.


- [x] **Settlement population graph in stockpile panel** — Replaced bar chart with line graph (GREEN growth, RED decline segments, white dots) in RenderSystem::DrawStockpilePanel.


- [x] **NPC flee from bandits** — panicTimer on DeprivationTimer. NPCs within 60u scatter at 1.5x speed for 2s after bandit intercept. Skip decisions during panic.


- [x] **Reputation loss from theft** — -0.5 via get_or_emplace<Reputation> in both food and water theft blocks of ConsumptionSystem.


- [x] **Reputation gain from charity** — +0.2 via get_or_emplace<Reputation> in AgentDecisionSystem charity block. Creates component if missing.


- [x] **Hauler loss-making trip log** — Log "[Hauler] completed a loss-making trip to [settlement] (-Xg)" when tripProfit < 0 in TransportSystem.


- [x] **Hauler trip history summary** — lifetimeTrips/lifetimeProfit on Hauler, piped through AgentEntry, shown as "Trips: N (total +Xg)" in tooltip.


- [x] **Convoy bandit deterrence** — `if (h.inConvoy) return;` in AgentDecisionSystem bandit intercept lambda. Bandits skip convoy haulers.


- [x] **Convoy log announcement** — Track wasInConvoy + convoyPartner in TransportSystem convoy check. Log "[A] formed convoy with [B] on the way to [settlement]." on false→true transition.


- [x] **Convoy visual on world map** — Faint green lines between convoy haulers (inConvoy && haulerState==1) within 60u in GameState::Draw.


- [x] **NPC remembers last meal source** — `lastMealSource` on DeprivationTimer. ConsumptionSystem records settlement name on eating; logs gratitude when hunger < 0.2.


- [x] **Settlement trade volume in tooltip** — Show "Trade volume: Xg/day" in settlement tooltip.


- [x] **NPC witness bandit confrontation** — Non-bandit NPCs within 120u gain +0.1 reputation. Logged per witness.

- [x] **Hauler bankruptcy warning log** — bankruptWarned flag on Hauler, log at 50% BANKRUPTCY_HOURS. Resets on recovery.

- [x] **NPC skill-up notification** — Extended existing milestone system in ScheduleSystem with "Apprentice" (0.25) and "Skilled" (0.75) thresholds.

- [x] **Bandit flee visual indicator** — fleeTimer + fleeVx/fleeVy piped through AgentEntry. Red/orange trail drawn behind fleeing bandits in GameState::Draw.

- [x] **Settlement morale boost on bandit cleared** — +0.05 morale to road.from/road.to settlements after confrontation. Logged to EventLog.

- [x] **Settlement mood indicator** — moodScore (avg NPC need satisfaction) on SettlementEntry. Green inner glow ≥0.7, red <0.3. Skipped during active events.

- [x] **Hauler route memory** — bestProfit/bestRoute on Hauler, logged on new record, shown in tooltip as "Best: A→B +Xg" in faint GOLD.

- [x] **NPC greeting interactions** — Idle NPCs within 40u greet same-settlement neighbours. greetCooldown (2 real-sec) on DeprivationTimer. Logs to EventLog.

- [x] **Gang disbands on last member removed** — After confrontation scatter, check remaining bandits with same gangName. If none, log "[gang] has been disbanded."

- [x] **NPC migration considers bandit danger** — -5% migration score per bandit near road midpoint (100u radius, min 20%). Added in FindMigrationTarget after memory bonus.

- [x] **Bounty pool shown in settlement tooltip** — "Bounty: Xg" in faint GOLD in settlement hover tooltip when bountyPool > 0.5. Added to SettlementStatus, piped from Settlement.

- [x] **Bounty pool shown in stockpile panel** — "Bounty: Xg" in faint GOLD below treasury line when bountyPool > 0. Added to StockpilePanel, piped from Settlement in WriteSnapshot.

- [x] **Settlement danger indicator** — Pulsing red "!" next to settlement name when connected roads have 3+ total bandits. Sums banditCount from RoadEntry by nameA/nameB match.

- [x] **Road condition overlay toggle** — O key toggles m_showRoadCondition: Safety mode (bandits override) vs Condition mode (pure condition palette). Label in bottom-left.


- [x] **Reputation affects charity willingness** — Starving NPCs with Reputation::score < -0.5 skipped in charity block. Log: "[Helper] refused to help [NPC] (bad reputation)."


- [x] **Reputation shown in NPC tooltip** — Already implemented: "Rep: +X.X" in GREEN/RED, threshold 0.05, in HUD.cpp.


- [x] **Hauler profit summary in tooltip** — estimatedProfit computed from dest market price in WriteSnapshot. Shows "Est. profit: +Ng" in GREEN/RED during delivery.


- [x] **Hauler convoy formation** — Hauler::inConvoy set when GoingToDeposit within 60u of another hauler to same destination. 25% speed bonus, "[Convoy]" in green tooltip.


- [x] **Bandit scatter on confrontation** — Gang members with matching gangName get fleeTimer=3s + velocity away from player at 1.5x speed. Flee check added to bandit section of AgentDecisionSystem.


- [x] **Gang log announcement** — Logs "[Name] joined [gang] on the A-B road." when gangName transitions from empty to non-empty.


- [x] **Bandit bounty board** — Settlement::bountyPool accumulates 0.5g/hr per adjacent bandit (from treasury). Paid to player on confrontation with log.


- [x] **Road danger colour on world map** — Orange for 1-2 bandits, red for 3+. Overrides condition-based colouring.


- [x] **NPC reputation decay** — 0.01/game-hour toward zero for all Reputation entities.


- [x] **Hauler road avoidance** — 15% route score penalty per bandit (max 60%). Nervous log when departing on bandit road.


- [x] **Bandit gang name** — 2+ bandits at same road form named gang (e.g. "The X-Y Wolves"). Stored in DeprivationTimer::gangName, shown in tooltip.


- [x] **Bandit territory warning in road tooltip** — "Bandits: N" in faint RED in road tooltip. Pre-count map in WriteSnapshot, `banditCount` field on `RoadEntry`.


- [x] **Bandit flee on confrontation log detail** — Log includes bandit name, gold recovered, and nearest road's settlement names: "Player confronted X on the A-B road, recovered Ng."


- [x] **Bandit density cap per road** — Max 3 bandits per road midpoint; excess wander randomly. Pre-count map built before bandit loop; lurk selection skips full roads.

- [x] **Exile indicator in tooltip** — "[Exiled]" in faint RED on behavior line for homeless exiles.

- [x] **Wanderer re-settlement** — Exiled NPCs with 30g+ buy fresh start at nearest settlement.

- [x] **Charity radius shown on hover** — Faint 80u LIME circle around charity-ready NPCs on hover.

- [x] **Fatigue count in settlement tooltip** — "Fatigued workers: N" in ORANGE in settlement tooltip.

- [x] **Fatigue indicator in NPC tooltip** — "(fatigued)" in ORANGE appended to behavior line.

- [x] **Gratitude shown in world dot** — Faint LIME ring around grateful NPCs on world map.

- [x] **Charity cooldown shown in tooltip** — "Gave charity (X.Xh ago)" in faint LIME when timer > 0.

- [x] **Event log entry colour coding** — Added "stole"→RED, "saw through"→ORANGE, "Ally trade"→GREEN.

- [x] **Event colour in minimap ring** — Minimap ring uses ModifierColour() instead of generic yellow.

- [x] **Skill degradation with age** — Elder NPCs (65+ days) lose skills at 0.0002/tick, min 0.1.

- [x] **Theft log includes skill level** — Shows "farming X%" or "water X%" after theft penalty.

- [x] **NPC grudge after being stolen from** — Grateful NPCs within 80u see through thief; log social event.

- [x] **Settlement theft count in stockpile panel** — "Thefts: N" in faint red below treasury line.

- [x] **Alliance trade bonus log** — Log "Ally trade: X → Y (+Zg bonus)" on allied deliveries.

- [x] **Thief dot colour on world map** — Thieves drawn in MAROON for ~2 game-hours after stealing.

- [x] **Thief flees home after stealing** — 4-second sprint away from settlement after theft.

- [x] **Skills penalty on theft** — All 3 skills reduced by 0.02 per theft (social ostracism).

- [x] **Settlement founding event log** — Log includes name, coordinates, cost, and settler count.

- [x] **Worker fatigue accumulation** — Fatigued workers (energy < 0.2) produce at 80%.

- [x] **Abundant-supply notification** — Already implemented via s_loggedAbundance in RandomEventSystem.

- [x] **NPC reputation score** — Reputation component: +0.1 charity, -0.2 theft, +0.05 delivery.

- [x] **Settlement morale shown in NPC tooltip** — "Home morale: X%" faint line in NPC tooltip.

- [x] **Strike duration shown in tooltip** — "On strike (Xh left)" with remaining hours.

- [x] **NPC wage shown in tooltip** — "Wage: ~X.Xg/hr" for working NPCs based on skill.

- [x] **Facility morale included in est. output** — estOutput now includes morale production modifier.

- [x] **Market price spike log** — Logs "Price spike: X at Y now Zg (+N%)" rate-limited per 12h.

- [x] **Desperation count in settlement tooltip** — "Desperation buys: N/day" in red when > 0.

- [x] **Recovery morale bump** — +0.01 morale per recovered resource when scarcity clears.

- [x] **Scarcity causes NPC migration nudge** — +0.3 migration score per scarce resource at home.

- [x] **Settlement import/export balance** — Import/export counts displayed in settlement tooltip.

- [x] **Idle hauler dimming** — Idle empty haulers drawn at 50% opacity.

- [x] **Bankruptcy log includes gold balance** — Shows "(Xg left)" in bankruptcy log.

- [x] **Hauler graduation gold threshold shown in tooltip** — Shows "Hauler at: 100g" for NPCs approaching graduation.

- [x] **NPC mood emoji in residents list** — Colored mood dot per resident in stockpile panel.

- [x] **Stockpile bar chart in panel** — 3 horizontal bars showing food/water/wood levels.

- [x] **Wealthy NPC golden ring** — Faint gold ring on NPCs with balance > 80g.

- [x] **Bankruptcy countdown in tooltip** — Shows "~Xh left" countdown for near-bankrupt haulers.

- [x] **Hauler return trip line** — Gray return-trip line drawn for haulers heading home.

- [x] **Route line colour by cargo type** — Route lines now coloured by cargo (green/blue/brown).

- [x] **Hauler loyalty bonus** — 5% extra earned on home settlement deliveries.

- [x] **Trade hub badge in settlement tooltip** — "[Trade Hub]" badge shown for settlements with ≥5 trades/day.

- [x] **Low-morale NPC grumbling log** — Grumbling events logged during gossip at low-morale settlements.

- [x] **Hauler state label in tooltip** — Hauler status shown in hover tooltip.

- [x] **Production output shown in settlement tooltip** — Output rates per resource in tooltip.

- [x] **Morale factor shown in production tooltip** — Morale +/-% shown in facility tooltip.

- [x] **NPC desperation purchase log** — Logs "X desperate — bought food at Y market for Zg."

- [x] **Scarcity recovery log** — Logs "Recovery: X food stores recovering." on resource recovery.

- [x] **Hauler idle duration warning** — Logs "idle for 12h — no profitable routes." once per idle period.

- [x] **Graduation log includes gold saved** — Shows "(125g)" in hauler graduation log.

- [x] **Morale bar in stockpile panel** — Horizontal bar below treasury, green/yellow/red by threshold.

- [x] **Near-bankrupt tooltip warning** — Show "!! Near bankruptcy !!" in red in hauler tooltip.

- [x] **Hauler route line on world map** — Faint sky-blue line from hauler to destination.

- [x] **Settlement trade volume counter** — Track deliveries per settlement, show "Trades: N/day" in tooltip.

- [x] **Hauler route efficiency tooltip** — Show "Route: Xkm" in tooltip for haulers en route.

- [x] **Morale-dependent work speed** — Continuous morale factor in ProductionSystem: 1.0 + 0.3*(morale - 0.5).

- [x] **Scarcity log event** — Log "Shortage: X running low on food, water" once per scarcity
  period per resource, using bitmask tracking in RandomEventSystem.cpp.

- [x] **Hauler graduation celebration** — New haulers celebrate 2h and home morale +0.02.

- [x] **Abundance end log** — Log "Abundance fading at X — stores declining." when settlement
  exits abundance.

- [x] **Bankrupt hauler flash before demotion** — Red pulsating ring on haulers near bankruptcy
  (timer > 75% of BANKRUPTCY_HOURS). Add `nearBankrupt` bool to AgentEntry.

- [x] **Hauler profit margin in tooltip** — Show "Profit: ~Ng" for haulers with cargo. Add
  `haulerBuyPrice` and `haulerCargoQty` to `AgentEntry`. Green if positive, red if negative.

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

- [x] **Hauler profit margin in tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), when hovering
  a hauler with cargo, show "Profit: ~Ng" calculated as `money.balance - buyPrice * cargoQty`
  (approximate margin from last trade). Read `Hauler::buyPrice` and `Inventory::contents` from
  `AgentEntry` fields (add `float haulerBuyPrice` and `int haulerCargoQty` to `AgentEntry` in
  `RenderSnapshot.h`, populate in SimThread). Display in green if positive, red if negative.

- [x] **Bankrupt hauler flash before demotion** — In `EconomicMobilitySystem.cpp`, when a
  hauler's `s_bankruptTimer` exceeds `BANKRUPTCY_HOURS * 0.75` (18h), set a `bool nearBankrupt`
  flag on a new `AgentEntry` field. In `GameState.cpp`'s agent render loop, draw a faint red
  pulsating ring (like celebrating but red) around near-bankrupt haulers. Gives visual warning
  before demotion. Add `bool nearBankrupt` to `AgentEntry` in `RenderSnapshot.h`.

- [x] **Abundance end log** — In `RandomEventSystem::Update`'s abundance block, when a
  settlement exits abundance (was in `s_loggedAbundance` but `abundant` is now false and not yet
  below the scarcity reset), log "Abundance fading at [settlement] — stores declining." before
  erasing from the set. Gives a narrative arc: prosperity → warning → scarcity.

- [x] **Hauler graduation celebration** — In `EconomicMobilitySystem.cpp`'s NPC→Hauler
  graduation block, set the new hauler's `AgentState::behavior = Celebrating` for 2 game-hours
  (set `celebrateTimer = 2.f` on `Goal`). Also bump home settlement morale by +0.02.
  Becoming a hauler is a proud moment for the community.

- [x] **Scarcity log event** — In `RandomEventSystem::Update`'s scarcity check, log "Shortage:
  [settlement] running low on [resource]" once per scarcity period per resource. Use a
  `static std::map<entt::entity, int> s_loggedScarcity` keyed by entity, value is a bitmask
  (bit 0=food, 1=water, 2=wood). Set bit on first trigger, clear when that resource rises above
  20. Keeps scarcity visible in the event log without spam.

- [x] **Morale-dependent work speed** — In `ProductionSystem.cpp`'s per-worker contribution
  block, multiply worker output by a morale factor: `1.0 + 0.3 * (morale - 0.5)`. At morale
  0.8 workers produce +9%, at 0.2 they produce -9%. Read morale from `try_get<Settlement>` via
  worker's `HomeSettlement`. Makes morale mechanically important beyond cosmetic display.

- [x] **Hauler route efficiency tooltip** — In `HUD::DrawHoverTooltip`, for haulers with an
  active route (`hasRouteDest`), show "Route: Xkm" (Euclidean distance from current pos to
  dest / 100). If the hauler also has a `homeSettlementName`, show round-trip distance. Helps
  players evaluate trade route length. No new snapshot fields — compute from existing pos/dest.

- [x] **Settlement trade volume counter** — Add `int tradeVolume = 0` to `SettlementEntry` in
  `RenderSnapshot.h`. In `TransportSystem.cpp`'s delivery block, increment a `static
  std::map<entt::entity, int> s_tradeVolume` per destination. In SimThread's settlement
  snapshot, read and write it to the entry. Display in settlement tooltip as "Trades: N"
  showing total deliveries received. Reset counter every 24 game-hours.

- [x] **Hauler route line on world map** — In `GameState.cpp`'s agent render loop, when a
  hauler has `hasRouteDest` true, draw a thin dashed line from the hauler's position to
  `(destX, destY)` using `DrawLineV` with `Fade(SKYBLUE, 0.3f)`. This makes active trade
  routes visible on the world map without needing to hover the hauler. No new snapshot fields.

- [x] **Near-bankrupt tooltip warning** — In `HUD::DrawHoverTooltip` (HUD.cpp), when
  `best->nearBankrupt` is true, show a "!! Near bankruptcy !!" line in `Fade(RED, 0.9f)`.
  Add to lineCount and width calc. Complements the red ring with textual detail.

- [x] **Scarcity resource label in log** — Already implemented as part of "Scarcity log event"
  task. Uses `s_loggedScarcity` bitmask in `RandomEventSystem.cpp`.

- [x] **Morale bar in stockpile panel** — In `RenderSystem::DrawStockpilePanel`, below the
  existing treasury line, draw a small horizontal morale bar (width 100px, height 8px). Fill
  proportional to `panel.morale` (already in `StockpilePanel`). Colour: green ≥0.7, yellow
  ≥0.3, red <0.3. Label "Morale" to the left. Visual complement to the M:XX% in status bar.

- [x] **Graduation log includes gold saved** — In `EconomicMobilitySystem.cpp`'s hauler
  graduation log, append the NPC's balance: "X saved enough (125g) to become a hauler at Y".
  Gives the player a sense of how much wealth is involved in the career transition.

- [x] **Hauler idle duration warning** — In `TransportSystem.cpp`, when a hauler has been in
  `HaulerState::Idle` for more than 12 game-hours (`waitTimer > 12.f`), log "Hauler X idle
  for 12h at Y — no profitable routes." once per idle period using a `static
  std::set<entt::entity> s_loggedIdle`. Clear on state transition away from Idle. Surfaces
  stuck haulers that may need player attention.

- [x] **Scarcity recovery log** — In `RandomEventSystem::Update`'s scarcity bitmask block,
  when a resource bit is cleared (rises above 20), log "Recovery: X food stores recovering."
  per resource. Use the existing `s_loggedScarcity` map — detect clearing before `mask &= ~bit`.
  Completes the scarcity narrative arc: shortage → recovery.

- [x] **NPC desperation purchase log** — In `ConsumptionSystem.cpp`'s emergency market purchase
  block (where an NPC buys from a distant settlement because local stockpile is empty), log
  "X desperate — bought food from Y market at Ng." once per NPC per 12 game-hours. Use a
  `static std::map<entt::entity, float> s_desperateCooldown` drained by gameHoursDt. Shows
  when NPCs are forced into expensive emergency purchases.

- [x] **Morale factor shown in production tooltip** — In `HUD::DrawFacilityTooltip` (HUD.cpp),
  append "Morale: +X%" or "Morale: -X%" to the facility tooltip when morale factor differs
  from 1.0. Add `float morale = 0.5f` to `FacilityEntry` in `RenderSnapshot.h`, populate from
  `Settlement::morale` via facility's `settlement` field in SimThread. Green when positive,
  red when negative.

- [x] **Production output shown in settlement tooltip** — In `HUD::DrawSettlementTooltip`
  (HUD.cpp), add a line "Output: food X/h, water Y/h" showing the sum of `baseRate * scale`
  for each facility type at this settlement. Add `float foodRate, waterRate, woodRate` to
  `SettlementStatus` in `RenderSnapshot.h`, computed in SimThread by iterating
  `ProductionFacility` components with matching settlement.

- [x] **Hauler state label in tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), for haulers
  show their current state as a label: "Idle — seeking route", "Loading at X", "Delivering to
  X", "Returning home". Map from `HaulerState` enum values. Add `int haulerState = 0` to
  `AgentEntry` in `RenderSnapshot.h`, populated from `Hauler::state` in SimThread. No gameplay
  changes — purely informational.

- [x] **Low-morale NPC grumbling log** — In `AgentDecisionSystem.cpp`'s gossip block, when
  two NPCs at a settlement with morale < 0.3 gossip, 20% chance to log "X and Y grumble about
  conditions at Z." Use the existing gossip proximity check. Rate-limit per settlement to once
  per 12 game-hours via `static std::map<entt::entity, float> s_grumbleCooldown`. Adds social
  texture to low-morale settlements.

- [x] **Trade hub badge in settlement tooltip** — In `HUD::DrawSettlementTooltip` (HUD.cpp),
  when `tradeVolume >= 5`, show "[Trade Hub]" in `Fade(GOLD, 0.8f)` after the settlement name
  on line 1. Recognises settlements with high trade throughput. No new snapshot fields — read
  directly from `best->tradeVolume` already in `SettlementEntry`.

- [x] **Hauler loyalty bonus** — In `TransportSystem.cpp`'s delivery block, if the hauler's
  `HomeSettlement` matches the destination settlement, add a 5% bonus to `earned` (loyalty
  discount from the local merchant). Log "X received local loyalty bonus at Y." once per
  delivery. Credits the hauler's balance. No new components — use existing `HomeSettlement`
  and `Money` checks.

- [x] **Route line colour by cargo type** — In `GameState.cpp`'s hauler route line, colour
  the line based on `a.cargoDotColor` instead of always SKYBLUE: green for food, blue for
  water, brown for wood, skyblue when empty. Makes trade flow resource types visible.
  No new snapshot fields — `cargoDotColor` and `hasCargoDot` already available.

- [x] **Hauler return trip line** — In `GameState.cpp`'s agent render loop, for haulers
  without `hasRouteDest` but with a `homeSettlementName`, draw a faint gray line back toward
  home. Requires adding `float homeX, homeY` to `AgentEntry` in `RenderSnapshot.h` (populated
  from `HomeSettlement` position in SimThread). Shows both outbound (coloured) and return
  (gray) trade flow.

- [x] **Bankruptcy countdown in tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), when
  `best->nearBankrupt` is true, also show "Bankrupt in: ~Xh" estimating remaining time.
  Add `float bankruptTimer = 0.f` to `AgentEntry` in `RenderSnapshot.h`, populated from
  `Hauler::nearBankrupt` flag plus the static timer in EconomicMobilitySystem. Since the timer
  is static in EconomicMobilitySystem, add a `float bankruptProgress` field to `Hauler` struct
  that gets set alongside `nearBankrupt`. Display as `24 - bankruptProgress` hours remaining.

- [x] **Wealthy NPC golden ring** — In `GameState.cpp`'s agent render loop, when an NPC
  (non-hauler, non-player) has `balance > 80`, draw a faint `Fade(GOLD, 0.25f)` outer ring
  (radius 8). Makes wealthy NPCs visually distinct and highlights economic stratification.
  No new snapshot fields — `balance` is already in `AgentEntry`.

- [x] **Stockpile bar chart in panel** — In `RenderSystem::DrawStockpilePanel`, below the
  per-resource text lines, draw 3 small horizontal bars (food=green, water=blue, wood=brown)
  whose width is proportional to `qty / 200.f` (capped at 100px). Background in dark gray.
  Gives a visual at-a-glance read of relative stockpile levels. No new snapshot fields.

- [x] **NPC mood emoji in residents list** — In `RenderSystem::DrawStockpilePanel`'s residents
  loop, after the profession abbreviation and gold amount, append a tiny mood indicator based
  on the resident's contentment stored in `StockpilePanel::AgentInfo`. Add `float contentment`
  to `StockpilePanel::AgentInfo` in `RenderSnapshot.h`, populate in SimThread. Show a green
  dot (>0.7), yellow dot (>0.4), or red dot (<0.4) after each resident's name line.

- [x] **Hauler graduation gold threshold shown in tooltip** — In `HUD::DrawHoverTooltip`
  (HUD.cpp), for non-hauler NPCs with balance > 50, show "Hauler at: 100g" (the graduation
  threshold) with a tiny progress bar. Read `GRADUATION_THRESHOLD` value (100g) as a constant.
  Shows NPC's progress toward becoming a hauler. No new snapshot fields — `balance` already
  available.

- [x] **Bankruptcy log includes gold balance** — In `EconomicMobilitySystem.cpp`'s bankruptcy
  log, append the hauler's remaining balance: "X went bankrupt (2g left) — returned to labor
  at Y". Mirrors the graduation log's gold display. Use `money.balance` already in scope.

- [x] **Idle hauler dimming** — In `GameState.cpp`'s agent render loop, when a hauler has
  `behavior == Idle` and `haulerCargoQty == 0`, draw them at 50% opacity (`Fade(drawColor, 0.5f)`)
  to visually distinguish active traders from idle ones. No new snapshot fields — `behavior`
  and `haulerCargoQty` already in `AgentEntry`.

- [x] **Settlement import/export balance** — In `TransportSystem.cpp`'s delivery block, track
  net goods flow per settlement: `static std::map<entt::entity, int> s_exportCount` incremented
  at source on pickup, `s_importCount` incremented at destination on delivery. Add
  `int imports = 0, exports = 0` to `SettlementEntry` in `RenderSnapshot.h`. Display in
  settlement tooltip as "Trade: +N imports / -N exports". Reset every 24 game-hours alongside
  `tradeVolume`.

- [x] **Scarcity causes NPC migration nudge** — In `AgentDecisionSystem.cpp`'s
  `FindMigrationTarget` scoring, when the NPC's home settlement has any stockpile below 10
  (read from `Stockpile` component), add +0.3 to the migration score of all other settlements.
  Makes NPCs more likely to migrate away from scarce settlements. Use
  `registry.try_get<Stockpile>(home)` with the same threshold as `SCARCITY_THRESHOLD` (10).

- [x] **Recovery morale bump** — In `RandomEventSystem::Update`'s new recovery detection block,
  when a resource recovers (bit cleared from `s_loggedScarcity`), bump settlement morale by
  +0.01 per recovered resource. Model: recovery from scarcity is a small community boost.
  Use `registry.try_get<Settlement>(e)->morale` already in scope.

- [x] **Desperation count in settlement tooltip** — Track emergency purchases per settlement
  per 24h cycle. Add `int desperatePurchases = 0` to `Settlement` component in `Components.h`.
  Increment in `ConsumptionSystem.cpp` when a purchase fires. Reset alongside `tradeVolume`
  in `RandomEventSystem.cpp`. Add to `SettlementEntry` in `RenderSnapshot.h`. Display in
  `HUD::DrawSettlementTooltip` as "Desperation buys: N/day" in red when > 0.

- [x] **Market price spike log** — In `PriceSystem.cpp`'s price update loop, when a resource
  price increases by more than 20% in a single update cycle, log "Price spike: food at X now
  Yg (+Z%)." once per resource per settlement per 12 game-hours. Use a `static
  std::map<std::pair<entt::entity,int>, float> s_priceSpikeCooldown` (entity + resource type
  as key). Surfaces sudden price changes that might trigger migration or trade shifts.

- [x] **Facility morale included in est. output** — In `HUD::DrawFacilityTooltip` (HUD.cpp),
  multiply `estOutput` by the morale factor `1.0 + 0.3*(morale - 0.5)` so the estimated
  output reflects the actual production speed. Currently the estimate ignores morale.
  No new snapshot fields — `best->morale` already available.

- [x] **NPC wage shown in tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), for working NPCs
  (non-hauler, non-player, behavior == Working), show "Wage: ~X.Xg/hr" calculated as
  `WAGE_RATE * (0.5 + skill)` using the NPC's primary skill. Add `float wage = 0.f` to
  `AgentEntry` in `RenderSnapshot.h`, computed in SimThread from the worker's skill and
  `WAGE_RATE` (0.3). Display in `Fade(GOLD, 0.7f)`.

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

- [x] **Strike duration shown in tooltip** — Extend the "On strike" line in `HUD::DrawHoverTooltip`
  to show the remaining duration: `"On strike (%.0f h left)"` using `DeprivationTimer::strikeDuration`
  exposed as a new `float strikeHoursLeft` field in `AgentEntry`. Populate it in `SimThread::WriteSnapshot`
  alongside `onStrike`. Divide `strikeDuration` by 60 to convert sim-seconds to game-hours.

- [x] **Settlement morale shown in NPC tooltip** — Add the home settlement's morale as a faint
  line at the bottom of the hover tooltip: `"Home morale: 72%"` (skip if NPC has no home).
  Expose it via a new `float homeMorale` field in `AgentEntry` (default -1 = no home).
  Set it in `SimThread::WriteSnapshot` via `registry.try_get<Settlement>(hs->settlement)->morale`.
  In `HUD::DrawHoverTooltip`, if `homeMorale >= 0`, add the line coloured GREEN/YELLOW/RED
  using the same thresholds as the morale bar in `DrawStockpilePanel`.

- [x] **NPC reputation score** — Add a `Reputation` component (float `score`, initially 0) to
  each NPC. Increase it when the NPC gives charity (`charityTimer` resets), decrease it when
  they steal. Haulers gain +0.05 rep per completed delivery. In `HUD::DrawHoverTooltip`, expose
  the score as a `float reputation` field in `AgentEntry` and display `"Rep: +3.2"` (or negative
  in RED) when non-zero. This creates visible social hierarchy without requiring new UI panels.

- [x] **Abundant-supply notification** — When the morale surplus bonus first activates for a
  settlement (i.e., was NOT abundant last tick, now IS abundant), log a one-time message to
  `EventLog`: `"Greenfield: plentiful supplies boost morale"`. Track this with a
  `bool abundantLastTick` field on `Settlement` (Components.h). Prevents silent changes and
  gives the player feedback that their supply investment is paying off.
  NOTE: Already implemented via `s_loggedAbundance` static set in RandomEventSystem.cpp.

- [x] **Worker fatigue accumulation** — In `NeedDrainSystem.cpp`, when an NPC with `Schedule` is
  in `Working` state and energy need falls below 0.2, apply a 20% production penalty via a new
  `bool fatigued` bool on the `Schedule` component. In `ProductionSystem.cpp`, multiply yield by
  `0.8f` if `sched->fatigued`. `fatigued` is cleared when energy recovers above 0.5 (also in
  `NeedDrainSystem`). This makes sleep deprivation have visible production consequences.

- [x] **Settlement founding event log** — In `SimThread::ProcessInput` (SimThread.cpp), when the
  player successfully founds a settlement (P key, cost 1500g), log it to `EventLog`: `"Player
  founded [settlement name] at (x, y)"`. Look up the newly created Settlement entity immediately
  after `WorldGenerator::FoundSettlement` returns. This makes founding visible in the event log
  alongside other major events.

- [x] **Bandit flee on confrontation log detail** — When the player confronts a bandit (E key),
  log the bandit's name, gold recovered, and the road they were lurking near. In `SimThread::ProcessInput`'s
  bandit confrontation block, find the nearest Road entity and include its endpoint settlement names
  in the log message: "Player confronted [name] on the [A]–[B] road, recovered [N]g."

- [x] **Bandit territory warning in road tooltip** — When hovering a road in HUD tooltip, if the
  pre-built `banditsPerRoad` count for that road is > 0, show "Bandits: N" in faint RED. Add
  `int banditCount = 0` to `RenderSnapshot::RoadEntry`. In `SimThread::WriteSnapshot`, build the
  same nearest-road map used in `AgentDecisionSystem` and write the count per road entry.

- [x] **Bandit gang name** — When 2+ bandits lurk at the same road, assign them a shared gang name
  (e.g. "The [Road-A]-[Road-B] Wolves"). Add `std::string gangName` to `DeprivationTimer`. In
  `AgentDecisionSystem`'s bandit lurk block, after selecting a road, if the road already has 1+
  bandits, copy the existing gang name; otherwise generate one from the road's endpoint settlement
  names. Show gang name in bandit tooltip after "[Bandit]".

- [x] **Hauler road avoidance** — Haulers with cargo should prefer roads with fewer bandits. In
  `TransportSystem`'s route selection (if applicable) or `AgentDecisionSystem`'s hauler pathfinding,
  add a penalty to road attractiveness proportional to nearby bandit count. Haulers already know their
  destination; if multiple paths exist, prefer the safer one. If only one path, proceed but log a
  warning: "[Hauler] nervously travels the [A]-[B] road (N bandits spotted)."

- [x] **NPC reputation decay** — Reputation should slowly decay toward 0 over time. In
  `AgentDecisionSystem`, reduce `reputation` by 0.01 per game-hour toward zero (positive reps decay
  down, negative decay up). This prevents permanent reputation from a single act and encourages
  ongoing good/bad behavior. Add `reputationDecayRate` constant near other reputation constants.

- [x] **Road danger colour on world map** — In `GameState::Draw`, colour road lines based on
  bandit presence. Use `RoadEntry::banditCount`: 0 = normal grey, 1-2 = Fade(ORANGE, 0.5f),
  3+ = Fade(RED, 0.6f). Gives immediate visual feedback about road safety without hovering.

- [x] **Bandit bounty board** — When a settlement has bandits on an adjacent road, NPCs at that
  settlement accumulate a `bountyPool` (add `float bountyPool = 0.f` to `Settlement` component).
  Each game-hour, each settlement adds 0.5g per adjacent-road bandit to the pool. When the player
  confronts a bandit on a road adjacent to a settlement with bountyPool > 0, they receive the pool
  as bonus gold (credited from treasury). Log: "Collected Ng bounty from [settlement]."

- [x] **Gang log announcement** — When a new gang forms (a bandit gets a gangName that was empty
  before), log it: "[NPC name] joined [gang name] on the [A]-[B] road." In `AgentDecisionSystem`'s
  bandit lurk block, compare old gangName with new before overwriting. If old was empty and new is
  not, push an EventLog entry.

- [x] **Bandit scatter on confrontation** — When the player confronts a bandit, other bandits in
  the same gang (same gangName) should flee briefly. In `SimThread::ProcessInput`'s confrontation
  block, after removing BanditTag from the confronted bandit, iterate nearby BanditTag entities
  with matching gangName; set their `fleeTimer = 3.f` and velocity away from the player. Makes
  confrontation feel more dynamic.

- [x] **Hauler convoy formation** — When 2+ haulers travel the same road in the same direction
  within 60 units of each other, they form an informal convoy. Add `bool inConvoy = false` to the
  `Hauler` component. In `TransportSystem`'s `GoingToDeposit` state, check for other GoingToDeposit
  haulers heading to the same destination within 60u; if found, set `inConvoy = true`. Convoys get
  a 25% speed bonus. Pipe `inConvoy` through `RenderSnapshot::AgentEntry` and show in tooltip.

- [x] **Hauler profit summary in tooltip** — Show estimated trip profit in the hauler tooltip.
  Add `float estimatedProfit = 0.f` to `RenderSnapshot::AgentEntry`. In `SimThread::WriteSnapshot`,
  compute `(destPrice - buyPrice) * cargoQty * 0.8` (after 20% tax). Show as "Est. profit: +Ng"
  in faint GREEN below the cargo line. Helps the player understand hauler economics at a glance.

- [x] **Reputation shown in NPC tooltip** — (already implemented) "Rep: +X.X" in GREEN/RED tooltip line.

- [x] **Reputation affects charity willingness** — NPCs with negative reputation should be less
  likely to receive charity. In `AgentDecisionSystem`'s charity block, when checking if a neighbour
  qualifies for help, skip NPCs whose `Reputation::score < -0.5f`. Antisocial NPCs get cold-shouldered
  by the community. Log when charity is refused: "[Helper] refused to help [Thief] (bad reputation)."

- [x] **Road condition overlay toggle** — Pressing 'O' toggles between two road colour modes:
  the default bandit/condition mode and a pure condition view (ignoring bandits). Add `bool
  m_showRoadCondition = false` to `GameState`. When toggled, road colours use condition-only
  palette. Small "Road: Safety / Condition" label in corner shows active mode.

- [x] **Settlement danger indicator** — Settlements adjacent to bandit-heavy roads (3+ bandits
  total on connected roads) show a faint red exclamation mark above the settlement name on the
  world map. In `GameState::Draw`'s settlement loop, sum `banditCount` from all roads where
  `nameA == settlement.name || nameB == settlement.name`; if total >= 3, draw the indicator.

- [x] **Bounty pool shown in stockpile panel** — Add `float bountyPool = 0.f` to
  `RenderSnapshot::StockpilePanel`. In `SimThread::WriteSnapshot`'s stockpile panel section, copy
  `Settlement::bountyPool` into the panel. In `HUD::DrawStockpilePanel`, show "Bounty: Xg" in
  faint GOLD below the treasury line when bountyPool > 0. Lets players know which settlements
  are worth patrolling for bounty payouts.

- [x] **Bounty pool shown in settlement tooltip** — Add `float bountyPool = 0.f` to
  `RenderSnapshot::SettlementStatus`. In `SimThread::WriteSnapshot`'s worldStatus block, copy
  `Settlement::bountyPool`. In `HUD::DrawWorldStatus` settlement tooltip, show "Bounty: Xg" in
  faint GOLD when bountyPool > 0.5. Quick glance at which towns are offering bounties.

- [x] **NPC migration considers bandit danger** — In `AgentDecisionSystem`'s `FindMigrationTarget`
  (or migration trigger block), penalize settlements connected by bandit-heavy roads. For each
  candidate settlement, sum `banditCount` on its connecting roads; apply a -5% migration score
  penalty per bandit. NPCs prefer safer destinations. Affects the `MigrationMemory`-based scoring.

- [x] **Gang disbands on last member removed** — In `SimThread::ProcessInput`'s bandit confrontation
  block, after removing the BanditTag, check if any other bandit still has the same gangName. If not,
  log "[gang name] has been disbanded." in EventLog. Gives satisfying closure when the player clears
  a gang. Check via `registry.view<BanditTag, DeprivationTimer>` filtering by gangName match.

- [x] **NPC greeting interactions** — Idle NPCs within 40 units of each other occasionally exchange
  greetings. In `AgentDecisionSystem`, when an NPC is in Idle schedule state and hasn't greeted
  recently (add `float greetCooldown = 0.f` to `DeprivationTimer`), find the nearest idle NPC
  within 40 units. If found, log "[Name] greets [Other]" and set cooldown to 120 game-seconds.
  Adds ambient social texture to settlements without gameplay impact.

- [x] **Hauler route memory** — Haulers remember their most profitable completed trip. Add
  `float bestProfit = 0.f` and `std::string bestRoute = ""` to `Hauler`. On delivery completion
  in `TransportSystem`, if profit exceeds bestProfit, update both fields and log "[Hauler] sets
  new personal record: +Xg on [A]→[B]". Pipe bestRoute through RenderSnapshot and show in tooltip
  as "Best: [A]→[B] +Xg" in faint GOLD.

- [x] **Settlement mood indicator** — Settlements where average NPC need satisfaction is high
  (all needs > 0.6 for 80%+ of residents) display a faint green glow, while struggling settlements
  (any need < 0.3 for 50%+ of residents) show faint red. In `SimThread::WriteSnapshot`, compute
  per-settlement mood score from NPC needs. Add `float moodScore = 0.5f` to `RenderSnapshot::SettlementEntry`.
  In `GameState::Draw`, tint the settlement circle border based on mood.

- [x] **Settlement morale boost on bandit cleared** — In `SimThread::ProcessInput`'s confrontation
  block, after removing a bandit, find road-adjacent settlements (road.from / road.to) and add
  +0.05 morale to each `Settlement::morale` (clamped to 1.0). Log: "[Settlement] morale improved
  (+5%) after bandit threat reduced." Connects player action to settlement wellbeing.

- [x] **Bandit flee visual indicator** — In `GameState::Draw`'s agent loop, when an agent has
  `isBandit == true` and `fleeTimer > 0` (add `float fleeTimer = 0.f` to `RenderSnapshot::AgentEntry`),
  draw a brief speed trail: a fading line from (x, y) in the opposite direction of velocity.
  Pipe `DeprivationTimer::fleeTimer` through `SimThread::WriteSnapshot` into the new field.

- [x] **NPC skill-up notification** — When an NPC's skill (farming/water/woodcutting) crosses a
  0.25 threshold (0.25, 0.50, 0.75), log "[Name] is becoming skilled at [type]" in EventLog. In
  `ProductionSystem`, after applying skill gain, check if the new value crossed a threshold while
  the old didn't. Brief faint GOLD text particle above the NPC would be a bonus but the log alone
  is the core feature.

- [x] **Hauler bankruptcy warning log** — When a hauler's `bankruptProgress` crosses 0.5 for the
  first time, log "[Hauler] is struggling financially" in EventLog. Add `bool bankruptWarned = false`
  to `Hauler`. In `EconomicMobilitySystem`, when bankruptProgress >= 0.5 and !bankruptWarned, log
  and set flag. Gives players advance notice before a hauler actually goes bankrupt.

- [x] **NPC witness bandit confrontation** — In `SimThread::ProcessInput`'s confrontation block,
  after the scatter, find non-bandit NPCs within 120 units. For each witness, add +0.1 to their
  `Reputation::score` (they saw justice done) and log: "[NPC] witnessed [player] confront [bandit]."
  Uses existing `Reputation` component; creates social memory of player's actions.

- [x] **Settlement trade volume in tooltip** — Add `float tradeVolume24h = 0.f` to
  `RenderSnapshot::SettlementStatus`. In `SimThread::WriteSnapshot`, copy `Settlement::tradeVolume`
  (accumulated over rolling window). In `HUD::DrawWorldStatus` settlement tooltip, show
  "Trade volume: Xg/day" in faint WHITE. Helps player identify economic hubs vs backwaters.

- [x] **NPC remembers last meal source** — Add `std::string lastMealSource = ""` to
  `DeprivationTimer`. When an NPC eats (in `ConsumptionSystem`), record the settlement name. When
  hunger drops below 0.2 and lastMealSource is set, log "[NPC] is grateful to [Settlement] for
  food." Clear on next meal. Adds narrative flavour connecting NPCs to specific settlements.

- [x] **Convoy visual on world map** — In `GameState::Draw`'s agent loop, when a hauler has
  `inConvoy == true`, draw a faint connecting line between convoy members. Iterate agents to find
  pairs with `inConvoy && haulerState == 1` within 60u; draw `DrawLineEx` in `Fade(GREEN, 0.25f)`
  between their positions. Makes convoys visible at a glance.

- [x] **Convoy log announcement** — In `TransportSystem`'s GoingToDeposit convoy check, when
  `inConvoy` transitions from false to true, log: "[Hauler A] formed convoy with [Hauler B] on
  the way to [settlement]." Add a `bool wasInConvoy` local to compare before/after. Use the same
  EventLog pattern as the nervous-travel log. Only log once per formation (check old state).

- [x] **Convoy bandit deterrence** — In `AgentDecisionSystem`'s bandit intercept block, skip
  haulers with `Hauler::inConvoy == true`. Bandits won't attack a convoy — too risky. This gives
  convoys a gameplay purpose beyond speed: safety. Already have the `inConvoy` field on the Hauler
  component; just add an `if (h.inConvoy) return;` check in the intercept lambda.

- [x] **Hauler trip history summary** — Add `int lifetimeTrips = 0; float lifetimeProfit = 0.f`
  to `Hauler` component. In `TransportSystem`'s GoingToDeposit arrival block (where cargo is sold),
  increment `lifetimeTrips` and add actual profit to `lifetimeProfit`. Pipe both through
  `RenderSnapshot::AgentEntry` and show "Trips: N (total +Xg)" in faint LIGHTGRAY in tooltip.

- [x] **Hauler loss-making trip log** — In `TransportSystem`'s GoingToDeposit arrival block,
  after computing actual profit from selling cargo, if profit < 0, log: "[Hauler] completed a
  loss-making trip to [settlement] (-Xg)." Helps player identify failing trade routes. Uses
  existing EventLog and TimeManager access pattern from the nervous-travel log.

- [x] **Reputation gain from charity** — In `AgentDecisionSystem`'s charity block, after a
  successful gift transfer, add +0.2 to the helper's `Reputation::score` (use `emplace_or_replace`
  if missing). Giving charity should build reputation, creating a positive feedback loop where
  generous NPCs are well-regarded and receive help when they need it themselves.

- [x] **Reputation loss from theft** — In `ConsumptionSystem`'s theft block (where NPCs steal
  from stockpiles), subtract 0.5 from the thief's `Reputation::score`. If the Reputation component
  doesn't exist, emplace it with score = -0.5. Connects the existing theft mechanic to the
  reputation system so thieves gradually become social pariahs.

- [x] **NPC flee from bandits** — Non-bandit NPCs within 60 units of a bandit who just stole
  from a hauler should briefly flee. In `AgentDecisionSystem`'s theft aftermath (after bandit takes
  cargo), find nearby non-bandit NPCs and set their velocity away from the bandit for 2 seconds.
  Add `float panicTimer = 0.f` to `DeprivationTimer`. NPCs with panicTimer > 0 skip normal
  decision-making. Creates a visceral sense of danger around bandit activity.

- [x] **Settlement population graph in stockpile panel** — In `RenderSystem::DrawStockpilePanel`,
  when `panel.popHistory` has 2+ entries, draw a simple line graph (60px tall, spanning panel
  width) showing population over time. Use GREEN for growth segments, RED for decline. Already
  have `popHistory` in StockpilePanel — just need the rendering code. Draw below the resident list.

- [x] **NPC morale comment in tooltip** — When hovering an NPC, show a one-line mood comment
  based on contentment: >0.8 "Content", 0.5-0.8 "Getting by", 0.3-0.5 "Struggling", <0.3
  "Desperate". Add to the tooltip line sequence in `HUD.cpp`'s agent tooltip section. Use matching
  colors (GREEN/YELLOW/ORANGE/RED). No new data needed — contentment already in AgentEntry.

- [x] **Bandit threat radius visual** — When hovering a bandit NPC, draw a faint RED circle
  showing the 80-unit intercept range where they can attack haulers. In `GameState::Draw`'s agent
  loop (where charity radius is drawn for helpers), add a parallel check: if hovered agent is
  `isBandit`, draw `DrawCircleLinesV` at 80u radius in `Fade(RED, 0.2f)`. Helps player gauge danger.

- [x] **NPC family visit behaviour** — NPCs with a `FamilyTag` occasionally visit family members
  at other settlements. In `AgentDecisionSystem`, when an NPC is idle and has family elsewhere
  (check FamilyTag::name matches across settlements), 5% chance per game-hour to set movement
  toward the family member's settlement. Log "[Name] is visiting family in [Settlement]." Return
  home after 30 game-minutes. Add `float visitTimer = 0.f` to DeprivationTimer.

- [x] **Settlement rivalry events** — When two adjacent settlements both have morale > 0.7
  and pop > 15, trigger a "rivalry" modifier. In `RandomEventSystem`, check pairs of connected
  settlements. Rivalry reduces trade between them by 20% (apply penalty in TransportSystem's
  route scoring). Log "[A] and [B] are competing for regional dominance." Lasts 24 game-hours.
  Add `std::string rivalWith = ""` to Settlement.

- [x] **Greeting builds affinity** — In `AgentDecisionSystem`'s new greeting block, after logging
  the greeting, gain +0.01 mutual affinity between the two NPCs (same pattern as evening chat's
  `AFFINITY_GAIN`). Uses existing `Relations` component. Over many greetings, casual acquaintances
  become friends — bridging daytime greetings and evening chat into a unified social fabric.

- [x] **NPC complains about need in greeting** — Extend the greeting log with need context. When
  the greeting NPC has any need below 0.3, append " (complains about [need])" to the greeting
  message. In `AgentDecisionSystem`'s greeting block, after building the base msg string, check
  `needs.list[i].value < 0.3` and append the need name. Pure log flavour — no gameplay effect.

- [x] **Hauler preferred route bias** — Haulers with a `bestRoute` set prefer that route when
  evaluating trades. In `TransportSystem::FindBestRoute` (or equivalent), when scoring candidate
  routes, add a +10% score bonus if the source→dest matches `hauler.bestRoute`. This creates route
  specialization: experienced haulers stick to known profitable corridors. Needs to pass the
  hauler entity into the route evaluation function.

- [x] **NPC age affects move speed** — In `MovementSystem`, scale NPC speed by age bracket.
  Children (age < 15) move at 80% speed, elders (age > 55) at 70%, prime adults at 100%. Read
  `Age::days` from the entity, compute bracket, multiply `MoveSpeed::value` by the factor. Exclude
  haulers and player (already have separate speed logic). Adds visible age-based behaviour.

- [x] **Mood score shown in settlement tooltip** — In `HUD::DrawSettlementTooltip`, show
  "Mood: X%" from `SettlementEntry::moodScore`. Add the line after the morale line with matching
  colour coding (GREEN ≥0.7, YELLOW ≥0.4, RED below). Uses data already piped through
  `RenderSnapshot::SettlementEntry::moodScore` — only needs tooltip rendering.

- [x] **NPC contentment affects work output** — In `ProductionSystem`, when computing worker
  contribution, multiply by a contentment factor: contentment ≥ 0.7 gives 1.0×, 0.4–0.7 gives
  0.85×, < 0.4 gives 0.65×. Read contentment from `Needs` (weighted average). Unhappy NPCs
  produce less, creating pressure on settlements to maintain quality of life.

- [x] **Player reputation affects trade prices** — In `SimThread::ProcessInput`'s player trade
  blocks (T and Q keys), apply a discount/markup based on `m_playerReputation`. At 50+ rep, 5%
  discount on purchases; at 100+, 10%. At -20 rep, 10% markup. Multiply the purchase price by
  `(1.0 - repDiscount)`. Gold flow: discount reduces gold paid to settlement treasury. Log when
  discount is first applied: "Your reputation earns you a discount at [Settlement]."

- [x] **Hauler avoids recently unprofitable routes** — Add `std::string worstRoute = ""` and
  `float worstLoss = 0.f` to `Hauler`. In `TransportSystem` delivery completion, track the worst
  loss. In `FindBestRoute`, penalize routes matching `worstRoute` by -20% score for 24 game-hours
  (add `float worstRouteTimer = 0.f`). Haulers learn from mistakes and avoid known bad trades.

- [x] **NPC remembers who helped them** — Add `entt::entity lastHelper = entt::null` to
  `DeprivationTimer`. In `AgentDecisionSystem`'s charity block, when an NPC receives charity, set
  `lastHelper` to the donor's entity. In the greeting block, if the greeter's `lastHelper` matches
  the other NPC, log "[Name] thanks [Helper] for past kindness" instead of a plain greeting and
  add +0.05 mutual affinity. Creates gratitude memory that reinforces social bonds.

- [x] **Bandit intimidation aura** — In `AgentDecisionSystem`'s main NPC loop, idle NPCs within
  50 units of a bandit (check `registry.view<BanditTag, Position>`) get a -0.02 morale drain per
  game-hour applied to their home `Settlement::morale`. Log: "[NPC] feels uneasy near bandits."
  with a cooldown (add `float intimidationCooldown = 0.f` to `DeprivationTimer`, 60 game-sec).
  Bandits' mere presence erodes settlement morale — incentivizes player to confront them.

- [x] **Skill milestone shown in NPC tooltip** — In `HUD.cpp`'s agent tooltip section, when the
  NPC has any skill ≥ 0.5, show "Journeyman [Type]" in faint GOLD. At ≥ 0.9, show "Master [Type]".
  Read from existing `farmSkill`, `waterSkill`, `woodSkill` in `RenderSnapshot::AgentEntry`. Find
  the highest skill that exceeds 0.5 and display the appropriate title. No new data piping needed.

- [x] **NPC shares food with family first** — In `AgentDecisionSystem`'s charity block, before
  checking generic nearby NPCs, prioritize family members (check `FamilyTag::name` match). If a
  starving NPC shares a `FamilyTag::name` with the helper, skip the reputation check and always
  offer charity. Log "[Name] feeds family member [Other]." Family bonds override reputation.

- [x] **Hauler bankruptcy shown in settlement stockpile** — In `RenderSystem::DrawStockpilePanel`,
  below the resident list, show "Struggling haulers: N" in faint RED when any haulers homed at the
  settlement have `bankruptWarned == true`. Add `int strugglingHaulers = 0` to
  `RenderSnapshot::StockpilePanel`. Pipe from `SimThread::WriteSnapshot` by counting Haulers
  with `bankruptWarned && HomeSettlement == selectedSettlement`.

- [x] **NPC celebrates skill milestone** — In `ScheduleSystem`'s skill milestone check (the
  `checkMilestone` lambda), when an NPC reaches Journeyman or Master, set their `AgentState::behavior`
  to `AgentBehavior::Celebrating` for 30 game-seconds (add `float celebrateTimer = 0.f` to
  `DeprivationTimer`). The existing celebration glow in `GameState::Draw` will automatically show.
  Creates a visible moment when NPCs achieve something meaningful.

- [x] **Witness count shown in confrontation log** — In `SimThread::ProcessInput`'s confrontation
  block, after the witness loop, append the count to the main confrontation log: "Player confronted
  [bandit] (N witnesses)." Replace the existing log call or add a follow-up entry. Makes the social
  impact of confrontation visible in the event log without needing a separate line per witness.

- [x] **NPC thanks player after witnessing** — In `AgentDecisionSystem`, idle NPCs with
  `Reputation::score > 0.3` who are within 40 units of the player occasionally log "[NPC] nods
  respectfully at you." Add `float thankCooldown = 0.f` to `DeprivationTimer` (60 game-sec
  cooldown). Only fires if the NPC has the Reputation component. Creates ambient feedback for
  the player's positive actions without adding gameplay mechanics.

- [x] **Skill training between NPCs** — In `AgentDecisionSystem`'s idle block, when two NPCs are
  within 30u and one has a skill ≥ 0.6 while the other has that same skill < 0.3, the higher-skilled
  NPC teaches the other: +0.005 per game-hour to the learner's skill, +0.02 affinity for both. Add
  `float teachCooldown = 0.f` to `DeprivationTimer` (120 game-sec cooldown). Log "[Teacher] teaches
  [Learner] about farming/water/woodcutting." Skilled NPCs spread knowledge organically.

- [x] **Settlement skill summary in stockpile panel** — In `RenderSystem::DrawStockpilePanel`,
  below the existing resident list, show "Top skill: [Type] (N masters, M journeymen)" in faint
  GOLD. Add `int masterCount[3]` and `int journeymanCount[3]` to `RenderSnapshot::StockpilePanel`.
  Pipe from `SimThread::WriteSnapshot` by counting NPCs homed at the settlement with Skills ≥ 0.9
  or ≥ 0.5. Pick the type with the most combined masters+journeymen. Gives players visibility into
  settlement specialisation at a glance.

- [x] **Family grief on death** — In `DeathSystem`, when an NPC dies, scan for other NPCs with
  matching `FamilyTag::name`. Set a `float griefTimer = 4.f` (game-hours) on their `DeprivationTimer`
  (add the field). During grief, `AgentDecisionSystem` skips idle social actions (greeting, visiting)
  and drains morale by -0.05/game-hour on home settlement. Log "[Name] mourns the loss of [Dead]."

- [x] **Family reunion greeting** — In `AgentDecisionSystem`'s greeting block, when two NPCs share
  a `FamilyTag::name` and haven't greeted in the current `greetCooldown` window, use a special log:
  "[Name] embraces [Other] warmly." Grant +0.08 mutual affinity (8× normal) instead of the standard
  +0.01. Family reunions are emotionally significant encounters.

- [x] **Hauler route shown in stockpile panel** — In `RenderSystem::DrawStockpilePanel`, after the
  struggling haulers line, show up to 3 haulers homed at the settlement with their current route
  (e.g. "  Hauler Orin: Riverwatch→Oakvale"). Add `struct HaulerInfo { std::string name; std::string
  route; bool struggling; }` and `std::vector<HaulerInfo> haulerRoutes` to `StockpilePanel`. Pipe
  from `SimThread::WriteSnapshot` by iterating Hauler+HomeSettlement+Name at the selected settlement.

- [ ] **Idle NPCs discuss hauler routes** — In `AgentDecisionSystem`'s greeting block, when
  both NPCs have `HomeSettlement` at a settlement with active haulers (check
  `registry.view<Hauler, HomeSettlement>`), 20% chance to replace the greeting log with "[Name]
  and [Other] discuss trade routes." No gameplay effect — purely flavour text that makes
  settlements with haulers feel like trading hubs.

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

- [x] **Theft indicator in tooltip** — Surface recent theft in the NPC tooltip. Add `bool recentlyStole = false`
  to `AgentEntry` in `RenderSnapshot.h`; set it when `stealCooldown > 46.f` (within 2 game-hours of a theft).
  In `SimThread::WriteSnapshot`, populate from `DeprivationTimer::stealCooldown`. In `HUD::DrawHoverTooltip`,
  when `recentlyStole`, append a faint RED " (thief)" suffix to line1. Mirror the `familyName` pattern already there.
  NOTE: Already implemented — recentlyStole exists in AgentEntry, SimThread, and HUD tooltip.

- [x] **Skills penalty on theft** — When a theft fires in `ConsumptionSystem`, read the thief's `Skills`
  component via `registry.try_get<Skills>`, and reduce all three skill floats (farming, water_drawing,
  woodcutting) by 0.02 each, clamped at 0. This models social ostracism without new components.

- [x] **Thief flees home after stealing** — After a successful theft, set the NPC's velocity away from the
  settlement centre for 3–5 real seconds (use a new `fleeTimer` float in `DeprivationTimer`, or reuse
  `helpedTimer` as a flee flag). In `AgentDecisionSystem`, when `fleeTimer > 0`, move away from home pos
  at full speed. This makes theft visible: a dot sprinting away from the settlement dot.

- [x] **Thief dot colour on world map** — When `recentlyStole` is true, tint the NPC's world-map
  dot with a dark red: `Fade(MAROON, 0.9f)`. In `GameState.cpp`, the agent draw loop already
  applies `Fade(GOLD, 0.85f)` for Celebrating; add an `else if (a.recentlyStole)` branch before
  the default color assignment. Use `AgentEntry::recentlyStole` (already in the snapshot). Makes
  thieves visible on the map for ~2 game-hours after stealing.

- [x] **Relation score shown in road tooltip** — The road hover tooltip (`HUD::DrawRoadTooltip` in
  HUD.cpp) already shows connected settlement names. Extend it to also show the current relation
  score between those two settlements: `"Relations: -0.62 (Rivals)"` / `"Relations: +0.71 (Allies)"`.
  Read from `SettlementEntry::relations` (expose as `std::map<std::string,float>` keyed by
  settlement name) populated in `SimThread::WriteSnapshot`'s settlement loop from `s.relations`.
  NOTE: Already implemented — relAtoB/relBtoA displayed with Rivals/Allied labels.

- [x] **Alliance trade bonus log** — When a hauler completes a delivery between allied settlements
  (relation score > 0.5), log it to `EventLog` as `"Ally trade: Greenfield → Wellsworth (+5g bonus)"`.
  In `TransportSystem.cpp`, after the existing ally tax reduction block, check the relation score
  and push a log entry using the hauler's carry quantity and the bonus gold saved. This makes the
  alliance benefit tangible and visible.

- [x] **Rival settlement tax UI indicator** — In `HUD::DrawRoadTooltip` (HUD.cpp), when the road
  connects rival settlements (relation < -0.5), append a red `"(+30% rival surcharge)"` note to
  the road condition line. Read from the same `SettlementEntry` relation data. Helps player
  understand why haulers on that route are less profitable.
  NOTE: Already implemented — line 1679 shows "Relations: [who] (+10% tariff)" in RED for rivals.

- [x] **Settlement theft count in stockpile panel** — Track how many times NPCs have stolen from a
  settlement. Add `int theftCount = 0` to `Settlement` component in `Components.h`; increment it
  in `ConsumptionSystem`'s theft block each time a theft succeeds. In `SimThread::WriteSnapshot`,
  populate a new `int theftCount = 0` in `StockpilePanel`. In `RenderSystem::DrawStockpilePanel`,
  show "Thefts: N" below the treasury line in faint RED when `theftCount > 0`.

- [x] **NPC grudge after being stolen from** — When an NPC's charity gift or help is followed by a
  theft from the same entity (helper's `helpedTimer > 0` and the helped entity steals), log a
  social event: "Aldric saw through Mira's gratitude." Implemented in `ConsumptionSystem`'s theft
  block: after confirming a theft, check all nearby NPCs (within 80 units) who have
  `gratitudeTarget == thief entity`; clear their `gratitudeTimer` and log the message. Purely
  social flavour — no new components.

- [x] **Theft log includes skill level** — Extend the theft log message in `ConsumptionSystem`'s
  theft block to include the thief's relevant skill after the penalty: "Mira stole food at
  Ashford (farming 23%)." Skills penalty applied before log; relevant skill (farming for food,
  water_drawing for water) shown as integer percent.

- [x] **Skill degradation with age** — In `NeedDrainSystem.cpp`, after the need-drain loop,
  second loop over `registry.view<Skills, Age>()`. When `age.days > 65.f`, reduce all three
  skills by `0.0002f * gameDt` per tick, clamped at `0.1f` minimum. Economic lifecycle.

- [x] **NPC idle chat radius** — When two NPCs of the same `HomeSettlement` are both `Idle` and
  within 25 units during hours 18–21, briefly stop both (`vel = 0`) for 30–60 game-seconds.
  NOTE: Already implemented — chatTimer, CHAT_RADIUS=25, plus affinity building via Relations.

- [x] **Event colour in minimap ring** — Minimap ring now uses `ModifierColour(s.modifierName)`
  instead of `Fade(YELLOW, 0.8f)`. One-line change in `HUD::DrawMinimap`.

- [x] **Event log entry colour coding** — Extended existing keyword colour system with "stole"→RED,
  "saw through"→ORANGE, "Ally trade"→GREEN. Core system already had plague/festival/drought/died.

- [x] **Gratitude approach stops at polite distance** — Already implemented: POLITE_DIST = 25.f
  in AgentDecisionSystem gratitude block; NPC stops when within range.

- [x] **Gratitude shown in tooltip** — NOTE: Already implemented — `isGrateful` in AgentEntry,
  "Grateful to neighbour" in LIME in tooltip, full pattern with lineCount/pw.

- [x] **Charity cooldown shown in tooltip** — `charityTimerLeft` in `AgentEntry`, populated from
  `dt->charityTimer`. Tooltip shows "Gave charity (X.Xh ago)" in faint LIME when > 0.

- [x] **Gratitude shown in world dot** — Faint LIME ring at `a.size + 2` when `a.isGrateful`
  and NPC role. Implemented in `GameState.cpp` agent draw loop.

- [x] **Warmth glow shown in tooltip** — NOTE: Already implemented — `recentWarmthGlow` in
  AgentEntry, "Warm from giving" in ORANGE in tooltip, full lineCount/pw pattern.

- [x] **Charity recipient log detail** — NOTE: Already implemented — log reads "X helped Y at Z."
  with recipient name, settlement, and frequency count (xN).

- [x] **Charity radius shown on hover** — Faint 80u LIME circle around hovered charity-ready NPC.
  Uses `charityReady` flag and world-mouse hover check in `GameState.cpp` agent draw loop.

- [x] **Charity recipient log detail** — (duplicate) Already implemented.

- [x] **NPC mood colour on world dot** — NOTE: Already implemented — GameState.cpp lines 344-346
  use GREEN/YELLOW/RED based on contentment thresholds 0.7/0.4.

- [x] **Charity recipient log detail** — (duplicate) Already implemented.

- [x] **Gratitude shown in world dot** — (duplicate) Already implemented in GameState.cpp.
  `AgentEntry`. Keeps the visual footprint small (just one extra ring draw per grateful NPC).

- [x] **Wanderer re-settlement** — Exile with 30g+ finds nearest non-ruin settlement with capacity,
  pays 30g (to treasury), resets theftCount, logs "X settled at Y (fresh start)."

- [x] **Exile indicator in tooltip** — Surface exile state in the NPC hover tooltip. Add
  `bool isExiled = false` to `AgentEntry` in `RenderSnapshot.h`; set when
  `home.settlement == entt::null && dt->theftCount >= 3` in `SimThread::WriteSnapshot`. In
  `HUD::DrawHoverTooltip`, when `isExiled`, append " [Exiled]" in `Fade(RED, 0.8f)` to line2
  (the behavior line), or show it as a separate faint red line below the role line.

- [x] **Wanderer re-settlement** — (duplicate) Already implemented.

- [x] **Exile indicator in tooltip** — Add `bool isExiled = false` to `AgentEntry` in
  `RenderSnapshot.h`; set in `SimThread::WriteSnapshot` when home entity is `entt::null` AND
  `dt->theftCount >= 3`. In `HUD::DrawHoverTooltip`, when `isExiled`, show a faint red
  "[Exiled]" line below the role/behavior lines — same `lineCount`/`pw` pattern as `showHelped`.

- [x] **Gratitude world dot ring** — (duplicate) Already implemented. While `isGrateful` is true, draw a faint LIME ring
  around the NPC's world dot in `GameState::Draw`. After `DrawCircleV` for the agent dot,
  add: if `a.isGrateful && a.role == RenderSnapshot::AgentRole::NPC`, call
  `DrawCircleLinesV({a.x, a.y}, a.size + 3.f, Fade(LIME, 0.45f))`. `isGrateful` is already
  in `AgentEntry`.

- [x] **Charity giver count in settlement tooltip** — In `SimThread::WriteSnapshot`, when
  building `worldStatus`, count how many NPCs at this settlement have `charityTimer > 0`
  (i.e. gave charity recently). Add `int recentGivers = 0` to `SettlementStatus` in
  `RenderSnapshot.h`. In `HUD`'s settlement list panel (`DrawWorldStatus`), append
  a faint `(Ng)` suffix (in LIME) when `recentGivers > 0` — like the existing `(Nc)` child suffix.

- [x] **Theft frequency shown in stockpile panel** — (duplicate) Already implemented —
  Settlement::theftCount incremented in ConsumptionSystem, shown in stockpile panel.

- [x] **Exile indicator in tooltip** — Add `bool isExiled = false` to `AgentEntry` in
  `RenderSnapshot.h`. Set it in `SimThread::WriteSnapshot` when `hs.settlement == entt::null` and
  the entity is not a bandit (bandit state supersedes exile). In `HUD::DrawHoverTooltip`, show
  "(exile)" in faded ORANGE below the profession line when `isExiled`. Lets the player identify
  wandering exiles before they turn bandit.

- [x] **Bandit density cap per road** — Prevent more than 3 bandits from lurking at the same
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

- [ ] **NPC friendship formation** — NPCs working at the same facility develop friendships over
  time. Add a `FriendTag` component (`Components.h`) with `entt::entity friend = entt::null`
  and `float bond = 0.f` (0–1). In `ProductionSystem.cpp`, when two NPCs share a facility
  for multiple consecutive work shifts, increment bond. Friends who lose their friend to death
  get a morale penalty (in `DeathSystem.cpp`). Show "Friend: Name" in the NPC hover tooltip
  (`HUD.cpp`). Populate `friendName` in `AgentEntry` of `RenderSnapshot.h`.

- [ ] **NPC mood contagion** — Happy NPCs spread contentment to nearby NPCs; miserable NPCs
  drag neighbours down. In `AgentDecisionSystem.cpp`, once per game-hour, each NPC checks
  nearby NPCs (within 40px). If average neighbour contentment differs by >0.1 from own, nudge
  own contentment 5% toward the average. This creates emergent mood clusters — prosperous
  settlements feel upbeat, struggling ones feel grim. Add a `moodContagionTimer` to
  `DeprivationTimer` in `Components.h` to rate-limit the check.

- [ ] **Hauler wait timer shown in tooltip** — When a hauler is in `Idle` state (haulerState==0),
  show "Wait: X.Xh" in the tooltip indicating how long they've been waiting for a profitable route.
  Add `float haulerWaitTimer = 0.f` to `AgentEntry` in `RenderSnapshot.h`, populated from
  `Hauler::waitTimer` in SimThread. In `HUD::DrawHoverTooltip`, append below the state line when
  haulerState==0 and waitTimer > 0.5. Helps the player spot haulers stuck without good routes.

- [ ] **NPC work shift fatigue** — NPCs who work consecutive shifts without rest gradually
  lose productivity. Add `float shiftFatigue = 0.f` to `DeprivationTimer` in `Components.h`.
  In `ProductionSystem.cpp`, increment fatigue by 0.02 per game-hour while Working; decay by
  0.05 per game-hour while not Working. Apply a `(1.0 - shiftFatigue * 0.3)` multiplier to
  production output. Cap fatigue at 1.0. Log "X is exhausted at Y" when fatigue exceeds 0.8
  (rate-limited once per NPC). Creates a natural reason to rest and rotate workers.

- [ ] **Morale recovery celebration** — When a settlement's morale crosses above 0.6 after being
  below 0.35, log "Spirits lift at Settlement — morale restored" and set all homed NPCs to
  `AgentBehavior::Celebrating` for 1 game-hour. Add `bool moraleCrisisLogged = false` to
  `Settlement` in `Components.h`, set true when morale drops below 0.35, cleared on recovery.
  Check in `RandomEventSystem.cpp` alongside existing morale checks. Creates a visible community
  response to recovering from hardship.

- [ ] **NPC mentorship** — Elder NPCs (age > 60) with high skill passively boost skill growth
  of nearby younger NPCs. In `ProductionSystem.cpp`, when an elder is present at the same
  facility as a younger worker, the younger NPC gains +50% skill XP. Add a tooltip line
  "Mentored by: Elder Name" when this is active. Add `std::string mentorName` to `AgentEntry`
  in `RenderSnapshot.h`, populated in SimThread when an elder shares the worker's facility.

- [ ] **Settlement rivalry log events** — When two connected settlements have `relAtoB < -0.3`
  (from `RoadEntry`), periodically log "Tensions rise between X and Y" once per 24 game-hours.
  Check in `RandomEventSystem.cpp` alongside other periodic checks. Add `float rivalryLogTimer`
  to `Road` component in `Components.h`. Drains by gameHoursDt; logs when timer <= 0 and
  relation is hostile, then resets to 24. Adds narrative texture to inter-settlement conflict.

- [ ] **Contentment sparkle on thriving NPCs** — In `GameState.cpp`'s agent render loop, when
  an NPC's `contentment >= 0.9`, draw a small yellow dot that pulses using `sinf(GetTime()*3)`
  offset by the agent's x position (so they don't all pulse in sync). Add `float contentment`
  to `AgentEntry` in `RenderSnapshot.h` (already exists). Purely visual — makes prosperous
  settlements visibly sparkle compared to struggling ones.

- [ ] **NPC generosity reputation** — NPCs who give charity frequently gain a "Generous"
  tag visible in their tooltip. Add `int charityGiven = 0` to `DeprivationTimer` in
  `Components.h`. Increment in the charity-giving block of `AgentDecisionSystem.cpp`.
  When `charityGiven >= 5`, show "Generous" badge in `HUD::DrawHoverTooltip`. Add
  `bool isGenerous = false` to `AgentEntry` in `RenderSnapshot.h`, set in SimThread
  when `charityGiven >= 5`. Generous NPCs get +0.01 morale per game-hour (self-reward).

- [ ] **Settlement population milestone log** — Log events when a settlement reaches
  population milestones (10, 20, 30). In `BirthSystem.cpp`, after a successful birth,
  check if pop just crossed a milestone. Use `static std::map<entt::entity, int>
  s_lastMilestone` to track. Log "Population at X reached Y!" Adds celebratory
  narrative markers to growing settlements.

- [ ] **NPC night-time rest glow** — In `GameState.cpp`'s agent render loop, when an NPC's
  behavior is `AgentBehavior::Sleeping`, draw a tiny dim yellow dot (radius 2, alpha 0.2)
  behind them to suggest lantern/campfire light. Uses existing `behavior` field in `AgentEntry`.
  Purely visual — makes settlements look alive at night with scattered warm glows.

- [ ] **Hauler convoy detection log** — In `TransportSystem.cpp`, when two haulers are within
  30px of each other and both in `GoingToDeposit` state heading to the same destination,
  log "X and Y travel together toward Z" once per pair per trip. Use `static std::set<
  std::pair<entt::entity, entt::entity>> s_loggedConvoy` cleared when either hauler changes
  state. Adds emergent social narrative to hauler behaviour without gameplay changes.

- [ ] **Seasonal NPC clothing colour shift** — In `SimThread::WriteSnapshot`, when computing
  `drawColor` for NPCs, tint the NPC body color slightly blue in Winter and slightly warm in
  Summer by blending with `ColorTint`. Uses existing `Season` from `TimeManager`. Purely visual
  — NPCs subtly reflect the current season through their appearance.

- [ ] **NPC tavern gathering at night** — In `AgentDecisionSystem.cpp`, idle NPCs between
  hours 20–23 with contentment > 0.5 walk toward the centre of their home settlement (within
  20px). When 3+ NPCs cluster within 25px at night, log "Tavern gathering at X" once per
  night per settlement. Use `static std::map<entt::entity, int> s_tavernDay` to track last
  logged day. Adds emergent social atmosphere without new components.

- [ ] **NPC grudge after theft** — When an NPC steals food/water (in `ConsumptionSystem.cpp`),
  nearby NPCs within 30px who witness the theft develop a grudge. Add `entt::entity grudgeTarget
  = entt::null` and `float grudgeTimer = 0.f` to `DeprivationTimer` in `Components.h`. Witness
  NPCs set grudgeTarget to the thief; grudgeTimer decays at 0.1/game-hour. While grudgeTimer > 0,
  the witness refuses to share charity with the thief (checked in the charity block of
  `AgentDecisionSystem.cpp`). Log "X witnessed Y stealing at Z" once per theft event.

- [ ] **Elder storytelling event** — In `AgentDecisionSystem.cpp`, when an elder (age > 60) is
  idle at night (hours 20–22) and 2+ younger NPCs are within 30px, 10% chance per game-hour
  to log "Elder X tells stories to youngsters at Y." Listening NPCs get +0.02 contentment.
  Rate-limit per elder to once per 24h via `static std::map<entt::entity, int> s_storyDay`.
  Creates intergenerational social texture.

- [ ] **Sick NPC visual tint** — In `GameState.cpp`'s agent render loop, when `a.ill` is true,
  apply a green-ish tint to the NPC's draw color by blending with `Color{100, 180, 100, 255}`
  at 30% weight. Makes illness visually noticeable without tooltips. Uses existing `ill` field
  in `AgentEntry`. No new snapshot fields needed.

- [ ] **NPC work pride log** — In `ProductionSystem.cpp`, when an NPC's relevant skill reaches
  0.85 (Master rank), log "X has mastered farming/water/woodcutting at Y" once. Use `static
  std::set<entt::entity> s_loggedMastery` to ensure one-time logging. Add skill check after
  the existing skill increment block. Gives a narrative moment to skill progression.

- [ ] **NPC jealousy of wealthy neighbours** — In `AgentDecisionSystem.cpp`'s gossip block,
  when NPC A has balance < 10 and NPC B has balance > 60, 15% chance to log "X envies Y's
  wealth." NPC A gets -0.01 contentment. Rate-limit per NPC pair to once per 24 game-hours
  via `static std::map<std::pair<entt::entity,entt::entity>, float> s_envyCooldown`. Creates
  class tension narrative in economically stratified settlements.

- [ ] **Stockpile trend arrows in panel** — In `RenderSystem::DrawStockpilePanel`, after each
  resource's bar chart bar, draw a tiny arrow (▲/▼/—) based on `netRatePerHour`: green ▲ if
  net > 0.5, red ▼ if net < -0.5, gray — otherwise. Position at `barX + BAR_MAX_W + 4`.
  Uses existing `netRatePerHour` from `StockpilePanel`. No new snapshot fields.

- [ ] **NPC apprentice skill gain** — In `ProductionSystem.cpp`, children aged 12–15 working
  at a facility gain skill XP at 50% of the adult rate. Currently children are excluded from
  production bonuses. Find the skill increment block and add a branch for child workers using
  `Age` component check (`age->days >= 12 && age->days < 15`). The skill gained should match
  the facility's output type. Log "X begins apprenticeship at Y" once per child via `static
  std::set<entt::entity> s_loggedApprentice`.

- [ ] **Settlement founding celebration** — In `ConstructionSystem.cpp`'s settlement founding
  block (triggered by player pressing P), after the new settlement is created, set all NPCs
  within 100px to `AgentBehavior::Celebrating` for 2 game-hours and give the settlement +0.1
  morale. Log "Settlement X founded — locals celebrate!" Uses existing `AgentState` and
  `Settlement::morale`. No new components needed.

- [ ] **NPC homesickness during migration** — In `AgentDecisionSystem.cpp`'s migration block,
  when an NPC begins migrating away from their home settlement, 30% chance to log "X leaves
  Y with a heavy heart." The migrating NPC gets -0.05 contentment. If the NPC's bond with
  home is strong (lived there > 20 game-days, checked via `Age::days` minus an estimated
  arrival time), add `float residencyDays = 0.f` to `DeprivationTimer` in `Components.h`,
  incremented alongside existing timers in `ConsumptionSystem.cpp`.

- [ ] **Charity recipient thank-you log** — In `AgentDecisionSystem.cpp`'s charity block,
  when charity is successfully given, 40% chance to log "X thanks Y for food/water at Z."
  Uses existing Name components and settlement lookup. Rate-limit per recipient to once per
  6 game-hours via `static std::map<entt::entity, float> s_thankCooldown`. Adds warm social
  texture to the charity mechanic.

- [ ] **Road condition colour gradient** — In `GameState.cpp`'s road render loop, colour
  road lines by condition: `Fade(GREEN, 0.3f)` at condition 1.0, `Fade(YELLOW, 0.3f)` at
  0.5, `Fade(RED, 0.3f)` at 0.0. Lerp between colors based on `r.condition`. Uses existing
  `condition` field in `RoadEntry`. Currently all roads are drawn the same color regardless
  of quality. No new snapshot fields needed.

- [ ] **NPC first-day-at-work log** — In `ScheduleSystem.cpp`, when an NPC transitions from
  `Idle` to `Working` for the first time (tracked via `static std::set<entt::entity>
  s_loggedFirstWork`), log "X begins working at Y." Uses existing `AgentState`, `Name`, and
  `HomeSettlement` components. Gives a narrative moment when new NPCs or immigrants start
  contributing to the economy.

- [ ] **Settlement night-time lantern glow** — In `GameState.cpp`'s settlement render loop,
  between hours 20–5 (nighttime, check `snap.hourOfDay`), draw a faint warm circle
  `DrawCircleV(center, radius*0.6, Fade(ORANGE, 0.08f))` behind each settlement with pop > 0.
  Simulates lantern light from inhabited settlements at night. Uses existing `hourOfDay` and
  settlement position. No new snapshot fields.

- [ ] **NPC wealth class label in tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), after the
  gold line, show a wealth class label: "Destitute" (< 5g), "Poor" (< 20g), "Comfortable"
  (< 60g), "Wealthy" (< 120g), "Rich" (>= 120g). Color-coded from red to gold. Uses existing
  `balance` in `AgentEntry`. No new snapshot fields.

- [ ] **Trade deficit warning log** — In `RandomEventSystem.cpp`, when resetting tradeVolume
  every 24h, if a settlement's `importCount > exportCount * 3` (importing far more than
  exporting), log "Trade deficit at X — heavily dependent on imports." Rate-limit to once per
  3 resets via a `int deficitLogSkip = 0` field on `Settlement` in `Components.h`. Helps the
  player identify economically vulnerable settlements.

- [ ] **NPC satisfaction survey in stockpile panel** — In `RenderSystem::DrawStockpilePanel`,
  below the residents list, show a one-line summary: "Avg mood: X%" where X is the mean
  contentment of all residents. Color green (> 70%), yellow (> 40%), red (< 40%). Uses
  existing `AgentInfo::contentment` already populated. No new snapshot fields needed.

- [ ] **Morale-driven work slowdown** — In `ProductionSystem.cpp`, when producing resources,
  scale output by a morale factor: `moraleFactor = 0.5f + 0.5f * settlement.morale` (so morale
  0 = 50% output, morale 1 = 100%). Apply to the `produced` amount after seasonal modifiers.
  Settlement morale is already accessible via `registry.try_get<Settlement>`. Visible effect:
  low-morale settlements produce less, creating economic feedback loops.

- [ ] **Festival morale bonus** — In `RandomEventSystem.cpp`'s festival event block, when a
  festival triggers, bump settlement morale by +0.05 in addition to the existing production
  bonus. Log "Festival lifts spirits in X — morale rising." This creates a positive feedback
  loop where prosperous settlements get occasional morale boosts that further improve output.

- [ ] **Desperation triggers morale drain** — In `ConsumptionSystem.cpp`, when the settlement's
  `desperatePurchases` counter crosses 5 in a single 24h cycle, apply a one-shot morale penalty
  of -0.02 to the settlement. Track with a `static std::set<entt::entity> s_despMoralePenalized`
  that resets alongside `desperatePurchases` in `RandomEventSystem.cpp`. Models the social cost
  of widespread emergency buying — settlements under persistent need pressure lose cohesion.

- [ ] **NPC gratitude for low-price settlement** — In `ConsumptionSystem.cpp`, when an NPC
  makes an emergency purchase and the market price is below 2.0g, bump `Settlement::morale`
  by +0.001 (tiny but cumulative). Models NPCs appreciating affordable markets. Conversely,
  when price exceeds 5.0g, apply -0.001 morale. Creates subtle feedback: fair pricing improves
  community mood while gouging corrodes it.

- [ ] **Price crash log** — Mirror of price spike: in `PriceSystem.cpp`, when a resource price
  drops more than 20% in one update cycle, log "Price crash: food at X now Yg (-Z%)." once per
  resource per settlement per 12 game-hours. Reuse the same `s_priceSpikeCooldown` map (key
  already includes entity+resource). Surfaces deflation events e.g. after a hauler flood.

- [ ] **Price trend arrows in settlement tooltip** — In `HUD::DrawSettlementTooltip`, after the
  resource stocks line, show tiny trend indicators using the existing `foodPriceTrend`,
  `waterPriceTrend`, `woodPriceTrend` chars from `SettlementStatus`. Display as colored arrows:
  "+" → green up-arrow text, "-" → red down-arrow, "=" → nothing. Append to the stocks line
  like "Food: 50 ^  Water: 30  Wood: 10 v". No new snapshot fields needed.

- [ ] **Facility health decay warning** — In `HUD::DrawFacilityTooltip` (HUD.cpp), when
  facility health drops below 50%, change the health line color from WHITE to RED and append
  " — needs repair" to the text. Visual cue that a facility is degraded. No new snapshot fields
  — `best->baseRate / 4.0f` already computed as `healthPct`.

- [ ] **Seasonal output forecast in facility tooltip** — In `HUD::DrawFacilityTooltip`, add a
  line showing next season's expected output modifier. Use `SeasonProductionModifier(nextSeason)`
  where `nextSeason` is `(curSeason + 1) % 4`. Display as "Next season: X% output". Helps
  players plan ahead for seasonal production shifts. No new snapshot fields needed.

- [ ] **Wage comparison in stockpile panel** — In `RenderSystem::DrawStockpilePanel`, after
  the residents list, show "Avg wage: X.Xg/hr" computed from the mean of `AgentInfo::wage` for
  working residents (where wage > 0). Add `float wage = 0.f` to `StockpilePanel::AgentInfo`
  in `RenderSnapshot.h`, populated from the same `wagePerHour` value already on `AgentEntry`.
  Copy into `AgentInfo` during the residents loop in `SimThread::WriteSnapshot`. Color: gold.

- [ ] **Wealth inequality indicator** — In `HUD::DrawSettlementTooltip`, compute a simple Gini
  coefficient from the agent balances of NPCs homed at that settlement. Add `float gini = 0.f`
  to `SettlementStatus` in `RenderSnapshot.h`, computed in `SimThread::WriteSnapshot` during
  the settlement status loop. Display as "Inequality: X%" (gini * 100). Color green (< 30%),
  yellow (< 60%), red (>= 60%). Surfaces economic stratification within settlements.

- [ ] **Strike contagion between NPCs** — In `RandomEventSystem.cpp`'s unrest block, when a
  settlement has morale < 0.3 and a strike fires, also give a 30% chance per non-striking
  working NPC at the same settlement to join the strike (set their `strikeDuration` to half the
  normal `STRIKE_DURATION_HOURS`). Log "Strike spreads at X — N workers join." Uses existing
  `DeprivationTimer::strikeDuration` and `HomeSettlement` to find co-located NPCs.

- [ ] **Post-strike morale recovery** — In `ScheduleSystem.cpp`, when an NPC's `strikeDuration`
  expires (transitions from > 0 to 0), bump their home settlement's morale by +0.005 per NPC.
  Models the resolution of grievances. Use `registry.try_get<HomeSettlement>` to find the
  settlement, then `registry.try_get<Settlement>` to access `morale`. Rate-limit: only apply
  if settlement morale < 0.6 (prevents overshoot from mass strike endings).

- [ ] **NPC morale-influenced migration** — In `AgentDecisionSystem.cpp`'s `FindMigrationTarget`,
  add `homeMorale` as a push factor: when home settlement morale < 0.3, add +0.5 to migration
  score for all other settlements. Use `registry.try_get<Settlement>(homeSettlement)->morale`.
  Makes NPCs flee unrest, creating a population drain that further depresses morale.

- [ ] **Morale tooltip color ring on NPCs** — In `GameState.cpp`'s agent render loop, when an
  NPC has `homeMorale >= 0` and `homeMorale < 0.3`, draw a faint red outer ring (radius +3,
  `Fade(RED, 0.15f)`) around the NPC dot. Visual cue that the NPC lives in a low-morale
  settlement. Skip for haulers and player. No new snapshot fields — `homeMorale` already in
  `AgentEntry`.

- [ ] **Reputation influences charity priority** — In `AgentDecisionSystem.cpp`'s charity
  matching block, when selecting which starving NPC to help, prefer NPCs with higher
  `Reputation::score`. Sort the candidate list by reputation descending before picking the
  first match. Models community trust: well-regarded NPCs receive help first. Uses existing
  `registry.try_get<Reputation>` — no new components needed.

- [ ] **Reputation shown in stockpile panel residents** — Add `float reputation = 0.f` to
  `StockpilePanel::AgentInfo` in `RenderSnapshot.h`. Populate from `Reputation::score` in the
  residents loop of `SimThread::WriteSnapshot`. In `RenderSystem::DrawStockpilePanel`, append
  a small "(+X.X)" or "(-X.X)" after each resident's name, colored green/red. No new ECS
  components — reuses existing `Reputation`.

- [x] **Fatigue indicator in NPC tooltip** — Behavior line shows "(fatigued)" in ORANGE when
  `Schedule::fatigued` is true. `bool fatigued` in AgentEntry, populated from Schedule.

- [x] **Fatigue count in settlement tooltip** — `fatiguedWorkers` in SettlementStatus, counted
  from Schedule::fatigued + AgentBehavior::Working. Shown in ORANGE in settlement tooltip.

- [ ] **Elder mentorship bonus** — In `ProductionSystem.cpp`, when a facility has at least one
  elder worker (Age::days > 65) and at least one non-elder, give the non-elder a +5% production
  bonus. Check `Age` component on each worker entity in the production loop. Log "Elder X
  mentoring at Y" on first activation (rate-limited per facility, e.g. once per 24h). This
  counterbalances skill degradation by giving elders a social role even with reduced personal
  output.

- [ ] **NPC remembers last theft victim** — Add `entt::entity lastTheftVictimSettlement = entt::null`
  to `DeprivationTimer`. Set it in `ConsumptionSystem` when a theft occurs (to `home.settlement`).
  In `AgentDecisionSystem`'s migration scoring, if `lastTheftVictimSettlement == home.settlement`,
  add +0.5 to migration score (shame-driven emigration). Clear the field on successful migration.
  Adds social consequence: thieves are more likely to leave town.

- [ ] **Seasonal production tooltip** — In `RenderSystem::DrawStockpilePanel`, below the modifier
  line, show the current season's production effect: "Spring: +10% farming" / "Winter: -20% all"
  etc. Read from `TimeManager::CurrentSeason()` via the snapshot (add `Season season` to
  `StockpilePanel` if not already present). Helps the player understand seasonal output swings.

- [ ] **Settlement trade volume trend arrow** — Add `char tradeVolumeTrend = '='` to
  `RenderSnapshot::SettlementStatus`. In `SimThread::WriteSnapshot`, compare current `tradeVolume`
  with a 24h-ago snapshot stored on the `Settlement` component (add `int prevTradeVolume = 0`).
  Set trend to '+' if current > prev * 1.2, '-' if current < prev * 0.8, '=' otherwise. Show the
  trend arrow after "T:X" in the scrollable panel, colored green/red/gray like `popTrend`.

- [ ] **NPC gossip about trade volume** — In `AgentDecisionSystem`'s idle gossip block, if the
  NPC's home settlement has `tradeVolume >= 5`, add a chance (10%) to log: "[NPC] remarks on the
  busy trade at [settlement]." If `tradeVolume == 0`, log: "[NPC] worries about the lack of trade
  at [settlement]." Requires reading `Settlement::tradeVolume` via `HomeSettlement`. Adds social
  commentary that connects NPC chatter to actual economic activity.

- [ ] **Trade hub morale bonus** — In `ScheduleSystem` or a new small system, once per 24h check
  each settlement's `tradeVolume`. If `tradeVolume >= 8`, grant +0.01 morale ("prosperous trade").
  If `tradeVolume == 0` for 48h (add `int noTradeHours = 0` to `Settlement`), apply -0.02 morale
  ("economic isolation"). Log at threshold transitions. Creates feedback loop: trade success boosts
  morale → better NPC contentment → less migration.

- [ ] **NPC water gratitude** — In `ConsumptionSystem`, mirror the food gratitude pattern for water.
  Add `std::string lastDrinkSource` to `DeprivationTimer`. When `hadWater` is true (line ~100),
  record `settl->name` in `timer.lastDrinkSource`. When `needs.list[1].value < 0.2` and
  `lastDrinkSource` is non-empty, log "[NPC] is grateful to [Settlement] for water." and clear.
  Completes the meal-memory system for both primary survival needs.

- [ ] **NPC favourite settlement** — Add `std::string favouriteSettlement; int mealCount = 0` to
  `DeprivationTimer`. In `ConsumptionSystem`, when `lastMealSource` is set and matches current
  settlement name, increment `mealCount`. When `mealCount >= 20`, set `favouriteSettlement` and
  log "[NPC] considers [Settlement] home in their heart." In `AgentDecisionSystem`'s migration
  scoring, add -0.3 penalty if destination != `favouriteSettlement` (reluctance to leave). Creates
  emotional ties that slow migration churn.

- [ ] **Gratitude reputation boost** — In `ConsumptionSystem`, when the gratitude log fires (hunger
  < 0.2 with `lastMealSource` set), also check if the NPC has a `Reputation` component and add
  +0.05 to `rep.score`. Rationale: gratitude is a social positive — NPCs who acknowledge their
  settlement's support are better community members. Uses existing `Reputation` component via
  `registry.try_get<Reputation>(entity)`.

- [ ] **Convoy tooltip badge** — In `HUD::DrawHoverTooltip` (`src/UI/HUD.cpp`), when rendering a
  hauler tooltip and `a.inConvoy == true`, append " [Convoy]" in `Fade(GREEN, 0.6f)` after the
  hauler state line. Also show the destination settlement name: "Convoy → [destSettlName]". Uses
  existing `AgentEntry::inConvoy` and `destSettlName` fields already piped through the snapshot.

- [ ] **Convoy speed bonus** — In `MovementSystem` (`src/ECS/Systems/MovementSystem.cpp`), when a
  hauler has `Hauler::inConvoy == true`, multiply their movement speed by 1.1f (10% bonus from
  coordinated travel). Requires checking `registry.all_of<Hauler>(e)` and reading `h.inConvoy`.
  Log once per convoy formation: "[Hauler] is travelling faster in convoy." Rate-limit log to one
  per hauler per trip (add `bool convoySpeedLogged = false` to `Hauler`, reset on state change).

- [ ] **NPC comments on passing convoy** — In `AgentDecisionSystem`'s idle block, if an idle NPC
  is within 80u of a hauler with `inConvoy == true`, log: "[NPC] watches a convoy pass through
  [settlement]." Rate-limited by `greetCooldown` on `DeprivationTimer` (reuse existing cooldown).
  Uses `registry.view<Hauler, Position>` to find nearby convoy haulers. Adds social flavour
  connecting bystanders to economic activity.

- [ ] **Convoy dissolution log** — In `TransportSystem`'s GoingToDeposit convoy check, when
  `inConvoy` transitions from true to false (wasInConvoy && !hauler.inConvoy), log: "[Hauler]
  parted ways with their convoy near [settlement]." Uses the same `wasInConvoy` pattern already
  in place. Destination settlement name from `registry.try_get<Settlement>(hauler.targetSettlement)`.

- [ ] **Hauler remembers convoy partners** — Add `std::string lastConvoyPartner` to `Hauler`
  in `Components.h`. Set it in `TransportSystem` when convoy forms (the convoyPartner's Name).
  In the GoingToDeposit arrival block (where cargo is sold), if `lastConvoyPartner` is non-empty,
  log: "[Hauler] and [Partner] completed a successful trade run to [settlement]." Clear after
  logging. Pipe `lastConvoyPartner` through `RenderSnapshot::AgentEntry` for tooltip display.

- [ ] **Bandit frustrated by convoy log** — In `AgentDecisionSystem`'s bandit intercept lambda,
  when `h.inConvoy` causes the bandit to skip a hauler, log: "[Bandit] eyes the convoy but
  doesn't dare attack." Rate-limit with a `float convoyFrustrationCd = 0.f` on `DeprivationTimer`
  (decrement by `gameHoursDt`, reset to 4.f on log). Uses existing `Name` component for bandit
  name. Adds narrative tension — players see bandits being deterred.

- [ ] **Bandit targets solo haulers preferentially** — In `AgentDecisionSystem`'s bandit intercept
  lambda, after the `inConvoy` skip, add a secondary preference: sort candidate haulers by cargo
  value (sum of `qty * 3.f` for each resource). Instead of taking the first in range, track the
  best target and intercept only the richest solo hauler. Replace the `intercepted` early-return
  with a `bestTarget` entity + `bestValue` float. Makes bandits smarter predators.

- [ ] **Hauler retirement at old age** — In `EconomicMobilitySystem`'s hauler bankruptcy loop,
  also check `Age::days > 65` for each hauler. If over 65, convert back to regular NPC (same
  as bankruptcy demotion logic: return cargo, remove Hauler, restore energy drain, add Schedule).
  Log: "[Hauler] retired from trading after N trips (total +Xg)." using `lifetimeTrips` and
  `lifetimeProfit`. Gives haulers a natural lifecycle endpoint with a satisfying summary.

- [ ] **Hauler efficiency rating in tooltip** — In `HUD::DrawHoverTooltip`, below the trip
  history line, compute and show "Avg profit: +X.Xg/trip" when `lifetimeTrips >= 3` (avoid noisy
  early data). Use `best->lifetimeProfit / best->lifetimeTrips`. Color: GREEN if > 5g, YELLOW
  if > 0g, RED if negative. No new snapshot fields needed — computed from existing data.

- [ ] **Hauler route abandonment** — In `TransportSystem`'s GoingToDeposit section, after a
  loss-making trip (tripProfit < 0), add the route to a `std::set<std::string> avoidedRoutes`
  on the `Hauler` component. In the Idle state's route selection, skip routes matching any entry
  in `avoidedRoutes`. Clear `avoidedRoutes` after 48 game-hours (add `float avoidTimer = 0.f`).
  Log: "[Hauler] avoids the [A]→[B] route after losing money." Creates learning behaviour.

- [ ] **NPC mourns loss-making hauler** — In `EconomicMobilitySystem`'s bankruptcy demotion
  block, find homed NPCs at the same settlement within 100u. For each, log: "[NPC] is saddened
  by [Hauler]'s bankruptcy." and apply -0.01 morale to the settlement. Uses existing
  `registry.view<HomeSettlement, Position, Name>` pattern. Rate-limit: only log for up to 3
  witnesses per bankruptcy event. Adds social consequence to economic failure.

- [ ] **Reputation tooltip color** — In `HUD::DrawHoverTooltip` (`src/UI/HUD.cpp`), where the
  reputation line is drawn, color it based on value: GREEN if `reputation >= 1.0`, YELLOW if
  `>= 0`, RED if negative. Currently shows in a single color. Use the existing `best->reputation`
  field. Simple visual improvement that makes reputation legible at a glance.

- [ ] **High-reputation NPC attracts migrants** — In `AgentDecisionSystem`'s migration scoring
  block, when evaluating a destination settlement, check if any NPC there has `Reputation::score
  >= 2.0`. If so, add +0.2 to the migration attractiveness score. Uses existing
  `registry.view<HomeSettlement, Reputation>` to find high-rep NPCs at each candidate settlement.
  Creates a "famous resident" pull effect — prestigious NPCs make their town a migration target.

- [ ] **Thief shunned from work** — In `ScheduleSystem` (`src/ECS/Systems/ScheduleSystem.cpp`),
  when assigning work during daytime, check `registry.try_get<Reputation>(entity)`. If
  `rep->score < -1.0`, skip the work assignment and leave the NPC idle. Log once per day:
  "[NPC] was turned away from work (bad reputation)." Add `bool shunnedLogged = false` to
  `DeprivationTimer` to rate-limit. Resets when reputation rises above -0.5. Social consequence
  that links reputation to economic participation.

- [ ] **Reputation decay toward zero** — In `AgentDecisionSystem`'s per-NPC update (or a new
  small block in the idle section), decay `Reputation::score` toward 0 by 0.01 per game-hour.
  Use `registry.view<Reputation>` and multiply by `gameHoursDt`. Positive scores decrease,
  negative scores increase — reputation fades over time unless maintained by actions. Prevents
  permanent stigma and encourages ongoing prosocial behaviour.

- [ ] **Panic visual indicator** — In `GameState::Draw`'s agent loop, when an NPC has
  `panicTimer > 0` (need to pipe through `RenderSnapshot::AgentEntry` as `bool isPanicking`),
  draw a small yellow '!' above the NPC's head using `DrawText("!", a.x - 2, a.y - a.size - 12,
  10, Fade(YELLOW, 0.8f))`. In `SimThread::WriteSnapshot`, set `isPanicking = (dt.panicTimer > 0)`
  where `dt` is the `DeprivationTimer`. Makes panic visible to the player.

- [ ] **NPC reports bandit sighting** — In `AgentDecisionSystem`, after the panic scatter block,
  for each panicking NPC that has a `HomeSettlement`, log: "[NPC] reports seeing a bandit near
  [road/settlement]!" Rate-limit with `greetCooldown` (reuse existing cooldown on DeprivationTimer).
  Uses existing `Name` and `HomeSettlement` components. Creates information propagation where NPC
  fear translates into visible community awareness of bandit threats.

- [ ] **Resource price history sparkline** — In `RenderSystem::DrawStockpilePanel`, below each
  resource line, draw a tiny 3-line-high sparkline of the last 20 price samples. Add
  `std::map<ResourceType, std::vector<float>> priceHistory` to `StockpilePanel` in
  `RenderSnapshot.h`. In `SimThread::WriteSnapshot`, record `mkt->GetPrice(type)` each snapshot
  (cap at 20 entries via ring buffer or push_back + erase). Use same GREEN/RED line style as
  the population graph. Makes price trends visible without cluttering the display.

- [ ] **Population graph hover tooltip** — In `RenderSystem::DrawStockpilePanel`, when the mouse
  is inside the population chart area, show the exact population value for the nearest data point.
  Use `GetMousePosition()` to find the closest X coordinate, then draw a small tooltip box with
  "Day N: pop X" near the cursor. Uses existing `popHistory` data — no new snapshot fields needed.

- [ ] **Mood-based NPC idle chatter** — In `AgentDecisionSystem`'s idle gossip/greeting block,
  use the NPC's contentment to select different chat messages. When `contentment > 0.8`, log
  cheerful remarks ("[NPC] hums a tune at [settlement]"). When `contentment < 0.3`, log
  complaints ("[NPC] grumbles about conditions at [settlement]"). Rate-limit with existing
  `greetCooldown` on `DeprivationTimer`. Uses `Needs` to compute contentment inline (same
  weighted average as SimThread::WriteSnapshot). Creates emotionally varied social chatter.

- [ ] **Mood contagion between neighbours** — In `AgentDecisionSystem`'s idle block, when two
  NPCs are within 30u and both idle, blend their contentment slightly: the happier NPC loses
  0.01 contentment, the sadder gains 0.01 (clamped 0-1). Modify `Needs` values directly by
  nudging the lowest need of the sad NPC up by 0.01. Rate-limit to once per 6 game-hours per
  pair (use `greetCooldown`). Log once: "[NPC] cheered up [NPC] at [settlement]." Creates
  emergent social support networks where happy NPCs stabilise struggling neighbours.

- [ ] **Bandit territory marking** — In `GameState::Draw`, when NOT hovering any agent, draw a
  faint red-tinted region around each road midpoint that has `banditCount > 0` (from `RoadEntry`).
  Use `DrawCircleV` at the road midpoint `((x1+x2)/2, (y1+y2)/2)` with radius 40u and
  `Fade(RED, 0.04f * banditCount)` (capped at 0.15f). Makes bandit-infested roads visible on
  the world map without requiring hover. Uses existing `RoadEntry::banditCount` field.

- [ ] **Bandit gang name in tooltip** — In `HUD::DrawHoverTooltip`, when `a.isBandit` and
  `a.gangName` is non-empty, show "Gang: [gangName]" as a tooltip line in `Fade(MAROON, 0.8f)`.
  Add after the bandit line. Uses existing `AgentEntry::gangName` field already piped through
  the snapshot. Adds identity to bandit groups and makes them feel like named threats.

- [ ] **Family visit gift exchange** — When a visiting NPC arrives within 30u of the target
  settlement (in the visit handler in `AgentDecisionSystem`), if the visitor has `money.balance > 50`,
  transfer 5–15g to a random family member at that settlement. Log "[Name] gave a gift to [Family]
  in [Settlement]." Credits the recipient's `Money` balance directly (no treasury involvement — it's
  a personal gift). Strengthens the feeling that family bonds have economic meaning.

- [ ] **NPC homesickness when visiting** — While an NPC is on a family visit (`visitTimer > 0`),
  drain `needs.list[0]` (hunger) and `needs.list[1]` (thirst) 20% faster (multiply drain by 1.2
  in `NeedDrainSystem` when `timer.visitTimer > 0`). This soft penalty makes visits a trade-off:
  the NPC misses meals at home. Also add a "Visiting family" mood state to the tooltip — in
  `HUD::DrawHoverTooltip`, if `visitTimer > 0` (pipe through `AgentEntry`), show "Visiting family"
  in `Fade(SKYBLUE, 0.9f)` before the mood line.

- [ ] **Family reunion celebration** — When a visiting NPC returns home (visitTimer transitions
  from >0 to 0 in `AgentDecisionSystem`), set `goal.celebrateTimer = 3.f` and log "[Name] returned
  home from visiting family." Other family members at the home settlement with matching `FamilyTag::name`
  also get `celebrateTimer = 2.f`. Creates a visible burst of celebration when a traveller comes home.

- [ ] **Rivalry morale effect** — While `Settlement::rivalryTimer > 0`, boost morale by +0.003
  per game-hour (competitive pride). In the `RandomEventSystem` settlement `.each()` block, after
  the morale drift logic (line ~48), add: if `rivalryTimer > 0`, `s.morale += 0.003f * gameHoursDt`.
  Clamp to 1.0. Makes rivalry a double-edged sword: trade suffers but the populace rallies.

- [ ] **Rivalry visual on world map** — In `GameState::Draw`, when rendering road lines between
  settlements, check if both endpoints have `rivalryTimer > 0` and `rivalEntity` pointing at each
  other. If so, draw the road in `Fade(ORANGE, 0.5f)` instead of the default colour, and draw a
  small crossed-swords icon (two short diagonal lines) at the road midpoint. Uses existing
  `SettlementStatus` — pipe `rivalEntity` as `entt::entity rivalTarget` in `RenderSnapshot.h`.

- [ ] **Rivalry end trade boom** — When a rivalry expires (in `RandomEventSystem` rivalry tick-down,
  where `rivalryTimer` hits 0), give both settlements a temporary +20% production modifier for 6
  game-hours (`s.productionModifier = 1.2f; s.modifierDuration = 6.f; s.modifierName = "Post-Rivalry Boom"`).
  Log "[A] and [B] resume trade — post-rivalry boom." Reward for surviving the rivalry period.

- [ ] **Friend greeting uses name** — In `AgentDecisionSystem`'s greeting block, after the affinity
  gain, check if the two NPCs' mutual affinity exceeds `FRIEND_THRESHOLD` (0.5). If so, change
  the log message from "[A] greets [B]" to "[A] warmly greets friend [B]". No new components —
  uses existing `Relations::affinity` check. Makes high-affinity relationships visible in the log.

- [ ] **NPC introduces friend to stranger** — In `AgentDecisionSystem`'s evening gathering chat
  block (after the chat partner is found), if the chatting NPC has a friend (affinity > 0.5) nearby
  who has zero affinity with the chat partner, set both strangers' mutual affinity to 0.05 and log
  "[A] introduces [B] to [C]." Check `Relations::affinity` on both the friend and the chat partner.
  Social networks grow organically through introductions rather than random encounters alone.

- [ ] **Greeting complaint spreads morale impact** — In `AgentDecisionSystem`'s greeting block,
  when the greeter complains about a need (the new complaint branch), reduce the listener's
  `contentment` by 0.01 (in `DeprivationTimer::contentment`). Hearing complaints makes neighbours
  slightly less content. Conversely, if neither NPC has any need below 0.3, boost both by +0.005.
  Creates emotional contagion: struggling NPCs drag down community mood, thriving ones lift it.

- [ ] **NPC shares food with hungry friend** — In `AgentDecisionSystem`'s greeting block, when
  the greeter complains about hunger (need[0] < 0.3) and the listener has `money.balance > 20`,
  the listener buys 1 unit of food from the local stockpile at market price and gives it to the
  greeter (greeter's hunger need += 0.3, listener's money -= price, settlement treasury += price).
  Log "[Listener] shares a meal with [Greeter]." Follows Gold Flow Rule — money goes to treasury.
  Check `Stockpile` has food > 1 before proceeding. Max once per greeting cooldown cycle.

- [ ] **Hauler route loyalty log** — In `TransportSystem`, when a hauler picks a route that
  matches their `bestRoute` (the preferred-route bonus fired), log "[Hauler] sticks to their
  favourite route [bestRoute]." In the Idle→GoingToDeposit transition (after `FindBestRoute`
  returns), compare `best.dest` against `hauler.bestRoute` using the same name→name format.
  Pure flavour log — makes hauler specialization visible in the event feed.

- [ ] **Hauler explores new route after losses** — In `TransportSystem`'s Idle state, when
  `hauler.nearBankrupt` is true and `hauler.bestRoute` is non-empty, clear `hauler.bestRoute`
  (set to "") so the hauler stops preferring its old route and explores alternatives. Log
  "[Hauler] abandons familiar route after losses." This creates dynamic route adaptation:
  haulers stuck in unprofitable patterns break free when facing bankruptcy.

- [ ] **Elder NPC mentors young worker** — In `AgentDecisionSystem`'s idle block, when an elder
  NPC (age > 55 days) is near a working young NPC (age < 25 days) at the same settlement, 3%
  chance per game-hour to boost the young NPC's highest skill by +0.02 (in `Skills` component).
  Log "[Elder] mentors [Young] in [skill]." Check `try_get<Skills>` on both. Creates
  intergenerational knowledge transfer — elders slow down but pass on expertise.

- [ ] **Child NPC follows parent** — In `AgentDecisionSystem`'s idle block, when a child NPC
  (age < 15 days) has a `FamilyTag`, find the nearest adult (age > 15) with matching
  `FamilyTag::name` at the same settlement. If within 80u, move toward parent at 0.5x speed
  (same `MoveToward` pattern). If parent is absent (different settlement), child stays near home
  settlement centre. Log nothing — purely visual behaviour. Adds visible family structure.

- [ ] **Settlement mood ring colour** — In `GameState::Draw`, when rendering settlement circles,
  tint the outer ring by `SettlementEntry::moodScore`: GREEN (≥0.7), YELLOW (≥0.4), RED (<0.4).
  Currently the ring uses food/water/season logic. Add mood as a secondary thin ring (2px) just
  outside the existing one, using `DrawCircleLinesV` with radius `s.radius + 3`. Pure visual.

- [ ] **Low-mood NPC wanders aimlessly** — In `AgentDecisionSystem`'s idle block, when an NPC's
  average need satisfaction (mean of `needs.list[0..3].value`) is below 0.3, instead of normal idle
  drift, set velocity to a random direction at 0.3x speed for 2 game-seconds (add `float wanderTimer`
  to `DeprivationTimer`). Log nothing. Creates visible restlessness when NPCs are struggling —
  they pace instead of standing still. Reset wander on need recovery above 0.4.

- [ ] **Contentment shown in NPC tooltip** — In `HUD::DrawHoverTooltip`, add a "Content: X%"
  line for non-hauler NPCs. Pipe `float contentment` through `AgentEntry` in `RenderSnapshot.h`
  (compute as avg of 4 needs in `SimThread::WriteSnapshot`). Color-code GREEN ≥0.7, YELLOW ≥0.4,
  RED below. Place after the mood line. Makes the contentment→production link visible to player.

- [ ] **Discontented NPC complains to settlement leader** — In `AgentDecisionSystem`'s idle block,
  when an NPC's contentment < 0.3 and `timer.greetCooldown <= 0`, find the highest-reputation NPC
  at the same settlement via `registry.view<Reputation, HomeSettlement, Name>`. Log "[NPC]
  complains to [Leader] about conditions." Set greetCooldown = 5.f to rate-limit. Gives settlements
  a sense of social hierarchy where discontented citizens voice grievances to respected members.

- [ ] **Reputation discount shown in trade log** — In `SimThread::ProcessInput`'s T-key and Q-key
  purchase log messages, when `repFactor < 1.0f`, append " (X% rep discount)" to the existing buy
  message. E.g. "Bought 5 food at Riverside for 40g (5% rep discount)". Pure log enhancement —
  no new gameplay effect. Uses existing `repFactor`/`repFactor4` variables.

- [ ] **Negative reputation blocks settlement entry** — In `SimThread::ProcessInput`, when
  `m_playerReputation <= -50`, prevent T-key and Q-key purchases at settlements. Log "The merchants
  of [Settlement] refuse to trade with you." In `AgentDecisionSystem`, NPCs at that settlement
  also refuse E-key work requests from the player (check `PlayerTag` proximity and rep via
  `RenderSnapshot::playerReputation`). Reputation must recover above -30 to re-enable trade.

- [ ] **Hauler worst route shown in tooltip** — In `HUD::DrawHoverTooltip`, when the hovered
  agent is a hauler with non-empty `worstRoute` and `worstRouteTimer > 0`, show "Avoiding: [route]
  (Xh left)" in `Fade(RED, 0.6f)`. Pipe `worstRoute` and `worstRouteTimer` through `AgentEntry`
  in `RenderSnapshot.h` (add `std::string worstRoute; float worstRouteTimer = 0.f`). Extract in
  `SimThread::WriteSnapshot` from the Hauler component. Makes route avoidance visible to the player.

- [ ] **Hauler gossips about bad route** — In `TransportSystem`, when a hauler records a new
  `worstRoute` (loss-making trip), find other haulers at the same home settlement via
  `registry.view<Hauler, HomeSettlement>`. If another hauler has the same `bestRoute` as this
  hauler's `worstRoute`, set that hauler's `worstRoute` to match and `worstRouteTimer = 12.f`
  (half duration — secondhand info). Log "[Hauler A] warns [Hauler B] about [route]." Creates
  information sharing between haulers — bad news travels through the trade network.

- [ ] **Gratitude chain: helped NPC helps others sooner** — In `AgentDecisionSystem`'s charity
  block, when setting `starvingTmr->lastHelper`, also reduce the starving NPC's `charityTimer`
  by 50% (halve any remaining cooldown). This means recently helped NPCs "pay it forward" faster.
  Log nothing — purely mechanical. Check `starvingTmr->charityTimer > 0` before halving.

- [ ] **NPC avoids past thieves** — In `AgentDecisionSystem`'s greeting block, add a check: if
  the other NPC has `Reputation::score < -0.3` and `timer.lastHelper == entt::null` (no gratitude
  override), skip the greeting entirely and log "[Name] avoids [Other] (mistrusted)". Uses existing
  `try_get<Reputation>`. Creates visible social shunning where low-rep NPCs are frozen out of
  casual interactions, complementing the existing charity refusal for bad-rep NPCs.

- [ ] **NPCs flee settlement when intimidation is high** — In `AgentDecisionSystem`'s idle block,
  after the bandit intimidation check, if `Settlement::morale < 0.15` and the NPC has been
  intimidated (intimidationCooldown was just set), 10% chance per game-hour to trigger emergency
  migration to a random connected settlement. Log "[NPC] flees [Settlement] — too dangerous."
  Uses existing migration logic (set `state.behavior = AgentBehavior::Migrating`). Creates visible
  population flight from bandit-terrorized settlements.

- [ ] **Bandit presence shown in settlement tooltip** — In `HUD::DrawSettlementTooltip`, count
  bandits within 80u of the settlement centre (pipe `int nearbyBandits` through
  `RenderSnapshot::SettlementEntry`). If > 0, show "Bandits nearby: X" in `Fade(RED, 0.8f)`.
  Compute in `SimThread::WriteSnapshot` by scanning `BanditTag, Position` vs settlement position.
  Makes the bandit threat visible without needing to hover individual NPCs.

- [ ] **Mood contagion shown in NPC tooltip** — In `HUD.cpp`'s `DrawHoverTooltip`, when
  `moodContagionCooldown > 0` on the hovered NPC, show "Recently cheered up" in faint GREEN.
  Pipe `bool recentlyCheered` through `RenderSnapshot::AgentEntry` from `SimThread::WriteSnapshot`
  (set true when `timer.moodContagionCooldown > 0`). Struggling NPCs who received a boost get
  visual feedback in the tooltip.

- [ ] **Happy NPCs attract idle neighbours** — In `AgentDecisionSystem`'s idle block, after the
  mood contagion check, when an idle NPC has avg needs < 0.3 and there is a happy NPC (avg > 0.8)
  within 60u, set velocity toward the happy NPC at 0.3× speed for up to 30 game-seconds (use
  `chatTimer`). Log nothing (ambient drift only). Miserable NPCs naturally gravitate toward cheerful
  neighbours, creating visible social clustering around happy NPCs.

- [ ] **Mood contagion chain reaction** — In `AgentDecisionSystem`'s mood contagion block, when an
  NPC receives mood contagion (morale boost applied), also boost the receiving NPC's lowest need by
  +0.02 (one-time). This creates a small direct benefit beyond morale — the happy NPC's presence
  genuinely helps. Cap the boost so the need doesn't exceed 0.5. Emotional support has tangible
  health benefits.

- [ ] **Celebration builds affinity between participants** — In `AgentDecisionSystem`'s Celebrating
  block, after the friend-recruitment scan, when two celebrating NPCs are within 20u of each other
  (both have `skillCelebrateTimer > 0`), boost mutual `Relations::affinity` by +0.03/game-hour
  (use `get_or_emplace<Relations>`). Gate with a `static float` per-frame check to avoid O(n²)
  every tick — only run once per 2 real-seconds. Shared celebration strengthens social bonds.

- [ ] **Celebration shown in NPC tooltip** — In `HUD.cpp`'s `DrawHoverTooltip`, when the hovered
  NPC has `AgentBehavior::Celebrating` and `skillCelebrateTimer > 0` (piped as `bool celebrating`
  through `RenderSnapshot::AgentEntry`), show "Celebrating!" in faint GOLD. Add the bool in
  `SimThread::WriteSnapshot`'s agent loop. Simple visual feedback for an active social event.

- [ ] **Master count shown in settlement status bar** — In `SimThread::WriteSnapshot`'s
  `worldStatus` loop, count NPCs homed at each settlement with any skill >= 0.9 (via
  `registry.view<Skills, HomeSettlement>`). Pipe as `int masterCount` on
  `RenderSnapshot::SettlementStatus`. In `RenderSystem::DrawWorldStatus`, append "M:N" in GOLD
  after the population display when masterCount > 0. Settlements with masters are visibly special.

- [ ] **Apprentice milestone boosts NPC contentment** — In `ScheduleSystem`'s `checkMilestone`
  lambda, when an Apprentice milestone (idx 2, threshold 0.25) is reached, give the NPC a one-time
  need boost: set the lowest `Needs::list[i].value` to `max(current, 0.5)`. First skill progress
  gives NPCs a morale lift — learning a trade feels rewarding even at the lowest level.

- [ ] **Witness tells others about player bravery** — In `AgentDecisionSystem`'s greeting block,
  when an NPC with `lastHelper != entt::null` (set by confrontation witness) greets another idle NPC
  within 25u, propagate `lastHelper` to the other NPC's `DeprivationTimer` (via
  `get_or_emplace<DeprivationTimer>`). Log "[NPC] tells [Other] about the player's bravery." Gate
  with `greetCooldown`. Word of heroic deeds spreads through NPC social networks.

- [ ] **Witness reputation bonus decays** — In `AgentDecisionSystem`'s reputation decay block
  (top of `Update`), when an NPC has `lastHelper != entt::null` and the helper is the player entity,
  clear `lastHelper` after 48 game-hours. Add `float lastHelperTimer = 0.f` to `DeprivationTimer`,
  set to 48.0 when `lastHelper` is assigned. Decrement by `gameHoursDt` each frame; when it reaches
  0, set `lastHelper = entt::null`. Memories fade — gratitude doesn't last forever.

- [ ] **Gossip hop limit prevents infinite spread** — In `AgentDecisionSystem`'s greeting block
  gossip propagation (where `lastHelper` is spread), add `int gossipHops = 0` to `DeprivationTimer`.
  Set to 0 for direct witnesses (in `SimThread::ProcessInput`'s witness loop), increment by 1 when
  spreading via gossip. Only spread when `gossipHops < 3`. This caps how far the player's fame
  travels — direct witnesses are more credible than third-hand accounts.

- [ ] **NPC greeting mentions shared knowledge** — In `AgentDecisionSystem`'s greeting block,
  when both NPCs have `lastHelper == playerEntity`, replace the normal greeting log with
  "[NPC] and [Other] discuss the player's deeds." (no gossip spread needed since both already know).
  Use the existing `isGratitude`/`isFamilyReunion` priority pattern — check this before the normal
  greeting case. Shared knowledge creates a sense of community narrative.

- [ ] **Bad-rep player warned in NPC tooltip** — In `HUD.cpp`'s `DrawHoverTooltip`, when the
  hovered NPC has `Reputation::score < -0.5`, show "Fears you" in `Fade(RED, 0.7f)`. Pipe
  `bool fearsPlayer` through `RenderSnapshot::AgentEntry` (set true in `SimThread::WriteSnapshot`
  when `Reputation::score < -0.5` AND the NPC is within 60u of the player). Visual feedback that
  your reputation is causing NPCs distress.

- [ ] **NPC avoidance escalates with worse reputation** — In `AgentDecisionSystem`'s avoid-player
  block, scale the flee radius and speed by reputation severity: rep < -0.5 → 30u/0.8×,
  rep < -1.0 → 50u/1.0×, rep < -2.0 → 80u/1.2×. Use `std::abs(rep->score)` to index into
  thresholds. Severely disliked players clear entire areas. Keep the same panicTimer = 2.f and
  log message pattern.

- [ ] **NPC beckons player toward settlement** — In `AgentDecisionSystem`'s idle block, after the
  wave-at-player check, when an idle NPC has avg needs > 0.7 and the player is 50–100u from the
  NPC's home settlement, 0.5% chance per real-second to log "[NPC] beckons you toward [Settlement]."
  Gate with `thankCooldown`. NPCs at thriving settlements advertise their home as a welcoming place.

- [ ] **Sad NPC sighs near player** — In `AgentDecisionSystem`'s idle block, near the wave check,
  when an idle NPC has avg needs < 0.3 and is within 40u of the player, 0.5% chance per real-second
  to log "[NPC] sighs wearily." Gate with `thankCooldown`. Struggling NPCs create ambient feedback
  that communicates the world's problems without explicit UI indicators.

- [ ] **Teaching tooltip shows skill name** — In `SimThread::WriteSnapshot`'s agent loop, replace
  `recentlyTaught` bool with `std::string teachingSkill` on `RenderSnapshot::AgentEntry`. When
  `timer.teachCooldown > 0`, also check the NPC's `Skills` component to find which skill is highest
  (farming/water/woodcutting) and set teachingSkill to that name. In `HUD.cpp`, show
  "Recently taught [Skill]" instead of generic text. More informative tooltip.

- [ ] **Mentor shown in NPC tooltip** — Add `std::string mentorName` to
  `RenderSnapshot::AgentEntry`. In `SimThread::WriteSnapshot`, when `timer.teachCooldown > 0`,
  scan nearby NPCs (30u) with higher skill to find the probable mentor via `Relations::affinity`.
  In `HUD.cpp`'s tooltip, show "Learned from [Mentor]" in faint SKYBLUE. Creates visible
  mentor-student relationships in the UI.

- [ ] **Learner seeks out mentor again** — In `AgentDecisionSystem`'s idle block, after skill
  training, when an NPC has `lastHelper != entt::null` and the helper is not the player, and the
  helper is within 80u, 2% chance per real-second to set velocity toward the helper at 0.5× speed
  (using `chatTimer = 3.f` to drift). No log — ambient movement only. Learners naturally seek out
  their mentors, creating visible student-teacher clustering.

- [ ] **Repeated mentoring deepens bond** — In `AgentDecisionSystem`'s skill training block, after
  the existing `lastHelper = entity` line, check if `oTimer.lastHelper` was *already* set to the
  same teacher entity. If so, double the affinity gain (+0.04 instead of +0.02). Add a `static
  std::map<uint64_t, int>` keyed by `(learner_id * 10000 + teacher_id)` to count mentoring sessions.
  When count >= 3, log "[Learner] considers [Teacher] a trusted mentor." Long-term relationships
  emerge from repeated interaction.

- [ ] **Specialisation shown in settlement tooltip** — In `HUD.cpp`'s settlement tooltip (or
  `RenderSystem::DrawWorldStatus`), when a settlement has ≥3 masters in a resource type, show
  "Specialises in [Type]" in GOLD. Pipe `std::string specialisation` through
  `RenderSnapshot::SettlementEntry` from `SimThread::WriteSnapshot` (count masters per type via
  `registry.view<Skills, HomeSettlement>`). Visual feedback for the production bonus.

- [ ] **Specialisation attracts migrants** — In `AgentDecisionSystem`'s migration block, when an
  NPC is considering migration targets, settlements with ≥3 masters in any skill get a +20% score
  bonus in the destination scoring. Access via a per-frame cache of master counts (same pattern as
  `ProductionSystem`'s `masterCount` map). Skilled communities attract talent.

- [ ] **Idle skilled NPC seeks work at matching facility** — In `AgentDecisionSystem`'s idle block,
  when an idle NPC has any skill ≥ 0.5 and is not currently on strike or grieving, 5% chance per
  game-hour to set `AgentBehavior::Working` if a matching facility exists at their home settlement.
  Check via `registry.view<ProductionFacility>` matching `HomeSettlement::settlement`. Uses existing
  `ScheduleSystem` work hours — only fire during work hours (7–18). Skilled NPCs don't let their
  talents go to waste.

- [ ] **Skill rust shown in event log** — In `AgentDecisionSystem`'s idle block, when an NPC
  has any skill ≥ 0.7 and has been idle for 24+ game-hours (track via a new
  `float idleHoursAccum = 0.f` on `DeprivationTimer`, reset when behavior != Idle), log
  "[NPC]'s [Skill] skills are getting rusty." once per 48 game-hours (use idleHoursAccum modulo).
  Creates narrative awareness of wasted talent.

- [ ] **Grieving NPC walks slowly** — In `MovementSystem`, when an entity has
  `DeprivationTimer::griefTimer > 0`, multiply effective speed by 0.5f. Check via
  `registry.try_get<DeprivationTimer>` in the movement loop. Grief physically slows NPCs,
  making their state visible without tooltip hover.

- [ ] **NPC comforts grieving friend** — In `AgentDecisionSystem`'s idle block, after the grief
  drain section, when a non-grieving idle NPC is within 25u of a grieving NPC (griefTimer > 0)
  with `Relations::affinity >= 0.3`, reduce the grieving NPC's griefTimer by 0.5 game-hours
  (one-time, gate with `greetCooldown`). Log "[NPC] comforts [Grieving NPC]." Friends help
  each other through loss — grief heals faster in good company.

- [ ] **Grief penalty shown in stockpile panel** — In `SimThread::WriteSnapshot`'s stockpile panel
  section, count grieving workers at the selected settlement (NPCs with `AgentBehavior::Working` and
  `griefTimer > 0`). Pipe as `int grievingWorkers` on `RenderSnapshot::StockpilePanel`. In
  `RenderSystem::DrawStockpilePanel`, when > 0, show "Grieving workers: N (-50% output)" in faint
  PURPLE. Update panel height calculation. Visible economic consequence in the settlement view.

- [ ] **Multiple deaths compound grief** — In `DeathSystem`'s family grief block, when setting
  `griefTimer = 4.0f` on surviving family members, check if `griefTimer` is *already* > 0 (meaning
  they're still grieving a previous loss). If so, set `griefTimer = current + 3.0f` instead of 4.0f
  (compounding grief). Cap at 12.0f game-hours to prevent infinite stacking. Log "[NPC] is
  overwhelmed by loss." Multiple family deaths in quick succession create compounding grief.

- [ ] **Grief comfort builds affinity** — In `AgentDecisionSystem`'s greeting block, in the
  family reunion grief-clearing section, after halving griefTimer, also boost mutual
  `Relations::affinity` by +0.1 (via `get_or_emplace<Relations>`). Comforting someone through loss
  strengthens the bond significantly — more than a normal greeting (+0.01) or even gratitude (+0.05).

- [ ] **Grieving NPC seeks family** — In `AgentDecisionSystem`'s idle block, when an NPC has
  `griefTimer > 0` and a `FamilyTag`, scan for family members at different settlements (via
  `registry.view<FamilyTag, HomeSettlement, Position>`). If found within 200u, set velocity toward
  them at 0.6× speed and `chatTimer = 5.f`. No log — ambient movement. Grieving NPCs naturally
  seek out family, creating visible grief-driven migration patterns.

- [ ] **Comforting builds affinity** — In `AgentDecisionSystem`'s comfort-grieving-neighbour block,
  after reducing griefTimer, boost mutual `Relations::affinity` by +0.06 (via `get_or_emplace`).
  Comforting someone through grief strengthens the bond more than a casual greeting (+0.01) but
  less than family reunion comfort. No new fields — uses existing Relations component.

- [ ] **Comfort shown in NPC tooltip** — In `HUD.cpp`'s `DrawHoverTooltip`, when
  `comfortCooldown > 0` on the hovered NPC, show "Recently comforted someone" in faint PURPLE.
  Pipe `bool recentlyComforted` through `RenderSnapshot::AgentEntry` from
  `SimThread::WriteSnapshot` (check `timer.comfortCooldown > 0`). Visual feedback for the
  comfort interaction.

- [ ] **Hauler trip count shown in stockpile panel** — In `RenderSystem::DrawStockpilePanel`,
  after the profit display on each hauler route line, show trip count: " (N trips)" in faint WHITE.
  Add `int lifetimeTrips` to `StockpilePanel::HaulerInfo`. Pipe from `SimThread::WriteSnapshot`
  via `Hauler::lifetimeTrips`. Gives context to the profit number — is this from 1 trip or 50?

- [ ] **Hauler efficiency ranking in stockpile panel** — In `RenderSystem::DrawStockpilePanel`,
  below the hauler routes section, when there are ≥2 haulers, show "Best: [Name] ([profit/trip]g/trip)"
  in faint GOLD. Compute from `lifetimeProfit / max(1, lifetimeTrips)` across haulerRoutes entries.
  No new piping — pure render-side calculation on existing HaulerInfo data.
