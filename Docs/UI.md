# UI

> **Covers:** the CommonUI architecture (layers, input modes, activation), UI controllers, HUD behaviour, the "Field Ledger" UI
> kit, the CommonUI and UMG facts that cost diagnoses, the dev console and developer menu. How to BUILD and test Widget Blueprints:
> `Docs/UI_TOOLING.md`. The kit itself: `Docs/UI_Kit_Implementation.md` + `DesignKit/MO57_UI_Kit/`.
> **Current state (2026-10-17):** CommonUI with 5 layer stacks in `WBP_PrimaryGameLayout`; every UI surface uses Menu input mode;
> pushes activate synchronously (`LayerTransitionSeconds` 0); the kit's tokens / buttons / icons / 9-slices are live and most menus
> were restyled on Oct 12 (fonts are stand-ins until the OFL fonts are downloaded); Codex's UI consolidation stages 0-3 are done,
> stage 4 (skills / knowledge + quest collections) is next (`Docs/agent/ui/`).
> **Entry points:** `UMOGameUIManagerSubsystem`, `UMOPrimaryGameLayout`, `UMOActivatableWidget` (`GetDesiredInputConfig`,
> `NativeOnActivated` / `NativeOnDeactivated`, `AssertInputConfig`), `UMOUIManagerComponent` (orchestrator, `UpdateReticleVisibility`,
> `RegisterHUDElement`, `RefreshGameplayMode`), `UMOUIControllerBase` (`CloseCachedMenu`, `IsCachedMenuOpen`) + 6 controllers,
> `UMOGameplayInputStub::ApplyGameplayInput`, `UMOUITokens`, `UMOCommonButton` / WBP `MOButton`, `AMOPlayerController::InputKey`
> (Tilde console, F1 HUD mode), `FMODevMode`. **Dev verbs:** `MO.UI.Enable`, `MO.UI.Open <menu>`, `MO.UI.Show <class>`,
> `MO.UI.SkipIntro`, `MO.UI.RunTest <name>`, `MO.UI.RunAllTests`, `MO.HUD.Mode`, `MO.Test.FindWidget|ClickWidget|HitTest|PressKey`,
> `MO.Console.Run`, `devmenu [Group]`. **Tests:** `MOFramework.UI.*`, `MOFramework.UITokens.*`, the live CommonUI suite (`CommonUI.*`,
> `HUD.*`), `Tools/tests/test_ui_tooling.py`. **Open items:** `Docs/BACKLOG.md` § UI. **Decisions:** DECISIONS 2026-10-11 (one cohesive
> design system), 2026-10-14 (dev mode in Shipping), 2026-10-09 (tutorial text yields to menus).

## 1. Architecture

### CommonUI architecture (TECHNICAL_REFERENCE, May 2026, verbatim)

### Layer Stack

| Layer Tag | Z-Order | Purpose | Example Widgets |
|-----------|---------|---------|-----------------|
| `MO.UI.Layer.HUD` | 0 | Always-visible | Reticle, Colony Bar, Mode Indicator |
| `MO.UI.Layer.Game` | 50 | One-at-a-time gameplay menus | Inventory, Crafting, Building, Skills |
| `MO.UI.Layer.GameOverlay` | 100 | Overlays on gameplay | Context menus (non-stack), Progress bars |
| `MO.UI.Layer.Menu` | 150 | System menus | In-Game Menu, Possession Menu |
| `MO.UI.Layer.Modal` | 200 | Modal dialogs | Confirmation dialogs |

**CRITICAL**: Colony bar MUST be HUD layer. Game layer only shows one widget at a time.

### Widget Hierarchy

```
UCommonActivatableWidget
    └── UMOActivatableWidget (base: bAutoRestoreFocus, GetDesiredInputConfig)
            ├── UMOMenuWidget (ECommonInputMode::All, bIsBackHandler)   [SUPERSEDED 2026-10-17: every UI surface returns ECommonInputMode::Menu; see section 2]
            │       └── UMOMenuWidgetBase → all gameplay menus
            ├── UMOModalWidget (ECommonInputMode::Menu, bIsModal)
            │       └── UMOInGameMenu
            └── UMOConfirmationBase, UMOProgressWidgetBase, etc.

UCommonUserWidget
    ├── UMOPrimaryGameLayout (layer stack container)
    ├── UMOContextMenuBase (ephemeral popups, NativeOnKeyDown for Escape)
    │       └── Item, Ground, Ghost, Station, Survivor, KeepOnHarvest
    └── UMOStatusField

UUserWidget — 32 non-interactive content panels (entry widgets, grids, info panels)
```

### Input Flow

```
Key press → CommonUI ActionRouter → bIsBackHandler → NativeOnHandleBackAction()
  → RequestClose() → OnRequestClose delegate → UIController closes menu
  → Widget popped from stack → CommonUI restores input from next widget
  → Stack empty → WBP_GameplayStackStub provides gameplay input config
```

### UI Pitfalls (Quick Reference)

Full pitfall list: section 5 below (from the archived `Docs/Archive/UI_Overhaul_Architecture.md`).

1. `ECommonInputMode::All` leaks WASD — need IMC_MenuToggle context swap (M8) **[SUPERSEDED: fixed by Menu mode for every surface; zero `::All` constructions remain in live code]**
2. `FUIInputConfig` members protected in UE5.7 — use constructors
3. `PushWidget` auto-activates in UE5.5+ — never call `ActivateWidget()` after push
4. Missing `RootContentWidgetClass` — causes input death after all menus close
5. `IsAnyMenuOpen()` must check ALL layers — Game + Menu + Modal (M9)
6. Widget re-creation GC pressure — use reset-state pattern for frequent menus (L5)
7. No dedicated server guards — check `IsRunningDedicatedServer()` (M10)

### Debug Commands

