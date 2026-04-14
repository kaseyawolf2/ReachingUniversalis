# ReachingUniversalis — Dev Backlog

**Priority: NPC behaviour depth and inter-NPC interaction — building toward a living world.**

Maintained by Claude Code cron. Each session picks the top `[ ]` task, implements it, commits,
marks it done, then appends 2–3 new concrete tasks to keep the queue full.

---

## In Progress

- [ ] **Jealousy-driven skill motivation** — In `AgentDecisionSystem.cpp`'s skill growth block, after the jealousy scan, check if an NPC's `Relations::affinity` toward any same-profession NPC at the settlement is < 0.1 (jealousy threshold) AND that target's matching skill is >= 0.8. If so, apply `growth += 0.0003f` as competitive motivation. Log "[NPC] trains harder to surpass [Rival] at [Settlement]" at 1-in-12 frequency via `s_teachRng`. Creates a feedback loop: jealousy → motivation → eventual pride announcement → more jealousy.

## Done

- [x] **Work buddy co-migration** — In `AgentDecisionSystem.cpp`'s friend co-migration block, extend the co-migration check to also consider `Relations::workBestFriend`. When an NPC migrates and their work best friend is at the same settlement with `stockpileEmpty >= migrateThreshold * 0.7f` (close to migrating anyway), 1-in-4 chance the buddy follows to the same destination. Log "[Buddy] follows work partner [Migrant] to [Destination]" at full frequency. Strengthens the social pull of workplace bonds.


- [x] **Work buddy grief support** — In `AgentDecisionSystem.cpp`'s comfort-grieving block, when the comforter's `Relations::workBestFriend` equals the grieving NPC, double the comfort effectiveness (reduce `griefTimer` by 1.0 instead of 0.5) and boost mutual affinity by +0.03. Log "[Comforter] stays by work buddy [Griever]'s side at [Settlement]" at 1-in-5 frequency. Uses existing `workBestFriend` field. Creates deeper emotional support between coworkers.


- [x] **Vigil badge on settlement tooltip** — In `SimThread::WriteSnapshot`'s settlement loop, add `bool vigil = false` to `SettlementEntry` in `RenderSnapshot.h`. Set when 3+ NPCs at the settlement have `griefTimer > 0` (check via `DeprivationTimer`). In `HUD.cpp`'s settlement tooltip, display "[Vigil]" in muted purple after existing badges. Gives the player visibility into communal grief events.


- [x] **Vigil morale recovery** — In `AgentDecisionSystem.cpp`'s grief vigil gathering block, after the affinity boost, apply +0.03 to `Settlement::morale` (cap 1.0). This offsets the individual grief morale drain by giving settlements a collective healing mechanism. Log "[Settlement]'s vigil brings comfort" at 1-in-3 frequency after the vigil log. No new fields needed — purely extends the existing vigil block.


- [x] **Grief support network** — In `AgentDecisionSystem.cpp`'s comfort-grieving block, when an NPC comforts a grieving NPC and both have `lastGriefDay >= 0` (both experienced grief before), double the comfort effectiveness: reduce `griefTimer` by 1.0 instead of 0.5. Log "[Comforter] understands [Griever]'s pain at [Settlement]" at 1-in-6 frequency. Uses existing `lastGriefDay` field. Creates empathy-based social dynamics where experienced grievers are better comforters.


- [x] **Grief anniversary remembrance** — In `AgentDecisionSystem.cpp`'s grief block, track each NPC's `lastGriefDay` from `DeprivationTimer`. When `tm.day - lastGriefDay` equals exactly 30 (one month anniversary) and the NPC has `Relations::affinity >= 0.4` toward any NPC at the same settlement, set `griefTimer = 1.f` (brief 1-hour renewed grief). Log "[NPC] reflects on those lost at [Settlement]" at 1-in-4 frequency. Creates a recurring emotional beat that reinforces social bonds through shared memory.


- [x] **Elder mediation of workplace rivalry** — In `ScheduleSystem.cpp`'s rival profession taunt block, when an elder (age>60, skill>=0.7) is working at the same facility, 1-in-6 chance the taunt is suppressed and both rivals gain +0.02 affinity toward the elder instead. Log "[Elder] calms tensions between [NPC1] and [NPC2]" at full frequency. Uses existing `Age`, `Skills` try_get. Gives elders a conflict-resolution role beyond skill teaching.


- [x] **Elder departure farewell feast** — In `AgentDecisionSystem.cpp`'s migration block, when an elder (age>60) with 3+ local friends does eventually migrate (threshold exceeded despite resistance), all friends at the old settlement lose -0.01 morale on `Settlement` and gain +0.03 mutual affinity among themselves (bonding over shared loss). Log "[Settlement] holds a farewell feast for [Elder]" once. Creates a memorable social event when a community pillar finally leaves.


- [x] **Elder storytelling event** — In `RandomEventSystem.cpp`, add a new stochastic event (1-in-200 per settlement per check). When triggered, find an elder (age>60, skill>=0.7) at the settlement. All NPCs at the settlement with `Relations::affinity >= 0.2` toward the elder gain +0.02 mutual affinity. The elder gains +0.01 affinity toward all attendees. Log "[Elder] tells tales of the old days at [Settlement]" once. Boost settlement morale by +0.02. Creates a social gathering event around respected elders.


- [x] **Elder council road maintenance discount** — In `ConstructionSystem.cpp`'s road maintenance block, when both endpoint settlements have 2+ skilled elders (age>60, skill>=0.7), reduce `ROAD_MAINT_COST_EACH` by 20% for that road. Log "[Road]'s upkeep eased by elder oversight" at 1-in-8 frequency. Reuses the `skilledElderCount` map. Extends the elder council theme to infrastructure maintenance.


- [x] **Elder apprentice fast-track** — In `AgentDecisionSystem.cpp`'s elder wisdom skill boost block, track each NPC's highest-affinity elder via a new `entt::entity elderMentor = entt::null` field on `Skills` in `Components.h`. When the elder mentor dies, the apprentice gets `growth += 0.0003f` for 5 days (accelerated learning spurt to honour their teacher). Log "[Apprentice] redoubles their efforts in memory of [Elder]" at 1-in-4 frequency. Counterbalances wisdom grief with motivated tribute.


- [x] **Mourning procession movement** — In `AgentDecisionSystem.cpp`'s grief block, when 3+ NPCs at the same settlement have `wisdomGriefDays > 0` simultaneously, set their `AgentState::behavior = Celebrating` (repurposed as gathering) for 1 game-hour and move them toward the settlement center. Log "[Settlement] gathers to honour [Elder]'s memory" once per elder death via `static std::set<entt::entity> s_honouredElders`. Boosts mutual affinity +0.02 among participants. Creates a visible group mourning event.


- [x] **Wisdom lineage tracking** — In `DeathSystem.cpp`'s elder wisdom fading block, when `wisdomGriefDays` is applied to a mourner, also set a new `entt::entity wisdomLineage = entt::null` field on `Skills` in `Components.h` to the deceased elder's entity (for narrative tracking). In `AgentDecisionSystem.cpp`'s skill growth block, when an NPC with `wisdomLineage != entt::null` crosses skill >= 0.8, log "[NPC] carries on [Elder]'s legacy at [Settlement]" at full frequency and clear the field. Creates a narrative thread connecting elder deaths to future mastery.


- [x] **Charity chain reaction** — In `AgentDecisionSystem.cpp`'s trade gift block, when the giver has `Reputation::score >= 0.5`, 1-in-6 chance that the recipient also donates 3g to a nearby NPC with `Money::balance < 10g` at the same settlement (balance-to-balance, Gold Flow Rule). Log "[Recipient] passes on [Giver]'s generosity at [Settlement]" at full frequency. Creates a cascade of kindness triggered by high-reputation donors.

- [x] **Procession badge on settlement tooltip** — In `SimThread::WriteSnapshot`'s settlement loop, add `bool mourning = false` to `SettlementEntry` in `RenderSnapshot.h`. Set when any NPC at the settlement has `wisdomGriefDays > 0` and `skillCelebrateTimer > 0` (actively in a mourning procession). In `HUD.cpp`'s settlement tooltip, display "[Mourning]" in muted grey after existing badges. Makes mourning processions visible to the player at the settlement level.

- [x] **Post-procession comfort bonus** — In `AgentDecisionSystem.cpp`'s comfort-grieving block, when the comforter was a participant in a mourning procession (check `skillCelebrateTimer > 0` and `wisdomGriefDays > 0`), double the comfort effectiveness: reduce `griefTimer` by 1.0 instead of 0.5. Log "[Comforter] draws strength from the procession to comfort [Griever]" at 1-in-6 frequency. Creates a connection between the group mourning event and individual grief support.

- [x] **Mourning procession morale boost** — In `AgentDecisionSystem.cpp`'s mourning procession block (after participants gather), apply `Settlement::morale += 0.02f` (cap 1.0) to the settlement where the procession occurs. This represents the community healing benefit of collective mourning. No new fields needed — uses existing `Settlement::morale`. Log "[Settlement] finds solace in shared grief" at 1-in-3 frequency after the procession log.

- [x] **Teaching chain tooltip badge** — In `SimThread::WriteSnapshot`'s NPC loop, add `bool isExpert = false` to `AgentEntry` in `RenderSnapshot.h`. Set when any skill >= 0.8 and matching `Profession::type`. In `HUD.cpp`'s NPC tooltip, display "[Expert]" in amber after the specialisation line. Makes the teaching chain hierarchy visible to the player.

- [x] **Hauler mentorship affinity boost** — In `TransportSystem.cpp`'s convoy block, when two haulers travel together and one has `Hauler::mentorBonus > 0` (experienced/second-chance hauler) and the other has `tripCount < 5` (novice), boost the novice's affinity toward the mentor by +0.02 per trip and set the novice's `mentorBonus = 0.05f` (small learned bonus). Log "[Mentor] shows [Novice] the ropes on the [Route] road" at 1-in-5 frequency. Creates a mentorship dynamic in convoy travel.

- [x] **Lineage chain multi-generation** — In `DeathSystem.cpp`'s elder wisdom fading block, when the dying NPC themselves has `wisdomLineage != entt::null` (they were carrying a legacy but died before achieving mastery), pass the original `wisdomLineageName` to the mourner instead of the dying NPC's name. This allows a lineage to chain across multiple deaths, eventually producing a "[NPC] carries on [Original Elder]'s legacy" log when mastery is finally reached. No new fields needed — reuses `wisdomLineageName`.

- [x] **Lineage pride tooltip badge** — In `SimThread::WriteSnapshot`'s NPC loop, add `bool wisdomHeir = false` to `AgentEntry` in `RenderSnapshot.h`. Set when `Skills::wisdomLineage != entt::null` (NPC is carrying an elder's unfinished legacy). In `HUD.cpp`'s NPC tooltip, display "[Heir]" in soft violet after the generous badge. Makes it visible which NPCs are on the path to fulfilling an elder's legacy.

- [x] **Expert gratitude from novice** — In `AgentDecisionSystem.cpp`'s idle chat block, when a novice (skill < 0.5) chats with an expert (skill >= 0.8) of the same profession at the same settlement, boost novice→expert affinity by +0.03 instead of the normal +0.02. Log "[Novice] thanks [Expert] for the guidance at [Settlement]" at 1-in-8 frequency. Uses existing `Profession` and `Skills` try_get. Complements the mastery teaching chain with a social bond component.

- [x] **Repeated reconciliation deepens bond** — In `ScheduleSystem.cpp`'s reconciliation block, track how many times a pair has reconciled via a `static std::map<std::pair<entt::entity,entt::entity>, int> s_reconCount`. On 2nd+ reconciliation, use +0.08 affinity instead of +0.05, and boost `reconcileGlow` to 4 game-hours instead of 2. Log "[NPC1] and [NPC2] are becoming unlikely friends at [Settlement]" at full frequency on 3rd+ reconciliation. Creates an escalating friendship arc from repeated conflict resolution.

- [x] **Reconciliation glow visible in tooltip** — In `SimThread::WriteSnapshot`'s NPC loop, add `bool reconciling = false` to `AgentEntry` in `RenderSnapshot.h`. Set when `DeprivationTimer::reconcileGlow > 0`. In `HUD.cpp`'s NPC tooltip, display "[Harmonious]" in soft green after existing badges. Makes the post-reconciliation state visible to the player.

- [x] **Generous donor tooltip badge** — In `SimThread::WriteSnapshot`'s NPC loop, add `bool generousDonor = false` to `AgentEntry` in `RenderSnapshot.h`. Set when `Reputation::score >= 0.6` (high reputation from charity/donations). In `HUD.cpp`'s NPC tooltip, display "[Generous]" in gold after the specialisation line. Makes charitable NPCs visible to the player.

- [x] **Second-chance hauler graduation bonus** — In `EconomicMobilitySystem.cpp`'s NPC→Hauler graduation block, when the graduating NPC has `DeprivationTimer::bankruptSurvivor == true`, set `Hauler::mentorBonus = 0.15f` (higher than normal 0.1) as a self-taught advantage. Log "[Name] returns to hauling with hard-won wisdom at [Settlement]." No new fields needed — reuses existing mentorBonus.

- [x] **Bankruptcy survivor inspiration** — In `AgentDecisionSystem.cpp`'s idle chat block, when a bankruptcy survivor (`DeprivationTimer::bankruptSurvivor == true`) chats with a non-survivor NPC at the same settlement, 1-in-10 chance to boost the non-survivor's `Relations::affinity` toward the survivor by +0.02 and log "[Survivor] inspires [Other] with their comeback story at [Settlement]." Uses existing idle chat stagger and proximity check.

