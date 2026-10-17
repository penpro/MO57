# MO57 Vision

> **Current state:** what MO57 is meant to be. The "Project Vision" and "Design Principles" sections were moved verbatim from
> `CLAUDE.md` on 2026-10-17; "What A-rated means" and "The four gates" are verbatim from the July 2026 Fable 5 charter
> (`Docs/Archive/MO57_Fable5_Charter.md`). Wes's later goal (2026-10-11): "a complete polished AAA level game".
> **Read when:** making any design call, judging scope, or writing for players.
> **Related:** `Docs/ENGINEERING_PRINCIPLES.md` (Principle 11: the simulation IS the design), `Docs/PROGRESSION_ROADMAP.md`
> (the progression that dictates content), `Docs/MedievalDynasty_Village_Reference.md` (village-loop north star),
> `Docs/MO57_Colony_Management_Design.docx` (colony design), `Docs/DECISIONS.md` (Wes's binding calls).

## Project Vision

**MO57** is an ultra-realistic procedural open-world survival game with a fully destructible/mutable voxel terrain. Think Minecraft's freedom meets hardcore realism - no fantasy creatures, grounded physics, detailed medical/survival simulation.

### Core Pillars
1. **Realism First** - All systems rooted in real-world mechanics (medical, crafting, physics)
2. **Emergent Civilization** - Solo primitive survival → multi-pawn settlements → castle cities
3. **Total World Mutability** - Dig, mine, build, terraform via Voxel Plugin (open-source dev-phy build)
4. **Modding Foundation** - Full C++ mod support; base game is a realistic framework others can reskin/extend

### Multiplayer
- Steam-based co-op (Satisfactory-style): play solo or invite friends to help
- Not MMO - small group collaboration on shared worlds

### Pawn System
- **Possession**: Player can possess any pawn they control; idle pawns run on AI
- **Assignments**: Assign pawns to jobs (gather wood, teach, craft) and bind to house + workplace
- **Relationships**: Pawns have family, loyalty, morale; villages can ally or wage war
- **AI Autonomy**: Full survival instincts (eat, sleep, flee) with streamlined routines for jobs
- **Permadeath**: Pawn death is permanent; if last pawn dies, respawn ~5 miles away as new pawn, old pawn's gear remains at death location
- **Population Cap**: Soft cap via resource/survival difficulty, not arbitrary limits

### Skills & Progression
- **Extensive Skill Trees**: Primitive crafting (knapping, pitch-making) through medieval engineering and beyond
- **Learning Methods**:
  - Direct action (slow)
  - Being taught by skilled pawn (2x speed)
  - Schools maintain entire skill tree (prevents decay)
- **Skill Decay**: Unused skills degrade over time unless maintained via schooling
- **Tech Accessibility**: No hard locks; player has "genetic memory" (lore: galactic seeding program) allowing attempts at any tech, but practical prerequisites make skipping difficult (can't smelt without foundry, can't build foundry without tools/materials)

### World Generation
- Voxel Plugin (open-source dev-phy build) for destructible/buildable terrain
- Finite large flat world with world border (engine supports earth-sized spheres for future)
- Procedurally generated biomes, resources, points of interest
- Chunked loading for performance

### Lore (Revealed Gradually)
- Players are colonists with encoded genetic memory, sent to seed new planets
- Knowledge unlocks feel like "remembering" rather than inventing
- Sci-fi origins revealed in late-game/DLC content

### Development Phases
1. **MVP - Solo Survival Loop**: Single pawn, primitive survival, core medical/crafting systems
2. **Pawn Discovery**: Find survivors after exploring ~100mi², simple automated tasks initially
3. **Full Pawn AI**: Autonomous survival behavior, job systems, relationships
4. **Civilization Building**: Housing, workplaces, teaching, population growth
5. **Multiplayer Polish**: Steam integration, world sharing
6. **DLC Pipeline**: Medieval → Industrial → Modern → Sci-fi planetary expansion

### Modding Philosophy
- Full C++ mod support (like Java Minecraft modding)
- Realistic foundation that modders can extend with any theme (fantasy, sci-fi, historical)
- Clean separation of engine/framework code from content

---

## Design Principles (Derived from SCUM, Zomboid, RimWorld, Kenshi, DayZ/Tarkov research)

1. **Tiered Complexity** - Simple overview for quick checks, detailed view for interested players
2. **Visual Feedback Over Numbers** - Moodles/icons for status, color coding for severity
3. **Graceful Degradation** - Injuries impair, don't immediately kill; death preventable in hindsight
4. **Automation at Scale** - Manual control for 1 pawn, priorities/schedules/jobs for many
5. **Emergent Narrative** - Character traits, relationships, memorable moments from systems interacting

---

## What "A-rated" means (Fable 5 charter, July 2026, verbatim)

The bar is the tier of **Valheim, Satisfactory, Core Keeper, V Rising, Enshrouded** — not AAA fidelity, but *cohesion, feel, and a loop that holds a co-op group for 40+ hours*. Concretely, MO57 hits A-rated when:

1. **A friend can join your world from the Steam friends list in two clicks**, and everything you both do stays in sync.
2. **The core loop is legible and satisfying**: chop → knap → craft → build → survive → settle, each step with clear feedback and game feel.
3. **Combat feels weighty** and the wound sim is a *headline feature*, not a hidden spreadsheet.
4. **The world looks intentional** — cohesive lighting, weather, terrain, vegetation, and a consistent art read.
5. **There's a reason to keep playing** — a progression arc from lone survivor to settlement, with escalating threats and goals.
6. **It never crashes, never desyncs, never loses a save**, and runs at a stable frame rate.
7. **The first 15 minutes teach you the game** without a wiki.

Everything below serves those seven outcomes.


## The four gates (Fable 5 charter, July 2026, verbatim)

Every system this epoch touches must pass four gates before it's "done." This is the definition of framework→game:

| Gate | Question | How we verify (post-`ue.py`) |
|------|----------|------------------------------|
| **Co-op correct** | Does it replicate + stay in sync for a remote client? | 2-client PIE harness (Pillar 0) + Steam smoke |
| **Legible** | Can a new player tell what happened and why, from UI/feedback alone? | Play the loop as a naive user; UI-drive harness |
| **Fun in the loop** | Does it make the moment-to-moment better, not just more simulated? | Playtest the vertical slice |
| **Verified** | Is there a regression gate so it can't silently break? | `MO.Test.*` / automation test in `ue.py test` |

**Rule:** no pillar is "shipped" until all four gates are green. "It compiles" and "the host can do it" are *table stakes*, not done.


## Later goal (Wes, 2026-10-11)

"Commit what you did and keep going ... set a goal of a complete polished AAA level game." The measurable quality gates that
define "polished" are in `Docs/PROJECT_STATUS.md` (Quality gates); the work toward them is in `Docs/BACKLOG.md`.
