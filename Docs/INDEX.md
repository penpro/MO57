# Docs index

One line per live document: what it answers and when to read it. `CLAUDE.md` routes the common topics directly; anything else starts here.
Archived (dated) documents are listed in `Docs/Archive/INDEX.md`. Rebuilt 2026-10-17.

## Start here

| Doc | Answers | Read when |
|---|---|---|
| `CLAUDE.md` (repo root) | The rules every agent follows + the topic router | Every session (loaded automatically for Claude; Codex reads it via AGENTS.md) |
| `Docs/BACKLOG.md` | All open work, with ids, status and owner | Picking or adding work |
| `Docs/DECISIONS.md` | Wes's binding calls (dated, his words) and the forks waiting for him (F1-F23) | Before any design call; before re-asking anything |
| `Docs/TRAPS.md` | Symptom -> cause -> fix for everything that cost a debugging round | When something behaves strangely; before a new subsystem |
| `Docs/TOOLING.md` | Every dev entry point: ue.py verbs, nettest, build, DataTables, art tools, console verbs, Steam patch notes | Before building, testing or authoring data |
| `Docs/PROJECT_STATUS.md` | Where each track stands, key numbers, strengths to preserve | Orientation |
| `Docs/ENGINEERING_PRINCIPLES.md` | The 11 principles in full, the tedium rule, verification lessons | Before fixing a bug or adding any instant / auto mode |
| `Docs/VISION.md` | What the game is: pillars, pawns, progression, lore, phases, the A-rated bar | Any design or scope question |

## Systems (one doc per system; each opens with a current-state header)

| Doc | Answers | Read when |
|---|---|---|
| `Docs/Terrain_Generation_Exploration.md` | The C++ world generator (relief, coasts, rivers, lakes, biomes, spawn), the terrain ground material, the dated generator log | Terrain, biomes, terrain material work |
| `Docs/World_Creation.md` | A world's creation settings: shape knobs (biome size, height, mountains, lakes, caves, vegetation), pace multipliers, the starting biome; the New Game panel | World creation, new game, multipliers |
| `Docs/Caves_And_Geology.md` | Cave shape / gates, bedrock and ores, dig yields and dig time, cave PCG, outcrops | Caves, ore, digging |
| `Docs/PCG.md` | Vegetation tiers, chokepoints, density rules, palettes, PCG perf, removal ledger, the world reveal | Any PCG / vegetation / removal / loading work |
| `Docs/World_Features_Architecture.md` | POI compositions (Electric Dreams assemblies, spires), the placer, terrain under compositions | New world content, cliffs, POIs |
| `Docs/Water.md` | Ocean and lakes (Oceanology), swimming, breath, underwater look, shores | Water, swimming |
| `Docs/Water_And_Illness.md` | Drinking, salt water, waterborne illness, containers, boiling (with research) | Drinking / illness |
| `Docs/Light_Sources.md` | Torches, flames, eye adaptation, cave and night darkness, light below the horizon | Light, exposure, darkness |
| `Docs/Weather_And_Time.md` | The clock, offline catch-up, UDS / UDW + bridge, weather director, co-op weather sync | Weather, time, sky |
| `Docs/Medical_And_Survival.md` | Medical cascade, death path, survival meters, thermal model, breath, illness, treatment | Medical / survival work |
| `Docs/Crafting_And_Progression.md` | Recipes, queue, stations, tool gate, harvesting, item state / liquids / action registries, discovery, progression checks | Crafting / harvesting / items code |
| `Docs/PROGRESSION_ROADMAP.md` | **The design authority** for technology progression: eras, heat ladder, materials, discovery precursors, gaps | Before adding or changing any item, recipe, station or knowledge |
| `Docs/Building.md` | Placement, server-driven construction, build-state look, building mode, creative instant building | Building work |
| `Docs/Farming_And_Husbandry.md` | Crops (seed, field, growth, harvest) and domestic animals (capture, pens, taming, breeding, products): design and build order | Farming, livestock |
| `Docs/Movement_And_Traversal.md` | Vault / hop up / pull-up and climbing (cliffs, rocks, trees): the rules, the co-op prediction, the costs | Movement, traversal, climbing |
| `Docs/Terraform_Excavation_Plan.md` | Player terraform + designation-based pawn excavation (plan and stage record) | Terraform / excavation |
| `Docs/AI_And_Colony.md` | Creatures, survivor jobs, the village loop, recruitment, possession, colony UI plan | AI, survivors, colony |
| `Docs/Coop_And_Networking.md` | Sessions, pawns in co-op, per-machine terrain, spawn settle, RPC rules, interaction targeting, co-op testing | Anything multiplayer |
| `Docs/UI.md` | CommonUI architecture, input mode, activation, HUD, UMG traps, the UI kit, dev console | Any UI work |
| `Docs/Persistence.md` | Save domains, identity, generator identity in saves, crash / bug reports | Saves, identity, bug reports |
| `Docs/Audio.md` | Audio subsystem, heartbeat meter, NaN guard, backend, footsteps | Audio reports or sound work |
| `Docs/Packaging.md` | Development vs Shipping, cook traps, ShipDiag, packaged settings / saves | Packaging, Shipping-only bugs |
| `Docs/Performance.md` | Targets, measuring protocols, quality tiers, findings | Perf work or any FPS claim |
| `Docs/TECHNICAL_REFERENCE.md` | Modules + load-bearing patterns, conventions, subsystems / components / interfaces, replication, perf guidelines, plugins | Writing new C++ systems |