- [x] **Hauler farewell toast on retirement** — In `TransportSystem.cpp`'s hauler retirement block (deferred `retireList` processing), when a veteran retires, scan all haulers at the same home settlement. For each with `Relations::affinity >= 0.3`, boost their affinity toward the retiree by +0.05 (cap 1.0). Log "[Hauler] raises a toast to [Retiree]'s years of service at [Settlement]" at 1-in-3 frequency per attending hauler. Creates a social send-off event.

- [x] **Convoy speed bonus for high affinity** — In `TransportSystem.cpp`'s convoy speed calculation (line where `convoySpeed` is set), when the convoy partner has `Relations::affinity >= 0.7` toward the hauler, increase the convoy speed bonus from 25% to 35% (`speed * 1.35f`). No log needed — the effect is visible through faster travel. Uses existing `convoyPartner` entity and `Relations` check.

- [x] **Hauler trade gossip** — In `TransportSystem.cpp`'s delivery block, after a successful sale, if another hauler from the same home settlement is within 80 units (check via position scan), the delivering hauler shares trade info: set the other hauler's `bestRoute` to this delivery's route name if profit exceeded 50g. Log "[Hauler] tips off [Other] about the [Route] route" at 1-in-6 frequency. Creates information-sharing between hauler peers.

- [x] **Hauler route rivalry reconciliation** — In `TransportSystem.cpp`'s delivery block (GoingToDeposit → arrival), when a hauler arrives at a destination and finds another hauler from the same home settlement already there (check via `Hauler::state == Idle` or `GoingHome` with same `cargoSource`), if their `Relations::affinity < 0.2` (rivalry), 1-in-8 chance to reconcile: boost mutual affinity by +0.03. Log "[HaulerA] and [HaulerB] share a drink at [Destination]" at full frequency. Creates a counterbalance to route competition.

- [x] **Seasonal work shanty** — In `ScheduleSystem.cpp`'s work song block, check `TimeManager::season`. During harvest season (`Season::Autumn`), increase the work song chance from 1-in-30 to 1-in-15 (more singing during busy harvest). During winter (`Season::Winter`), boost the affinity gain from +0.01 to +0.02 (huddling together). Log variant: "[Name] leads a harvest shanty" (autumn) or "[Name] leads a fireside song" (winter). Uses existing `TimeManager` season field.

- [x] **Work song morale lift** — In `ScheduleSystem.cpp`'s new work song block, after the song triggers, apply +0.01 to the home `Settlement::morale` (cap 1.0). Only when 4+ coworkers participate (larger group = bigger lift). Log "[Settlement] hums along" at 1-in-4 frequency after the song log. Makes work songs a tangible community benefit beyond individual affinity.

## Backlog

- [ ] **Jealousy reconciliation through teaching** — In `AgentDecisionSystem.cpp`'s mastery teaching chain block, when an expert (skill >= 0.8) teaches a novice (skill < 0.5) who has `Relations::affinity < 0.2` toward the expert (i.e. jealousy-strained), boost the novice's affinity toward the expert by +0.02 on top of the normal growth. Log "[Novice] warms to [Expert] through learning at [Settlement]" at 1-in-8 frequency. Creates a path for jealousy to resolve through knowledge-sharing.

- [ ] **Harmony-driven migration preference** — In `AgentDecisionSystem.cpp`'s migration destination scoring block, add a bonus of `+0.1 * harmony` to destination settlement scores where harmony data is available (pre-compute harmony in the settlement aggregation map). NPCs prefer socially cohesive settlements. Log "[NPC] is drawn to [Settlement]'s friendly community" at 1-in-12 frequency. Uses the same friendshipPairs / possiblePairs formula from `SimThread::WriteSnapshot`.

- [ ] **Low harmony triggers NPC complaints** — In `AgentDecisionSystem.cpp`'s idle chat block, when the home settlement's harmony (pre-computed per settlement) is < 0.15 and both chatting NPCs have `Relations::affinity < 0.3` toward each other, 1-in-10 chance to log "[NPC1] and [NPC2] grumble about tensions at [Settlement]" and apply -0.005 to `Settlement::morale` (floor 0.0). Creates a feedback loop: low harmony → morale drain → potential migration.

- [ ] **Blight solidarity** — In `RandomEventSystem.cpp`'s blight trigger (case 1), after destroying food stockpile, mirror the drought/plague solidarity pattern: scan NPC pairs at the settlement with mutual `Relations::affinity >= 0.3`, boost by +0.02 (cap 1.0). Log "[Settlement] residents share what little food remains." Smaller boost than drought/plague since blight is less severe. Completes the crisis solidarity pattern across all crisis types.

- [ ] **Crisis survivor badge on NPC tooltip** — In `SimThread::WriteSnapshot`'s NPC loop, add `bool crisisSurvivor = false` to `AgentEntry` in `RenderSnapshot.h`. Set when the NPC's home settlement has `modifierName == "Drought"` or `"Plague"`. In `HUD.cpp`'s NPC tooltip, display "[Crisis]" in orange after existing badges. Gives the player visibility into which NPCs are currently enduring hardship.

- [ ] **Post-crisis morale surge** — In `RandomEventSystem.cpp`'s modifier expiry block, right after the post-crisis community gathering, apply `+0.05` to `Settlement::morale` (cap 1.0) when a Drought or Plague ends. Log "[Settlement] breathes a sigh of relief" at 1-in-2 frequency. Simple morale recovery that complements the bond-strengthening effect already in place.

- [ ] **Crisis memory affects migration scoring** — In `AgentDecisionSystem.cpp`'s migration destination scoring block, add a new `float crisisMemory = 0.f` field to `DeprivationTimer` in `Components.h`. Set to `3.f` (game-days) when a Drought or Plague starts at the NPC's settlement (in `RandomEventSystem.cpp` drought/plague trigger blocks). In migration scoring, if `crisisMemory > 0`, apply a `-0.2` penalty to the NPC's current settlement score (making them more likely to leave). Tick down `crisisMemory` by 1 per game-day in `NeedDrainSystem.cpp`. Creates a lasting psychological impact of crisis.

- [ ] **Return letter from old friends** — In `AgentDecisionSystem.cpp`'s idle chat block, when an NPC chats with someone whose `Relations::affinity` toward a migrant (at a different settlement, tracked via `prevSettlement`) is >= 0.4, 1-in-12 chance the chatting NPC gains +0.01 affinity toward the migrant too (word of mouth). Log "[NPC] hears news of [Migrant] from [Friend]" at 1-in-8 frequency. Creates indirect social connections that maintain migrant relevance in old communities.

- [ ] **Migrant nostalgia on crisis** — In `RandomEventSystem.cpp`'s drought/plague trigger blocks, when a crisis begins, scan NPCs at the settlement whose `HomeSettlement::prevSettlement` is non-null (recent migrants). For each, if they have 2+ friends (affinity >= 0.4) at their old settlement, apply `effectiveMigrateThreshold *= 0.7f` via a new `bool nostalgicMigrant = false` flag on `DeprivationTimer`. Log "[Migrant] longs for [OldSettlement] during the crisis" at 1-in-6 frequency. Crisis makes homesick migrants more likely to return.

- [ ] **Lonely NPC seeks friendship proactively** — In `AgentDecisionSystem.cpp`'s idle chat block, after the lonely migrant check, when an NPC is flagged lonely (no local friends >= 0.3), increase their idle chat `affinityGain` from 0.02 to 0.04 for the next chat (eager to bond). Log "[NPC] eagerly befriends [Other] at [Settlement]" at 1-in-8 frequency. Creates a self-correcting mechanism: lonely NPCs form bonds faster, eventually escaping loneliness.

- [ ] **Loneliness visible on NPC tooltip** — In `SimThread::WriteSnapshot`'s NPC loop, add `bool isLonely = false` to `AgentEntry` in `RenderSnapshot.h`. Set when the NPC has `Relations::affinity` entries but none >= 0.3 at their current settlement (same check as lonely migrant). In `HUD.cpp`'s NPC tooltip, display "[Lonely]" in gray-blue after existing badges. Makes the social isolation state visible to the player.

- [ ] **Tribute tooltip badge** — In `SimThread::WriteSnapshot`'s NPC loop, add `bool inTribute = false` to `AgentEntry` in `RenderSnapshot.h`. Set when `Skills::tributeDays > 0` (NPC is in accelerated growth honouring a fallen mentor). In `HUD.cpp`'s NPC tooltip, display "[Tribute]" in soft gold after existing badges. Makes the post-mentor-death learning spurt visible to the player.

- [ ] **Apprentice seeks new mentor after tribute** — In `AgentDecisionSystem.cpp`'s elder wisdom block, when `sk.tributeDays` ticks from 1 to 0 (tribute ending), if the NPC still has no `elderMentor` (i.e. the old one died and no replacement found yet), scan elders of the same profession at the settlement. If a replacement elder exists with affinity >= 0.3 (lower than normal 0.6 threshold), adopt them as `elderMentor`. Log "[NPC] finds guidance anew with [Elder] at [Settlement]" at full frequency. Creates a narrative arc: loss → tribute → recovery.

- [ ] **Elder council road repair priority** — In `ConstructionSystem.cpp`'s autonomous road repair block, when both endpoint settlements have 2+ skilled elders (reuse `skilledElderCount`), reduce `ROAD_REPAIR_COST` by 20% (from 30g to 24g per endpoint). Log "[Road] repaired efficiently under elder guidance" at 1-in-6 frequency. Extends the elder council infrastructure theme to active repairs, not just maintenance.

- [ ] **Elder council morale stabiliser** — In `AgentDecisionSystem.cpp`'s once-per-day block (near the `s_lastSkillGrowthDay` check), when a settlement has 3+ skilled elders (`skilledElderCount >= 3`, pre-computed from the existing elder scan), apply `Settlement::morale += 0.005f` (cap 1.0). Log "[Settlement]'s elder council steadies the community" at 1-in-10 frequency. Gives settlements with many elders passive morale recovery.

- [ ] **Storytelling attendee affinity toward elder** — In `RandomEventSystem.cpp`'s elder storytelling event block, after the event triggers, each attendee also gains +0.02 affinity toward the storytelling elder (not just mutual among attendees). Log "[NPC] is captivated by [Elder]'s tales at [Settlement]" at 1-in-6 frequency per attendee. Strengthens the elder-community bond bidirectionally.

- [ ] **Repeated storytelling familiarity** — In `RandomEventSystem.cpp`'s elder storytelling event block, track a `static std::map<entt::entity, int> s_storyCount` counting how many times each elder has told stories. On 3rd+ telling, increase the morale boost from +0.02 to +0.04 and affinity gains from +0.02 to +0.03 (the elder's tales grow richer with retelling). Log "[Elder] enthralls [Settlement] with their legendary tales" on 3rd+ telling. Creates progression in the storytelling mechanic.

- [ ] **Farewell feast morale recovery** — In `AgentDecisionSystem.cpp`'s idle chat block, when two NPCs who both attended a farewell feast (check mutual affinity >= 0.4 at the same settlement after an elder departure) chat, 1-in-8 chance to log "[NPC1] and [NPC2] reminisce about [Settlement]'s farewell feast" and apply `Settlement::morale += 0.005f` (cap 1.0). Creates a slow morale recovery arc after an elder leaves.

- [ ] **Elder welcome feast on migration arrival** — In `AgentDecisionSystem.cpp`'s migration arrival block (where `Migrating` NPCs reach their destination), when an elder (age>60) arrives at a new settlement with 5+ residents, apply `Settlement::morale += 0.01f` (cap 1.0) and boost the elder's affinity toward all residents by +0.01. Log "[Settlement] welcomes [Elder] with open arms" once. Mirrors the farewell feast with a welcoming event at the destination.

- [ ] **Elder mediation reputation** — In `ScheduleSystem.cpp`'s elder mediation block (just added), after a successful mediation, boost the elder's `Reputation::score` by +0.1 (cap 1.0). Uses existing `Reputation` component via `try_get`. Log "[Elder] gains respect for keeping the peace" at 1-in-4 frequency. Connects elder conflict resolution to the reputation system.

- [ ] **Repeated mediation friendship arc** — In `ScheduleSystem.cpp`'s elder mediation block, track mediations per pair via `static std::map<std::pair<entt::entity,entt::entity>, int> s_mediationCount`. On 3rd+ mediation for the same pair, the two rivals also gain +0.02 mutual affinity (growing respect through repeated elder guidance). Log "[NPC1] and [NPC2] learn to respect each other at [Settlement]" at full frequency on 3rd+. Creates a progression from rivalry to grudging respect.

- [ ] **Grief anniversary vigil gathering** — In `AgentDecisionSystem.cpp`'s grief vigil gathering block, extend the check to also gather NPCs in anniversary grief (triggered by the grief anniversary system). When 2+ NPCs at the same settlement have `griefTimer > 0` from anniversary triggers (both have `lastGriefDay % 30 < 2`), boost mutual affinity by +0.03 instead of the normal vigil +0.02. Log "[Settlement] holds a remembrance vigil" at 1-in-3 frequency. Creates a communal anniversary event.

- [ ] **Grief fading over time** — In `AgentDecisionSystem.cpp`'s grief anniversary block, reduce the anniversary grief duration based on how many months have passed. When `daysSince / 30 >= 3`, the anniversary grief lasts only 0.5 hours instead of 1. When `daysSince / 30 >= 6`, the anniversary stops triggering entirely (grief has healed). Log "[NPC] has found peace at [Settlement]" once when the final anniversary passes. Creates a natural arc of emotional recovery.

