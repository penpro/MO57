# Crafting, harvesting, items and progression (how the code does it)

> **Covers:** recipes and the crafting queue, stations, tool gates and craft time, harvesting and foraging, item instance state
> and liquids, the item / fire / world action registries, discovery, and how progression is measured. WHAT the progression should
> be is decided in `Docs/PROGRESSION_ROADMAP.md` (design authority); this doc is the implementation.
> **Current state (2026-10-17):** 126 of 136 recipes reachable from bare hands; one tool gate; water is an ingredient measured in
> carried containers; discovery is taught by inspecting precursors; heat fields exist but carry no values yet. Codex's audit
> batches A-F (inventory capacity, stack replication, equipment slots, server-derived station context, ingredient reservation,
> cancel RPCs, survivor ingredient transfer, survival meters persistence, timed wound treatment) are implemented. Survivor craft
> phase/cargo recovery also lives on the saved job, with durable crafting results. Exact validation and commit checkpoints:
> `Docs/agent/audit/SESSION_STATE.md`.
> **Entry points:** `UMOCraftingSubsystem` (validation, execution), `UMOCraftingQueueComponent` (queue; ticks on real `UtcNow`),
> `AMOCraftingStationActor`, `UMOHarvestSubsystem` (`HasToolForAction`, per-harvester context), `UMOForagingSubsystem`
> (`GetDigTableAt`, `FindGatherableCover(Kinds)`, `BeginGatherCover`), `UMOInventoryComponent` (`CountIngredient` /
> `ConsumeIngredient` / `AddIngredient`, `GetBestToolEffectiveness`), `FMOItemInstanceState` + `FMOLiquidContents`,
> `FMOItemActions`, `FMOFireActions`, `IMOWorldActionProvider`, `UMOTimedActionComponent`, `FMOItemSourceAnalysis`.
> **DataTables:** DT_Items, Recipes (`FMORecipeDefinitionRow`), DT_Resources, DT_Carcasses, DT_Skills, DT_Biomes (`DigYields`,
> species `ResourceNodeId`), DT_CaveScatter. **Dev verbs:** `MO.Test.Craft <Recipe>`, `MO.Test.GrantRecipe [PlayerIndex] [Recipe]`,
> `MO.Harvest.Do <ActionId|list> [r]`, `MO.Harvest.Enable 1`, `MO.Test.Forage dig`, `MO.Cover.Gather [r] [go]`, `MO.Item.*`,
> `MO.Fire.SpawnLit|Actions|Do|Menu`, `MO.Test.ValidateData`, `MO.Test.ProgressionMap`, `MO.Test.WorldAction list|<Id>`.
> **Open items:** `Docs/BACKLOG.md` § Crafting, progression, content. **Decisions:** DECISIONS (Oct 15 every world thing gets an action,
> starting kit; Oct 17 inspect discovery; F10 heat values).

## 1. Recipes, validation, queue

```cpp
CanCraftRecipeAtStation(RecipeId, Knowledge, Skills, Inventory, ActualStation)
  → EnqueueCraftAtStation(RecipeId, Count, ActualStation)  // repeats the authoritative checks
  → ReserveIngredients → timed queue → consume one repeat → retain/deliver products → reward/notify
```
Tool system uses `EMOToolType`; multi-tool items via effectiveness ratings. (Pattern verbatim from TECHNICAL_REFERENCE, May 2026.)

- **Craft time = CraftTime / average effectiveness of the tools used** (`GetAdjustedCraftTime`): a hammerstone (0.3) makes a 300 s
  recipe 1000 s. Compute before declaring a queue stuck.
- **The queue ticks on real `UtcNow`, NOT the game clock.** Gates can't speed it with TimeScale; read state IMMEDIATELY after a
  resume, before the craft completes and empties the queue. Partial progress survives pause/resume and save/load (H25: the anchor is
  back-dated by Progress x Duration, d8331172). Offline catch-up is capped at 4 h, the clock's cap (`Docs/Weather_And_Time.md`).
- Results distinguish failure kinds at the data layer (`FMOCraftResult`: `FailedItems` vs `DepletedItems`, `SpilledLiquid`).
- **A missing tool is named, everywhere** (crafting, harvesting, cover gathering, carcasses): `UMOItemDatabaseSettings::DescribeToolNeed` gives the
  tool and the simplest item that serves it, "Needs a tool: Hammer (Hammerstone or better)." (`FindSimplestToolItem`, cached). New tool gates
  use it instead of their own text (BR18-28).
- **A station's menu lists its own recipes** (Oct 18, N-083): `UMOCraftingSubsystem::GetAvailableRecipes(Station)` returns the recipes that
  need that station plus the station-less ones that need heat (`MinimumProcessTemperatureCelsius > 0`: any fire serves them); plain hand
  recipes stay in the hand crafting menu (the campfire used to list "Select Crude Club", a splint, a bandage, a hide blanket). Test
  `MOFramework.Crafting.StationListsItsOwnRecipes`.