```
CommonUI.DumpActivatableTree    ; Dump widget activation tree
CommonUI.DebugInputRouter 1     ; Show input routing decisions
Slate.ShowFocusedWidget 1       ; Show focused widget
```


### UI controller architecture (TECHNICAL_REFERENCE, May 2026, verbatim)

Sibling components on AMOPlayerController. Find each other via `GetOwner()->FindComponentByClass<T>()` with weak pointer caching.

| Controller | Key Methods |
|------------|-------------|
| `UMOUIControllerBase` | Input modes, modal background, pawn caching |
| `UMOCharacterUIController` | ToggleSkillsPanel, TogglePlayerStatus, StartItemInspection |
| `UMOBuildingUIController` | ToggleBuildingMenu, ShowGhostContextMenu, ShowBuildWidget |
| `UMOCraftingUIController` | ToggleCraftingMenu, ShowStationContextMenu, StartHarvestOperation |
| `UMOSystemMenuUIController` | ToggleInGameMenu, TogglePossessionMenu, ShowConfirmationDialog |
| `UMOInventoryUIController` | ToggleInventoryMenu, OpenInventoryWithContainer, ShowItemContextMenu |
| `UMOQuestUIController` | ToggleQuestLog, UpdateQuestHUD |

### Standard UI Delegates (MOUIDelegates.h)

Generic: FMOUIRequestClose, FMOUIActionTriggered, FMOUIProgressUpdate, FMOUIVisibilityChanged
Crafting: FMOUICraftRequest, FMOUIRecipeSelected
Inventory: FMOUISlotClicked, FMOUIDragStarted, FMOUIDropCompleted
Building: FMOUIBuildRequest

### UI widget patterns (CLAUDE.md until 2026-10-17, verbatim)

**BindWidget Meta** - Blueprint/C++ widget binding:
```cpp
// Required binding (compile error if missing in Blueprint)
UPROPERTY(BlueprintReadOnly, meta=(BindWidget))
TObjectPtr<UScrollBox> ContentScrollBox;

// Optional binding (null-safe, no error if missing)
UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
TObjectPtr<UMOCommonButton> OptionalButton;
```

**Common UI Button Binding**:
```cpp
// In NativeConstruct()
if (MyButton)
{
    MyButton->OnClicked().RemoveAll(this);  // Prevent duplicate bindings
    MyButton->OnClicked().AddUObject(this, &UMyWidget::HandleButtonClicked);
}
```

## 2. Input: Menu mode, toggles, focus

- **Menu input is Menu mode, NOT All.** `UMOActivatableWidget::GetDesiredInputConfig` returns `ECommonInputMode::Menu` (NoCapture,
  cursor visible) for every UI surface; passive overlays (progress, HUD) override to Game. Toggle-key-while-menu-open works through the
  UI controllers' frame-debounce and `AMOPlayerController::TryCloseMenuForToggleKey` (the open key closes the menu, called from the
  widget's preview handler), NOT via `::All`. Tab / Escape are handled in `NativeOnPreviewKeyDown`, not CommonUI's back-action chain.
  New menus inherit an activatable base and push to the right layer.
- **Gameplay input only through the router:** `UMOGameplayInputStub::ApplyGameplayInput` (router + engine call). A direct
  `PlayerController::SetInputMode` desyncs `UCommonUIActionRouterBase` (it skips a config equal to its cached one), and the router adopts a
  newly activated widget as root only once it was PAINTED above the current root, so `UMOActivatableWidget::AssertInputConfig` runs on
  activation and when the menu above closes (fix for "crafting opened from inventory closes on any click", 58859243).