- [ ] **Empathic comforter affinity boost** — In `AgentDecisionSystem.cpp`'s comfort-grieving block, after a successful empathic comfort (where both have `lastGriefDay >= 0`), boost mutual affinity by +0.03 between comforter and griever (stronger bond than normal comfort). No separate log needed — the empathic comfort log already fires. Uses existing `Relations::affinity`. Creates a social reward for shared grief experience.

- [ ] **Comfort chain reaction** — In `AgentDecisionSystem.cpp`'s comfort-grieving block, after a successful comfort, if the griever's `griefTimer` reaches 0 (fully comforted), the now-recovered griever gains `comfortCooldown = 0` (immediate readiness) and scans for another grieving NPC within 25 units to comfort in turn. Log "[Griever] pays it forward, comforting [Other] at [Settlement]" at 1-in-4 frequency. Creates a cascading comfort dynamic.

- [ ] **Vigil strengthens elder wisdom bonds** — In `AgentDecisionSystem.cpp`'s grief vigil gathering block, after the morale boost, if any participant has `Skills::wisdomGriefDays > 0` (mourning a specific wise elder), all other vigil participants gain +0.01 affinity toward any elder (age>60) at the settlement. Log "[Settlement]'s vigil honours its elders" at 1-in-4 frequency. Connects the vigil system to the elder wisdom narrative.

- [ ] **Seasonal vigil intensity** — In `AgentDecisionSystem.cpp`'s grief vigil gathering block, check `tm.CurrentSeason()`. During `Season::Winter`, increase the vigil affinity boost from +0.02 to +0.03 and morale boost from +0.03 to +0.05 (longer nights draw closer bonds). Log "[Settlement] huddles together through winter grief" at 1-in-3 frequency instead of the normal vigil log. Uses existing `TimeManager::CurrentSeason()`.

- [ ] **Grief count on settlement tooltip** — In `SimThread::WriteSnapshot`'s settlement loop, add `int griefCount = 0` to `SettlementEntry` in `RenderSnapshot.h`. Set from the existing `SettlAgg::griefCount`. In `HUD.cpp`'s settlement tooltip, after the vigil badge, show "Grieving: N" as a new line when `griefCount > 0`. Gives the player a numeric read on how widespread grief is.

- [ ] **Vigil ring colour** — In `RenderSystem.cpp`'s settlement ring drawing, when `SettlementEntry::vigil == true`, blend a subtle purple tint into the ring colour (lerp toward `{100, 50, 150, 255}` by 0.3). No new fields needed — uses existing `vigil` bool. Creates a visual indicator of communal grief visible from the map view without hovering.

- [ ] **Work buddy grief vigil priority** — In `AgentDecisionSystem.cpp`'s grief vigil gathering block, when a work buddy pair (`Relations::workBestFriend`) are both in the vigil (both have `griefTimer > 0` at the same settlement), boost their mutual affinity by +0.04 instead of the normal vigil +0.02. Log "[NPC1] and [NPC2] lean on each other through the vigil at [Settlement]" at 1-in-4 frequency. Adds work buddy depth to the vigil system.

- [ ] **Work buddy comfort badge on NPC tooltip** — In `SimThread::WriteSnapshot`'s NPC loop, add `bool workBuddyNearby = false` to `AgentEntry` in `RenderSnapshot.h`. Set when the NPC's `Relations::workBestFriend` is at the same settlement and within 30 units (check via `Position`). In `HUD.cpp`'s NPC tooltip, display "[Buddy]" in warm yellow after existing badges. Makes the work buddy relationship visible to the player.

- [ ] **Work buddy co-migration farewell** — In `AgentDecisionSystem.cpp`'s work buddy co-migration block, after the buddy starts migrating, apply -0.005 to `Settlement::morale` (floor 0.0) at the origin settlement (losing two workers at once). Log "[Settlement] feels the loss as [Buddy] and [Migrant] leave together" at 1-in-3 frequency. Creates a visible community impact when workplace pairs depart.

- [ ] **Work buddy reunion at destination** — In `AgentDecisionSystem.cpp`'s migration arrival block (where `Migrating` NPCs reach their destination), when the arriving NPC's `Relations::workBestFriend` is already at the destination settlement, boost mutual affinity by +0.05 and apply `Settlement::morale += 0.005f` (cap 1.0). Log "[NPC] reunites with work buddy [Buddy] at [Settlement]" at full frequency. Creates a satisfying narrative payoff when separated work buddies find each other again.

- [ ] **Profession diversity tooltip indicator** — In `SimThread::WriteSnapshot`'s settlement loop, check if all 3 profession types are present among homed NPCs (reuse or mirror the bitmask from `ProductionSystem.cpp`). Add `bool diverse = false` to `SettlementEntry` in `RenderSnapshot.h`. Display "[Diverse]" tag in gold after settlement name in `HUD.cpp`'s settlement tooltip when true. Makes the diversity bonus visible to the player.

- [ ] **Monoculture warning** — In `ProductionSystem.cpp`'s diversity check, when a settlement has 3+ workers but `profDiversity` bitmask has only 1 bit set (all workers same profession), log "[Settlement] lacks workforce diversity." once per game-day at 1-in-10 frequency. Counterpart to the diversity bonus message — warns player about overspecialised settlements.

- [ ] **Rivalry reconciliation** — In `AgentDecisionSystem.cpp`'s evening chat block, when two chatting NPCs have `Relations::affinity` < 0.2 (rivals from workplace competition) and both have `Needs` contentment > 0.7, 1-in-15 chance per chat to increase affinity by +0.05 and log "[Name] and [Name] make amends at [Settlement]." at 1-in-3 frequency. Happy NPCs are more forgiving. Uses existing `Relations`, `Needs`, proximity scan.

- [ ] **Rivalry production penalty** — In `ProductionSystem.cpp`'s worker contribution block, after the contentment factor, check if this worker has any coworker at the same settlement with `Relations::affinity` < 0.1. If so, apply `workerContrib *= 0.9f`. Log "[Name] is distracted by workplace tensions." once per game-day per NPC at 1-in-10 frequency. Makes rivalry have gameplay consequences beyond just the social layer.

- [ ] **Crisis aid between allied settlements** — In `RandomEventSystem.cpp`, after a drought or plague hits a settlement, check allied settlements (relations > 0.5). The closest ally with treasury > 100g donates 30g to the afflicted settlement's treasury. Log "[Ally] sends aid to [Settlement] during [crisis]." Gold flows treasury-to-treasury. Boosts relations by +0.05 for both sides.

- [ ] **Post-crisis morale recovery event** — In `RandomEventSystem.cpp`'s modifier tick-down block (where `modifierDuration <= 0`), when a plague or drought ends, give the settlement a +0.05 morale bonus and log "[Settlement] celebrates the end of [crisis]." at 1-in-2 frequency. Settlements that survive hardship get a small morale bounce.

- [ ] **Satisfaction-based work ethic** — In `ProductionSystem.cpp`, after the existing morale modifier, add `workerContrib *= (0.8f + 0.4f * lastSatisfaction)` using `DeprivationTimer::lastSatisfaction` from the worker entity. Satisfied workers produce 20% more, unsatisfied workers produce 20% less. Read via `registry.try_get<DeprivationTimer>`.

- [ ] **Wealth milestone tiers** — In `AgentDecisionSystem.cpp`'s wealthy celebration block, extend to track multiple thresholds (500g, 1000g, 2000g) using an `int wealthTier = 0` on `DeprivationTimer` (replace bool). Log different messages: "prosperous" at 500, "wealthy" at 1000, "a merchant prince" at 2000. Each tier fires once.

- [ ] **Hauler loyalty log** — In `TransportSystem.cpp`'s `consecutiveRouteCount` tracking (around line 677), when a hauler reaches exactly `consecutiveRouteCount == 10`, log "[Name] has become a regular on the [lastRoute] route." at full frequency. At `consecutiveRouteCount == 20`, log "[Name] is a veteran of the [lastRoute] route." Uses existing `Name`, `Hauler::lastRoute`.

- [ ] **Hauler route disruption event** — In `TransportSystem.cpp`'s idle evaluation, when a hauler with `consecutiveRouteCount >= 5` picks a *different* route (loyalty broken), log "[Name] abandons the [lastRoute] route for [newRoute]." at 1-in-3 frequency. Detect by comparing pre-trip `lastRoute` with the newly chosen route label. Creates narrative around hauler behaviour shifts.

- [ ] **NPC work gossip about neighbours** — In `AgentDecisionSystem.cpp`'s evening chat block, when two chatting NPCs both have `Profession` components, 1-in-4 chance one comments on the other's profession: log "[Name] asks [Name] about life as a [profession] at [Settlement]." If the listener has skill < 0.3 in that profession's matching skill, 1-in-3 chance to boost it by +0.01 (knowledge transfer through conversation). Uses existing `Skills`, `Profession`, `Relations`.

- [ ] **NPC grudge from failed trade** — In `ConsumptionSystem.cpp`'s emergency market purchase block, when an NPC tries to buy from a settlement stockpile but it's empty (purchase fails), find the wealthiest NPC at that settlement via `HomeSettlement` match and `Money::balance`. Reduce `Relations::affinity` toward them by 0.03 (floor 0.0). Log "[Name] resents [Wealthy] for hoarding at [Settlement]." at 1-in-5 frequency. Scarcity breeds social tension.

- [ ] **Generalist mentoring bonus** — In `AgentDecisionSystem.cpp`'s mentor-apprentice block, when the elder mentor has all three skills ≥ 0.4 (generalist), grant the apprentice child an additional +0.001 to *all* three skills instead of just the matching profession skill. Log "[Elder] teaches [Child] a breadth of trades at [Settlement]." at 1-in-5 frequency. Generalist elders produce well-rounded apprentices.

- [ ] **Specialisation tooltip color coding** — In `HUD.cpp`'s milestone line draw call (around line 888), color the specialisation text based on type: Master titles in `Fade(GOLD, 0.9f)` (already done), Journeyman in `Fade(GOLD, 0.6f)` (already done), Generalist in `Fade(SKYBLUE, 0.8f)`, Veteran in `Fade(ORANGE, 0.8f)`. Currently all non-master titles share the same color. Differentiate to make titles more visually meaningful.

- [ ] **Shared hardship friendship boost** — In `AgentDecisionSystem.cpp`'s need satisfaction block, when two NPCs at the same settlement both have any need < 0.3 (both struggling), boost `Relations::affinity` between them by +0.02 per game-day (capped at 1.0). Log "[Name] and [Name] bond through hardship at [Settlement]." at 1-in-10 frequency. Adversity creates friendships. Gate with `entity % 4 == s_frameCounter % 4` stagger.

- [ ] **Wealthy NPC philanthropy** — In `AgentDecisionSystem.cpp`, NPCs with `wealthCelebrated == true` and `balance > 600g` contribute 10g to their settlement's `treasury` once per 72 game-hours (reuse `charityTimer`). Log "[Name] donates to [Settlement]'s treasury." Gold flows `Money::balance` → `Settlement::treasury`. Follows Gold Flow Rule.

- [ ] **Overwork warning log** — In `ScheduleSystem.cpp`, when `consecutiveWorkHours` first reaches 10 for an NPC, log "[Name] is overworked at [Settlement]." at 1-in-3 frequency. Use a `bool overworkWarned` flag on `Schedule` (add to `Components.h`, reset alongside `consecutiveWorkHours`). Notifies the player that an NPC is hitting the penalty threshold.

- [ ] **Overwork-driven need drain** — In `NeedDrainSystem.cpp`'s energy drain block, when `Schedule::consecutiveWorkHours >= 10`, increase energy drain by 50% (`energyDrain *= 1.5f`). Overworked NPCs burn through energy faster, creating a natural pressure to stop working. Uses existing `Schedule` component via `registry.try_get<Schedule>`.

- [ ] **Master count change event log** — In `SimThread::WriteSnapshot` or a new lightweight system, track previous master count per settlement in a `static std::unordered_map<entt::entity, int>`. When master count increases, log "[Settlement] now has N masters." When it decreases, log "[Settlement] lost a master (N remain)." Fires once per change. Makes the teaching ecosystem dynamics visible without hovering.

- [ ] **Settlement workforce breakdown in tooltip** — In `SimThread::WriteSnapshot`'s `settlAgg`, count workers by profession type (Farmer/WaterCarrier/Lumberjack). Store as `int farmers, waterCarriers, lumberjacks` on `SettlementEntry` in `RenderSnapshot.h`. Display in `HUD.cpp`'s settlement tooltip as "Workers: N farmers, N water, N wood" after the masters line. Gives quick profession diversity visibility.

- [ ] **Master attraction migration factor** — In `AgentDecisionSystem.cpp`'s migration destination scoring, when evaluating a target settlement, check if it has any masters (via `settlAgg.masterCount > 0` or a per-settlement scan). If so, apply a +20% migration attractiveness bonus. NPCs prefer to move to settlements with masters, creating a self-reinforcing growth cycle. Log "[Name] is drawn to [Settlement]'s master [skill]." at 1-in-5 frequency.

- [ ] **Master departure warning** — In `AgentDecisionSystem.cpp`'s migration departure block (where the NPC leaves a settlement), when a departing NPC has `masterSettled == true`, log "[Settlement] loses master [skill] [Name] to migration." at full frequency. Paired with the homecoming log to create a complete master movement narrative.

