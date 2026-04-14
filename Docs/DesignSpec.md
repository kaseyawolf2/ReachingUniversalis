# Reaching Universalis — Design Specification

This document describes the vision, design principles, and target architecture for Reaching Universalis. It is the authoritative source of intent for anyone (human or AI) working on this project. When a design question isn't answered by the code or CLAUDE.md, look here.

---

## 1. One-Line Vision

A data-driven, clusterable simulation engine that can host any world setting (medieval, Naruto, Battletech, WW2, modern day, Gate: JSDF) as an open-world sandbox where the player is just another agent.

---

## 2. Core Design Principles

These are non-negotiable. Every system must satisfy them.

### 2.1 Simulation-First

The simulation runs whether the player participates or not. Nothing scales to the player's presence, power, or expectations. The world is the protagonist.

### 2.2 Player = NPC

The player uses the exact same rules as every NPC: same needs, same mortality, same economic constraints, same tools. The only difference is that the player has a keyboard attached. NPCs have access to everything the player has; the player has access to everything NPCs have.

### 2.3 Causal Accuracy Over Decorative Realism

Detail exists because it changes outcomes. For any simulated thing, the engine should be able to answer: Where did it come from? Who owns it? Who needs it? How does it move? Who knows about it? What happens if it disappears? If a detail doesn't change a decision anywhere in the simulation, it is noise — cut it.

### 2.4 Emergent Narrative

There is no scripted story. All drama arises from systems colliding — a famine, a power vacuum, a broken supply line, a grudge. The world already has memory when the player spawns.

### 2.5 Data-Driven Everything

World definitions live in config files (TOML), not C++ code. A new world setting (sci-fi, post-apocalyptic, anime) should require zero engine changes — only new config files. This also enables modding and total conversion.

### 2.6 Simple Graphics, Rich Data

Graphics are deliberately simple (circles, arrows, colored dots). This frees compute for the simulation and makes the engine genre-agnostic. The UI is data-focused: if the player would know it, the UI should show it.

---

## 3. NPC Agent Model

NPCs are the heart of the simulation. They are not quest dispensers or background decoration — they are agents living lives.

### 3.1 Priority Hierarchy

An NPC always takes care of lower-numbered concerns first:

| Priority | Scope | Primary Concerns |
|----------|-------|------------------|
| 1 | **Self** | Personal needs (food, water, shelter, heat, protection), personal wants (money, luxuries) |
| 2 | **Family** | Family needs (food, water, heat, shelter, protection), family wants (money, luxuries) |
| 3 | **Village/Settlement** | Town needs (food, water, protection) |
| 4 | **County** | Protection, trade stability |
| 5 | **Duchy** | Protection, political stability |
| 6 | **Kingdom** | Protection, territorial integrity |
| 7 | **Empire** | Protection, hegemony |

Higher-scope concerns only matter once lower-scope concerns are met. A starving NPC won't fight for the kingdom.

### 3.2 Agent Data Model

Every NPC carries:

- **Identity** — Name, age, sex, physical attributes, family ties, cultural background, personality traits (risk tolerance, greed/generosity, loyalty radius, aggression threshold, honesty, sociability)
- **Needs** — Defined per-world in config. The medieval world has 4 (hunger, thirst, energy, heat). A sci-fi world might have oxygen, radiation shielding, etc. Needs drain over time and must be satisfied or the NPC suffers and eventually dies.
- **Skills** — Defined per-world. Improve through practice, degrade through disuse. A blacksmith's son starts with metalworking basics.
- **Knowledge** — Bounded. NPCs don't know things they haven't seen, been told, or figured out. Information propagates along trade routes and social networks at finite speed. Rumors can be wrong. Market info can be stale.
- **Memory & Relationships** — NPCs remember events that involved them. Relationships have valence (positive/negative) and dimensions (trust, respect, fear, affection, debt). Grudges and bonds fuel emergent drama.
- **Goals** — Defined per-world. Assigned dynamically based on personality, circumstances, and opportunity.