- **Focus after window re-activation:** the base `NativeGetDesiredFocusTarget` falls back to self and the UI manager reclaims focus on any
  non-mouse move to the viewport (window re-activation moved Slate focus to SViewport and Menu mode then blocked the game's keys).
- **Input actions are handled in C++** in `AMOPlayerController` (`SetupInputComponent`; raw keys Enhanced Input can't bind in `InputKey`).
  Key map (IMC_MODefault): I inventory, K skills, Esc / Tab back-menu, C craft, B build, E interact, P possess, Y status, T terraform
  toggle, R terraform cycle, F1 HUD mode, LeftCtrl crouch, LeftShift hustle, Space jump, Tilde dev console.
- **Back closes the topmost thing, context menus first** (Oct 19): a context menu (`UMOContextMenuBase`: forage, ghost, station, survivor,
  keep-on-harvest, world action) closes itself on Esc / Tab while it has keyboard focus; when focus drifted to the viewport,
  `AMOPlayerController::HandleBackMenu` asks `UMOContextMenuBase::CloseNewestOpenFor` first (every context menu counts itself open from
  NativeConstruct to NativeDestruct). Before, Back opened the system menu over the popup.
- **The forage (ground) menu closes when the player walks away** (Oct 19): movement keys still move the pawn under a Game-and-UI popup, and the
  hidden cursor stays on it, so it never timed out; it now closes once the pawn is 3 m from where it opened (`UMOGroundContextMenu::CheckMousePosition`).

### Gamepad look (Oct 20)
`IMC_MODefault` maps the right stick to `IA_MOLook` and the mouse to `IA_MOMouseLook` (Negate Y); the controller bound only the mouse, so a gamepad
could not turn the camera (nor finish the Look tutorial). `AMOPlayerController::GamepadLookAction` (IA_MOLook, loaded by path when unset) ->
`HandleGamepadLook`: the held deflection x `GamepadLookRate` x the frame time, Y flipped like the mouse's, into the same `ApplyLookInput`.
The rate is in the mouse's look units (this project keeps the engine's legacy 2.5 deg a unit): 64 = 160 deg/s, measured 161 yaw / 162 pitch.
Test `MOFramework.Input.GamepadLook.StickTurnsTheView`.

## 3. Activation, stacks and the HUD

- **Pushes activate synchronously:** `UMOPrimaryGameLayout::LayerTransitionSeconds` = 0 (the engine default 0.4 s left a pushed widget
  pending; a close in that window orphaned menus). Close a cached widget with `UMOUIControllerBase::CloseCachedMenu`, never a hand-rolled
  `IsActivated()` gate; `PopWidgetFromLayer` closes a pushed-not-yet-active widget the tick after it activates (`DeferCloseUntilActivated`).
  **Never `Stack->RemoveWidget(pending)`** (a stuck active entry; every later test waits 3 s); `RemoveWidget` is fine for a covered entry.
  Test `CommonUI.CloseBeforeActivation`.
- Opening one menu closes the others on the Game layer (a SWITCH, not a stack). Same-frame close + reopen legitimately fails (CommonUI
  reconciles next tick, finding F21): frame-dependent UI tests go in frame-stepped sequences, not synchronous bodies.
- **HUD yields to menus:** `UMOUIManagerComponent::UpdateReticleVisibility()` (the reticle chokepoint) ->
  `UMOQuestUIController::SetHUDYieldedToMenus` (tracker) + `UMOTutorialHintWidget::SetSuppressedByMenu`. `UMOActivatableWidget::NativeOnActivated`
  calls `RequestUpdateReticleVisibility()` (controllers call `UpdateReticleVisibility()` synchronously after a push, when the menu count
  is still 0): don't remove it. A new HUD element that should yield hooks `SetHUDYieldedToMenus`.
- **HUD modes:** F1 cycles Full -> Minimal -> Off. A HUD widget joins with `UMOUIManagerComponent::RegisterHUDElement(widget, bVital)`;
  the mode drives the widget's ColorAndOpacity ALPHA (the one channel no HUD widget sets itself). A new HUD widget must register or it
  ignores F1. Test `MOHUDModeTests`.
- Tutorial hints: a triggered tutorial outranks a pending one; `GetActiveTutorialHint` orders by `SortOrder` (quest SortOrder default =
  last); the first hint (possession) is gated on the first recruit.
- A hint that teaches an optional skill must not outlive the moment: `Tutorial_BuildControls` (the "Rotate and Flip" hint) has an OPTIONAL
  sequential Rotate objective and a required non-sequential PlaceGhost objective, so the quest completes when the first ghost is placed,
  rotated or not (Oct 18, N-083: the hint used to come back after the first build and stay). Test
  `MOFramework.Quest.TutorialHint.FreshGameStartsWithMovement` reads the live DT_Quests.
- Its first step, "Open the build menu" (event `OpenedBuildMenu`), fires when the B key OPENS the menu (`AMOPlayerController::HandleBuild`; Oct 19:
  only entering placement fired it, so the tracker sat at 0/1 with the menu open). Entering placement still fires it.
- **An item objective may name a kind of item** (Oct 19): `ItemPickup` / `ItemCraft` / `ItemDrop` match the item id OR one of the item
  definition's `Tags` (`UMOQuestSubsystem::ProcessEventForObjectives`). "Collect Fallen Branches" targets `Stick` (Stick01 green branch, Stick02
  stripped branch -- the ground has both; only Stick01 counted before) and "Harvest Bark" targets `Bark` (Bark01, BirchBark01, OakBark01).
  A new member of a kind needs only the tag on its DT_Items row. Test `MOFramework.Quest.ItemObjectiveCountsItsKind`.
### In-game menu: Save, Load and the exits (one save per world, Oct 17)
- **Save** (`UMOInGameMenu::HandleSaveClicked`) saves this world to its own slot immediately: `OnSaveRequested` with an empty slot name ->
  `UMOSystemMenuUIController::HandleSaveRequested` -> `SaveThisWorld`, plus a toast ("World saved" / "The host saves this world" /
  "Could not save the world"). There is no Save panel any more, no Save As and no overwrite confirm. `WBP_SavePanel` is still a child of the
  menu's focus switcher but unreachable (BACKLOG N-159); if it is shown, it still only saves this world.
- **Load** lists other worlds: `UMOInGameMenu::HideCurrentWorldInLoadPanel` calls `UMOSaveSlotListPanel::SetHiddenSlot` before every
  refresh. The main menu's Load panel is a separate instance and lists every world.
- **Exits** save first with no dialog (`Docs/PAUSE_POLICY.md` "Leaving a world"). The only dialog is "Could Not Save -- Leave Anyway / Stay".
- `bFilterToCurrentWorld` on the list panels is a same-MAP filter (every world on a map shares the identifier), not a same-world one.
- **The lists are built when a panel is SHOWN** (Oct 22), never in `NativeConstruct`: every listing reads each save's metadata (a whole
  world save the first time; `UMOPersistenceSubsystem` keeps it per slot while the file is unchanged), and the main menu constructs its
  hidden Load panel at boot (it read every save twice before the title screen). A new show path must call `RefreshSaveList`.
### Options: Apply never changes the display unasked (Oct 28)
- A Steam player (Draklamar): turned the music down, pressed Apply -> the 1080p screen went to 2560 x 1440 with the Apply button off screen and
  no way back; the Options list showed 2560 x 1440 from the start. Cause: the engine's default is BORDERLESS fullscreen, which draws at the
  desktop's size and never reads the stored resolution (so it can hold anything), and the Fullscreen box applied EXCLUSIVE fullscreen at that
  stored size on every Apply. Fix (`UMOGameSettings::WindowModeForFullscreenBox / ResolutionForMode / DisplayChanged`, test
  `MOFramework.Graphics.ApplyKeepsTheDisplay`): "Fullscreen" is borderless at the desktop's size (exclusive stays only if already in use);
  the panel shows what the screen draws at now and Apply touches the display only when the mode or a window's size changed; a window never
  exceeds 90 % of the desktop; exclusive never exceeds the desktop; `ValidateSettings` puts a bad stored size right on load. The resolution
  list is greyed in borderless (it only sizes a window).
