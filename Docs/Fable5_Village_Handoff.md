# Fable 5 Handoff — The Path to a Full Village System

**To:** Fable 5 (next executor)
**From:** the 2026-07-04 planning pass
**Reads with:** `MedievalDynasty_Village_Reference.md` (the north star + system mapping), `MO57_Fable5_Charter.md` (Pillar 5 = Pawns/Civilization), `Fable5_PCG_Path.md` (the world it sits in), `AUTONOMOUS_TOOLING.md` (the loop you'll build it with).

Your job: take MO57 from *"a survivor alone in a procedural world"* to *"a living village you found, staff, feed, and grow"* — the Medieval Dynasty settlement loop with MO57's simulation depth. **Build it as autonomously as the loop allows, and where the loop CAN'T reach, make the gap loud and build the tool that closes it.** The sharpest, most reusable output of this work is not the village — it's the **asset pipeline** the village forces you to build.

---

## 1. The Autonomy Thesis (read this first)

This session proved the autonomous loop can build **systems** end-to-end with verification: C++ (`ue.py cycle`), DataTable content (MCP `rows set`), AI/blackboard (`MO.AI.*`), input/UI drive + assertion (`MO.Test.Input`/`ClickWidget`), co-op (`ue.py mptest`), headless tests (`ue.py auto`). **The entire village *simulation* is inside that envelope.**

What is **NOT** inside it today is **art** — the meshes, characters, animations, props, and icons that make a village *look* like a village. That is the gap this work exists to expose and close.

> **The village SIM is ~fully autonomously buildable. The village PRESENTATION is the build-tool gap. Build the sim on gray-box art first (autonomous, fun-provable), and build the asset pipeline in parallel — then real art is a data-swap, not a rewrite.**

### The map — what's autonomous vs. what's the gap

| Village capability | Autonomy verdict | How |
|---|---|---|
| Settlement/colony data model, membership, residency | ✅ autonomous | C++ + DataTable; `ue.py cycle` + `MO.Test.*` |
| Villager AI: go to station, run a real job, deposit output | ✅ autonomous | BT/blackboard (`MO.AI.Dump/SetKey`) + existing job queue + crafting/harvest |
| Recruitment, mood/personality, needs, upkeep tick | ✅ autonomous | C++ (colony subsystem) + real metabolism/medical sims |
| Colony UI (bar / overview / character card / assignment) | ✅ autonomous* | CommonUI + `MO.Test.ClickWidget`; *WBP layout is editor work (MCP-assistable) |
| Save/load of the whole settlement | ✅ autonomous | existing GUID persistence + a save-roundtrip auto-test |
| Balancing (quotas, wages, mood weights, recruit thresholds) | ✅ autonomous | DataTable-driven; iterate via MCP + PIE |
| **Building MESHES** (house/workshop/fence/furniture) | ⚠️ **THE GAP** | acquire (semi-manual) **or** generate (autonomous gray-box) → import+assign (autonomous) |
| **Villager characters + animations** | ⚠️ **THE GAP** | MetaHuman (mostly autonomous) + action anims (acquire/author) |
| **Props / icons / decals** | ⚠️ **THE GAP** | acquire **or** procedural; import+assign autonomous |

Everything green ships *now* on gray-box art. Everything amber is §5.

---

## 2. Current State — what you're building on

- **Designed, unbuilt:** the whole colony system is spec'd in `MO57_Colony_Management_Design` / CLAUDE.md — `UMOColonyManagerSubsystem` (alert tiers, roster, task delegation), `UMOPersonalityComponent` (Conscientiousness/Sociability/Stability), `UMOCharacterHistoryComponent` (events/relationships), colony widgets (`UMOColonyBarWidget`, `UMOColonyOverviewWidget`, `UMOCharacterCardWidget`, `UMOColonyPortrait`, `UMOTaskAssignmentWidget`). **Start here — the design is done.**
- **Built, reusable:** `UMORecruitmentComponent` (membership state), `UMOSurvivorJobQueueComponent` (task API), `MOSurvivorController` + `BT_Survivor` + `BB_Survivor` (job AI), possession, building system (weighted parts + persistence), 122-recipe crafting + harvest, skills/knowledge + decay + genetic-memory, GUID persistence, multi-axis shelter/exposure, authoritative clock, `BP_MOMetaHuman` pawns.
- **Proven tooling:** the full `ue.py` loop (this session). Use it.

---

## 3. The Build Path (gray-box sim first)

Four phases. Each ends green in `ue.py mptest`-style verification; each is a charter "four-gate" pass (co-op-correct, legible, fun, verified).

### V0 — One villager, one real job, end-to-end *(fully autonomous)*
The vertical slice of the whole pillar. Prove the granular loop MD *doesn't* have.
- `UMOColonyManagerSubsystem` settlement record + roster; a house record on `AMOBuildableActor` (residency: pawn ⇄ house) and a workshop record (station + assigned job).
- Recruit a survivor (cheat command to start: `MO.Colony.Recruit`), assign a house, assign them to a workbench with a recipe.
- The villager AI (extend `BT_Survivor`): walk to station → pull real ingredients from communal storage → run the **real** `EnqueueCraft` path → deposit the real output. *You watch a colonist actually knap flint.*
- Verify: `MO.AI.DumpBlackboard <villager>` shows the job state; the output item appears in storage; co-op-correct (server-authoritative, `mptest`).
- **Art needed: NONE** — gray-box hut + MetaHuman villager.

### V1 — The settlement loop *(autonomous sim + editor UI)*
- Communal storage pool; **upkeep tick** (villagers eat real food from storage via metabolism; starve → leave/die).
- Housing rules (capacity, "no home → mood decay → leaves"), shelter-quality → mood via the exposure sim.
- `UMOPersonalityComponent` + mood driven by the **real** sims (hunger/pain/cold/sleep) + personality axes.
- The colony UI: `MO.Test.ClickWidget`-drivable bar + overview + character card + assignment widget.
- Verify: run a settlement of 3–4 villagers through a day; mood/needs/production move correctly; assign via the UI harness.

### V2 — Economy, progression, dynasty *(autonomous sim)*
- Production **quotas + priorities** (the MD abstraction layer, for scale) on top of V0's concrete jobs.
- **Teaching** (skilled → unskilled 2×) + **school** building (maintains the tree vs. decay).
- **Family/relationships** (`UMOCharacterHistoryComponent` graph): recruitment gating by settlement standing, population growth, the permadeath→heir generalization.
- Seasons-as-gameplay (resource windows, winter upkeep spike) on the existing clock.

### V3 — Presentation *(the asset pipeline, §5)*
- Real building/villager/prop art via the pipeline you built in parallel. Swap gray-box meshes for acquired/authored ones by **reassigning DataTable rows** — the sim never changes.

---

## 4. Why gray-box first is the autonomy play

Gray-boxing isn't a compromise — it's the mechanism that keeps the pillar inside the autonomous loop for as long as possible and **isolates the gap**:
1. V0–V2 need zero external art → you build + verify them solo, fast, with `ue.py`.
2. The *moment* you want it to look good, the asset gap is the only thing standing there — sharply defined, not tangled into sim bugs.
3. Real art becomes a **data operation** (reassign the mesh on a recipe row via MCP), which IS autonomous — so even V3's *assignment* half is automatable; only *acquisition* isn't.

---

## 5. The Asset Pipeline — the gap, and the tool to close it

The village needs art in four buckets: **buildings** (house/workshop/storage/fence/furniture, per tier), **villagers** (skeletal meshes + action animations), **props** (tools, sacks, barrels, decor), **icons** (UI for every item/building/job). Today MO57 acquires and assigns these by hand, and some ship as gray/placeholder meshes (`#151` was literally "gray meshes"). Close it in three parts:

### 5A. Make the gap QUERYABLE — the Asset Manifest *(autonomous, do first)*
You can't fill a gap you can't see. Build an **art-coverage manifest + validator**, modeled on the `MO.Test.ValidateData` pattern that already catches dangling data refs:
- A tracked list of every **art slot** each system needs: `FMORecipeDefinitionRow` → building mesh / item world-mesh / icon; villager → skeletal mesh + ABP; job → icon.
- `MO.Test.ValidateArt` (new console verb): walk the DataTables and report every slot that's **unset or pointing at a placeholder** (e.g. the engine cube, `BasicShapes`, or a known gray material). Output like ValidateData: `N slots, M missing, K placeholder`.
- Now "what art do we still need?" is one command, and V3 has a burn-down list. **This is pure autonomous C++ — build it early; it also grades the current project honestly.**

### 5B. Make ASSIGNMENT autonomous — `ue.py asset` *(autonomous)*
Once an asset is *in the project*, wiring it to the game is fully scriptable via the MCP:
- `ue.py asset import <file>` → MCP `AssetTools.import_file` (FBX/glTF/texture → uasset).
- `ue.py asset assign <table> <row> <field> <assetPath>` → MCP `rows set` (point a recipe's `StaticMesh`/icon at the imported asset) — reuses the proven one-row-at-a-time authoring.
- `ue.py asset shot <assetPath>` → load into a capture scene + `EditorAppToolset` CaptureViewport → a PNG the agent can *look at* to QA the assignment (does the axe read as an axe?). This is the visual-verification rung the loop is missing (charter Pillar-0 gap #6).
- Result: given a folder of assets, the loop imports, assigns, and visually QAs them **with no human in the loop**.

### 5C. Make ACQUISITION as autonomous as possible — two sources
This is the genuinely hard part. Two routes; use both.

**Route A — Procedural / gray-box generation (fully autonomous; do for V0–V2 + primitives forever).**
- Auto-generate a stand-in mesh for *every* building recipe from its footprint/tier via **Geometry Script** (`UDynamicMesh` — box-and-roof huts, walls, fences) or the existing build-part meshes. A `MO.Build.GenGrayboxMeshes` commandlet gives the whole village a coherent blockout with zero external art.
- Villagers: **MetaHuman** (already in use — `BP_MOMetaHuman`) is a near-autonomous character source; variation via the MetaHuman API/params.
- Props/tools: many are simple enough to Geometry-Script or kitbash from existing item meshes.
- **This route alone gets you a fully playable, legible, co-op village.** Real art is polish, not a blocker.

**Route B — Agentic Fab acquisition (semi-manual; for the A-rated V3 look).**
Fab (Epic's marketplace) has **no clean purchase/download API**, so this route is browser-driven and needs a human for the buy step — but the *evaluation* can be agentic:
- A **claude-in-chrome** playbook: search Fab for the manifest's open slots (`5A`), screenshot candidates, score them against the need (style match, tri-count, license, UE5.8 + **voxel-compatible** — many are Landscape-only), and produce a **ranked shortlist** with reasons.
- Human does the one non-automatable step: purchase / "Add to project" via the Epic launcher.
- Then `5B` takes over fully: import → assign → `asset shot` visual-QA → commit.
- **This is exactly the "highlight the gap" the brief asks for:** the agent does search + evaluate + import + assign + QA; the human does *only* the account/purchase gate. Document each Fab run's hit/miss in `AUTONOMOUS_TOOLING.md` so the browser-agent workflow hardens like the MCP loop did.

### The recommendation
Build **5A + 5B now** (autonomous, high leverage, grades the whole project's art debt and makes assignment free). Build **Route A gray-box generation** to unblock V0–V2. Defer **Route B (Fab acquisition)** to V3, when the sim has proven fun and the manifest gives a precise shopping list. When you hit either the browser-purchase wall or a "no good asset exists → build it" fork, **stop and surface it** — those are the build-tool gaps this exercise exists to find, and each one is a candidate for a new tool or a human decision, not a silent workaround.

---

## 6. First Moves for Fable 5

1. **`MO.Test.ValidateArt` (5A)** — one `cycle`; now the art gap is a number and a list. (Also: run it, log the current debt in `PROJECT_STATUS.md`.)
2. **`ue.py asset import/assign/shot` (5B)** — the assignment+QA pipeline; proves visual verification, closes charter Pillar-0 gap #6.
3. **V0 — one villager, one real job** — the sim vertical slice, on gray-box art, verified in `mptest`. This is the fun-gate for the whole pillar; do not build breadth until it's proven.
4. Then V1 → V2 on the loop; Route-A gray-box generation as needed; Route-B Fab acquisition only when V3 demands the A-rated look.

**The meta-goal:** every rung above that says "autonomous" you should actually do solo with `ue.py`/MCP — and every time you hit the amber "gap," treat it as a *tooling deliverable*, not a chore. The village is the excuse; a loop that can acquire, assign, and visually-QA its own art is the prize.
