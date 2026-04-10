# Questions for Kasey

These came up during autonomous development. None are blocking — I've made
reasonable default decisions for each — but your input may change the design.

---

## Simulation Balance


at this time its hard to tell since the towns run out of supplies long before being able to balance at this time

**Q1** — At 1x speed, a full need lasts ~20 game-hours (Hunger) and ~13 game-hours
(Thirst) without supply. Does this feel like the right pacing, or should the
world run faster/slower? (Adjustable via drain rates in `WorldGenerator.cpp`)

**Q2** — Haulers carry a max of 5 units per trip. With 4 haulers per settlement
and 20 NPCs consuming ~0.5 Food/hr each (10 Food/hr total), supply can't keep
up with demand until the production rate is tuned up. This is intentional
(scarcity drives the cascade), but do you want the starting stockpile seed
amount adjusted so you have more time to observe before collapse?

---

## Player Feel

**Q3** — Currently the player (yellow dot) is purely keyboard-controlled (WASD)
and does NOT auto-seek facilities when hungry — they must manage needs manually.
Should the player auto-satisfy needs when idle, like NPCs do?

for now the player isnt really needed as we need to get the world working smoothly first

**Q4** — When the player dies, the plan says press `R` to respawn. Should the world
reset on respawn, or does the simulation continue with the player re-entering?
(I implemented: world continues, player respawns in a random settlement.)

this is a fine implemtation

---

## Event Log

**Q5** — The event log (WP8) shows the last 50 events in a bottom panel. Should
this also write to a file on disk (e.g., `events.log`) so you can review longer
runs? Implemented as in-game-only for now.

good for now

---

## Visual / UI

**Q6** — Haulers are currently rendered as slightly larger white circles with a
colored cargo dot. Any preference on making them more visually distinct
(different shape, outline color, etc.)?
 not any at this time