## References and tools

| Doc | Answers | Read when |
|---|---|---|
| `Docs/AUTONOMOUS_TOOLING.md` | Deep manual of the build / verify loop (bridge, MCP, ue.py, multi-process tests, packaged probes, hit/miss history) | When TOOLING.md's one line isn't enough |
| `Docs/UI_TOOLING.md` | Building / checking / exercising Widget Blueprints from spec files (`ue.py ui`) | Any UMG change |
| `Docs/UI_Kit_Implementation.md` | The Field Ledger kit in the game (tokens, 9-slices, fonts, gallery, kit traps) | UI look work |
| `Docs/UI_Screen_Inventory.md` | Every widget Blueprint: kit spec, native parent, input mode / capture, focus, who uses it (generated: `Tools/ui_inventory.py`) | Planning UI work, retiring widgets |
| `Docs/Agent_PIE_Testing.md` | The June / July bridge playbook (still the transport under ue.py) | Low-level bridge work |
| `Docs/Voxel_Plugin_Reference.md` | Voxel plugin reference (graphs, stamps, materials, PCG, sculpting, determinism) + our integration | Voxel graph / stamp work |
| `Docs/Item_Art_Pipeline.md` | Fab scan -> item mesh -> tint / kitbash -> icon studio; Fab panel traps | Giving an item art or an icon |
| `Docs/Technology_History.md` | How technology really advanced (heat ladder, materials, mining) with sources | Progression / heat / mining design |
| `Docs/ElectricDreams_Lessons.md`, `Docs/ElectricDreams_Migrated.md` | What to take from Epic's Electric Dreams sample; what was migrated | Environment / PCG references |
| `Docs/DarkRuins_Assets.md` | The Dark Ruins Megascans sample: the cliffs migrated (gitignored, `ede_migrate.py --source darkruins`), the western cliff, candidates for rocks, items and ruins | Environment / PCG references |
| `Docs/MedievalDynasty_Village_Reference.md` | The village-loop north star mapped to MO57 | Colony / village design |
| `Docs/MO57_Colony_Management_Design.docx` | The colony management design (Word) | Colony UI / personality design |
| `Docs/Engine_Patches.md` + `Docs/EnginePatches/` | Engine-side bugs: evidence, diffs, apply / verify / revert, upstream report | Before touching engine source |
| `Docs/PAUSE_POLICY.md` | No pause, ever: policy, rationale, enforcement | Before anything that pauses |
| `Docs/STEAM_RELEASES.md` | Date / commit of each Steam patch (anchor for patch notes) | When Wes asks for Steam patch notes |
| `Docs/Survival_Single_Source_Proposal.md` | Fork brief F1 (survival meters single source) | Deciding / building F1 |
| `Docs/Thermal_Model_Reproduction.md` | Fork brief F2 (heat balance) | Deciding / building F2 |
| `Docs/Ground_Coverage_2026-10-14.plan.json` | Live palette plan data read by `Content/Python/biome_palettes.py` (not a doc) | Palette regeneration |
| `Docs/CS399/` | Wes's CS 399 practicum: plan, historical closure, weekly progress (every Sunday), final report skeleton | School deliverables |
| `Tools/UE_CSV_FORMAT.md`, `Tools/terrain_lab/README.md`, `Content/Python/README_UI_TOOLS.md`, `SteamArt/README.md`, `DesignKit/MO57_UI_Kit/MO57_UI_Kit/README.md` | Tool-local references | Using that tool |
| `README.md`, `Plugins/MOFramework/README.md` | Public project overview; plugin overview for modders (partly June-era) | New contributors / modders |

## Other agents' working state

| Doc | Answers |
|---|---|
| `Docs/agent/` (Codex) | Codex's audit records: UI consolidation (`ui/`: SESSION_STATE, MIGRATION_PLAN, TOUCH_LOG, findings), the July repo / MCP audit, and the Oct 16-18 remediation (`audit/SESSION_STATE.md`, `audit/MEDICAL_HANDOFF.md`). Finished knowledge lands in the system docs above (Wes N7). |
| `Docs/Deep_Dive_Top_10_Flaws_2026-10-16.md` | Codex's historical ranked source audit; subsequent remediation evidence is in `Docs/agent/audit/SESSION_STATE.md` (BACKLOG N-001) |
| `Docs/Archive/Handoff_2026-10-13.md` | Dated handoff, extracted; archived with its source-comment reference during the audit handoff (BACKLOG N-004) |
| `claude-codex-coop.md` (repo root) | The append-only Claude-Codex coordination log (older entries in `Docs/Archive/coop/`) |