- The panel is built once with its menu: it now reads the live settings every time it is shown (`ShowOptionsPanel` in the main and in-game
  menus), or Apply saved back what the settings were when the menu was built.
- The same player's music came back loud after a restart: fixed Oct 25 (629327e1, "slider classes held": a reloaded sound class reset to full
  volume) but not in the Oct 16 Steam build. Check: `python Tools/check_options_display.py` (the player's state reproduced, Apply, the display
  and the saved settings read back; music saved at 10 % then a restart: on the music class at boot and after a garbage collection).

### Main menu: New Game creates a world (Oct 22)
- `WBP_NewGamePanel` (960 x 860, scrolls): name, seed, the starting-biome cards, the World and Pace knobs, Defaults / Back / Start. The rows
  and cards are made at run time from the knob table and DT_Biomes (`Docs/World_Creation.md`); spec `Content/Python/ui_specs/world_creation.py`.
### Character creator: hair, beard and mustache styles; orbit; no outside close (Oct 25)
- `WBP_CharacterCreatorPanel` (`UMOCharacterCreatorPanel`, spec `kit_character_creator.py`) frames the new game's first survivor in the world.
  Rows: name, skin tone, hair colour, red in the hair, eyes, then **Hair style / Beard / Mustache** pickers ("<  Side swept fringe (medium)  >";
  None -- "Bald" for hair -- first, then the library short to long). Face and body stay "coming soon".
- Oct 26 (Wes: "make the existing character 'femme' and mark 'masc' as a customization coming soon"): "Body type: Femme" is the first
  Appearance row (the one body there is), and "Masc body type -- Coming soon" joins the Coming soon rows. The rows were rebuilt the
  spec's documented way (remove the rows below, `ui build`).
- **Oct 27: the Base picker and the build sliders** (Wes: "i made a few bases with the new metahumans stuff, see if you can get them all
  customizable and out into the game"). "Body type" became **Base** ("<  Jalena  >": `UMOSurvivorBaseSettings` in `Config/DefaultGame.ini`,
  `MOSurvivorBase.h`: Femme (original) -- the survivor Blueprint's MH_F_1 -- then Jalena, Zuri, Harvey and Trey since Oct 27 night, Wes:
  "drop lillian for now and add the four variants i made, 2 male 2 female"; Lillian's line is commented out in the config), with **Bust / Hips and butt / Waist / Thighs** sliders
  under it (-1..1, 0 = the base as made; the body shapes, `Plugins/MOBodyShape`). A base puts on its body and face, its own grooms (hair,
  eyebrows, eyelashes, and a man's beard and mustache -- a base without them takes the last one's off; the Hair style row names a base's own
  hair by its library namesake -- Zuri's reads "Coil buzz cut (short)" -- and
  the arrows step from there; every library style has a binding to every base's face; a swap keeps the Blueprint's strands at every distance,
  `UMOAppearanceSubsystem::KeepGroupSettings`, TRAPS §12) and its body: the
  survivor starts with the body fat and muscle the base was made with (`AMOMetaHumanCharacter::ApplyBaseStartingBody`). Everything rides the
  look (`FMOCharacterAppearance::BaseId`, `BodyBuild`): replicated, saved. The Body "coming soon" row went; Face and Masc stay. The whole panel was
  rebuilt (`ui remove ... CreatorPanel_Size`, `ui build`) and moved up 60 px for the four rows.
- The styles are the MetaHuman Creator groom library: 38 hair, 16 beards / goatees, 16 mustaches under
  `/Game/MetaHumans/Common/Optional/Grooms/GroomAssets` (`UMOAppearanceSubsystem` catalogs `HairContentPath`; names by `MakeGroomDisplayName`).
  A choice is a groom path in `FMOCharacterAppearance` (`HairAssetPath`, `BeardAssetPath`, `MustacheAssetPath`; empty = the Blueprint's own
  groom, `FMOCharacterAppearance::NoGroom` = none), set on the pawn with `AMOMetaHumanCharacter::SetAppearanceLook` (replicated, saved; a save
  restores it, a joiner sees it). `UMOAppearanceSubsystem::ApplyGroomsToCharacter` swaps each slot's groom component (Hair, Eyebrows1, Beard,
  Mustache; it creates the beard / mustache components the survivor Blueprint lacks) with the groom's binding to the face (`FindGroomBinding`).
- **No bald gap:** a slot keeps its old groom until the new groom and binding can draw (`UGroomAsset::IsValid`, `UGroomBindingAsset::IsValid`);
  `AMOMetaHumanCharacter::ApplyGrooms` retries every 0.2 s with the latest look (30 s timeout, then it swaps anyway) and re-tints the new groom.
  The panel preloads the styles either side of each current one (`PreloadGroomNeighbours`), so a step usually swaps at once (Wes: "it takes
  like 5-10 seconds for hair changes to take effect, until it updates they look bald").
- **The world behind the panel is `OrbitArea`** (a transparent, hit-testable Border first in the canvas): a left or right drag orbits the camera
  round the survivor (yaw free, pitch -15..40 deg, 0.35 deg per pixel), the wheel zooms 110-420 cm, and the camera is swept out of any slope or
  trunk behind them (`PlacePreviewCamera`), and pulled in front of any grass or foliage between it and the face (N-213, Oct 29; Wes, Oct 25:
  "there needs to be a 'can see face' check ... and that there isn't shit blocking the face"): plants have no collision, so the sweep passes
  through them; `PullInFrontOfPlants` walks out from the head and stops the camera before the first plant instance on the line (never
  closer than 60 cm). The plants round the survivor are gathered again on the 0.5 s timer (they stream in after the creator opens) and the
  camera placed again when they change. Checked Oct 29 (`Tools/check_traversal_world.py` opens the creator): a plant 60 cm from the face, the
  camera pulled in from 264 to 60 cm, the line clear. How close that looks is on Wes's by-eye list. A click there never reaches the world, and the panel has `bClosesOnOutsideClick = false` too (Wes:
  "if you click into the world ... it closes the character builder"; "click drag the world to pivot the camera around the character").