### 3.3 Behavior Architecture: Habit + Obligation + Utility

NPCs are not pure optimizers (that looks alien) or pure behavior trees (too rigid). They use three layers:

1. **Habit** — A schedule template by role: sleep, work shift, meals, errands, social time. This is the default.
2. **Obligation** — Hard pressures that override habits: job shift, debt payment, child care, militia duty, medical emergency.
3. **Utility** — Within scheduled windows and after obligations, NPCs pick the best action using weighted evaluation: safest, most profitable, least exhausting, socially expected, aligned with personality.

This produces legible behavior — you can watch an NPC and understand why they're doing what they're doing.

### 3.4 Institutions Over Personalities

People live through structures, not just traits:

- **Households** — Shared pantry, beds, dependents, budget, care duties
- **Employers** — Budget, staff roles, work orders, inventory
- **Factions** — Territory, leadership, military, treasury, laws
- **Legal systems** — Property rules, contracts, crime and punishment
- **Debt and obligation** — People go to work because they need wages, because rent is due, because their household needs food

A miner goes to work because he needs wages, the company scheduled him, his household needs food, and the transport leaves at 06:00. That chain of institutional causes is more valuable than 200 abstract personality moodlets.

### 3.5 Emergent Social Structures

Don't hard-code "there is a king." Model the mechanisms and let structure emerge:

- **Authority** comes from control of resources, military force, social consensus, or tradition
- **Laws** are local norms enforced by whoever has power
- **Factions** form when NPCs with aligned interests and sufficient trust cooperate
- **Hierarchy** emerges when one NPC accumulates enough authority, loyalty, or fear

---

## 4. Economy: Logistics Is Destiny

### 4.1 Local Markets

No global price list. Every market is local. Prices emerge from supply and demand at each settlement. Merchants perform arbitrage — buying cheap locally, selling dear elsewhere — and this distributes goods across the world.

### 4.2 Production Chains

Production has real inputs. Every step takes time (skill-dependent), consumes materials (tracked), requires tools (which degrade), requires facilities (which were themselves built), and can fail.

Example (medieval): `Wheat Field -> Mill -> Bakery -> Bread`
Example (sci-fi): `Ore Deposit -> Refinery -> Fabricator -> Components -> Assembly`

The specific chains are defined in world config, not engine code.

### 4.3 Transport and Logistics

Goods have weight and volume. Moving them costs energy, time, and money. Transport infrastructure matters (roads vs. trails vs. rivers). Supply disruptions cascade: if the iron mine floods, downstream smiths run out of stock, tool prices rise, farmers can't replace plows, crop yields fall, food prices rise, people get hungry, some migrate, the town shrinks.

### 4.4 Gold Flow Invariant

Every gold deduction from a balance must be accompanied by a credit to a recipient (settlement treasury or another balance), unless it is a legitimate infrastructure sink (road maintenance, facility construction, housing). Trade, wages, taxes, and purchases must always balance.

---

## 5. Combat

Combat is lethal, locational, and consequential. Not an HP-sponge system.