- [ ] **Migration welcome log** — In `AgentDecisionSystem.cpp`'s migration arrival block (when NPC arrives at destination and `HomeSettlement` is reassigned), scan residents at the new settlement for the NPC with highest `Relations::affinity` toward the newcomer. If affinity ≥ 0.3, log "[Resident] welcomes [Name] to [Settlement]." at 1-in-3 frequency. Complements the farewell log.

- [ ] **Friend mourning on death** — In `DeathSystem.cpp`, when an NPC dies, scan their `Relations::affinity` map for friends (affinity ≥ 0.5) at the same settlement. The top friend by affinity gets -0.03 morale on their home settlement and logs "[Friend] mourns the loss of [Name] at [Settlement]." at 1-in-2 frequency. Death has social consequences.

- [ ] **Master arrival morale boost** — In `AgentDecisionSystem.cpp`'s migration arrival block, right after the master homecoming log, when a `masterSettled` NPC arrives at a new settlement, apply `+0.03` morale to the destination `Settlement`. Log "[Settlement] celebrates the arrival of a master." at 1-in-2 frequency. Mirrors the master loss morale penalty — settlements gain morale when they receive masters.

- [ ] **Morale cascade from master loss** — In `AgentDecisionSystem.cpp`'s migration departure block, after the master loss morale penalty, if the departing master's home settlement now has `morale < 0.3` (tipped into unrest by the loss), boost `stockpileEmpty` by +1.0 for all NPCs homed there (via an inline scan of `HomeSettlement`). A master leaving can trigger a chain migration if morale was already fragile. Log "[Settlement] spirals into unrest after losing a master." at full frequency.

- [ ] **Inherited wealth gratitude** — In `AgentDecisionSystem.cpp`'s gossip/social block, when an NPC's `Money::balance` increased by ≥ 5g from friend inheritance (track via a new `float inheritedGold = 0.f` on `DeprivationTimer`), the heir boosts affinity toward the deceased's family members (`FamilyTag::name` match) by +0.1 and logs "[Heir] honours the memory of [Deceased]'s family." once. Set `inheritedGold = 0` after processing. Adds emotional consequence to inheritance.

- [ ] **Deathbed farewell log** — In `DeathSystem.cpp`, when an NPC with `Age::days >= Age::maxDays * 0.95f` (old-age death) has a best friend (affinity ≥ 0.5) at the same settlement, log "[Deceased] bid farewell to [Friend] before passing." at 1-in-2 frequency. Uses `entitiesBySettlement` for lookup. Adds emotional depth to natural death events.

- [ ] **Exile farewell to settlement** — In `AgentDecisionSystem.cpp`, when an NPC loses their home settlement (becomes exile), if they had any friend (affinity ≥ 0.4) at the old settlement, log "[Name] was forced to leave [Friend] behind at [Settlement]." at 1-in-3 frequency. Uses the NPC's `Relations::affinity` map. Makes exile a social event, not just an economic one.

- [ ] **Resettlement welcome from old friends** — In `AgentDecisionSystem.cpp`'s wanderer resettlement block, after a wanderer successfully resettles at a settlement with friends, boost affinity by +0.05 between the wanderer and each friend at the new settlement. Log "[Friend] welcomes [Wanderer] back." for the top friend at 1-in-3 frequency. Closes the loop on friend-preference resettlement.

- [ ] **Retired hauler wealth investment** — In `AgentDecisionSystem.cpp`'s idle NPC block, when an NPC has no `Hauler` component but `lifetimeTrips` tracking would require a new field: add `bool retiredHauler = false` to `DeprivationTimer` in `Components.h`, set true in `TransportSystem.cpp`'s retirement block. When `retiredHauler && balance > 300g`, 1-in-100 chance per hour to donate 50g to home `Settlement::treasury`. Log "[Name] invests in [Settlement] from hauling savings." Gold flows `Money::balance` → `Settlement::treasury`.

- [ ] **Retirement celebration log** — In `TransportSystem.cpp`'s retirement block, after the retirement log, scan NPCs at the same settlement via `HomeSettlement` match (max 5 checked). If any have `Relations::affinity >= 0.4` toward the retiree, log "[Friend] celebrates [Retiree]'s retirement at [Settlement]." at 1-in-3 frequency. Uses existing `Relations`, `Name`, `HomeSettlement`.

- [ ] **Workplace friendship milestone log** — In `ScheduleSystem.cpp`'s workplace affinity block, when cumulative workplace gain for a pair crosses 0.3 (first meaningful friendship threshold), log "[Name] and [Coworker] have become friends through working together at [Settlement]." once per pair via static set. Makes organic friendships visible.

- [ ] **Leisure socialising affinity** — In `ScheduleSystem.cpp`'s leisure wandering block (hour 18–22 evening cluster), scan for other Idle NPCs within 40u at the same settlement. Tick `Relations::affinity` by +0.001 per game-hour, capped at 0.3 from leisure alone (separate static tracker). Evening socialising builds weaker but broader social bonds than workplace proximity.

- [ ] **Cohesion bonus shown in settlement tooltip** — Add `int cohesionPairs = 0` and `float cohesionBonus = 0.f` to `SettlementEntry` in `RenderSnapshot.h`. Set in `WriteSnapshot` by counting mutual friendship pairs (same logic as `ProductionSystem`). Display "Social cohesion: N pairs (+X%)" in `RenderSystem.cpp`'s settlement panel after morale bar, in Fade(LIME, 0.6f).

- [ ] **Gift thank-you affinity boost** — In `AgentDecisionSystem.cpp`'s trade gift block, after the escalated gift (8g), boost giver's affinity toward recipient by +0.03 (on top of existing +0.05 reciprocity for recipient). Mutual generosity strengthens both sides of the relationship. Only triggers on escalated gifts (giftAmt > GIFT_AMOUNT). No new fields needed.

- [ ] **Best friend gift log variation** — In `AgentDecisionSystem.cpp`'s trade gift block log, when `giftAmt > GIFT_AMOUNT` (escalated gift), change the log message to "[Giver] gives a generous gift of 8g to [Recipient] at [Settlement]." instead of the standard gift log. Makes escalated gifts visually distinct in the event log.

- [ ] **Restless NPC settlement satisfaction decay** — In `AgentDecisionSystem.cpp`'s satisfaction update block, when `Profession::careerChanges >= 3`, apply `lastSatisfaction *= 0.9f` — restless NPCs are harder to keep satisfied even with good conditions. Makes career changers a distinct personality archetype that's harder to retain. Uses existing `DeprivationTimer::lastSatisfaction`.

- [ ] **Wanderlust trait from career changes** — In `SimThread::WriteSnapshot`'s agent data block, add a `bool restless` flag to `AgentEntry` in `RenderSnapshot.h`. Set true when `Profession::careerChanges >= 3`. Display "(restless)" in `HUD.cpp`'s NPC tooltip after the career changes line, in `Fade(ORANGE, 0.7f)`. Makes the career changer personality visible to the player.

- [ ] **Veteran production bonus** — In `ProductionSystem.cpp`'s worker contribution block, after the jack-of-all-trades check, when `Profession::careerChanges >= 2` and the worker's active profession skill ≥ 0.6, apply `workerContrib *= 1.08f`. Veterans bring cross-discipline experience. No new components — uses `registry.try_get<Profession>` and `registry.try_get<Skills>`.

- [ ] **Veteran mentoring speed bonus** — In `AgentDecisionSystem.cpp`'s mentor-apprentice block, when the elder mentor has `Profession::careerChanges >= 2` (veteran), increase apprentice skill growth from +0.003 to +0.004. Veterans are better teachers due to broader experience. Log "[Veteran Elder] shares varied experience with [Child]." at 1-in-10 frequency.

- [ ] **Gossip-driven career curiosity** — In `AgentDecisionSystem.cpp`'s idle chat gossip block (just added), when the listener has `careerChanges == 0` and contentment < 0.5, 1-in-10 chance to boost `DeprivationTimer::stockpileEmpty` by +0.5 — hearing about varied careers makes dissatisfied NPCs more likely to consider migration/change. Log "[Listener] wonders about life elsewhere." Uses existing fields.

- [ ] **Chat topic variety** — In `AgentDecisionSystem.cpp`'s idle chat log block, vary the chat message based on context: if both NPCs share the same `Profession::type`, log "[A] and [B] discuss [profession] techniques." If one is an elder (age > 60), log "[Elder] shares wisdom with [Younger]." Default remains generic chat. 1-in-10 frequency for each variant. No new fields — uses existing `Profession`, `Age`, `Name`.

- [ ] **Skill rust morale drain** — In `AgentDecisionSystem.cpp`'s skill rust notification block (where skill drops below 0.5), apply `-0.01` to home `Settlement::morale` (floored at 0.0). NPCs losing skills hurts community confidence. Mirrors the +0.02 skill recovery boost — makes skill dynamics affect settlement morale bidirectionally.

- [ ] **Settlement morale tooltip breakdown** — In `HUD.cpp`'s settlement tooltip, after the morale bar, add a line showing recent morale sources: count of skill recoveries, master arrivals/departures, and deaths in the last game-day. Store as `int moraleUpEvents, moraleDownEvents` on `SettlementEntry` in `RenderSnapshot.h`, tracked via `static std::unordered_map` in `SimThread::WriteSnapshot`. Display "Morale: +N / -N events today" in green/red.

- [ ] **Cohesion decay on death** — In `DeathSystem.cpp`, when an NPC dies, iterate their `Relations::affinity` map and remove the dead entity from each friend's affinity map. This cleans up stale entity references and naturally reduces the settlement's cohesion pair count, making death socially meaningful beyond the population number.

- [ ] **Lonely NPC greeting on arrival** — In `AgentDecisionSystem.cpp`'s migration arrival block, when a lonely NPC (no friends at new settlement) arrives, scan settlement residents and set `Relations::affinity` to 0.1 with the nearest NPC. Log "[Name] introduces themselves to [Resident] at [Settlement]." at 1-in-3. Seeds initial social connection for isolated newcomers.

- [ ] **Social network shown in F1 debug panel** — In `HUD.cpp`'s F1 overlay, after settlement breakdown, add a "Social" section showing total friendship pairs across all settlements, average affinity of existing relations, and count of lonely NPCs (zero friends at home). Read from `RenderSnapshot` — add `int totalFriendPairs`, `int lonelyNpcCount` fields to `RenderSnapshot`.

- [ ] **Begging gratitude affinity boost** — In `ConsumptionSystem.cpp`'s begging block, after the 3g transfer, boost `Relations::affinity` between beggar and helper by +0.05 (both directions, capped at 1.0). Receiving help strengthens the friendship bond. No new fields needed — modify existing begging block.

- [ ] **Repeated begging strain** — In `ConsumptionSystem.cpp`'s begging block, track a static `std::map<std::pair<entt::entity,entt::entity>, int>` counting how many times each pair has begged. After 3 begging events from the same pair, decay `Relations::affinity` by -0.02 per additional beg. Log "[Friend] is growing tired of helping [Name]." at 1-in-3 when the penalty kicks in. Friendship has limits.

- [ ] **Food crisis morale impact** — In `ConsumptionSystem.cpp`'s food crisis warning block, when a crisis is logged (≥ 3 starving), apply -0.03 morale to the settlement via `Settlement::morale`. Chronic starvation demoralises the community. Only apply once per game-day (already gated by `s_crisisLogDay`).

- [ ] **Crisis-triggered food import request** — In `ConsumptionSystem.cpp`'s food crisis block, when a crisis fires, set a `bool foodCrisis` flag on `Settlement` (add to `Components.h`). In `TransportSystem.cpp`'s `FindBestRoute`, destinations with `foodCrisis == true` get +25% route score bonus for food deliveries. Flag cleared when food stock > 20. Creates demand-pull trade response to starvation.

- [ ] **Satisfaction color on NPC dot** — In `RenderSystem.cpp`'s NPC drawing block, when satisfaction < 0.25, tint the NPC's ring color with a faint red pulse (`Fade(RED, 0.3f * sinf(elapsed * 4))`) to make deeply unsatisfied NPCs visually identifiable on the map without hovering.

- [ ] **Settlement average satisfaction in tooltip** — Add `float avgSatisfaction = 0.5f` to `SettlementEntry` in `RenderSnapshot.h`. Compute in `WriteSnapshot` by averaging `DeprivationTimer::lastSatisfaction` across all NPCs at each settlement. Display "Avg satisfaction: X%" in `RenderSystem.cpp`'s settlement panel after morale with RED/YELLOW/GREEN gradient.

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

- [ ] **NPCs discuss weather during seasonal extremes** — In `AgentDecisionSystem`'s greeting
  block, in the normal greeting case, when season is Summer or Winter (from `TimeManager`), 15%
  chance to replace greeting with "[Name] and [Other] complain about the [heat/cold]." Check
  `tm.CurrentSeason()` which is already available. Pure flavour text — NPCs react to seasons.

- [ ] **NPCs discuss food prices when hungry** — In `AgentDecisionSystem`'s greeting block, in the
  normal greeting case, when both NPCs have `needs.list[0].value < 0.5` (hunger below 50%), 25%
  chance to replace greeting with "[Name] and [Other] grumble about food prices." No gameplay
  effect — flavour text that creates ambient narrative about economic conditions.

- [ ] **Bandit surrender on low health** — In `AgentDecisionSystem`'s bandit behaviour block, when
  a bandit's `needs.list[0].value < 0.15` (starving) AND `money.balance < 1g`, instead of lurking,
  the bandit walks toward the nearest settlement at normal speed. On arrival (within 20u of
  settlement position), remove `BanditTag`, set `HomeSettlement` to that settlement, log
  "[Name] surrenders and begs for shelter at [Settlement]." Gold flow: no gold changes. Adds
  redemption arc for desperate bandits.