- **The camera frames the FACE, and finds it again** (Oct 27; Wes: a swap "sometimes pivots the camera to the back of the character ... make
  sure there's a check when swapping that it re-finds face"): the orbit's zero is where the head looks (`UMOCharacterCreatorPanel::FaceYawDeg`:
  the body's head bone against its reference pose, so a turned actor, mesh or pose all count), not the actor's yaw taken once at opening. A
  base swap re-aims in front of the new face 0.2 s later (its first pose; the orbit back to the front); the 0.5 s timer re-aims, keeping the
  player's orbit, whenever the survivor has turned over 30 deg or moved over 50 cm from the framing (a load's re-grounding). Checked by
  `Tools/check_creator_bases.py` (after a swap; the survivor turned 180 deg behind the camera's back).
- Randomize picks colours and a hair style (never bald) and takes any beard / mustache off.
- **Hair physics runs only near the world origin (`MO.Hair.Simulation 2`, the default since Oct 26; it was 0 = off): the engine's hair
  simulation jitters far from the origin** (Wes, Oct 25:
  the simulated strands -- the shipped updo's bangs, a whole bob -- "shifting back and forth", "fluttery", a blur halo; the buns, which don't
  simulate, steady; "they just stopped" with physics off). `UNiagaraDataInterfaceHairStrands::ComputeWorldTransform` casts the groom's and the
  head bone's world transforms to float before taking their relative transform "in double precision": at the start sites, ~17 km out (single
  precision ~1 mm), the simulation's frame jumps by rounding error every frame. Measured with a bangs patch tracked against the face over
  consecutive frames (`Tools/motion_probe.py capture` + the per-frame step of hair minus face): 0.8-0.9 px per frame with a lag-1
  autocorrelation of -0.4 to -0.5 at the start site; 0.19-0.21 px, positive (= physics off) with the same groom 5 m from the origin. Engine
  patch staged in `Docs/Engine_Patches.md`; physics back on needs it (DECISIONS F-fork). Wrong turns, each measured live: a bend-damping floor
  (0.05 halved it far out but did nothing at the origin -- reverted to the authored values), `r.HairStrands.SwapType 3` (the 5.4 forum fix for a
  lagging groom), `tick.AnimationDelaysEndGroup 0`, the groom's tick group, the face at LOD 0, the face's own animation, no head velocity fed to
  the solver, anti-aliasing, motion blur; a world-space simulation doubled it (all its positions absolute). Since Oct 26 the start is the world
  origin: the same start measures 0.200 px with physics ON (0.193 OFF), so physics near the colony needs no engine patch. Fork F39 option D
  (Wes, Oct 26: "build F39 option D"): a groom simulates while every coordinate of it is within `MO.Hair.SimulationRadiusKm` (5) of the
  origin -- the jitter measured against the distance breaks where a float's step doubles, 5.24 km on a coordinate (`Docs/Engine_Patches.md`
  #3) -- and stops past it, back on 0.5 km inside (no flapping). `UMOAppearanceSubsystem::WantsHairSimulation` (unit-tested,
  `Appearance.HairSimulationNearOrigin`) decides; each MetaHuman pawn re-checks every `HairSimulationCheckSeconds` (1 s); a groom that
  crosses the line is rebuilt once (a one-off settle of the strands). 0 = never, 1 = always (for A/B).
