# Questions for Kasey

These came up during autonomous development. None are blocking — reasonable
defaults are in place — but your answers will shape the next phase of work.

---

## Previous Questions (answered)

**Q1** — Drain rate pacing (~20hr Hunger, ~13hr Thirst): _hard to tell until towns stabilize_
**Q2** — Hauler capacity / starting stockpile seeds: _adjusted (seeds 120, capacity 15, 6 haulers)_
**Q3** — Player auto-satisfy needs: _not needed yet, world stability first_
**Q4** — Respawn behavior: _world continues, player re-enters — fine as-is_
**Q5** — Event log to disk: _good for now (in-game only)_
**Q6** — Hauler visual distinction: _nothing at this time_

---

## New Questions

### Population & Lifecycle

**Q7** — When an NPC dies in a settlement, should a replacement eventually be
born (current birth system: pop < 35, enough food/water)? Or should death be
permanent and settlement collapse be a possible end-state?

**Q8** — Should there be a win/loss condition, or is this an open-ended
observation sandbox? If there are conditions, what are they? (e.g., "lose if
any settlement population hits 0")
open ended
---

### Economy & Trade

**Q9** — Haulers currently move whichever resource has the biggest surplus.
Should the player be able to direct trade routes — e.g., "always send Food from
Greenfield to Wellsworth"? Or keep it fully autonomous?
once money gets implemeted then haulers should look to see how much a resouce is selling for in one town and then estamate how much it would be worth in a different town and then decide if it would like to take the risk to buy and transport it to the other town to attempt to make a profit
also traders/haulers should be willing to wait to attempt to secure a better profit margin


**Q10** — Right now the two settlements produce exactly one resource each
(Food / Water). Do you want a third settlement, a new resource type (e.g.,
Wood, Tools), or a production chain (e.g., Farm needs Water to produce Food)?

---

### Random Events

**Q11** — Should random events fire periodically — drought (water production
halved), blight (food spoilage), or road bandits (road blocked for N hours)?
These would stress-test the cascade mechanics. If yes, what frequency feels
right?

yes, but for now lets just get the basics down
---

### Player Role

**Q12** — The player currently navigates the world as a passive observer with
needs. Should the player gain active powers — e.g., place a new farm, redirect
a hauler, open/close the road manually? If so, which action first?
the player should be no different to an NPC. they shouldnt have a special power or anything other then the fact that the player is controlled by a real person

---

### Save / Load

**Q13** — Do you want a save/load system (serialize registry state to disk)
before adding more complexity, or is that out of scope for this alpha?
saving would be nice but that can be push to the side for now, but plan on adding this later