- [ ] **NPC shares food with starving neighbour** — In `AgentDecisionSystem`, after the comfort
  block but before thank-player, idle NPCs with `needs.list[0].value > 0.7` and `money.balance > 5`
  scan for NPCs within 25u with `needs.list[0].value < 0.2`. Helper pays 1g to home settlement
  treasury (food purchase), receiver gets `needs.list[0].value += 0.3`. Helper's `charityTimer = 300`
  (5 min cooldown). Log "[Name] shares food with [Other]." Follows Gold Flow Rule: 1g from helper
  balance to settlement treasury.

- [ ] **Bandit warning in NPC greeting** — In `AgentDecisionSystem`'s greeting block, normal
  greeting case, if either NPC has `lastHelper != entt::null` and the registry has any `BanditTag`
  entities, 15% chance to replace greeting with "[Name] warns [Other] about bandits on the roads."
  Pure flavour — NPCs reference the danger that bandits pose. No gameplay effect.

- [ ] **Goal celebration visible to neighbours** — In `AgentDecisionSystem`, when
  `goal.celebrateTimer` transitions from > 0 to <= 0 (goal just completed), scan idle NPCs within
  30u. Those with `Relations::affinity >= 0.2` get a brief `chatTimer = 0.15` (game-hours) and log
  "[Friend] congratulates [Achiever] on completing their goal." Uses existing celebrating block
  pattern from skill celebrations. No new components needed.

- [ ] **Goal progress shown in stockpile residents** — In `RenderSnapshot::StockpilePanel::AgentInfo`,
  add `std::string goalSummary`. In `SimThread::WriteSnapshot`'s stockpile residents block, populate
  from `Goal` component as "Save Gold 42%" (progress/target * 100). In `RenderSystem::DrawStockpilePanel`,
  append goal summary in dim SKYBLUE after the existing resident line. Pure display — no gameplay effect.

- [ ] **NPC congratulates goal completion** — In `AgentDecisionSystem`'s goal system section, when
  a goal completes (just before `celebrateTimer = 2.f`), scan idle NPCs within 30u via
  `registry.view<Position, AgentState>`. If any have `Relations::affinity >= 0.2` toward the
  achiever, log "[Friend] congratulates [Achiever]!" and boost mutual affinity by +0.02. Max 2
  congratulators per completion. Uses existing Relations component — no new structs.

- [ ] **Milestone halfway shown in tooltip** — Add `bool halfwayReached = false` to
  `RenderSnapshot::AgentEntry`. In `SimThread::WriteSnapshot`, set it from `Goal::halfwayLogged`.
  In `HUD::DrawHoverTooltip`, when `halfwayReached` is true and `goalDescription` is non-empty,
  append a small "(50%+)" suffix in `Fade(GOLD, 0.5f)` after the goal line. Pure display indicator.

- [ ] **SaveGold goal auto-completes on threshold** — In `EconomicMobilitySystem.cpp`'s graduation
  block, after setting the BecomeHauler goal progress, also check if the NPC has a `Goal` with
  `type == GoalType::SaveGold` and `money.balance >= goal.target`. If so, set `goal.progress =
  goal.target`. Currently SaveGold goals only update in `AgentDecisionSystem`'s goal tick — this
  ensures the log fires immediately when a graduation-eligible NPC also meets their savings goal.

- [ ] **Graduation announcement to settlement** — In `EconomicMobilitySystem.cpp`, after the
  graduation log push, scan NPCs at the same `HomeSettlement` via `registry.view<HomeSettlement,
  Name, DeprivationTimer>`. For up to 3 NPCs whose `HomeSettlement::settlement` matches the
  graduate's, set `dt.chatTimer = 0.1f` (brief chat animation) and log "[Neighbour] cheers for
  [Graduate]'s promotion." Uses existing chatTimer field — no new components needed.

- [ ] **Migration memory entry count in tooltip** — In `SimThread::WriteSnapshot`, add
  `int migrationMemoryCount = 0` to `RenderSnapshot::AgentEntry`. Set from `mm->known.size()`.
  In `HUD::DrawHoverTooltip`, when `migrationMemoryCount > 0` but `migrationMemorySummary` is
  empty (only 1 entry), show "Knows 1 settlement" in dim GRAY instead of nothing. Currently
  NPCs with exactly 1 memory entry show no migration info at all.

- [ ] **NPCs share migration knowledge on greeting** — In `AgentDecisionSystem`'s greeting block,
  after the gossip propagation section (~line where `lastHelper` is spread), if both NPCs have
  `MigrationMemory`, pick one random entry from each and call `other.Record(...)` to teach
  settlements the other doesn't know. Cap at 1 exchange per greeting. Log "[Name] tells [Other]
  about [Settlement]." Uses existing `MigrationMemory::Record` — no new structs.

- [ ] **Memory freshness shown in migration tooltip** — In `SimThread::WriteSnapshot`, when
  building `migrationMemorySummary`, append the age of each entry in days as a suffix:
  e.g. "Knows: Wellsworth (food 2g, 5d ago)" by computing `tm.day - snap.lastVisitedDay`. In
  `MigrationMemory::PriceSnapshot`, `lastVisitedDay` is now available. No new components needed —
  pure display enhancement of existing piped data.

- [ ] **Very stale memories evicted** — In `AgentDecisionSystem`'s migration arrival block, after
  recording the new settlement, iterate `mem->known` and erase any entry where
  `tm.day - snap.lastVisitedDay > 90` (3 months stale). This prevents NPCs from carrying
  ancient price knowledge that will never be relevant again. Keeps memory map bounded by
  relevance, not just by count.

- [ ] **Illness spreads between NPCs** — In `RandomEventSystem`'s per-NPC event section, when
  an NPC has `illnessTimer > 0`, scan idle NPCs within 20u via `registry.view<Position,
  DeprivationTimer, AgentState>`. If the neighbour has `illnessTimer <= 0` and a random roll
  (5% per game-hour) succeeds, set `neighbour.illnessTimer = 4.f` (shorter secondary illness)
  and `neighbour.illnessNeedIdx = dt.illnessNeedIdx`. Log "[Name] caught [Other]'s illness."
  No new components — uses existing `illnessTimer` on `DeprivationTimer`.

- [ ] **NPC avoids ill neighbours** — In `AgentDecisionSystem`'s idle wander section, when
  an NPC without `illnessTimer` sees a nearby NPC (within 15u) with `illnessTimer > 0`, set
  velocity away from the ill NPC at 0.5× speed for 1s via `panicTimer`. Log "[Name] avoids
  [Ill NPC] who looks unwell." 30s cooldown via `greetCooldown` to avoid spam. Uses existing
  `DeprivationTimer` fields — no new structs.

- [ ] **Late sleeper log** — In `ScheduleSystem.cpp`, when an NPC transitions to Sleeping
  behaviour but their position is more than 150u from their home settlement (long commute),
  log "[Name] has a long walk home from [Settlement]." 50% chance to avoid spam. Pure flavour
  log using existing data — no new components or snapshot fields.

- [ ] **NPC yawns when tired** — In `AgentDecisionSystem`'s idle block, when
  `needs.list[2].value < 0.3` (low energy) and the NPC is not sleeping, 2% per game-hour
  chance to log "[Name] yawns wearily." Gated by `greetCooldown > 0` check (reuse cooldown).
  Pure flavour — no gameplay effect, no new components.

- [ ] **Morning stretch log** — In `ScheduleSystem.cpp`, when an NPC transitions from Sleeping
  to Idle at wake-up time (the block where `state.behavior = AgentBehavior::Idle` is set on
  waking), 20% chance to log "[Name] stretches and greets the morning." No gameplay effect,
  no new components — pure flavour using existing Name and state transition.

- [ ] **Evening gathering chat boost** — In `AgentDecisionSystem`'s greeting block, during
  hours 18–22, double the greeting chance (reduce `greetCooldown` reset from current value to
  half) so NPCs who are clustered in the evening gathering actually interact more frequently.
  No new components — just scale the cooldown by 0.5 when `currentHour >= 18 && currentHour < 22`.

- [ ] **Crowded settlement morale drain** — In `ProductionSystem.cpp`, after the crowding log
  block, when `workerHeadCount[settl] >= 6` (severely crowded), apply a small morale penalty:
  `settlement.morale -= 0.005f * gameHoursDt` (capped at 0). This gives overcrowded settlements
  a subtle negative pressure that encourages migration. No gold changes — pure morale.

- [ ] **Worker arrives early log** — In `ScheduleSystem.cpp`, when an NPC transitions from
  Idle to Working at the start of their work shift, if their `needs.list[2].value > 0.9`
  (well-rested), 10% chance to log "[Name] arrives at work bright and early." Pure flavour
  using existing Name and Needs — no new components.

- [ ] **Chatting shown in NPC tooltip** — In `HUD::DrawHoverTooltip` (HUD.cpp), when
  `best->chatting` is true, show "Chatting" in faint YELLOW below the mood line. Follow the
  existing tooltip line pattern: add `showChatting` bool, increment `lineCount`, add width
  measurement, add `DrawText`. Pure display — no new snapshot fields needed beyond existing
  `chatting` bool.

- [ ] **Chat partner name in tooltip** — Add `std::string chatPartnerName` to
  `RenderSnapshot::AgentEntry`. In `SimThread::WriteSnapshot`, when `chatTimer > 0` and
  `AgentState::target != entt::null`, get the target entity's Name and set it. In
  `HUD::DrawHoverTooltip`, show "Chatting with [Name]" instead of just "Chatting". Gives
  richer social context to the player.

- [ ] **Chat builds affinity faster with friends** — In `AgentDecisionSystem`'s chat pairing
  block (where `AFFINITY_GAIN = 0.02f` is applied), check existing `Relations::affinity` before
  the gain. If affinity is already >= 0.5 (friends), double the gain to 0.04f. This creates
  a positive feedback loop — friends who chat become closer friends faster. No new components.

- [ ] **Lonely NPC seeks chat** — In `AgentDecisionSystem`'s idle block, when an NPC has
  `chatTimer == 0` and hasn't chatted for > 60 game-seconds (add `float lastChatAge = 0.f` to
  `DeprivationTimer`), increase CHAT_RADIUS from 25u to 40u for that NPC. Lonely NPCs actively
  seek out conversation partners from further away. Reset `lastChatAge` when chatTimer is set.

- [ ] **Gathering count in settlement tooltip** — In `HUD::DrawSettlementTooltip` (HUD.cpp),
  during hours 18–21, add a line "Evening gathering: N NPCs nearby" in faint SKYBLUE. Count
  agents in `snap.agents` whose `homeSettlementName` matches the hovered settlement and are
  within 40u. No new snapshot fields — reuse existing agent position data.

- [ ] **Dawn dispersal animation** — In `ScheduleSystem.cpp`, when hour transitions from
  sleeping to working (wake-up), if the NPC is within 30u of their settlement centre, add a
  brief outward velocity nudge: `vel.vx += (pos.x - homePos.x) * 0.02f` (and vy). This creates
  a visible "dispersal" as NPCs fan out from the overnight cluster. No new components — uses
  existing Position, Velocity, and HomeSettlement.

- [ ] **Founding settlers keep old family ties** — In `SimThread::ProcessInput`'s settlement
  founding block, the 4 settlers spawned from the nearest settlement currently get no `FamilyTag`.
  Before spawning, snapshot the families present at the source settlement. Assign each new settler
  a random existing `FamilyTag::name` from that snapshot (weighted by family size). This makes
  the family reunion log more meaningful and seeds the new settlement with family diversity.

- [ ] **NPC nostalgia log for birthplace** — In `AgentDecisionSystem`, when an NPC with
  `MigrationMemory` has been away from their original `HomeSettlement` (the one assigned at spawn)
  for more than 60 game-days, 5% chance per day to log "[Name] reminisces about life in
  [birthplace]." Requires adding `birthSettlement` (entt::entity) to a suitable component — either
  extend `HomeSettlement` or add a new lightweight `Birthplace` struct. Pure flavour, no behaviour
  change.

- [ ] **Settler morale boost on founding** — In `SimThread::ProcessInput`'s founding block, after
  spawning settlers and the hauler, give the new settlement an initial morale boost of +0.15
  (capped at 1.0). Also push an EventLog entry: "[Settlement] settlers are optimistic about their
  new home." This makes founding feel impactful beyond the mechanical log.

- [ ] **Adopted orphan gratitude greeting** — In `AgentDecisionSystem.cpp`'s greeting/chat pairing
  block (~line 1304), when an adopted child (has `ChildTag` + `FamilyTag` that was emplaced by
  adoption, not birth) encounters their adopter (check `lastHelper` or match by `FamilyTag::name`
  + proximity), 15% chance to log "[Child] smiles gratefully at [Adopter]." Pure flavour, uses
  existing `DeprivationTimer::chatTimer` cooldown. No new components.

- [ ] **Adoption shown in NPC tooltip** — Add `bool isAdopted = false` to `AgentEntry` in
  `RenderSnapshot.h`. In `SimThread::WriteSnapshot`, set true for NPCs with `ChildTag` + `FamilyTag`
  whose family name doesn't match any adult at their home settlement with the same `FamilyTag::name`
  who is a biological parent (simplification: just check if the child was born at a different
  settlement than current home). Show "Adopted" in faint PURPLE in `HUD.cpp` tooltip.