- **The hair looks copper / gold in sunlight now** (Wes, Oct 26: "the hair is super shiny and gold now?", then "looks good up close, but at
  a distance it looks really odd ... an LOD change issue?"): the colour values are unchanged (melanin 0.8, redness 0.25), the LOD is still
  forced to 0, and the strands shade the same at any distance from the origin (measured in open sky, `Docs/Engine_Patches.md` #3). It is the
  scene: the new start is an open, sunlit clearing (the origin shift re-rolled the plants there) and melanin 0.8 in full sun reads copper,
  an orange blob at a distance. Whether the default should be darker is a look call (BACKLOG, Wes: check by eye).
- **For a hair-length option later** (Wes: "keep note of the stuff that makes the hair longer"): raising the strands' stretch damping (0.2+)
  let gravity pull the bangs visibly longer (simulated strands only) -- a sag, not a clean length change. The proper control is `UGroomComponent::SetHairLengthScale` /
  `SetHairLengthScaleEnable` (the groom's own length scale, needs the groom's deformation flags), not the physics.
### Inventory menu: the transfer buttons follow the visible panel (Oct 18)
- `UMOUnifiedInventoryMenu::GetTransferTarget()` is the one rule for "<" / ">" / "<<" / ">>": the open container on its tab (all four), the
  ground on Nearby ("<<" picks up all, ">>" drops all), nothing on Gear / Details (all four disabled). Never act on a container the panel
  doesn't show (BR18-27: ">>" on Gear emptied the pack into a basket off screen).
- The menu is pooled: `InitializeMenu` with no container calls `ClearSecondaryInventorySource()`, and `UMOInventoryUIController` drops its
  `CurrentContainerActor` whenever the menu deactivates uncovered (Escape included), not only in `CloseInventoryMenu`.
### HUD: skill / knowledge toast (Oct 20)
- Skill and knowledge changes show `UMOSkillToastWidget` (`WBP_SkillToast`, spec `ui_specs/kit_skill_toast.py`), the text notification's
  look in the same right-hand column (plate at 1480,196 under the notification's 40, clear of the quest tracker's 392): headline
  "Woodworking +2 XP" / "Woodworking reached level 1" / "New knowledge: Wood Properties", detail "Level 0 · 2 / 100 XP to level 1", a bar,
  a skill or book glyph. `UMONotificationComponent::ShowSkillToast(Id, bIsKnowledge, XP, LevelBefore, LevelAfter)` carries the grant
  (`ClientShowSkillPopup` forwards it to a joiner). Before (Wes: "the ui for the skill-ups/knowledge-ups sucks and is hard to read"): the
  Skills panel's row widget floated at 20% / 80% behind the pawn with no plate, faint grey text, a 2%-filled black bar, no XP or level
  change, and knowledge ids raw ("WoodProperties").
- One id-to-name rule: `UMOSkillDatabaseSettings::GetSkillOrKnowledgeDisplayName` (skill row, else item, else `HumanizeId`); the toast, the
  Skills panel and the notifications use it (four camel-case splitters before). Test `MOFramework.UI.SkillToast.SaysWhatChanged`.
- The grant carries the standing the server read right after it (`FMOInspectionXPGrant::CurrentXPAfter` / `XPToNextLevelAfter`, the last two
  arguments of `ShowSkillToast` and `ClientShowSkillPopup`): a joiner's replicated skills arrive a tick after the toast, which read "+2 XP" over
  "0 / 100" (N-179, Oct 20; now "2 / 80").

### Resource menu in the kit (Oct 20)
`WBP_HarvestContextMenu` (trees, bushes, rocks) was the one context menu never rebuilt in the kit: its title sat washed out over the
plate's top edge with ~80 px of empty plate under the buttons. `ui_specs/kit_harvest_menu.py`: an auto-sizing plate like the ground and
station menus; the runtime buttons take the collapsed `StyleButton`'s style and slot padding (`UMOKeepOnHarvestContextMenu::CreateButton`).

### Runtime action buttons update in place (Oct 20)
`UMOUIUtils::RebuildActionButtons` (the station, item, ground and world-action menus) destroyed and recreated its buttons on every call, and the
station menu calls it twice a second: a click spanning a rebuild was lost, hover flickered, gamepad focus dropped. The buttons carry their
action (`UMOCommonButton::ActionId`); the same actions in the same order are updated in place (label, enabled, tooltip), and only a changed set
is rebuilt. Measured: the campfire menu's buttons kept their widgets over three samples in 6 s (before: new ones every 0.5 s).

### Inventory: a left click selects (Oct 20)
`UMOUnifiedInventoryMenu` bound only right / shift click and drops on its grids, so a left click highlighted a slot and the Details tab
still said "Click an item for details" (the old `UMOInventoryMenu` handled it). `HandlePlayerSlotClicked` / `HandleSecondarySlotClicked`
call `SetSelectedItem` (an empty slot clears it).

### Inventory Details in the kit (Oct 20)
`WBP_ItemInfoPanel` (`ui_specs/kit_item_info.py`): icon, name (primary text, not the rarity grey), type, "Weight 100 g" (`UMOItemInfoPanel::FormatWeight`:
grams under a kilogram, else kilograms to a tenth; the panel carries the label) beside the turntable (`WBP_MOItemPreview`), a rule, the description
wrapping under it. The old grid read "Weight: Weight: 1" and ran the description past the panel (N-180). Test `MOFramework.UI.ItemWeight.ReadsAsAWeight`.

### HUD: breath meter
`UMOBreathMeterWidget` is full-screen with its bar aligned bottom-centre (`SetPositionInViewport` resets viewport-slot anchors, so a point-anchored HUD bar placed that way ends up off-screen). `MO.Water.Meter` reports its state. Since Oct 23 (BR23-17, "breath meter is hard to read"): 56 x 18 segments (were 36 x 10), the label in `body.strong` / primary text; the colours are unchanged (calm blue, warning, critical red: `MOFramework.Water.BreathMeter`).

## 4. UMG / Slate facts that cost a diagnosis (5.8)

- `UEditableTextBox::SetWidgetStyle(const FEditableTextBoxStyle&)` leaves Slate with a pointer to the caller's local: crash in
  `FCachedTypefaceData`. Edit `TextBox->WidgetStyle` in place + `SynchronizeProperties()` (`UMOUIUtils::ApplyReadableTextInputStyle`).
  Typed text colour while focused = `FocusedForegroundColor`.
- `FViewport::ReadPixels` from game code is blank (+ an RHI ensure) in packaged builds: use `FScreenshotRequest::RequestScreenshot(false)`
  + `UGameViewportClient::OnScreenshotCaptured()` (`UMOPersistenceSubsystem::RequestThumbnailForSave`).
- Text ellipsis appears where the PAINT is clipped: clip the column (`SetClipping(ClipToBounds)`) and give it a right margin.
- `UUserWidget::SetPositionInViewport` / `SetDesiredSizeInViewport` RESET the viewport slot's anchors to (0,0): build a bottom / centre
  HUD element as a full-screen widget whose root Overlay aligns the content.
- A widget that re-reads authored state in `NativeConstruct` (re-runs on every re-add) captures runtime state (inventory icons vanished):
  read authored values once in `NativeOnInitialized`.
- CommonUI `SetIsSelected(false)` does nothing on a non-toggleable button: use `UMOCommonButton::SetSelectedState`.
- **Never use a native UserWidget class (e.g. `UMOCommonButton`) as a NODE in a Widget Blueprint**: no widget tree = a hit-test-invisible
  spacer; real clicks pass through while `SimulateClick` and delegates still work (every session-list row was unclickable). Use the WBP
  `MOButton` (`WBP_MOCommonButton`); `ui build` refuses it, `ui check` flags it. **Simulated clicks prove nothing about hit-testing: do
  one real click on anything a player must click** (`MO.Test.HitTest <Name>` is Slate's real hit test; `ClickWidget` bypasses it).
- Slate resolves fonts only through `UFont`; composite fonts can only be authored from C++. A running instance locks the .uasset files
  it loaded (stop it before `ui build`).
- `HighResShot` / `asset shot pie` renders the 3D scene ONLY (no UMG): to LOOK at UI use `ui shot` (whole editor window) or the packaged
  window capture (`Tools/win_input.ps1 -Action shot`).

## 5. CommonUI pitfalls (UI_Overhaul_Architecture.md, March 2026, verbatim)

#### PITFALL 1: ECommonInputMode::Menu vs All  **[SUPERSEDED 2026-10-17: the project uses Menu mode for every UI surface; toggle keys work through the UI controllers' frame-debounce and `TryCloseMenuForToggleKey`, not `::All` (which leaked WASD into gameplay, the old M8). Do not reintroduce `::All`.]**

**Problem:** Using `ECommonInputMode::Menu` blocks Enhanced Input actions.
**Symptom:** Toggle keys (I, C, B) stop working when menu is open.
**Solution:** Use `ECommonInputMode::All` in `GetDesiredInputConfig()`.

#### PITFALL 2: Missing bIsBackHandler

**Problem:** Widget never receives back action.
**Symptom:** Escape/Tab don't close menu.
**Solution:** Ensure constructor sets `bIsBackHandler = true`.

#### PITFALL 3: Double Delegate Binding

**Problem:** Code binds to BOTH OnRequestClose AND OnLegacyRequestClose.
**Symptom:** Close handler fires twice.
**Solution:** Bind to only ONE delegate. Prefer base class OnRequestClose for new code.

#### PITFALL 4: Focus Lost to Child Widget

**Problem:** After clicking button/slot, focus moves to child.
**Symptom:** NativeOnKeyDown-based close doesn't work.
**Solution:** FIXED by using `bIsBackHandler` instead of `NativeOnKeyDown`. CommonUI's back action works regardless of focus.

#### PITFALL 5: ActiveMenuCount Mismatch (DEPRECATED)

**Problem:** Manual menu counting drifts out of sync with CommonUI.
**Symptom:** Cursor stays visible, movement stays locked after all menus closed.
**Solution:** DELETE ActiveMenuCount system entirely. Use WBP_GameplayStackStub as RootContentWidgetClass. See Stage 3A.

#### PITFALL 6: Toggle Key During Animation

**Problem:** Pressing toggle key while menu is animating in/out.
**Symptom:** Menu state gets out of sync.
**Solution:** UIController should check `IsActivated()` before acting on toggle.

#### PITFALL 7: RequestClose vs DeactivateWidget

**Problem:** Calling `DeactivateWidget()` directly instead of `RequestClose()`.
**Symptom:** OnRequestClose delegate doesn't fire, controller doesn't clean up.
**Solution:** Always call `RequestClose()`. It broadcasts delegate, then controller calls `DeactivateWidget()`.

#### PITFALL 8: Subclass Forgets Super::RequestClose

**Problem:** Overriding RequestClose without calling Super.
**Symptom:** Base class OnRequestClose delegate doesn't fire.
**Solution:** Always call `Super::RequestClose()` in override.

#### PITFALL 9: Blueprint Widget Missing BindWidget

**Problem:** Blueprint widget doesn't have required BindWidget properties.
**Symptom:** Compile error or null pointer crash.
**Solution:** Check header for `meta=(BindWidget)` properties and ensure Blueprint has matching widgets.

#### PITFALL 10: NativeOnHandleBackAction Return Value

**Problem:** Returning false from NativeOnHandleBackAction.
**Symptom:** Back action propagates to other widgets unexpectedly.
**Solution:** Return true to consume the back action, false to let it propagate.

#### PITFALL 11: FUIInputConfig Direct Member Access (UE5.7)

**Problem:** Trying to set FUIInputConfig members directly.
**Symptom:** Compilation error "cannot access protected member".
**Solution:** Use constructors instead of direct assignment.

```cpp
// WRONG (UE5.7)
FUIInputConfig Config;
Config.InputMode = ECommonInputMode::All;  // Error: protected

// CORRECT (UE5.7)
FUIInputConfig Config(ECommonInputMode::All, EMouseCaptureMode::NoCapture, EMouseLockMode::DoNotLock, false);
```

#### PITFALL 12: Missing FObjectInitializer Constructor

**Problem:** Subclass uses default constructor instead of FObjectInitializer pattern.
**Symptom:** Compilation error "no appropriate default constructor available".
**Solution:** All MOMenuWidgetBase subclasses must use the FObjectInitializer constructor.

```cpp
// WRONG
UMOYourMenu::UMOYourMenu() { }

// CORRECT
UMOYourMenu::UMOYourMenu(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}
```

#### PITFALL 13: Widget Re-Push State Corruption

**Problem:** Same widget instance pushed to stack after being popped retains stale state.
**Symptom:** Focus wrong on second open, input config not applying, visual state stale.
**Solution Options:**

**Option A (Simple):** Create new widget instance each time:
```cpp
UMOInventoryMenu* Menu = CreateWidget<UMOInventoryMenu>(PC, InventoryMenuClass);
MenuLayerStack->AddWidget(Menu);
// Don't cache - GC cleans up when removed
```

**Option B (Performant for frequent menus):** Reset state in NativeOnActivated:
```cpp
void UMOInventoryMenu::NativeOnActivated()
{
    Super::NativeOnActivated();
    ScrollBox->ScrollToStart();
    ClearSelection();
    // Reset any state that shouldn't persist
}
```

**Recommendation:** Option A for infrequent menus (possession, in-game). Option B for frequent menus (inventory, crafting).

#### PITFALL 14: CommonGameViewportClient Not Set

**Problem:** Project using default `GameViewportClient` instead of `CommonGameViewportClient`.
**Symptom:** `GetDesiredInputConfig()` has no effect. All CommonUI input routing silently inactive.
**Solution:** Set in Project Settings > Maps & Modes, or in DefaultEngine.ini.

#### PITFALL 15: Missing RootContentWidgetClass

**Problem:** Widget stack has no `RootContentWidgetClass` set.
**Symptom:** Input goes dead, cursor stays visible, or movement locked after all menus close.
**Solution:** Create `WBP_GameplayStackStub` and set as `RootContentWidgetClass` on all layer stacks.

## 6. The UI kit ("Field Ledger") and the design system

Wes, 2026-10-11: "yes, we should make a more cohesive ui schema, give me a prompt to feed into claude design so we can get whatever assets
and design choices so you can go through all the ui and make it nice". The prompt (`Docs/Archive/UI_Design_Brief_Prompt.md`) produced
`DesignKit/MO57_UI_Kit/` (tokens, 9-slices, icons, 11 per-screen specs at 1920x1080); `Docs/UI_Kit_Implementation.md` is how it lives in
the game (tokens in `DT_UITokens` via `UMOUITokens`, kit-driven specs `ui_specs/kit_*.py`, `WBP_KitGallery` = the sheet to look at after
any kit change). Its "Screen status" table is from Oct 12 morning: that night crafting / building (`kit_recipes.py`), skills + quests
(`kit_journal.py`), status (`kit_status.py`), inventory (`kit_inventory.py`, `kit_slot.py`), the possession spec, the death recap
(`kit_death.py`), and later the station / world-action / ground / time menus were built in the kit. The screen inventory is `Docs/UI_Screen_Inventory.md` (generated by `python Tools/ui_inventory.py`; Oct 22: 94 WBPs, 72 in the kit, 5 rename redirectors, 2 unused). Fonts are stand-ins (Roboto / Droid Sans Mono) until the kit's OFL fonts are downloaded (Wes's yes). Accessibility token
swaps exist (`SetAccessibility`) but are not wired to the Options panel.

## 7. Dev console and developer menu

- Gate = `FMODevMode::IsEnabled()` (`MODevMode.h`), never `#if !UE_BUILD_SHIPPING` around a dev surface. Dev / editor builds: always on.
  Shipping: Options > Gameplay > Developer mode (`UMOGameSettings::bDeveloperMode`, off by default) or `-devmode`. Server dev RPCs check
  the SERVER's dev mode (`ServerAllowsDevCheats`).
- Works in Shipping because `bUseExecCommandsInShipping` defaults to true (`ConsoleCommand` -> UEngine::Exec -> cvars and
  FAutoConsoleCommands run in the precompiled Shipping engine); the engine's own console would need `bUseConsoleInShipping` (an engine
  rebuild: impossible on the launcher install). Shipping compiles UE_LOG out: commands that only log show nothing there.
- Popup: `AMOPlayerController::InputKey` (Tilde) -> `WBP_TextInputModal` -> `RunDevConsoleCommand`. The engine console owned Tilde
  (swallowed in UGameViewportClient): `DefaultInput.ini` no longer re-adds `+ConsoleKeys=Tilde` (engine console on `'`).
- `devmenu [Group]` (or Run with nothing typed) enumerates registered `MO.*` commands via IConsoleManager by namespace, help text as
  tooltip; help containing `<` = needs args -> console prefilled. `weather` / `time` open quick-pick modals (`UMOQuickPickDialog`).
- `starter` / `MO.Player.Starter` (authority only): 50 sticks, 20 stones (Wes, Oct 8).
- `MO.Dev.Glider [0|1]` (Wes, Oct 18, BR18-17): a dev hang glider for looking at the world from the air. While on, a FALLING pawn flies
  at trim (`MO.Dev.GlideSpeed` 1100 cm/s forward, `MO.Dev.GlideSink` 100 cm/s down), W along the view, A / D steer; jump off a slope or a
  cliff to launch. Movement input in the saved moves (FLAG_Custom_2), honoured only while the world's HOST is in developer mode
  (`UMOCharacterMovementComponent::IsGliding`), so a joiner flies it too; host and joiner should share the two numbers (a joiner
  predicting with others is corrected). Never the simulation: no glider exists in the game.

## 8. Debugging UI

```
CommonUI.DumpActivatableTree    ; Dump widget activation tree
CommonUI.DebugInputRouter 1     ; Show input routing decisions
Slate.ShowFocusedWidget 1       ; Show focused widget
```
`MO.UI.Enable` turns on the UI debug file logger (`Saved/Logs/UIDebug_*.log`, opt-in since June: `[Key]` lines show every raw key + focus
owner). Runtime widget queries need a PAINTED PIE viewport (`ue.py ui menu` closes asset tabs first). Frame-stepped UI tests:
`MO.UI.RunTest <name>`; the whole suite `MO.UI.RunAllTests` (~85 tests) is synchronously invalid against the same-frame debounce: run
frame-separated.
**Run it in a `-game` process** (`ue.py inst`): only there are Escape / Tab sent as real keys (an editor PIE batch cannot -- the editor ends PIE
on an unconsumed Escape -- so there the UI manager closes the menus instead and the test reads UNVERIFIED). Three outcomes since Oct 19 (N-128):
PASS, FAIL, and UNVERIFIED -- a placeholder, or a pass reached only through the fallback; never counted as a pass. Oct 19 in `-game`: 78 / 86
passed, 0 failed, 8 unverified (context menu, confirmation dialog, possession -- not implemented yet). A key sent before CommonUI activated the
menu does nothing: frame-stepped sequences assert `AssertAnyMenuActive` before Escape.
