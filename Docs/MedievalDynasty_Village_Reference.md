# Medieval Dynasty — Village-Structure Reference & MO57 Mapping

**Purpose:** Medieval Dynasty (Render Cube, 2021) is the **north star for MO57's *village structure*** — the founding→settlement→dynasty loop, villager management, housing, jobs, and the communal economy. MO57's *survival simulation* is already far deeper (medical/anatomy/metabolism/exposure vs. MD's casual hunger/thirst/temperature). **The goal is MD's legible settlement loop wearing MO57's simulation depth, plus co-op, possess-any-pawn, permadeath, and a procedural voxel world.**

This doc dissects MD system-by-system and maps each to MO57 with one of three verdicts:
- **↑ MO57 already deeper** — we exceed MD here; keep it, make it legible.
- **→ Go granular** — MD abstracts this; MO57 should simulate it concretely.
- **○ Not yet** — MO57 has no equivalent; this is net-new village scope.

Companion: `MO57_Fable5_Charter.md` (Pillar 5 = Pawns/Civilization), `Fable5_Village_Handoff.md` (the build path), `MO57_Colony_Management_Design` (the already-authored colony design).

---

## 0. The MD Core Loop (what we're chasing)

> Arrive as a nobody → survive the first winter → build a house → recruit your first villager → give them a home and a job → their labor frees you to expand → a hamlet becomes a village becomes a town → marry, raise an heir, and when you die, **play on as your heir** — the dynasty outlives the character.

It works because every layer *feeds the next and is legible*: survival forces building, building enables recruitment, recruitment multiplies labor, labor funds expansion, expansion demands management, and the dynasty gives it all a horizon longer than one life. MO57 already has the bookends (hardcore survival start; permadeath→respawn) — **the missing middle is the settlement-management spine.**

---

## 1. Settlement Founding & Growth

**MD:** You claim land freely, place buildings on an unowned valley, and the "village" is emergent — just the set of buildings + villagers you've assembled. Growth is gated by *Dynasty Reputation* (recruitment cap) and your own labor/material throughput.

**MO57 mapping:**
- **→ Go granular** — MO57's world is procedural + voxel-destructible, so "found a village" can be *place-anywhere on terrain you terraform yourself* (dig foundations, flatten a plot) — richer than MD's flat valley. The building system + terraforming already exist.
- **○ Not yet** — a *settlement identity*: a named place with a center, a membership roster, a shared storage, and a footprint. MD has no formal "settlement object"; **MO57 should have one** (a `UMOColonyManagerSubsystem`-owned settlement record) because co-op + multiple settlements + the colony UI all need it.

---

## 2. Villager Recruitment

**MD:** Wandering NPCs and townsfolk can be recruited once your *Dynasty Reputation* is high enough (reputation is a hard gate — each villager has a rep threshold). You talk to them, sometimes do a favor/quest, and they join. Reputation also has a **game-over floor**: sink too low and you're expelled from the valley.

**MO57 mapping:**
- **○ Not yet (partially scaffolded)** — `UMORecruitmentComponent` exists (tracks recruitment state / colony membership) but the *acquisition loop* (find survivors after exploration → earn trust → recruit) is Development-Phase-2 vision, unbuilt.
- **→ Go granular** — MD recruitment is a rep-check + dialogue. MO57 can make it *earned through demonstrated reliability* (the charter's "trust is earned" principle): survivors evaluate you, your settlement's safety/food/mood, and their own personality (a Reserved survivor joins a small camp; a Social one wants a bustling one). Ties to `UMOPersonalityComponent`.
- **↑ MO57 already deeper (identity)** — recruited pawns are GUID-persistent, possessable, and permadeath-real. An MD villager is a labor unit; an MO57 survivor is a *person you can become*.

---

## 3. Housing

**MD:** Each **house holds one male + one female villager + their children**. A villager *must* have a roof or their mood decays until they leave. **Wall insulation** raises mood (+1% per insulation point above 50%, up to +50% at 100% insulation). Houses are a hard capacity constraint on population.

**MO57 mapping:**
- **→ Go granular via the EXISTING shelter sim** — MD's "insulation %" is a single number. MO57 already has a **multi-axis shelter/exposure model** (overhead/wind/enclosure, `#96/#97`) and building tiers. A villager's home quality should feed *the same exposure sim the player uses* — a drafty stick hut vs. a chinked-timber cabin with a hearth measurably changes their warmth, health, and mood. This is MD's insulation idea but *simulated, not a slider*.
- **○ Not yet** — the **binding**: a pawn ⇄ house assignment (the vision's "bind to house + workplace"), house capacity, and "no home → mood decay → leaves." Needs a house record on the buildable + a residency table in the colony subsystem.
- **→ Go granular** — furniture/beds (Pillar 2 building interiors) as *real* mood/rest inputs, not decoration: a bed enables sleep-quality, storage enables the household economy.

---

## 4. Jobs & Workstations

**MD:** Assign a villager to a **workstation building** (hunting lodge, farm shed, woodshed, smithy, etc.) and pick a **job**; their **skill level** in the associated skill sets efficiency/yield. You set **production quotas** — tell a worker *what* to make and *how much focus* to split across products. Goods then **appear in the building's/village's resource storage automatically** — the labor is abstracted; you never watch them swing an axe.

**MO57 mapping — this is the biggest divergence and the biggest opportunity:**
- **→ Go granular (the headline)** — MD abstracts production to "goods appear in storage." MO57 can make villagers **actually run the real systems**: a survivor assigned to "knap flint" *walks to the workbench, consumes real `Flint01` from storage, runs the real `MO.Test.Craft` path, and deposits real `FlintFlake01`.* The `UMOSurvivorJobQueueComponent` + the 122-recipe crafting system + the harvest system already exist — wiring a villager's job to *drive the actual crafting queue* gives Kenshi/RimWorld-grade "watch your colonist do the thing," which MD explicitly does not have.
- **↑ MO57 already deeper (possession)** — you can *possess* the worker and do the job yourself (faster/higher quality per the "direct action vs. taught" learning model), or leave them on AI. MD has no possession.
- **○ Not yet** — the **assignment UI + job board**: pick a pawn, pick a station, pick a recipe/quota. The colony design specifies `UMOTaskAssignmentWidget`; unbuilt.
- **Design guardrail (from CLAUDE.md's SCUM/RimWorld lessons):** *tiered complexity + automation at scale.* One villager = watch them work (granular, fun). Twenty villagers = **standing orders / priorities / quotas** (MD-style abstraction) so it doesn't become tedium. Build BOTH altitudes; let the player choose the zoom.

---

## 5. Mood / Approval

**MD:** A single **Mood** scalar per villager, −100%…+100%. Productivity scales with it, but the only *hard* consequence is −100% (they leave with their family). Inputs: job assigned + skill match (**+2%/skill point, cap +20%**), marriage (**+10%**), each child (**+5%**), house insulation (**+1%/pt over 50%, cap +50%**), plus food/water/firewood provision.

**MO57 mapping:**
- **↑ MO57 already deeper (designed)** — the colony design replaces MD's single mood scalar with **three personality axes** (`UMOPersonalityComponent`: Conscientiousness, Sociability, Stability) + a `UMOCharacterHistoryComponent` (event log, relationships, mood/activity summaries). A *Volatile* survivor swings hard; a *Social* one needs company; a *Reserved* one performs better alone. This is RimWorld-tier, well beyond MD.
- **→ Go granular** — feed mood from the *real* sims MO57 already runs: hunger/thirst/pain/cold/injury/sleep-debt (medical + metabolism + exposure), not an abstract "needs met" flag. A villager in pain from an untreated wound should be miserable — and MO57 *has* the wound.
- **Keep MD's discipline:** surface mood as **moodles/portraits** (the colony bar/portrait widgets, already designed), not a spreadsheet. The tiered-UI rule again: glanceable strip → detailed character card.

---

## 6. The Communal Economy

**MD:** Village **resource storage** is shared. Villagers **consume** food/water/firewood from it (upkeep) and **produce** assigned goods into it. Seasonal **taxes** are owed (pay in coin or goods). Money comes from **selling surplus** to NPC town traders at semi-dynamic prices. It's a soft loop: keep storage in the black (produce > consume + taxes).

**MO57 mapping:**
- **○ Not yet** — a **settlement shared-storage** layer (containers already exist as buildings; a *communal* pool the colony subsystem reads for upkeep + production is new) and the **upkeep tick** (villagers draw food from storage; run out → hunger via the real metabolism sim → they leave/die).
- **→ Go granular** — MD "consumes food" abstractly; MO57 villagers **eat real item stacks** with real nutrition through the metabolism component. Production is real crafted items (§4), not abstract goods.
- **○ Not yet (later)** — trade/taxes/NPC towns. MO57's world is procedural with no hand-placed towns; the "trade" fantasy likely arrives via *other procedurally-discovered settlements* or the multiplayer/other-player economy, not MD's static merchants. Defer past the vertical slice.

---

## 7. Skills & Progression

**MD:** Six villager/player skills — **Extraction, Hunting, Farming, Diplomacy, Survival, Production** — each 1→10, **leveled by doing**. Player skill points unlock a **tech/perk tree** and building "schemes."

**MO57 mapping:**
- **↑ MO57 already deeper** — MO57's `UMOSkillsComponent` + `UMOKnowledgeComponent` already model per-skill XP curves, **skill decay**, three learning methods (direct/taught-2×/schooled-maintained), and the **"genetic memory"** tech-unlock fantasy (recipes unlock as *remembering*, `bRequiresDiscovery` + knowledge). MD's flat 6-skill grind is a subset of what MO57 already has scaffolded.
- **→ Go granular (villager side)** — apply the *same* skill/teaching system to villagers: a skilled survivor **teaches** an unskilled one (2× speed), a **school** building maintains the settlement's tree against decay (the vision's schooling pillar). This is MD's "skill up by working" plus a social-transmission layer MD lacks.

---

## 8. Dynasty, Family & Permadeath

**MD:** Marry, have children, name an **heir**; **Dynasty Reputation** gates recruitment and can end the game (expulsion). When your character dies (old age), you **continue as the heir** — the settlement persists across generations.

**MO57 mapping:**
- **↑ MO57 already deeper (permadeath)** — MO57's permadeath is *real and any-pawn*: if your possessed pawn dies it's gone; you respawn as another survivor ~5 miles out; the dead pawn's gear stays at the death site. This is MD's heir-continuation generalized — **any survivor is a potential "heir,"** and death is a hard event, not a scheduled old-age transition.
- **○ Not yet** — **family/relationships/lineage**: marriage, children, loyalty, the relationship graph (`UMOCharacterHistoryComponent` has relationship tracking designed but unbuilt), population growth. This is core MD village-feel and net-new MO57 scope.
- **○ Not yet** — a **reputation/standing** analog. MO57's version is likely *per-survivor trust + settlement reputation* rather than MD's single valley-wide number — and it should gate recruitment (§2) the way MD's does.

---

## 9. Seasons, Calendar & Time

**MD:** A calendar of **4 seasons × days**, a day/night cycle, seasonal farming windows, winter survival pressure, and taxes-per-season. Time is the metronome the whole economy runs on.

**MO57 mapping:**
- **↑ MO57 already deeper (clock)** — MO57 has an authoritative `UMOGameClockSubsystem` (DateTime, day/night events, `#87`) + UDS/UDW weather integration + a real exposure/temperature sim. The *engine* for seasons exists.
- **○ Not yet** — **seasons as a gameplay layer**: seasonal resource availability, crop windows, winter as a survival + settlement-upkeep spike (villagers need more firewood/food; the shelter sim already models cold). This turns the existing clock into MD's economic metronome.

---

## 10. Farming & Animal Husbandry

**MD:** Field lifecycle (plow → fertilize → sow → tend → harvest), orchards, and livestock (chickens/pigs/cows/sheep/horses) producing eggs/milk/wool/manure/mounts — assignable to villager jobs.

**MO57 mapping:**
- **○ Not yet** — farming is unbuilt but *fits the voxel terrain naturally* (till voxel soil, plots you terraform). A strong mid-game village pillar.
- **↑ Potential depth** — MO57's metabolism/nutrition sim means crops can have *real* nutritional profiles feeding the deep survival layer; livestock ties to the existing creature AI (deer/prey → domesticable). Bigger than the vertical slice; a clear V2+ expansion.

---

## 11. Building System (the physical village)

**MD:** Unlock building **schemes**, place freely (grid-free), gather wood/stone/straw, and **tap to build** through stages. Tiers: sticks/straw → wood → stone. Buildings are homes, workstations, storage, or decoration.

**MO57 mapping:**
- **↑ MO57 already deeper (construction)** — MO57's building system has **weighted build parts**, snapping (walls/half-walls/roofs/floors), real collision, ghost placement, deposit ledger + persistence, and **build-by-pawn** potential. It's a construction *simulation*, not tap-to-progress.
- **○ Not yet (the village layer on top)** — buildings need **roles**: *this is a house* (residency, §3), *this is a workshop* (hosts a job + station, §4), *this is communal storage* (§6). The `FMORecipeDefinitionRow` already has `ProvidedStationType`/`ContainerSlotCount`/building fields — the data hooks exist; the *colony-subsystem awareness* of them is the gap.
- **Cross-ref:** charter Pillar 2 (part variety, tiers, interiors) supplies the *physical* pieces; this doc's §3–§6 supply the *semantic* layer that turns "a hut" into "Brann's house where he sleeps and stores his tools."

---

## 12. The Synthesis — "MD structure × MO57 depth"

| MD system | MD's version | MO57 verdict | MO57's version |
|---|---|---|---|
| Survival | hunger/thirst/temp sliders | ↑ deeper | medical + anatomy + metabolism + multi-axis exposure |
| Villager psyche | one Mood scalar | ↑ deeper | 3 personality axes + history + relationships + real-sim-driven mood |
| Jobs | assign → goods appear in storage | → granular | villager runs the REAL crafting/harvest systems; or possess & do it |
| Housing | 1M+1F + insulation % | → granular | residency binding + the real shelter/exposure sim + interiors |
| Skills | 6 skills, level by doing | ↑ deeper | skills + decay + teaching(2×) + schools + genetic-memory unlocks |
| Economy | shared storage + taxes + NPC trade | → granular / ○ later | real-item communal storage + real consumption; trade deferred |
| Dynasty | marry → heir → continue | ↑ (death) / ○ (family) | any-pawn permadeath+respawn; family/lineage net-new |
| Time | 4 seasons metronome | ↑ (clock) / ○ (gameplay) | authoritative clock exists; seasons-as-gameplay net-new |
| World | hand-crafted valley + towns | ↑ deeper | procedural voxel-destructible world + PCG (see `Fable5_PCG_Path.md`) |
| Players | single-player dynasty | ↑ deeper | Steam co-op + possess-any-pawn |
| Building | scheme + tap-to-build | ↑ (construction) / ○ (roles) | weighted-part build sim + semantic building roles |

**The one-line thesis:** *Medieval Dynasty is a village-management game with a survival veneer; MO57 is a survival simulation that should grow a village-management spine. Take MD's loop — recruit, house, employ, feed, grow, inherit — and drive every rung with the concrete simulations MO57 already runs, at a zoom the player controls (possess one pawn's day, or set standing orders for twenty).*

**The trap to avoid (CLAUDE.md's own research):** depth ≠ tedium. MD stays fun by *abstracting* labor; MO57's edge is it *can* simulate labor — but must offer the MD-style abstraction (quotas, priorities, automation) the moment the settlement outgrows hands-on management. Ship both altitudes.

---

**Sources (village mechanics):** [Management — MD Wiki](https://medieval-dynasty.fandom.com/wiki/Management) · [Mood — MD Wiki](https://medieval-dynasty.fandom.com/wiki/Mood) · [Workers — MD Wiki](https://medieval-dynasty.fandom.com/wiki/Workers) · [Villager Mood & Approval Guide](https://gamerant.com/medieval-dynasty-villager-mood-approval-guide-happiness/)