- Recipe queuing and workshop batches are the approved QoL: N items at N x their real duration (`Docs/ENGINEERING_PRINCIPLES.md` tedium).
- **Execution context is server-derived from the actual station:** identity, type, completed building state, reach, line of sight,
  active/fuel state and configured process-temperature capability. Each entry keeps its station GUID; opening another menu cannot
  retarget it. A lost context suspends work and retries that station without earning blocked offline time. Harvest/build recipes and
  nonpositive/nonfinite durations cannot enter this queue. Legacy `ExecuteCraft`/`ProduceOutputsOnly` request timed work; completion
  production is private to the queue, not an instant public gameplay path.
- **Material ownership:** `MOIngredientPlans` builds joint physical/liquid plans before mutation. Queue entries save the original
  ingredient lots and per-repeat requirements; a completion consumes one repeat, installs durable products and a requested native
  completion receipt before observers. Cancellation retains exact remaining lots. Full inventories/missing vessels leave persisted
  pending products/refunds, retried without deleting them. Owning-client cancel RPCs provide intent, not a refund-policy choice.
- **Survivor crafts use the same service:** `TransferIngredientsTo` can carry the actual source vessel, preserving its GUID, wear
  and liquid profile. The saved job checkpoint owns its leg, handling interval, exact queued entry, borrowed vessels and actual product
  counts. It resumes paid work or a result completed before the controller rebound, and returns products plus borrowed vessels together.
  A blocked deposit retains cargo and retries after real handling time. Deposit and iteration acknowledgement have separate saved phases;
  the owning repeat count and fresh phase install atomically. Cancellation releases only that saved craft entry, including without AI.
  Actor references rehydrate by GUID; controller replacement does not invent another withdrawal or skip travel/handling.
- Pre-reservation saves lack material provenance: cancellation preserves an unresolved legacy record rather than inventing liquid refunds.
  Full save-slot/live-network survivor reload acceptance remains a separate gate from the reflected snapshot/controller regressions.

## 2. Stations and heat

- Stations are buildings (`bIsBuilding` recipe rows) that provide a station type; `AMOCraftingStationActor` holds fuel and active
  state (replicated; fuel burns on the server clock).
- Heat fields (Codex batch C): station building `ProvidedProcessTemperatureCelsius`, runtime
  `AMOCraftingStationActor::AchievableProcessTemperatureCelsius`, recipe `MinimumProcessTemperatureCelsius`. **No values are authored**:
  the proposed ladder is in `Docs/PROGRESSION_ROADMAP.md` §4 and waits on Wes (F10) and Codex's fuel-to-temperature rule.
- Fire: `FMOFireActions` = AddFuel.<ItemId> / Light / PutOut. Light needs fuel + Tinder01 + FireDrill01 or Branch01 x2 (DRY
  deadfall; Stick01 = green, refused); 3 min by hand / 90 s bow; `LightChance` 30 / 60 % + 6 % per level, rain x0.3, wet x0.6, cap
  95 %. Light a torch in the fire / a fire from a torch: `Docs/Light_Sources.md`. Station menu = `kit_station_menu.py`;
  `UMOUIUtils::RebuildActionButtons` builds runtime buttons for both menus.

## 3. Harvesting, foraging, digging

- **One tool gate:** `UMOHarvestSubsystem::HasToolForAction` (required tool at `MinToolQuality` via
  `UMOInventoryComponent::GetBestToolEffectiveness`) is used by the menu AND by `BeginHarvest` (before Oct 15 it was enforced only by
  the menu's button state: ore was mined with no pick).
- **Per-harvester context** (H23, 9c7178c6): `TMap<TWeakObjectPtr<AActor>, FMOHarvestContext>` keyed by the harvester; player and N
  survivors harvest concurrently; `BeginHarvest` cancels only the caller's own prior harvest; interrupts route per harvester.
- **Pattern still to fix:** the player's harvest timing is owned by a widget (`UMOHarvestProgressWidget::StartHarvest`, the
  subsystem vetoes early completion), unlike drinking, filling, boiling, fire lighting and eating, which run on the pawn's
  `UMOTimedActionComponent`.
- **E on a resource node** (KeepOnHarvest / ResourceNode_* / MOResource_*) opens the harvest menu (it used to take the item-pickup
  RPC and log "No item mapping").
- **A standing node's yields are finite** (`UMOResourceDepletionSubsystem`): each yield's count is rolled the first time it is taken from
  that node -- `InitialCountByItem` (sticks / cobbles / stones / flint 0-5, bark 10-20), else `DefaultYieldRange` (2-6 units; one harvest
  action takes one unit per `YieldsItems` entry). Until Oct 20 2026 an item missing from the map was unlimited (one 40 cm dead tuft gave
  tinder forever, a sapling deadwood forever) and a node rolled every configured item, so a tree held cobble counts it never yields and
  never read as fully depleted (its respawn never stamped). Respawn is still `RespawnHoursReal` real hours (DECISIONS F3).
  Test `MOFramework.Resource.EveryYieldIsFinite`.
