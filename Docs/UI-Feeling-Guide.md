# Overarching UI Description — Aurora (All Windows)

### Visual Style

A deep navy-to-near-black background is universal across every window, with text rendered in a single high-contrast color — most commonly cyan, light blue, or white — against the dark field. Accent hues are minimal and functional: cyan/blue for borders and structure, green to flag editable or live values, yellow for numeric readouts, and occasionally magenta/orange in the map and toolbar contexts. Typography is invariably a single small, uniform sans-serif at one weight with no size hierarchy — rank is communicated through color and column position, never scale. The aesthetic is flat, unpolished, and emphatically Windows Forms: zero gradients, zero shadows, zero rounded corners, zero iconography beyond the toolbar raster icons. It reads consistently as a hobbyist simulation tool or engineering console from the early-to-mid 2000s, where function completely eclipses finish.

### Layout Feel

Every window is information-saturated with minimal to no whitespace. Across the application, three structural patterns dominate and recur:

1. **Master-detail split** — a narrow hierarchical tree or selector on the left drives a large, tabbed detail workspace on the right.
2. **Canvas-with-control-rails** — a large central viewport (map, visualization, or data grid) is framed by tightly packed control strips on the top, left, and bottom edges.
3. **Stacked data surface** — horizontal bands of filter/query controls on top, a dominant tabular or list region in the middle, and a persistent action bar anchored at the bottom.

All three share the same density philosophy: every pixel is either a label, a value, a control, or a reserved output region. Task flow is absent — this is not a wizard or a guided experience. It is a reference console where the user navigates by expertise, not by prompt.

### Tone and Personality

Serious, technical, and unapologetically power-user. The entire application speaks with one voice: the voice of an engineer who built it for themselves and people who already know exactly what every abbreviation means. It evokes the spirit of a "mission control" or a deep-simulation cockpit — efficient, austere, slightly retro, and indifferent to aesthetic warmth. There is zero hand-holding; mastery is assumed.

### Design Patterns to Carry Forward

| Pattern | Where it appears |
|---|---|
| **Horizontal tab strip as primary navigation** across parallel facets of one entity | Nearly every multi-function window |
| **Left-hand hierarchical selector** scoping all right-side content | Economics, Commanders, Naval Org, System Generation |
| **Persistent bottom action bar** grouping all verbs for the current selection | Universal |
| **Top filter/toggle strip** (dropdowns + checkboxes) above a central data surface | Minerals, Technology, Events, Ground Forces |
| **Input-left / output-right parity** for configuration → live results | Sensor Design, Turret Design, Missile Design |
| **Multi-column label–value grids** with whitespace-only grouping | Economics Summary, Race Information, Race Comparison |
| **Edge-docked control chrome around a large central canvas** | System Map, Galactic Map |
| **Color as the primary semantic hierarchy mechanism** (not size or weight) | All windows |
| **Persistent global status in the title bar** (date, wealth, context) | All Economics and main view windows |
| **Two-level tab navigation** (domain tabs + context sub-tabs) | Naval Organization |

---

# Overarching UI Description — javaw / MekHQ (All Windows)

### Visual Style

Dark charcoal-grey background with a near-monochrome palette, meaningfully different from Aurora's pure navy-black. Accent color is a distinctive magenta/purple used for faction identity and highlighted text. Secondary semantic accents appear as soft yellow for warnings, muted blue for hyperlinks, and muted green/red for status — a richer, more purposeful color vocabulary than Aurora. Typography is a clean sans-serif with a two-tone label/value distinction (dim labels, brighter values), giving readable row-scanning without relying solely on column alignment. Controls are flat, slightly rounded rectangles — still utilitarian but with a touch more polish than raw Win32 chrome. The overall impression is a mature Java/Swing application in a dark theme: corporate-desktop rather than hobbyist-terminal.

### Layout Feel

Dense and information-rich, organized in clear horizontal bands: menu bar, grouped control clusters, a primary tab strip, then a tri-column work area (activity log | summary stats + procurement | reports sidebar). Titled group boxes with labeled borders carve space into discrete regions — a more structured approach than Aurora's whitespace-only grouping. A persistent footer strip carries quick-reference links and status counters. Nothing is hidden behind progressive disclosure; everything important is surfaced simultaneously, giving a cockpit/dashboard feel rather than a guided task flow.

### Tone and Personality

Serious, simulation-grade, and power-user oriented — it trusts the user to parse a lot at once. The magenta accents and iconography inject a hint of hobbyist warmth into what would otherwise feel like enterprise software. Slightly more approachable than Aurora but still firmly in the "earned mastery" camp.

### Design Patterns to Carry Forward

| Pattern | Where it appears |
|---|---|
| **Titled group boxes** clustering related actions and readouts into labeled regions | Main campaign window |
| **Horizontal primary tab bar** for major functional areas above a multi-column workspace | Main campaign window |
| **Label/value paired rows** with dim labels and brighter values for scannable data display | Summary stats panels |
| **Persistent footer of contextual reference links** plus always-visible key status counters | Main campaign window |
| **Sidebar of report/action buttons** flanking a central summary panel | Main campaign window |