- [ ] **Orphan count in settlement tooltip** — In `SimThread::WriteSnapshot`'s settlement loop,
  count children with `ChildTag` and `HomeSettlement` matching this settlement who have no
  `FamilyTag`. Add `int orphanCount = 0` to `SettlementEntry` in `RenderSnapshot.h`. Display
  "Orphans: N" in faint `Fade(ORANGE, 0.6f)` in `RenderSystem::DrawStockpilePanel` after the
  child count line when orphanCount > 0.

- [ ] **Family dynasty colour in residents list** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), when the largest family has ≥3 members, tint their names with a unique
  family colour. Use a simple hash of `familyName` to pick from a palette of 6 muted colours
  (defined as a static array). Apply via `Fade(palette[hash % 6], 0.8f)` instead of the default
  `LIGHTGRAY`. No new snapshot fields needed — uses existing `familyName` on `AgentInfo`.

- [ ] **NPC remembers adopter as mentor** — In `AgentDecisionSystem.cpp`'s orphan adoption block,
  after setting the orphan's `HomeSettlement` and `FamilyTag`, also set
  `registry.get_or_emplace<DeprivationTimer>(orphan).lastHelper = adopter entity`. This enables
  the existing gratitude greeting path so adopted children will eventually thank their adopter.
  Pure wiring — no new components or systems.

- [ ] **Wealthiest family highlighted in header** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), extend the existing "Largest family" header line: if the wealthiest family
  (by `familyWealth` sum, already computed) differs from the largest, append " | Richest: Jones
  (Xg)" in `Fade(GOLD, 0.5f)`. Reuse the `familyWealth` map from the pre-computation block
  (move it up to the header section scope). No new snapshot fields.

- [ ] **Family rivalry log** — In `AgentDecisionSystem.cpp`, after the chat pairing block (~line
  1304), when two chatting NPCs belong to different families that are both ≥2 members at the same
  settlement, 5% chance to log "[Name] and [Other] argue over family matters." Uses existing
  `FamilyTag` and `HomeSettlement` checks. Pure flavour, no gameplay effect.

- [ ] **Elder retirement from work** — In `ScheduleSystem.cpp`, NPCs with `age.days > 75` and
  at least one skill ≥ 0.5 have a 10% per-day chance to stop Working and switch to permanent
  Idle (set a `retired` bool on `DeprivationTimer`). Retired NPCs still mentor (their presence
  at facility counts for elder bonus) but don't produce in `ProductionSystem`. Log "[Name]
  retired after a lifetime of work at [settlement]."

- [ ] **Age-dependent need drain rates** — In `NeedDrainSystem.cpp`, scale need drain rates
  based on age: children (age < 15) drain Hunger/Thirst at 0.7× rate (smaller appetite),
  elders (age > 65) drain Energy at 1.3× rate (tire faster). Read via `registry.try_get<Age>`.
  No new components — modifies existing drain calculation inline.

- [ ] **Prime-age worker indicator in tooltip** — In `SimThread::WriteSnapshot`'s agent loop,
  add `bool primeAge = false` to `AgentEntry` in `RenderSnapshot.h`. Set true when age is
  25–55. In `HUD.cpp`'s `DrawHoverTooltip`, show "Prime years" in faint `Fade(GREEN, 0.5f)`
  after the age line when `primeAge` is true. Pure display.