- **Ground cover is gatherable by mesh** (`UMOCoverForageSettings`, Project Settings > MO57 > Cover Foraging): grass -> dry grass
  (reeds in the water-edge band), bracken -> fibre, stone scatters, hazel shoots -> straight rods, seedlings -> sticks, old logs ->
  deadwood / firewood / punk-wood tinder; entries may need a tool (sandstone + hammer). `FindGatherableCoverKinds` offers every kind
  within reach (3 m, measured to instance BOUNDS); the RPC carries the entry index; the server re-checks the exact instance and takes
  it through the removal ledger.
- **Digging** yields from the biome's dig table (`FMOBiomeDefinitionRow::DigYields`: sand on strands, subsoil clay in lowland, bog
  iron in the Rainforest) and, for terraforming, from the geology (`Docs/Caves_And_Geology.md` §4).
- Plain cover / bare ground with no mapping -> Forage menu on E (`UMOInteractorComponent::IsWorldCoverComponent`,
  `AMOPlayerController::TryOpenForageMenuAtCrosshair`): Wes, Oct 15, "every world thing gets an action".

## 4. Items: instance state, liquids, actions

- `FMOItemInstanceState` (durability + `FMOLiquidContents`) travels through inventory, transfer, save, drop and pickup.
- **Water is an ingredient measured in containers:** Water01 / CleanWater01 are liquid MEASURES (`LiquidMeasureMl` 250;
  `bMeasureNeedsSafeWater` counts only boiled fresh water). Every recipe count / consume / refund goes through
  `UMOInventoryComponent::CountIngredient / ConsumeIngredient / AddIngredient` for ordinary actions; queued crafting and medical work use
  the shared joint reservation planner. A liquid output pours into a container that holds
  nothing it would spoil; what does not fit is `FMOCraftResult::SpilledLiquid`, not "inventory full". Test
  `MOFramework.Inventory.LiquidIngredient`. Queue products that cannot be delivered remain owned and saved rather than spilling/deleting.
- **Registries, not per-widget buttons:** `FMOItemActions` (Drink / Fill / PourOut / BoilWithStones 20 min, 4 stones, 95 % kept /
  BoilOverFire 12 min / Eat on a timer), `FMOFireActions`, world actions (`IMOWorldActionProvider` + `UMOWorldActionMenu` +
  `AMOPlayerController::ServerWorldAction`, e.g. carcass butchering on the pawn's timed action). Menus, server RPCs and AI pawns read
  the same registries. A new target type = implement the interface.
- Item icons: `UMOItemDatabaseSettings::ResolveItemIcon` (+ `DefaultItemIcon`) is the one icon rule for every UI. Art pipeline:
  `Docs/Item_Art_Pipeline.md`.

## 5. Discovery and knowledge

- A recipe with `bRequiresDiscovery` is hidden until the player has its `DiscoveryKnowledgeId` at the required level; knowledge comes
  from inspecting items (`Inspection` field: `(Grants=((Id=...,bIsKnowledge=True,XPAmount=...,MaxLevel=...)))`).
- Since d6a3af69 every circular discovery knowledge has a precursor inspection grant (level 1, XP 100); the table is in
  `Docs/PROGRESSION_ROADMAP.md` §6.
- Learning methods in the vision (direct action, being taught 2x, schools prevent decay): `Docs/VISION.md`, `Docs/AI_And_Colony.md`.

## 6. Measuring progression

- `FMOItemSourceAnalysis` (run by `MO.Test.ValidateData` -> Data:Sources, and `MO.Test.ProgressionMap` -> `Saved/Logs/progression_map.json`):
  a fixpoint over resources (only rows a biome species or an MO Resource Spawner places), harvest recipes, carcass parts, digging,
  cover gathering, PCG pickups and recipe gates (ingredients, build parts, station + fuel, tools, discovery, knowledge). Reports
  orphan items, orphan knowledge, `unplaced_resources`, blocked recipes with their first gate, and a tier per item / recipe in two
  modes (today vs attempt-anything). Needs PIE. Rerun after any data change and read `unplaced_resources` first.
- **Edit content live**, never the mirrors: `python Tools/ue.py rows set <table> --file rows.json` (readback-verified; `rows get` each
  row first and add only what is missing). The `*_JSON.json` / CSV files in `Plugins/MOFramework/Content/Data` have not tracked the
  live tables since early checkpoints. `ue.py mcp dt remove_rows` deletes; Git Bash needs `MSYS_NO_PATHCONV=1`. After MCP edits:
  `ue.py refresh-data` (item + recipe static caches). `Docs/TOOLING.md` § 4 (DataTables).
- Before deleting an item id: scan all DataTables as JSON (`export_data_table_to_json_string`) + a binary .uasset grep.

## 7. Superseded

- "Flint01 / RawHide01 / DryTinder01" and 12 twin rows (Snare01/SnareWire01 ...) were merged into the referenced ids on Oct 14
  (b0753b0a, ee2fe069); the orphan rows are deleted so `ValidateData` catches anyone re-adding them.
- "Medical treatments are unwired" (Oct 15): the timed treatment service uses the shared joint ingredient planner; see
  `Docs/Medical_And_Survival.md` §7 for its request seam and explicit clinical/UI limits.
- The Feb 2026 progression (`TechProgression.txt` generated from Items.csv / Recipes.csv / Skills.csv) is archived: the live DTs replaced it.