- **Lethal and fast** — One good hit can kill. No grinding.
- **Locational damage** — Hits land on specific body locations with specific consequences (broken arm = can't use that hand, leg wound = slowed).
- **Equipment matters** — Weapons have damage type, reach, speed, durability. Armor has coverage zones, material, thickness.
- **Morale** — Untrained NPCs flee. Even trained fighters break under pressure. Morale depends on training, leadership, odds, injuries, fatigue, personality.
- **Rare and consequential** — Death is permanent. Injuries are debilitating. Most NPCs rationally avoid combat.

---

## 6. Player Experience

### 6.1 No Prescribed Goals

The player picks their own path. Possible emergent play styles:

| Style | Description |
|-------|-------------|
| Laborer/Tradesperson | Learn a craft, find work, build a livelihood |
| Merchant | Identify price differentials, build trade routes, manage logistics |
| Explorer | Map unknown regions, discover resources, find ruins |
| Soldier/Mercenary | Sell combat skills, join a faction's military |
| Political Operator | Build relationships, accumulate influence, govern |
| Criminal | Steal, smuggle, extort, fence goods |
| Farmer/Homesteader | Claim land, build a farmstead, survive seasons |
| Wanderer/Survivor | Own nothing, owe nothing, live off the land |

### 6.2 Expanding Authority

The player's progression is expanding authority through in-world status:

- **Early game** — You control only yourself. Work jobs, trade, survive.
- **Mid game** — You gain authority through status: team lead, shop manager, squad leader. Direct a few others.
- **Late game** — Institutional power: business owner, mayor, faction leader. Macro-level tools earned through position.

### 6.3 Permadeath

When you die, you die. The world continues. Options for continuation:
- Start a new character in the same living world (which continued without you)
- Play as an heir or family member (inheriting property and relationships, not skills)
- Play as an entirely new arrival

### 6.4 Map-View Interface

The main playing screen is a map view — a Google Maps / Aurora 4X-style eagle-eye perspective. The player scrolls around the map. Buttons open windows for buying/selling, issuing orders, viewing finances, inspecting NPCs, etc.

NPCs and the player are rendered as simple markers (dots, circles, arrows). No character models, no sprite art. All the richness is in the data panels and the simulation underneath.

### 6.5 Drill-Down Data

The UI follows a drill-down paradigm:
- **World/Map view** -> **Region** -> **Settlement** -> **Building** -> **Character** -> **Inventory/Body**

At each level, show only data relevant to that scale. Tooltips on everything. An inspection mode that explains *why* things are happening (why is bread expensive? why did this NPC leave town?).

---

## 7. UI Architecture

### 7.1 Decoupled from Simulation

The UI must be completely decoupled from the simulation thread. If the sim is lagging behind (running complex agent decisions, processing hundreds of NPCs), the player must still have a responsive UI/UX. The player can scroll the map, open panels, queue commands — all without waiting for the sim to catch up.

Currently implemented as: main thread (input + render) communicates with sim thread via `InputSnapshot` (main->sim) and `RenderSnapshot` (sim->main, mutex-protected).

### 7.2 Data-Driven Display

The HUD reads world config (WorldSchema) to know what needs, resources, and stats to display. No hardcoded "hunger bar" — it shows whatever needs the current world defines. Keybindings are loaded from world config.

### 7.3 Immediate Feedback

Player actions show pending state in the UI immediately (e.g., "buying..." indicator) without waiting for the sim round-trip.

---

## 8. Technical Architecture

### 8.1 Engine, Not Game

Reaching Universalis is a **simulation engine** that loads world definitions from config files. The medieval world is the first proof-of-concept. The engine itself should have no medieval-specific code.

### 8.2 ECS (Entity-Component-System)

All simulation state lives in an `entt::registry`. An NPC is an entity with components (Position, Velocity, Needs, Skills, Profession, Money, etc.). Systems operate over components in batch. This is how thousands of agents run efficiently.

### 8.3 Threading Model

Two threads communicate through shared structs:
- **Main thread** — Polls input, updates camera, renders from RenderSnapshot. Never touches the ECS registry.
- **Sim thread** — Owns the registry and all ECS systems. Runs: ProcessInput -> RunSimStep(dt) -> WriteSnapshot().

This ensures the UI stays responsive regardless of sim load.

### 8.4 Tick Speed (Paradox-Style)

Five speeds: 1x, 2x, 4x, 16x, uncapped. Speeds 1-4 run a fixed number of sim steps per frame. Speed 5 runs as many steps as fit in a 16ms wall-clock budget. The player controls the pace.

### 8.5 World Config Format

Worlds are defined as a directory of TOML files under `worlds/<name>/`:
- `world.toml` — Map size, economy settings, timing
- `needs.toml` — What needs agents have (hunger, thirst, oxygen, etc.)
- `resources.toml` — What resources exist (food, water, uranium, etc.)
- `skills.toml` — What skills agents can develop
- `professions.toml` — What jobs exist
- `facilities.toml` — What production buildings can be built
- `seasons.toml` — Seasonal cycle and modifiers
- `events.toml` — Random events (drought, plague, festival, etc.)
- `goals.toml` — NPC life goals
- `agents.toml` — NPC templates (villager, merchant, player, etc.)

All IDs are plain ints internally. String-to-ID lookup maps are built at load time so systems never do string comparisons in the hot loop.

### 8.6 Technology Stack

| Component | Choice | Rationale |
|-----------|--------|-----------|
| Language | C++ | Performance-critical simulation |
| ECS | entt | Fast, header-only, mature |
| Rendering | Raylib | Simple 2D, minimal overhead |
| Config | TOML (toml++) | Human-readable, easy to author |
| Build | CMake via build.sh | Simple, portable |

---

## 9. Clustering and Multiplayer

### 9.1 Clusterable Simulation

The simulation should be distributable across multiple machines to support high NPC counts at high detail. Each compute node handles a region (settlement + surrounding area). Cross-region interactions (trade, migration, events) are coordinated via message passing.

Target architecture:
1. **Per-region system execution** — Region-local systems (NeedDrain, Production, Schedule, Birth, Death) run independently per region.
2. **Thread pool for regions** — Each region ticks on its own thread from a pool on a single machine.
3. **Cluster distribution** — Regions can be assigned to different machines in a local cluster. Each machine computes its assigned regions and passes results to peers.

### 9.2 P2P Multiplayer

Multiplayer is peer-to-peer, not client-server. Each player's local region is authoritative. Cross-region interactions are negotiated between peers.

### 9.3 P2P Update Distribution

Game updates are downloaded and distributed via the P2P network, not from a central server.

### 9.4 Anti-Cheat: Client Trust Classification

To prevent cheating in multiplayer, clients are classified by trust level:

| Classification | Description |
|----------------|-------------|
| **Verified** | Owner-run or unbiased clients |
| **Trusted** | Players with little deviation from verified/trusted clients over time |
| **Untrusted** | New clients or clients with deviations from expected output |
| **Deviant** | Hostile clients with large deviations or confirmed hacking |

Verification works by having a separate node re-compute the same simulation step. If the output diverges (checked via checksum), the original node is flagged. Verification nodes are selected to avoid collusion:
- **Linked accounts** — Clients that joined around the same time or have similar network latency/IP are not used to verify each other (may be the same person).
- **New clients** — Never used as sole verifiers. Untrusted by default due to no history.
- **Marked clients** — Flagged by admins for extra verification.

---

## 10. World Generation

The world must exist before the player does.

### 10.1 Layered Physical World

Generate the world in dependent layers, each feeding the next:
1. **Geology** — Elevation, soil composition, mineral deposits, aquifers
2. **Hydrology** — Watersheds, rivers, lakes, water tables
3. **Climate** — Temperature bands, precipitation, seasons
4. **Biomes** — Flora and fauna based on climate/soil (determines what food grows, what materials are available)
5. **Resources** — Mineral veins, timber density, fish stocks. All finite. All depleting.

### 10.2 History Generation

Before the player enters, simulate decades to centuries of history at coarse resolution:
- Settlements founded at geographically sensible locations
- Population growth, migration, famine, plague
- Political entities forming, splitting, warring, trading
- Technology diffusion (regional and path-dependent, not a universal tech tree)
- Cultural drift: naming conventions, religions, trade customs, legal norms

When the player spawns, the world already has memory. A town exists because there's iron nearby. A ruin exists because of a war 80 years ago.

### 10.3 Level-of-Detail Simulation

Full-fidelity simulation for all NPCs at all times is not feasible at scale. Use LOD:

| Distance from Player | Fidelity |
|---------------------|----------|
| **Immediate area** (same settlement) | Full tick-by-tick agent simulation |
| **Regional** (neighboring settlements) | Abstracted daily resolution, NPCs aggregated into household/business units |
| **Distant** (far regions) | Statistical simulation, coarse monthly/seasonal ticks |

When the player travels to a new area, it spins up to full fidelity. When the player leaves, it spins down. NPCs the player has interacted with are tagged as persistent and always simulated at medium fidelity minimum.

---

## 11. Example World Settings

The engine should support radically different settings through config alone:

| Setting | Needs | Resources | Professions | Key Mechanics |
|---------|-------|-----------|-------------|---------------|
| **Medieval** (current) | Hunger, Thirst, Energy, Heat | Food, Water, Wood, Shelter | Farmer, Water Carrier, Woodcutter, Merchant | Seasonal farming, road trade, settlement founding |
| **Naruto** | Hunger, Chakra, Stamina | Food, Scrolls, Weapons | Ninja, Merchant, Farmer, Medic | Jutsu system, village loyalty, mission economy |
| **Battletech** | Hunger, Morale, Maintenance | Food, Parts, Ammo, Mechs | MechWarrior, Tech, Merchant, Commander | Mech combat, salvage economy, faction contracts |
| **WW2** | Hunger, Morale, Ammo | Food, Fuel, Ammo, Medicine, Parts | Soldier, Medic, Engineer, Logistics, Command | Supply lines, unit cohesion, terrain, air support |
| **Modern Day** | Hunger, Energy, Social, Money | Food, Fuel, Electronics, Services | Office worker, Tradesperson, Entrepreneur, etc. | Job market, housing, social media, commuting |
| **Gate: JSDF** | Hunger, Morale, Ammo | Food, Fuel, Ammo, Medicine, Magic Items | JSDF Soldier, Knight, Mage, Merchant, Diplomat | Tech vs. magic, cross-world trade, cultural clash |

---

## 12. Design Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Scope creep kills the project | Phased development. Each phase is playable. Resist adding systems out of order. |
| NPC AI is too expensive | LOD simulation. Full fidelity only near player. Budget CPU per tick. |
| Emergent behavior produces nonsense | Extensive logging and inspection tools. Watch what NPCs do. Tune utility weights. |
| No fun because no direction | Environmental pressure creates natural goals (you need to eat, winter is coming). The world's indifference IS the design. |
| World feels dead despite simulation | NPCs must visibly do things. The simulation must be legible through the UI. |
| Going global too early | Stay regional until the core loop is proven. Off-map world as abstraction. |
| Graphics scope-creep | Simple markers only. All richness is in data panels and simulation depth. |

---

## 13. Inspirations

| Game | What to Take |
|------|-------------|
| **Aurora 4X** | Obsessive physical modeling, drill-down data UI, map-view interface |
| **Dwarf Fortress** | Emergent characterization, history generation, layered world gen, "the simulation IS the content" |
| **Cataclysm: DDA** | Survival-item granularity, data-driven design enabling total conversion mods |
| **Gary Grigsby's War Series** | Logistics rigor — supply lines matter, terrain matters, maintenance matters |
| **MegaMek** | Mechanical fidelity for high-value assets |
| **Eve Online** | Player-as-one-agent-among-thousands economy, emergent multiplayer dynamics |
| **Paradox Grand Strategy** | Tick speed controls, map-centric UI, systems-driven narrative |

---

## 14. Current State

The project has a working medieval prototype with:
- Two-thread architecture (main + sim) with decoupled UI
- 14 ECS systems running in sequence each sim step
- Data-driven world definitions via TOML configs (WorldSchema)
- Generic needs and resources (driven by config, not hardcoded enums)
- Settlements, roads, production facilities, merchants, NPCs with needs/schedules/money
- Supply/demand pricing, trade, migration, birth/death, random events
- Paradox-style 5-speed time controls
- Player entity using the same systems as NPCs

See `TODO.md` for the active development backlog and `CLAUDE.md` for build/run instructions and architecture details.