- [ ] **Wisdom recipient gratitude** — In `RandomEventSystem.cpp`'s elder wisdom transfer block,
  after boosting the target's skill, set `registry.get_or_emplace<DeprivationTimer>(target).lastHelper
  = e` (the elder entity). This wires into the existing gratitude greeting path so wisdom
  recipients will thank their elder. Pure wiring — no new components.

- [ ] **Elder storytelling during evening gather** — In `AgentDecisionSystem.cpp`'s chat pairing
  block (~line 1304), when one chat partner is an elder (age > 65) and the other is younger,
  10% chance to log "[Elder] tells [Younger] a story of the old days at [settlement]." Uses
  existing `Age` component and `chatTimer`. Pure flavour, no gameplay effect.

- [ ] **Friendship strength indicator colour** — In `HUD.cpp`'s friend tooltip line, vary the
  colour by affinity strength: `Fade(LIME, 0.75f)` for ≥ 0.7, `Fade(GREEN, 0.6f)` for ≥ 0.5,
  and label "Close friend" vs "Friend" accordingly. No new snapshot fields — uses existing
  `bestFriendAffinity` on `AgentEntry`.

- [ ] **Friends migrate together** — In `AgentDecisionSystem.cpp`'s `FindMigrationTarget`, when
  an NPC decides to migrate, check if they have a friend (affinity ≥ 0.6) at the same settlement
  who is also Idle. If so, set the friend's migration target to the same destination and log
  "[Name] convinced [Friend] to migrate to [settlement] together." Uses existing `Relations`
  and `AgentState`. No new components.

- [ ] **Friend grief sets griefTimer** — In `DeathSystem.cpp`'s friend grief block, also set
  `oTmr.griefTimer = std::max(oTmr.griefTimer, 2.f)` on grieving friends (2 game-hours, shorter
  than family grief's 4h). This connects friend grief to the existing grief system — grieving
  friends will have reduced production and visible grief in tooltip. Pure addition to existing
  block; no new components.

- [ ] **Affinity decay for distant friends** — In `AgentDecisionSystem.cpp`, during the chat
  pairing block or a new per-NPC pass, decay `Relations::affinity` by 0.001 per game-hour for
  NPCs who are at different settlements than their friend. Friends who stay together maintain
  bonds; separated friends slowly drift apart. Prevents unbounded affinity accumulation across
  the whole map. No new components — modifies existing `Relations::affinity` map inline.

- [ ] **Birth celebration gathers friends** — In `BirthSystem.cpp`, after the friendship bonus
  block, set `DeprivationTimer::skillCelebrateTimer = 0.5f` on celebrating friends. This reuses
  the existing celebration mechanic to make friends briefly pause and gather. No new components.

- [ ] **Godparent assignment** — In `BirthSystem.cpp`, after the friendship celebration block,
  if the parent has a friend with affinity ≥ 0.7 at the same settlement, assign that friend's
  entity as `lastHelper` on the newborn's `DeprivationTimer` (godparent link). This wires into
  the existing gratitude greeting so the child will later thank their godparent. Log "[Friend]
  becomes godparent to [Child]." No new components — reuses existing `lastHelper` field.

- [ ] **Strike leader emerges** — In `RandomEventSystem.cpp`'s strike block, after setting
  `strikeDuration` on all NPCs, pick the NPC with the lowest contentment as the "strike leader".
  Log "[Name] leads the workers' strike at [Settlement]." Set their `DeprivationTimer::chatTimer
  = 2.f` so they visually gather. No new components — uses existing contentment calculation
  from `Needs` and `chatTimer`.

- [ ] **Post-strike morale memory** — In `ScheduleSystem.cpp`'s strike-end block, after logging,
  boost `Relations::affinity` by +0.05 between all NPCs who were on strike at the same settlement
  (shared hardship builds bonds). Cap affinity at 1.0. Iterate via `HomeSettlement` matching.
  No new components — modifies existing `Relations::affinity` map.

- [ ] **Migration farewell log** — In `AgentDecisionSystem.cpp`, when an NPC begins migrating
  (behavior set to `Migrating`, ~line 675), if they have any friends (Relations::affinity ≥ 0.5)
  at the settlement they're leaving, 30% chance to log "[Name] bids farewell to [Friend] before
  leaving [Settlement]." Pick the highest-affinity friend. Pure flavour, no gameplay effect.

- [ ] **Morale shown in settlement tooltip** — In `RenderSystem::DrawStockpilePanel`
  (RenderSystem.cpp), after the morale bar, when morale < 0.3 add a red text line
  "Unrest — NPCs may leave" in `Fade(RED, 0.7f)`. When morale > 0.8, show "Thriving" in
  `Fade(GREEN, 0.6f)`. No new snapshot fields — uses existing `panel.morale`.

- [ ] **Hauler avoids rival routes** — In `TransportSystem.cpp`'s route selection (where haulers
  pick their target settlement), add a -30% attractiveness penalty when the destination has
  `relations[homeSettlement] < -0.5` with the hauler's home. Read via `Settlement::relations`.
  This makes haulers naturally avoid hostile trade routes. No new components.

- [ ] **Rivalry escalation log** — In `TransportSystem.cpp`, when a hauler delivers to a rival
  settlement (relations < -0.5) and pays the tariff, 20% chance to log "[Settlement A] tensions
  rise with [Settlement B] after taxed delivery." and worsen relations by -0.02. Creates a
  visible rivalry spiral. Uses existing `Settlement::relations` map.

- [ ] **Migration caravan arrival log** — In `AgentDecisionSystem.cpp`'s migration arrival block, when an NPC arrives at their destination and was part of a co-migration group (2-3 NPCs arriving at same settlement within a short window), log "[Name] and companions arrive at [Settlement] and settle in." Track recent arrivals per settlement via `static std::map<entt::entity, std::vector<std::pair<entt::entity, int>>>` keyed by settlement, cleared each game-day. Fires when 2+ arrive same day.

- [ ] **AD:MainLoop inner profiling** — Sub-profiling shows AD:MainLoop is ~90% of AgentDecision time. Add finer-grained timing within the main per-NPC loop in `AgentDecisionSystem.cpp`: split into Sleeping/Panic/Flee/Visit/Celebrate (early exits), Working, Migration (arrival + trigger + co-migration), Satisfying, Gratitude, and IdleDecision. Use a second `SubProfile` array or extend the existing one. This pinpoints whether migration scoring, co-migration friend scans, or idle seeking dominates.

- [ ] **NPC mood contagion spreading** — In `AgentDecisionSystem.cpp`'s main loop idle section, when an NPC with satisfaction > 0.7 is within 30u of an NPC with satisfaction < 0.3, the happier NPC has a 1-in-10 chance per tick to boost the sad NPC's satisfaction by +0.02 (via `DeprivationTimer::lastSatisfaction`, capped at 0.5). Log "[Happy] cheers up [Sad] at [Settlement]." at 1-in-5 frequency. Uses `moodContagionCooldown` on the recipient (24h). Creates emotional cascading.

- [ ] **Friend reunion celebration** — In `AgentDecisionSystem.cpp`, after an NPC migrates and arrives at a new settlement, scan `Relations::affinity` for friends (≥ 0.5) already living there (same `HomeSettlement`). If found, log "[Name] reunites with [Friend] at [Settlement]!" at 1-in-2 frequency and boost both affinities by +0.05 (capped at 1.0). Creates emotional payoff for the loneliness-driven migration system.

- [ ] **Caravan safety bonus** — In `TransportSystem.cpp`, when multiple haulers are travelling the same road segment simultaneously (within 40u of each other, same route), reduce their banditry/loss risk by 50%. Check proximity to other haulers in the `GoingToDeposit` state each tick. Log "[Hauler1] and [Hauler2] travel together for safety on [Road]." at 1-in-5 frequency. No new components needed.

- [ ] **Homesick return welcome-back log** — In `AgentDecisionSystem.cpp`'s migration arrival block, detect when the NPC is returning to a settlement they previously lived at (compare `home.settlement` with the arriving `state.target` against stored `prevSettlement`). Log "[Name] returns home to [Settlement] after time away." at 1-in-2 frequency. Distinct from normal arrival log, creating a narrative of homecoming.

- [ ] **Nostalgia affinity decay prevention** — In `AgentDecisionSystem.cpp`, NPCs who have `prevSettlement != entt::null` and friends (Relations::affinity ≥ 0.4) still at that settlement decay affinity 50% slower than normal. Check `HomeSettlement::prevSettlement` against friend's `HomeSettlement::settlement` in the affinity decay loop. Keeps old friendships alive longer for homesick NPCs.

- [ ] **Gossip system spatial partitioning** — `AD:Gossip` scans all NPC pairs within 30u, which is O(n²). In `AgentDecisionSystem.cpp`'s gossip section, build a per-settlement resident position list once, then only check pairs within the same or adjacent settlements. Use the `s_entitySettlement` cache to bucket NPCs by settlement, eliminating cross-settlement distance checks.

- [ ] **Settlement entity cache for systems** — Multiple systems (`ConsumptionSystem`, `ProductionSystem`, `PriceSystem`) independently iterate `registry.view<Settlement>()` each tick. Add a `std::vector<entt::entity> settlementEntities` member to `SimThread`, rebuilt once per tick before system calls, and pass it to system `Update()` methods. Avoids redundant view construction across 3+ systems.

- [ ] **WriteSnapshot friendship pairs via pre-computed cache** — The friendship pair counting in `SimThread::WriteSnapshot()` (per-settlement O(n²) mutual affinity scan) was not covered by the `settlAgg` optimisation. Move it into the pre-compute pass: for each NPC with `Relations`, check if any friend at the same settlement has mutual affinity ≥ 0.5. Count pairs per settlement in `settlAgg.friendPairs`. Replaces the nested resident loop in the settlement section.

- [ ] **NPC daily routine variety** — In `ScheduleSystem.cpp`, NPCs currently have fixed work/sleep/idle blocks. Add a `restDay` counter on `Schedule` (increments each game-day, resets at 7). On day 7, the NPC stays in `Idle` state all day instead of working. Log "[Name] takes a rest day at [Settlement]." at 1-in-5 frequency. Creates weekly rhythm variation without new components beyond one int field.

- [ ] **ScheduleSystem child-follow caching** — In `ScheduleSystem.cpp`'s leisure wandering block (line ~514), each child scans all adults at the same settlement to find the nearest to follow. Cache the follow target per child, only re-evaluate when `hourChanged` is true or the target becomes invalid. Eliminates per-tick O(children × adults) scan during leisure hours.

- [ ] **Settlement event memory** — Add `std::vector<std::pair<int, std::string>> eventHistory` to `Settlement` in `Components.h` (max 5 entries, oldest dropped). In `RandomEventSystem.cpp`, push `{day, modifierName}` when a new event starts. Display in the stockpile panel tooltip in `HUD.cpp` as "Recent events: Plague (day 12), Festival (day 15)". Gives settlements visible history.

- [ ] **NPC workplace camaraderie** — In `AgentDecisionSystem.cpp`'s social block, when two NPCs are both in `AgentBehavior::Working` state at the same `ProductionFacility` (same `state.target`), boost `Relations::affinity` by +0.01 per game-day (capped at 1.0). Log "[Name] and [Name] bond over work at [Settlement]." at 1-in-10 frequency. Gate with `entity % 4 == s_frameCounter % 4`. Uses existing `AgentState::target`, `Relations`, `HomeSettlement`.

- [ ] **NPC neighbourhood memory** — Add `entt::entity lastNeighbour = entt::null` to `DeprivationTimer` in `Components.h`. In `AgentDecisionSystem.cpp`'s evening chat block, store the chat partner as `lastNeighbour`. On the next chat, if the partner is the same as `lastNeighbour`, boost affinity by an extra +0.01 (repeated-contact familiarity). Log "[Name] and [Name] are becoming regular companions at [Settlement]." at 1-in-8 frequency when this repeat bonus fires.

- [ ] **Benchmark regression detection** — In `benchmark.sh`, after appending to `benchmark_history.csv`, compare the new `avg_steps_s` against the previous row's value (if it exists). If throughput dropped by more than 20%, print a warning: "WARNING: steps/s dropped from X to Y (Z% decrease)". Helps catch performance regressions early. Pure shell — parse last two lines of the CSV.

- [ ] **Benchmark population stability check** — In `benchmark.sh`, after parsing the report, check if `total_deaths` exceeds 50% of `max_pop` and print "WARNING: high death rate — sim may be unhealthy" if true. Also warn if `final_day` is 0 (sim didn't advance). Catches sim health issues during long stability runs without needing manual report inspection.

- [ ] **WriteSnapshot friendship pairs via settlAgg** — The friendship pair counting in `SimThread::WriteSnapshot()` (lines ~1912-1930) does a per-settlement O(n²) mutual affinity scan. Move it into the `settlAgg` pre-compute pass: for each NPC with `Relations`, check mutual affinity ≥ 0.5 pairs at the same settlement. Add `int friendPairs` and a `std::vector<entt::entity> residents` to `SettlAgg`. Replaces nested resident loop.

- [ ] **NPC co-worker recognition** — In `AgentDecisionSystem.cpp`'s social block, when an NPC finishes a work shift (`Schedule::state` transitions from Working to Idle) and another NPC at the same settlement also just finished, boost mutual `Relations::affinity` by +0.005. Log "[Name] and [Name] walk home together from [Facility] at [Settlement]." at 1-in-8 frequency. Gate with `entity % 4 == s_frameCounter % 4`. Uses existing `Schedule`, `AgentState`, `HomeSettlement`, `Relations`.

- [ ] **Loyalty milestone title in tooltip** — In `SimThread::WriteSnapshot`'s agent specialisation logic, when a loyal NPC (prevType == type or prevType == Idle) has active skill ≥ 0.7, prepend "Dedicated " to their specialisation string (e.g. "Dedicated Journeyman Farmer"). Uses existing `Profession::prevType` check and `AgentEntry::specialisation` field in `RenderSnapshot.h`. No new components needed.

- [ ] **Loyal NPC homesickness resistance** — In `AgentDecisionSystem.cpp`'s migration trigger block, loyal NPCs (prevType == type or Idle) with active skill ≥ 0.7 get an additional migration threshold boost: `effectiveMigrateThreshold *= 1.3f` (stacks with master retention 1.5x). Rewards long-term career commitment with stronger settlement attachment. Uses existing `Profession`, `Skills`, `DeprivationTimer` components.

- [ ] **Career change skill transfer log** — In `ScheduleSystem.cpp`'s profession change block, after the new skill transfer code, log "[Name] applies their [old profession] experience to [new profession] at [Settlement]." at 1-in-3 frequency when the +0.05 bonus is actually applied (oldSkill >= 0.5). Uses the existing `s_profRng` and `EventLog`. Makes the skill transfer mechanic visible to players.

- [ ] **Multi-career veteran bonus** — In `AgentDecisionSystem.cpp`'s skill growth block, add `int careerChanges` to `Profession` in `Components.h`. Increment in `ScheduleSystem.cpp` on each profession change. NPCs with `careerChanges >= 3` get a flat +0.0003 growth bonus to ALL skills (versatility through breadth of experience). Log "[Name] draws on diverse experience at [Settlement]." once when crossing 3 changes, at 1-in-5 frequency.

- [ ] **Settlement skill specialisation indicator** — In `SimThread::WriteSnapshot`'s settlement section, after computing `avgFarming/avgWater/avgWood` from `settlAgg`, determine if one skill is dominant (>= 1.5x the average of the other two). If so, set a new `std::string skillSpeciality` field on `SettlementEntry` (e.g. "Farming Hub", "Forestry Hub"). Display in `HUD.cpp`'s tooltip after the skills line in a distinct colour. Highlights natural skill clustering.

- [ ] **Settlement skill growth trend** — Add `float prevAvgFarming, prevAvgWater, prevAvgWood` to `SettlementEntry` in `RenderSnapshot.h`. In `SimThread::WriteSnapshot`, store the current averages and compare with a cached previous snapshot (use a `static std::map<entt::entity, std::array<float,3>>` keyed by settlement entity, updated once per game-day). Display arrows in tooltip: "Skills: Farm 45% ↑ Water 30% ↓ Wood 20% →" using `settlAgg` data. Shows whether settlement workforce is improving.

- [ ] **Mentored apprentice graduation bonus** — In `BirthSystem.cpp` or `DeathSystem.cpp`'s age graduation block (where `ChildTag` is removed at age 15), check if the child's matching profession skill is >= 0.3. If so, log "[Name] graduates as a promising [profession] at [Settlement]." at 1-in-3 frequency. Children who were mentored will naturally have higher skills at graduation, making this event more common for settlements with active mentorship.

- [ ] **Elder retirement wisdom** — In `AgentDecisionSystem.cpp`'s death block or a new once-per-day check, when an elder (age > 60) with any skill >= 0.9 dies, boost ALL children at the same settlement's matching skill by +0.02 (legacy knowledge). Log "[Elder]'s wisdom lives on in the children of [Settlement]." at full frequency. Uses existing `Age`, `Skills`, `ChildTag`, `HomeSettlement` components. One-time effect on death.

- [ ] **Gift reciprocation cycle** — In `AgentDecisionSystem.cpp`'s trade gift block, add `entt::entity lastGiftFrom = entt::null` to `DeprivationTimer` in `Components.h`. Set it when receiving a gift. On the next gift cycle, if the NPC's best friend matches `lastGiftFrom`, skip the cooldown check (gift back immediately). Creates back-and-forth gift exchanges between close friends. Log "[Name] returns [Friend]'s generosity at [Settlement]." at 1-in-4 frequency.

- [ ] **Friendship decay notification** — In `AgentDecisionSystem.cpp`'s relations decay block (where distance-based affinity decreases), when a pair's affinity drops below 0.3 from above, log "[Name] and [Name] are drifting apart." at 1-in-5 frequency. Uses existing `Relations::affinity` comparison. Shows the social cost of migration — friends who moved away lose touch over time.

- [ ] **Work song contagion across professions** — In `ScheduleSystem.cpp`'s work song block, after the morale lift, scan NPCs of OTHER professions working within 2× WORK_ARRIVE radius at the same settlement. If 2+ other-profession workers are nearby, 1-in-6 chance they start their own work song (boost mutual affinity +0.005 among them). Log "[Name] picks up the tune from the [Profession]s at [Settlement]" at 1-in-6 frequency. Creates cross-profession social bonding triggered by the original song.

- [ ] **Shared meal affinity boost** — In `ConsumptionSystem.cpp`'s buy-from-stockpile block, when 2+ NPCs at the same settlement purchase food in the same sim step, treat it as a shared meal: boost mutual affinity by +0.005 among all buyers (cap 1.0). Use a `static std::vector<std::pair<entt::entity, entt::entity>> s_mealBuyers` collected per settlement per step. Log "[Name] and [Name] share a meal at [Settlement]" at 1-in-12 frequency. Adds a passive social bonding mechanic tied to basic need fulfilment.

- [ ] **Night watch camaraderie** — In `ScheduleSystem.cpp`'s sleep transition block, when an NPC's schedule says Sleep but they have `Needs::energy > 0.7` (not tired), 1-in-20 chance they stay awake as a night watch. Find other night-watch NPCs at same settlement (same condition triggered that step); if 2+ are awake, boost mutual affinity +0.01. Log "[Name] and [Name] keep watch together at [Settlement]" at 1-in-6 frequency. Creates a rare late-night bonding event between energetic NPCs.

- [ ] **Spring festival work song** — In `ScheduleSystem.cpp`'s work song block, during `Season::Spring`, when a work song triggers with 5+ coworkers, 1-in-10 chance to trigger a spring festival: all NPCs at the settlement (not just coworkers) gain +0.01 mutual affinity with each participant. Apply +0.02 to `Settlement::morale` (cap 1.0). Log "[Settlement] celebrates the spring with song and dance." at full frequency. Uses existing `HomeSettlement` scan pattern. Creates a rare communal event tied to the seasonal work shanty system.

- [ ] **Seasonal production enthusiasm** — In `ProductionSystem.cpp`'s worker contribution block, check `TimeManager::season`. During `Season::Autumn` (harvest), if the facility type is `ResourceType::Food`, apply a +10% `workerContrib` bonus. During `Season::Spring`, apply +5% to all facility types (renewed energy). Log "[Name] works with seasonal vigour at [Settlement]" at 1-in-15 frequency via a `static std::mt19937 s_seasonProdRng`. Uses existing `TimeManager` and `Facility::type` checks.

- [ ] **Winter huddle need drain reduction** — In `NeedDrainSystem.cpp`, when `Season::Winter` and 3+ NPCs share the same `HomeSettlement`, reduce the `heatDrainMult` by 10% for each NPC at that settlement (pre-compute settlement population counts before the drain loop). Log "[Settlement] residents huddle together for warmth" once per day per qualifying settlement at 1-in-8 frequency via `static std::mt19937 s_huddleRng`. Uses existing `SeasonHeatDrainMult` and `HomeSettlement` component.

- [ ] **Hauler rivalry escalation** — In `TransportSystem.cpp`'s GoingToDeposit arrival block, when two haulers from the same home settlement arrive at the same destination within the same game-hour and their `Relations::affinity < 0.1` (deep rivalry), 1-in-6 chance that the losing hauler (lower `tripProfit`) suffers -0.01 `Settlement::morale` on their home settlement. Log "[Loser] seethes at [Winner]'s success at [Destination]" at full frequency. Uses existing `Relations::affinity` and `Hauler::tripProfit`. Creates escalating social consequences for unresolved rivalries.

- [ ] **Hauler gift on return home** — In `TransportSystem.cpp`'s GoingHome arrival block (where `hauler.state` transitions from GoingHome back to Idle), when the hauler's `Money::balance > 50g` and has `Relations::affinity >= 0.6` toward any NPC at home settlement, 1-in-12 chance to gift 5g (balance-to-balance, Gold Flow Rule). Boost mutual affinity +0.02. Log "[Hauler] brings a gift home to [Friend] at [Settlement]" at full frequency. Creates a wealth-sharing mechanic tied to successful trade runs.

- [ ] **Gossip accuracy decay** — In `TransportSystem.cpp`'s trade gossip block, add a `float gossipAge = 0.f` field to `Hauler` in `Components.h`. Set to 0 when gossip is shared. Increment by `gameDt` each step (in the Idle state time tracking). When `gossipAge > 12.f` (12 game-hours), clear `bestRoute` to empty string so stale gossip expires. Prevents haulers from acting on outdated market information indefinitely.

- [ ] **Hauler market report at tavern** — In `AgentDecisionSystem.cpp`'s idle chat block, when a hauler (`registry.any_of<Hauler>(entity)`) chats with a non-hauler NPC and the hauler's `Hauler::bestRoute` is non-empty, 1-in-8 chance to log "[Hauler] tells [NPC] about trade on the [Route] route at [Settlement]." Boost the NPC's affinity toward the hauler by +0.01 (informational value). Uses existing idle chat proximity and stagger checks. Creates cross-class social interaction where haulers share economic knowledge with workers.

- [ ] **Convoy loyalty bonus** — In `TransportSystem.cpp`'s convoy camaraderie block (after delivery arrival), track consecutive convoy trips between the same pair via a `static std::map<std::pair<entt::entity,entt::entity>, int> s_convoyStreak`. On 3+ consecutive convoy trips together, boost mutual affinity by +0.06 instead of +0.04 and log "[Hauler1] and [Hauler2] are becoming inseparable on the road" at 1-in-4 frequency. Reset streak to 0 if either hauler delivers solo. Creates long-term travel partnerships.

- [ ] **Convoy split sadness** — In `TransportSystem.cpp`'s convoy check, when `wasInConvoy == true` but `hauler.inConvoy == false` (convoy broke up), and the previous convoy partner had `Relations::affinity >= 0.5`, apply -0.005 to `Settlement::morale` on the hauler's home settlement. Log "[Hauler] misses travelling with [Partner]" at 1-in-8 frequency via `static std::mt19937 s_splitRng`. Uses existing `wasInConvoy` flag. Creates emotional cost when established travel companions separate.

- [ ] **Retirement morale boost** — In `TransportSystem.cpp`'s retireList processing, after the farewell toast block, apply +0.02 to `Settlement::morale` (cap 1.0) on the retiree's home settlement when the retiree had `Hauler::lifetimeTrips >= 20`. Log "[Settlement] celebrates [Retiree]'s long career" at full frequency. Uses existing `HomeSettlement` and `Settlement::morale`. Creates a community-wide positive event from veteran retirement.

- [ ] **Retiree reputation transfer** — In `TransportSystem.cpp`'s retireList processing, after the farewell toast, if the retiree has `Reputation::score >= 0.4`, find the youngest hauler (lowest `Hauler::lifetimeTrips`) at the same settlement and boost their `Reputation::score` by +0.1 (cap 1.0). Log "[Retiree] passes their good name to [Successor] at [Settlement]" at full frequency. Uses existing `Reputation` and `Hauler::lifetimeTrips`. Creates a legacy transfer mechanic.

- [ ] **Survivor mentorship network** — In `AgentDecisionSystem.cpp`'s idle chat block, when two bankruptcy survivors (`DeprivationTimer::bankruptSurvivor == true` on both) chat at the same settlement, boost mutual affinity by +0.03 instead of the normal +0.02. Log "[SurvivorA] and [SurvivorB] compare notes on hard times at [Settlement]" at 1-in-8 frequency. Uses existing idle chat stagger. Creates a self-reinforcing survivor community.

- [ ] **Survivor morale resilience** — In `NeedDrainSystem.cpp`'s need drain loop, when an NPC has `DeprivationTimer::bankruptSurvivor == true` and `Needs::values[Hunger] < 0.3` (low hunger), reduce the hunger drain rate by 15% (survivor knows how to stretch food). No log needed — the effect is passive. Uses existing `Needs` array index and `DeprivationTimer`. Creates a tangible survival advantage from past hardship.

- [ ] **Second-chance hauler celebration** — In `EconomicMobilitySystem.cpp`'s graduation block, when a bankruptcy survivor graduates to hauler, scan NPCs at the same settlement with `Relations::affinity >= 0.4` toward the graduate. Boost their affinity by +0.02 and log "[Friend] cheers on [Survivor]'s return to hauling at [Settlement]" at 1-in-4 frequency. Uses existing `Relations` scan pattern. Creates a community response to the comeback story.

- [ ] **Bankruptcy near-miss relief** — In `EconomicMobilitySystem.cpp`'s bankruptcy timer reset block (where balance goes above threshold and timer is erased), when `hauler.bankruptProgress >= BANKRUPTCY_HOURS * 0.5f` (was close to going bankrupt), apply +0.01 to `Settlement::morale` (cap 1.0) and log "[Hauler] narrowly avoids bankruptcy at [Settlement]" at 1-in-4 frequency. Uses existing `bankruptProgress` field. Creates tension and relief around near-bankruptcy events.

- [ ] **Generous donor social magnet** — In `AgentDecisionSystem.cpp`'s idle chat block, when a generous donor (`Reputation::score >= 0.6`) chats with any NPC, boost the other NPC's affinity toward the donor by an extra +0.01 on top of normal gain. Log "[NPC] admires [Donor]'s generosity at [Settlement]" at 1-in-10 frequency. Uses existing idle chat stagger and `Reputation` try_get. Creates a social pull where generous NPCs become more popular.

- [ ] **Donor reputation decay** — In `AgentDecisionSystem.cpp`'s once-per-day block (or add a per-day guard), reduce all NPCs' `Reputation::score` by 0.01 (floor 0.0) each game-day. This means generosity must be ongoing to maintain [Generous] status. No log needed. Uses existing `Reputation` component and `TimeManager::day`. Creates a dynamic reputation system where status must be earned continuously.

- [ ] **Charity chain depth counter** — In `AgentDecisionSystem.cpp`'s charity chain reaction block, add a `static std::map<entt::entity, int> s_chainDepth` tracking how many times a chain extends per game-day (clear on day change). Cap at depth 3 to prevent runaway gold drainage. When chain reaches depth 3, log "[Settlement] is abuzz with generosity" at full frequency. Uses existing charity block and `TimeManager::day`.

- [ ] **Charity gratitude affinity** — In `AgentDecisionSystem.cpp`'s charity block, after the chain reaction, boost the recipient's `Relations::affinity` toward the chain target by +0.02 (the recipient helped someone, creating a new bond). Log "[Recipient] and [ChainTarget] share a grateful smile at [Settlement]" at 1-in-6 frequency. Uses existing `Relations` component. Creates social bonds that emerge from generosity chains.

- [ ] **Reconciliation ripple effect** — In `ScheduleSystem.cpp`'s reconciliation block, after two NPCs reconcile, scan for a third NPC at the same facility with `Relations::affinity < 0.2` toward either reconciling NPC. 1-in-6 chance the witness gains +0.02 affinity toward both reconcilers. Log "[Witness] is inspired by [NPC1] and [NPC2]'s truce at [Settlement]" at full frequency. Creates social ripple effects from conflict resolution.

- [ ] **Harmonious worker productivity bonus** — In `ProductionSystem.cpp`'s per-worker yield calculation, when 3+ workers at the same facility all have `DeprivationTimer::reconcileGlow > 0`, apply an additional +3% group harmony bonus on top of individual +5%. Log "[Facility] hums with cooperative energy" at 1-in-8 frequency per production tick. Uses existing `reconcileGlow` field. Rewards settlements that resolve conflicts collectively.

- [ ] **Unlikely friends co-migration** — In `AgentDecisionSystem.cpp`'s migration block, when an NPC migrates and has a reconciliation partner (entity in `s_reconCount` with count >= 3) at the same settlement, 1-in-5 chance the partner follows. Log "[Partner] follows unlikely friend [Migrant] to [Destination]" at full frequency. Extends the escalating friendship arc so that former rivals travel together.

- [ ] **Reconciliation anniversary** — In `AgentDecisionSystem.cpp`'s once-per-day block, track the game-day of each reconciliation via `static std::map<std::pair<entt::entity,entt::entity>, int> s_reconDay`. On the 30-day anniversary, if both NPCs are at the same settlement, boost mutual affinity by +0.02 and log "[NPC1] and [NPC2] mark a month since making peace at [Settlement]" at full frequency. Creates recurring social beats from conflict resolution history.

- [ ] **Novice-to-expert career aspiration** — In `AgentDecisionSystem.cpp`'s skill growth block, when a novice (skill < 0.5) has `Relations::affinity >= 0.5` toward an expert (skill >= 0.8) of the same profession at the same settlement, boost skill growth by +0.0002 (10% bonus). Log "[Novice] trains harder to impress [Expert] at [Settlement]" at 1-in-10 frequency. Uses existing `Relations` and `Skills`. Creates a mentorship aspiration that rewards social bonds with faster learning.

- [ ] **Expert recognition ceremony** — In `AgentDecisionSystem.cpp`'s once-per-day block, when an NPC's skill crosses 0.8 (journeyman→expert), scan for other experts at the same settlement with `Relations::affinity >= 0.3`. Each expert gains +0.02 affinity toward the new expert. Log "[Expert1] welcomes [NewExpert] into the guild at [Settlement]" at full frequency. Creates a peer-recognition moment that strengthens the professional community.

- [ ] **Heir completion pride boost** — In `AgentDecisionSystem.cpp`'s skill growth block, when an heir NPC (`wisdomLineage != entt::null`) achieves skill >= 0.8 and the legacy log fires, boost the NPC's `Reputation::score` by +0.1 (cap 1.0) and home `Settlement::morale` by +0.02. Makes fulfilling an elder's legacy a tangible community benefit beyond the narrative log.

- [ ] **Elder mentor tooltip detail** — In `SimThread::WriteSnapshot`'s NPC loop, add `std::string elderMentorName` to `AgentEntry` in `RenderSnapshot.h`. Set from `Skills::elderMentorName` when non-empty. In `HUD.cpp`'s NPC tooltip, display "Mentor: [Name]" in soft blue after the heir badge when non-empty. Makes the apprentice-mentor relationship visible.

- [ ] **Lineage generation counter** — In `Components.h`'s `Skills` struct, add `int wisdomLineageGen = 0` (how many deaths the lineage has passed through). In `DeathSystem.cpp`'s lineage chain block, set `mSkills.wisdomLineageGen = deadSkills->wisdomLineageGen + 1` (or 1 for first-gen). In the legacy completion log in `AgentDecisionSystem.cpp`, include the generation count: "[NPC] carries on [Elder]'s legacy (3rd generation)". Makes multi-gen lineages narratively richer.

- [ ] **Lineage mastery celebration** — In `AgentDecisionSystem.cpp`'s legacy completion block (where `wisdomLineage` is cleared after skill >= 0.8), when `wisdomLineageGen >= 2` (multi-generation legacy), trigger a settlement-wide event: all NPCs at the settlement with `Relations::affinity >= 0.3` toward the heir gain +0.03 mutual affinity. Boost `Settlement::morale` by +0.03. Log "[Settlement] celebrates [NPC]'s mastery — [Elder]'s legacy lives on" at full frequency. Rewards long lineage chains with a communal event.

- [ ] **Hauler mentorship graduation** — In `TransportSystem.cpp`'s delivery block, when a novice hauler (lifetimeTrips was < 5, now reaches 5) completes their 5th delivery while having `mentorBonus > 0`, log "[Novice] graduates from [Mentor]'s tutelage on the [Route] road" at full frequency. Boost the mentor's `Reputation::score` by +0.05. Uses existing `lifetimeTrips` and `mentorBonus`. Creates a narrative milestone for the mentorship arc.

- [ ] **Expert teaching aura** — In `AgentDecisionSystem.cpp`'s skill growth block, when an expert NPC (profession-matching skill >= 0.8) is working at the same facility as a non-expert of the same profession, the non-expert gains +0.0001 passive skill growth per step (proximity learning). Log "[NonExpert] watches [Expert] work at [Settlement]" at 1-in-12 frequency. Uses existing `Profession` and `Skills` try_get. Creates a passive mentorship effect from expert presence.

- [ ] **Expert count in settlement tooltip** — In `SimThread::WriteSnapshot`'s settlement aggregation (`SettlAgg` struct), add `int expertCount = 0`. Increment when an NPC has profession-matching skill >= 0.8. Add `int expertCount = 0` to `SettlementEntry` in `RenderSnapshot.h`. In `HUD.cpp`'s settlement tooltip, display "Experts: N" after the master count line when N > 0. Makes the settlement's teaching capacity visible.

- [ ] **Mourning procession size scaling** — In `AgentDecisionSystem.cpp`'s mourning procession block, scale the morale boost by procession size: `0.02f + 0.005f * (participants.size() - 3)` (capped at +0.05). Larger processions have more community healing impact. Log variant when 5+ participants: "[Settlement] holds a grand procession for [Elder]" at full frequency. Uses existing `participants` vector size.

- [ ] **Shared grief friendship persistence** — In `AgentDecisionSystem.cpp`'s mourning procession block, after the mutual affinity boost among participants, set a new `bool processedGrief = false` on `DeprivationTimer` in `Components.h` to `true` for each participant. In the idle chat block, when both chatters have `processedGrief == true`, boost affinity by an extra +0.01. Log "[NPC1] and [NPC2] share a quiet moment of remembrance at [Settlement]" at 1-in-8 frequency. Creates lasting bonds from shared mourning experiences.

- [ ] **Comforter reputation gain** — In `AgentDecisionSystem.cpp`'s comfort-grieving block, after successfully comforting a grieving NPC, boost the comforter's `Reputation::score` by +0.02 (cap 1.0). Log "[Comforter] earns respect for tending to the grieving at [Settlement]" at 1-in-8 frequency. Uses existing `Reputation` component. Creates a reputation path through emotional labour, not just economic activity.

- [ ] **Grief contagion in families** — In `AgentDecisionSystem.cpp`'s grief timer drain block, when a grieving NPC has a `FamilyTag` and another NPC at the same settlement shares the same `FamilyTag::name`, 1-in-8 chance to set the family member's `griefTimer = 0.5f` (mild sympathy grief). Log "[FamilyMember] worries about [Griever] at [Settlement]" at 1-in-6 frequency. Uses existing `FamilyTag` component. Creates emotional contagion within family units.

- [ ] **Mourning procession participant count in settlement tooltip** — In `SimThread::WriteSnapshot`'s `SettlAgg` struct, the `mourningCount` field is already computed. Add `int mourningCount = 0` to `SettlementEntry` in `RenderSnapshot.h` and pass it through. In `HUD.cpp`'s settlement tooltip, when `mourning == true`, change badge text from "[Mourning]" to "[Mourning: N]" showing participant count. Makes the scale of the procession visible.

- [ ] **Procession affinity scaling by size** — In `AgentDecisionSystem.cpp`'s mourning procession block, scale the mutual affinity boost by participant count: `0.02f + 0.005f * (participants.size() - 3)` (capped at +0.05). Larger processions create stronger bonds. Log variant when 6+ participants: "[Settlement] holds a grand procession" at full frequency. Uses existing `participants` vector size.

