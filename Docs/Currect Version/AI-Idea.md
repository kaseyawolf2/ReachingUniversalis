# Living World Simulation Sandbox — Design Blueprint

---

## I. Core Design Philosophy

### The Foundational Rule

**The simulation comes first. The player comes second.** Every system models a real process—not a gamified abstraction. The simulation runs whether the player participates or not, and nothing in the world scales to the player's presence, power, or expectations.

This is the spiritual intersection of all your inspirations: Aurora's obsessive physical modeling, Dwarf Fortress's emergent characterization and history generation [theseus.fi](https://www.theseus.fi/handle/10024/814557?show=full), CDDA's survival-item granularity [docs.cataclysmdda.org](https://docs.cataclysmdda.org/design-balance-lore/design-gameplay.html), Grigsby's logistics rigor, MegaMek's mechanical fidelity, and Eve Online's player-as-one-agent-among-thousands economy.

### Five Pillars

| Pillar | Meaning |
|---|---|
| **Simulation-first** | Every system models a causal process. The simulation runs whether the player participates or not. |
| **Indifferent world** | Nothing scales to the player. No plot armor. No level-gating. The economy, weather, politics, and violence exist on their own terms. |
| **NPC autonomy** | Every NPC is a full agent with needs, goals, memory, relationships, and a daily schedule. They are not furniture. |
| **Emergent narrative** | There is no scripted story. All drama arises from systems colliding—a famine, a power vacuum, a broken supply line, a grudge. |
| **Causal accuracy** | Detail exists because it changes outcomes. Precision is not encyclopedic realism—it is causal fidelity. |

### Defining "Accuracy" Correctly

For this kind of game, **accuracy means causal accuracy**, not decorative realism. The game should be able to answer:

- **Where did this come from?**
- **Who owns it?**
- **Who needs it?**
- **How does it move?**
- **Who knows about it?**
- **Who has authority over it?**
- **What happens if it disappears?**

If the game can answer those questions for people, goods, jobs, and events, it will feel real. Track things in units that matter to decisions: *time, distance, mass, volume, calories, fuel, money, labor-hours, ammo, maintenance state.*

If a detail does not change a decision anywhere in the simulation, it is noise—cut it.

### The Player's Role

You are **one person** in a populated, functioning world. You might become important. You might die in a ditch. The simulation does not distinguish between you and NPC #4,827—you simply happen to have a keyboard attached. The player uses the exact same rules as every NPC: same needs, same mortality, same economic constraints, same legal exposure.

---

## II. World Generation

The world must exist before the player does.

### A. Geography & Physical World

Following the layered approach Tarn Adams describes for Dwarf Fortress—where handling fields separately (temperature, rainfall, drainage, vegetation, salinity) produces vastly richer biomes than spawning them directly [gameaipro.com](http://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter41_Simulation_Principles_from_Dwarf_Fortress.pdf)—generate the world in dependent layers, each feeding the next:

1. **Geology** — Tectonic plates, elevation maps, soil composition, mineral deposits, aquifers. This determines where ore is, where farmland is fertile, where building is easy or hard.
2. **Hydrology** — Watersheds, rivers, lakes, water tables. Water availability drives settlement placement.
3. **Climate** — Temperature bands, prevailing winds, precipitation patterns, seasons. Model with enough fidelity that a continental interior is dry and a windward coast is wet.
4. **Biomes & Ecology** — Flora and fauna distributions based on climate/soil. Not just aesthetic—these determine what food grows, what animals exist, what materials are available locally.
5. **Resources** — Mineral veins, clay deposits, timber density, fish stocks. All finite. All depleting.

### B. History Generation

Before the player enters, simulate **decades to centuries** of history at coarse resolution:

- Settlements founded at geographically sensible locations (river confluences, harbors, mountain passes, resource sites)
- Population growth, migration, famine, plague
- Political entities forming, splitting, warring, trading
- Technology diffusion (not a universal tech tree—regional and path-dependent)
- Cultural drift: naming conventions, religions, trade customs, legal norms
- **Persistent historical figures** who may still be alive or whose legacies (buildings, laws, grudges, family lines) persist

The result: when the player spawns, the world already has *memory*. A town exists because there's iron nearby. A ruin exists because of a war 80 years ago. Two factions hate each other for a reason the simulation actually produced.

### C. Time System

The simulation ticks in **discrete increments** with variable granularity:

- Seconds during combat/direct action
- Minutes during routine activity
- Hours during travel/sleep
- Days/weeks for long-term management and macro processes

The player can **adjust the tick rate** contextually. All NPC behavior, economic cycles, weather, and decay processes advance on the same clock. Events interrupt the clock when something important happens nearby.

**Critical design challenge:** When the player is in real-time combat, the wider world continues at compressed time. The solution is to separate *local simulation* (always at the player's temporal resolution) from *macro simulation* (which can tick at coarser intervals simultaneously). Local combat runs second-by-second; distant wars resolve at hourly/daily resolution without blocking.

---

## III. Simulation Scale: Regional, Not Global

### Start with One Region

If you want NPC life and detail, **do not start with a planet or galaxy**. Start with:

- **1 region**
- **5–10 settlements**
- **300–1,000 persistent NPCs** in the full design
- **80–150 NPCs** in the first playable slice
- Outside world handled as **off-map abstraction** (imports, exports, political pressure, outside market shifts, war)

This gives you enough density for households, jobs, trade, crime, politics, shortages, militia/war, migration, and emergent stories—without drowning in scope.

### Multi-Scale Simulation Structure

| Layer | What Happens There | Update Frequency |
|---|---|---|
| **Player bubble** | Movement, combat, item interaction, room/building tasks | Seconds |
| **Active settlement** | Schedules, work shifts, local trade, services, patrols | 1–5 minutes |
| **Region** | Convoy travel, prices, faction movement, shortages, migration | Hourly |
| **Off-map world** | Imports/exports, outside politics, war pressure, big market shifts | Daily/weekly |

### Level-of-Detail Simulation

You cannot simulate tens of thousands of NPCs at full fidelity simultaneously. Use LOD simulation, a principle the Veloren project has codified—rtsim simulates the entire world at low resolution, throttling tick rates especially for distant entities, and is "deliberately designed with an almost aggressively simplistic data model" for distant actors [docs.veloren.net](https://docs.veloren.net/veloren_rtsim/index.html):

| Distance from Player | Simulation Fidelity |
|---|---|
| **Immediate area** (same settlement/local map) | Full tick-by-tick agent simulation, full physics, full item tracking |
| **Regional** (neighboring settlements, same region) | Abstracted daily resolution. NPCs aggregated into household/business units. Trade flows computed, not individual transactions. |
| **Distant** (far regions) | Statistical simulation. Population numbers, resource stocks, faction power levels. Coarse monthly/seasonal ticks. |
| **Historical** (before game start) | Pre-computed during world generation. Fixed record. |

When the player travels to a new area, it "spins up" to full fidelity, generating specific NPCs from the statistical model. When the player leaves, it gradually "spins down."

**Consistency solution:** Tag NPCs the player has interacted with as **persistent**. They are always simulated at medium fidelity minimum, even when far away. When you return, they still exist and remember you.

---

## IV. NPC Simulation — The Heart of the Game

NPCs are not quest dispensers. They are agents living lives.

### A. NPC Agent Data Model

Every NPC has:

#### Identity Layer
- Name, age, sex, physical attributes (height, weight, health conditions, injuries, scars)
- Family ties — parents, siblings, children, spouse (all simulated NPCs or dead historical figures)
- Cultural background — language(s), religion, customs, regional identity
- Personality model — Multi-axis weighted drives:
  - Risk tolerance, Greed/generosity, Loyalty radius (family → clan → faction → ideology), Aggression threshold, Curiosity/conservatism, Honesty tendency, Sociability

#### Needs Layer (Biological + Psychological)
- **Biological**: hunger, thirst, sleep, warmth, health, pain, fatigue
- **Psychological**: safety, social belonging, status/respect, purpose/meaning, comfort, stimulation
- **These needs drive behavior.** An NPC who is hungry and has no money will look for work, beg, steal, or leave town—depending on personality.

#### Knowledge Layer
NPCs have **bounded knowledge**. They don't know things they haven't seen, been told, or figured out.
- They know: their local geography, people they've met, prices they've seen, rumors they've heard, skills they've learned
- **Information propagation** is modeled. News travels along trade routes and social networks at finite speed. Rumors degrade and distort.

This is one of the biggest systems for selling "the world doesn't care about you." Separate **truth** from **belief**:
- Rumors can be wrong
- Market info can be stale
- Crimes may go unreported
- Military response can be delayed
- The player may arrive too late

#### Skills Layer
- Granular skills (not "Combat 5" but "shortbow accuracy," "field dressing," "barley farming," "copper smelting," "haggling")
- Skills improve through practice, degrade through disuse
- NPCs have skill sets reflecting their life history (a blacksmith's son knows metalworking basics)

#### Memory & Relationships
- NPCs remember **events** that involved them: trades, insults, gifts, violence, shared hardship
- Relationships have **valence** (positive/negative) and **dimensions** (trust, respect, fear, affection, debt)
- Grudges, debts, and bonds are the fuel for emergent drama

### B. NPC Behavior Architecture: Habit + Obligation + Utility

This is a critical design decision. **Do not make every NPC a pure optimizer.** That produces rational but alien-looking behavior. Do not rely solely on behavior trees (too rigid) or pure GOAP (computationally expensive and can produce bizarre plans). Instead, use a **layered system**:

#### Layer 1: Habit
A schedule template by role/household:
- Sleep, work shift, meal windows, errands, social/leisure, patrol/prayer/class

#### Layer 2: Obligation
Hard pressures that override habits:
- Job shift, debt payment, child care, militia duty, medical need, landlord demand, faction order

#### Layer 3: Utility
Within scheduled windows and after obligations are satisfied, NPCs pick the best action using weighted utility evaluation:
- Safest, most profitable, least exhausting, socially expected, aligned with personality

#### Daily Priority Ladder
1. Immediate survival (flee danger, eat if starving, sleep if collapsing)
2. Mandatory obligations
3. Household maintenance
4. Paid work
5. Advancement/opportunity
6. Leisure/socializing

This produces behaviors that are **legible**—you can watch an NPC and understand *why* they're doing what they're doing. A farmer wakes at dawn, eats breakfast, walks to fields, works crops, eats lunch, talks to a neighbor, hauls harvest, goes to the tavern, sleeps. A merchant checks inventory, compares memorized price differentials, loads a wagon (weight/volume limits matter), travels along a road (speed depends on terrain, weather, load), and sells at the next town.

### C. Institutions Matter More Than Personalities

A believable life sim is not just "people with traits." People live through **structures**:

- **Households** — shared pantry, beds, dependents, rent/mortgage, shared tools/vehicle, household budget, care duties
- **Employers** — budget, staff roles, work orders, inventory, buy/sell needs
- **Factions** — territory, leadership, military, treasury, laws, reputation
- **Legal systems** — property rules, contracts, crime and punishment, enforcement
- **Debt, currency, and obligation** — people go to work because they need wages, because their household needs food, because rent is due

A miner goes to work because he needs wages, the company scheduled him, his household needs food, he fears losing housing, and the transport truck leaves at 06:00. That chain of institutional causes is more valuable than 200 abstract personality moodlets.

**NPCs can and will:**
- Change professions if their current one isn't meeting needs
- Move to different settlements
- Form partnerships and rivalries
- Start businesses, go bankrupt
- Commit crimes when desperate or greedy
- Get sick, get injured, die
- Have children who grow up and become new agents
- Organize collectively (form militias, trading companies, religious movements, criminal gangs)

### D. Emergent Social Structures

Don't hard-code "there is a king." Model the **mechanisms** and let structure emerge:

- **Authority** comes from control of resources, military force, social consensus, or tradition
- **Laws** are local norms enforced by whoever has power
- **Factions** form when NPCs with aligned interests and sufficient trust cooperate
- **Hierarchy** emerges when one NPC accumulates enough authority, loyalty, or fear

---

## V. Core Simulation Systems

### A. Economy: Logistics Is Destiny

**No global price list.** Every market is local. Prices emerge from supply and demand at each location.

#### Production Chains
Model real production chains with actual inputs:

```
IRON ORE (mined from deposit)
  → SMELTER (requires fuel + labor + facility)
    → IRON INGOTS
      → BLACKSMITH (requires tools + labor + facility + fuel)
        → NAILS / TOOLS / WEAPONS / HORSESHOES
```

Every step takes **time** (skill-dependent), consumes **materials** (tracked by quantity/quality/type), requires **tools** (which degrade), requires **facilities** (which were themselves built from materials), produces **waste/byproducts**, and can **fail** (low skill = wasted materials, poor quality output).

#### Trade & Logistics
- Goods have **weight and volume.** Moving them costs energy, time, and money.
- **Transport infrastructure** matters: roads vs. trails vs. rivers vs. open terrain.
- Merchants perform **arbitrage**—buying cheap locally, selling dear elsewhere—and this distributes goods across the world.
- **Supply disruptions cascade.** If the iron mine floods, downstream smiths run out of stock. Tool prices rise. Farmers can't replace broken plows. Crop yields fall. Food prices rise. People get hungry. Some migrate. The town shrinks.

This is the Grigsby insight applied to civilian life: **logistics is everything.**

#### Three Classes of Things (Fidelity Rule)

Not everything needs the same tracking granularity. Use three classes:

| Class | Examples | Tracking |
|---|---|---|
| **Commodities** | Grain, coal, water, fuel, ore, lumber | Quantities with amount, quality, location, owner, reservation |
| **Objects** | Rifles, coats, tools, medicines, documents | Discrete entities with condition, material, ownership, wear |
| **Platforms** | Trucks, generators, mechs/tanks, workshops, ships | Subsystem-level detail: engine, armor, cargo, sensors, fuel, maintenance state |

This applies MegaMek-level granularity only where it matters—to high-value complex assets—while keeping the bulk economy efficient.

#### Currency & Barter
- Currency exists if the world history generated a monetary system. Otherwise, barter.
- Multiple currencies can coexist with exchange rates.
- Counterfeiting, inflation, debasement are all possible if the systems support them.

### B. Combat: Lethal, Locational, Consequential

Combat happens in the world, on the same map, at the finest time resolution.

#### Principles
- **Lethal and fast.** One good hit can kill. No HP sponges.
- **Locational damage.** Entities don't have a single HP bar. Hits land on specific body locations. A bullet has kinetic energy that resolves against the specific material properties of what it hits. Injuries have specific consequences: broken arm = can't use that hand, leg wound = slowed, gut wound = slow death without treatment.
- **Equipment matters granularly.** Weapons have damage type, material, weight, reach, speed, durability. Armor has coverage zones, material, thickness, weight, encumbrance, condition. Projectiles have caliber/weight, velocity, material → penetration calculated against armor.
- **Positioning matters.** Cover, elevation, flanking, range, lighting.
- **Morale.** NPCs have morale. Untrained people flee. Even trained fighters break under enough pressure. Morale depends on training, leadership, odds assessment, injuries, fatigue, personality.

#### Combat Is Rare and Consequential
Most NPCs avoid combat because the simulation makes it rationally terrifying. There's no respawn. Death is permanent. Injuries are debilitating and slow to heal. This means:
- Bandits prey on the weak and flee from strength
- Wars are devastating to communities
- The player should feel genuine fear in violent encounters

### C. Crafting & Construction

#### Items
Every item has:
- **Material composition** (affects weight, durability, flammability, value, function)
- **Quality** (affected by crafter skill, material quality, tool quality)
- **Condition** (degrades with use, affected by weather/storage)
- **Weight & volume**
- **Specific properties** depending on type (a blade has sharpness; a garment has insulation; food has caloric content and spoilage rate)

#### Crafting
- Recipes are **knowledge-gated** (learn from a teacher, book, experimentation, cultural background)
- Require specific tools, materials, facilities, and time
- Output quality depends on skill, material quality, and tool quality
- NPCs craft too, using the same system

#### Construction
Buildings are built from **materials over time**, not placed as prefab units:
- Foundation requires stone/packed earth
- Walls require timber/brick/stone + mortar
- Structures have **integrity** that degrades without maintenance

### D. Health & Body Simulation

The player and all NPCs share the same body model:

- **Nutrition**: macronutrients, vitamins, minerals. Deficiencies cause specific conditions over time (scurvy, anemia)
- **Hydration**: water quality matters (contaminated water → disease)
- **Temperature regulation**: body heat vs. ambient temperature, clothing insulation, wind chill, wetness
- **Injury model**: specific wounds on specific body parts. Cuts, fractures, punctures, burns, infections. Healing takes real time. Infections can kill.
- **Disease**: communicable and environmental diseases spread through modeled vectors. Epidemics can sweep through settlements.
- **Fatigue & sleep**: cumulative fatigue degrades all performance. Sleep quality matters.
- **Aging**: NPCs age. Physical capabilities peak and decline. Elderly NPCs have knowledge and social capital but declining bodies.
- **Mental health**: trauma, isolation, grief, chronic stress. These affect behavior and decision-making. NPCs self-medicate (alcohol, narcotics) if available and personality-inclined.

### E. Environment & Ecology

- **Weather** simulated regionally: temperature, precipitation, wind, storms. Seasonal patterns with stochastic variation.
- **Day/night cycle** with actual consequences (visibility, NPC schedules, predator behavior)
- **Ecosystems**: animal populations with birth rates, predation, migration. Overhunting depletes game. Deforestation causes erosion.
- **Natural disasters**: floods, droughts, wildfires, earthquakes—emerging from simulated conditions, not scripted events.
- **Decay and maintenance**: everything degrades. Untended buildings collapse. Abandoned roads overgrow. Corpses decompose. **Without wear, repair, replacement, and decay, the world reaches static equilibrium and stops feeling alive.** A functioning maintenance economy is essential.

---

## VI. Player Experience

### What the Player Actually Does

You are a person in this world. Your interface is through your character's body, senses, and knowledge:
- You see what your character sees (limited by line of sight, lighting, perception)
- You know what your character knows (no omniscient map—explore, ask, observe, read)
- You can do what your character can do (limited by skills, tools, physical state, location)

### No Prescribed Goals

Possible emergent play styles:

| Style | Description |
|---|---|
| **Laborer/Tradesperson** | Learn a craft, find work, build a livelihood |
| **Merchant** | Identify price differentials, build trade routes, manage logistics |
| **Explorer** | Map unknown regions, discover resources, find ruins from generated history |
| **Soldier/Mercenary** | Sell combat skills, join a faction's military |
| **Political Operator** | Build relationships, accumulate influence, govern |
| **Criminal** | Steal, smuggle, extort, fence goods |
| **Farmer/Homesteader** | Claim land, build a farmstead, survive seasons |
| **Wanderer/Survivor** | Own nothing, owe nothing, live off the land |

### Expanding Authority Through In-World Status

To merge life sim with strategy/logistics depth without breaking the embodied perspective, design the player's progression as **expanding authority**:

- **Early game**: You control only yourself—work jobs, scavenge/trade, join a household, take contracts, survive.
- **Mid game**: You gain authority through in-world status—team lead, shop manager, squad leader, convoy dispatcher, militia NCO. Now you can direct a few others.
- **Late game**: You gain institutional power—business owner, officer, mayor, logistics chief, faction organizer. Now you get macro-level tools, but only because you earned position.

This is how you get Aurora/Eve/Grigsby-style strategic depth while keeping the player embodied.

### Player Death

Permadeath. When you die, you die. The world continues. Options for continuation:
- Start a new character in the **same living world** (which has continued without you, and where your previous character's effects persist)
- Play as an **heir or family member** (inheriting some property and relationships, but not skills)
- Play as an associate, subordinate, or entirely new arrival

This reinforces "the world does not care about you."

---

## VII. Technical Architecture

This is where most projects of this ambition die. Plan scope ruthlessly.

### A. Entity-Component-System Architecture

**ECS is mandatory for this type of simulation.** Instead of heavy object-oriented inheritance hierarchies, ECS stores data in memory-contiguous arrays. An NPC isn't a massive monolithic object; it is an Entity that holds components like `Health`, `Hunger`, `Position`, `Inventory`, `AiBrain`, `Relationships`. Systems operate over these components in batch. This is how you track thousands of agents efficiently.

### B. Recommended Technology Stack

| Component | Recommendation | Rationale |
|---|---|---|
| **Language** | Rust or C++ | Performance-critical simulation. Rust preferred for safety in complex state management. |
| **Rendering** | ASCII / tile-based initially | Do NOT let graphics scope-creep kill the project. Start DF/CDDA-style. |
| **Data storage** | SQLite or custom binary | World state is large. Need fast serialization. |
| **Scripting layer** | Lua or embedded Python | For moddability, event scripting, AI behavior authoring |
| **UI** | Terminal UI (ratatui) or minimal SDL2/raylib | Functional information display over pretty graphics. |

### C. Data-Driven Design

Following CDDA's model, make as much as possible **data-driven**:
- Item definitions in JSON/YAML
- Crafting recipes in data files
- NPC behavior parameters exposed for modding
- World generation parameters configurable
- Material property tables in external databases
- Allow total conversion mods (different settings—sci-fi, post-apocalyptic, fantasy, historical)

### D. UI/UX: The "Drill-Down" Interface

Aurora 4X is notorious for its punishing UI despite brilliant simulation depth. Since you want accuracy, your interface challenge is displaying massive amounts of data without overwhelming the user. The solution is a **drill-down** paradigm:

- **World/Map view** → zoom to **Region** → zoom to **Settlement** → zoom to **Building** → zoom to **Character** → zoom to **Body Part/Inventory**

At each level, show only the data relevant to that scale. Detailed combat logs (like DF's), tooltips on everything, and an inspection mode that explains *why* things are happening (why is bread expensive? why did guards not respond? why did this NPC leave town?) are essential.

### E. Debugging and Inspection Tools

**This is non-negotiable.** If the game cannot explain itself, the player will perceive deep simulation as mere randomness. Build tools from day one that let you (and eventually the player) ask:

- Why is this NPC here?
- Why is bread expensive?
- Why did guards not respond?
- Why did this company fire workers?
- What is the supply chain status for this settlement?

Extensive logging, replay tools, and "watch what NPCs do" visualization are your primary development and tuning instruments.

---

## VIII. Development Phases

This cannot be built all at once. Each phase must be a playable game.

### Phase 0: The Logistic Toy (Months 1–3)

Before building anything complex, validate the core "indifferent world" loop with a minimal prototype:

- A map with **2 settlements** connected by **1 road**
- **~50 NPCs** with hunger needs
- Food produced only in Settlement A, consumed in both
- NPCs automatically generate transport routes
- Block the road → settlement B starves → NPCs migrate, die, or steal

**Validation checkpoint:** When you see cascading consequences from a disruption you didn't script, you have the core of the game.

### Phase 1: Foundation (Months 4–8)
- Tile-based world map (one region, 3–5 settlements)
- Time system with variable tick rates
- NPC agents: needs (hunger, sleep, safety), daily schedule, movement
- Item system with material properties, weight, volume, condition
- Basic production chains (farming, mining, smelting, smithing, cooking)
- NPC economic behavior (produce, trade, buy, sell)
- Local markets with emergent prices
- Player character as an NPC with keyboard input
- Weather and temperature system
- Basic health model
- Save/load

**Playable milestone:** You can survive by working, trading, or scavenging. The economy functions without you. You can watch prices fluctuate as supply/demand shifts.

### Phase 2: Danger & Detail (Months 9–13)
- Detailed combat system (locational hits, armor, morale)
- NPC hostility and aggression logic (bandits, wildlife, faction enemies)
- Injury and medical system (specific wounds, infections, recovery time)
- Permadeath and new character creation in same world
- Household simulation (shared pantry, dependents, budgets)
- Building/construction system
- Basic crafting catalog (~30 recipes)

**Playable milestone:** The world is dangerous. Combat is lethal. You can die and the world continues.

### Phase 3: Social & Political Systems (Months 14–19)
- NPC relationships, memory, personality effects on behavior
- Reputation system (local, per-NPC, not global)
- Faction formation, leadership, laws, enforcement
- Crime and consequence (all simulated, not scripted)
- NPC communication driven by knowledge, personality, and relationship
- Family/kinship simulation
- Organizations (employers, militias, guilds, gangs) with budgets and authority

**Playable milestone:** You can build relationships. NPCs remember you. Social dynamics create drama. You can join factions, gain or lose standing.

### Phase 4: World Scale & History (Months 20–26)
- Full procedural world generation (geology → hydrology → climate → biomes → history → settlements)
- History simulation (pre-game decades/centuries)
- LOD simulation system (full/regional/distant fidelity)
- Multiple regions with travel between them
- Trade routes between settlements
- Large-scale events (wars, famines, plagues, migrations)
- NPC population scaling (thousands at varying fidelity)
- Player authority expansion mechanics (leadership roles, organization management)

**Playable milestone:** A full living world with history. You are genuinely one person in a vast indifferent simulation.

### Phase 5: Polish & Depth (Ongoing)
- Extended crafting catalog
- Deeper ecology and agriculture
- Naval/river transport
- Education/apprenticeship system
- Cultural systems (music, art, religion as simulated phenomena)
- Mod support and documentation
- Accessibility features
- Sound/audio feedback

---

## IX. A Concrete Starter Setting

For your first playable region, build a **frontier colony district or border region** tied to a larger off-map economy:

### Settlements
- 1 port/depot town (main trade hub)
- 1 mining settlement
- 1 agricultural settlement
- 1 company compound/workshop
- 1 militia fort/outpost
- 1 outlaw/smuggler zone
- Wilderness/resource nodes between them

### Core Goods
Food, water, fuel, medicine, ammo, tools, spare parts, building materials, clothing, luxury goods

### Core Pressures
Weather events, convoy delays, strikes, outbreaks, raids, elections/policy shifts, maintenance failures, outside market shocks

This structure gives you Aurora/Eve-style off-map economic pressure, CDDA-style survival and scarcity, Dwarf Fortress-style household drama, Grigsby-style supply and terrain logistics, and MegaMek-style detailed platform combat when it occurs.

---

## X. Open Design Considerations

Several critical questions require deliberate decisions as development progresses:

### Multiplayer

Eve Online is a core inspiration, and its emergent economy derives substantially from multiplayer interaction. Even if you launch single-player, **architect the simulation with potential multiplayer in mind**: separate the simulation tick from player input, treat the player character as a standard NPC entity with an input handler, and design the server loop so that multiple input handlers could eventually attach to different NPCs. Even supporting 2–4 players in the same world would dramatically amplify emergent social dynamics.

### Offline World Persistence

When the player closes the game, what happens to the world? Three options with tradeoffs:

| Approach | Pros | Cons |
|---|---|---|
| **Freeze** | Simple, predictable | Breaks immersion; world doesn't feel alive |
| **Simulate forward in real-time** | Maximum verisimilitude | Player returns to find everything changed; potentially punishing |
| **Catch-up simulation on reload** | Balances immersion with playability | Requires fast abstract simulation; potential for jarring jumps |

The best approach for this design is **catch-up simulation**: on reload, run the macro simulation forward at abstract (LOD distant) fidelity for the elapsed real-world time, then spin up the player's local area. This is similar to how Dwarf Fortress handles world advancement.

### NPC Communication and Dialogue

Full natural-language dialogue generation is a trap at this scope. Instead, use **contextual template systems**: NPC dialogue is assembled from their current knowledge, relationship to the player, personality, and immediate needs. Display key information (trade offers, rumors, warnings, requests) through structured UI elements backed by the NPC's actual knowledge state, supplemented by short procedurally assembled text. Invest in the *information content* of dialogue, not in making it sound literary.

### Testing Emergent Systems at Scale

Verifying that thousands of interacting systems produce coherent rather than degenerate behavior is a massive QA challenge. Build **automated regression testing** from early on:
- Run headless simulations for hundreds of game-years and check for invariant violations (negative populations, infinite resources, economic singularities)
- Track key macro metrics over time (population, food supply, price stability, faction count) and flag anomalies
- Record and replay specific NPC life histories to catch pathological behavior loops
- The debugging/inspection tools described in Section VII.E are your primary QA instruments

### Team Size and Scope Reality

A project of this ambition, pursued by a solo developer or small team, requires **ruthless scope discipline**. Dwarf Fortress has been in development since 2002 by essentially two people. Aurora 4X was developed by a single person over 20+ years. CDDA survives through a large open-source community. Be realistic:
- A solo developer should expect to spend 2–4 years reaching Phase 2 with a solid foundation
- Open-sourcing early and building a modding community (data-driven design enables this) is the most viable path to the full vision
- Every phase must be a playable, releasable game—not a tech demo waiting for completion

### Sound and Audio

A living world communicates much of its activity through sound. Even in an ASCII/tile-based game, ambient audio (settlement bustle, hammering, animal sounds, weather, distant conflict) dramatically increases the sense of a living, indifferent world. Plan for a simple audio layer from Phase 1 onward, even if it's minimal.

### Accessibility

Information-dense, text-heavy simulations have serious accessibility barriers. From early in development:
- Support screen readers for text output
- Ensure all color-coded information has non-color alternatives
- Allow rebinding of all controls
- Provide adjustable text sizes and high-contrast modes
- Design the drill-down UI so information at each level can be navigated sequentially, not just visually

---

## XI. Critical Design Risks & Mitigations

| Risk | Mitigation |
|---|---|
| **Scope creep kills the project** | Phased development. Each phase is a playable game. Resist adding systems out of order. |
| **NPC AI is too expensive** | LOD simulation. Full fidelity only near player. Budget CPU time per tick. Spread expensive calculations across multiple ticks. |
| **Emergent behavior produces nonsense** | Extensive logging and inspection tools. Watch what NPCs do. Tune utility weights. Accept some chaos as a feature. |
| **No fun because no direction** | Environmental pressure creates natural goals (you need to eat, you need shelter, winter is coming). The world's indifference IS the game design. |
| **Simulating decorative detail** | Apply the causal accuracy test ruthlessly: if it doesn't change decisions, cut it. |
| **NPCs with traits but no structure** | Without households, employers, schedules, and access rules, "NPC AI" becomes wandering nonsense. Build institutions first. |
| **Omniscient systems** | If everyone instantly knows everything, the world feels gamey. Information must propagate physically. |
| **World feels dead despite simulation** | Invest in observable NPC behavior. Players must *see* NPCs doing things. The simulation must be legible. |
| **Going global too early** | Deep life sim + global scope = unfinishable. Stay regional until the core loop is proven. |

---

## XII. The Shortest Version

**Build a regional NPC-first life sim where households, organizations, logistics, and information flow are the real core systems.**

NPCs live through institutions, not just personalities. Goods move through physical space, not teleportation. Knowledge spreads through social networks, not omniscience. Everything decays, everything costs maintenance, and disruptions cascade through interconnected systems.

The world is the protagonist. You're just visiting.

**Start with the Logistic Toy. Get 50 NPCs transporting food between two towns. Block the road. Watch what happens. Everything else follows from there.**