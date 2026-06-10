# MO57 UI Class Structure Report

> **REFERENCE DOCUMENT**: This is a snapshot of the UI class structure as of March 28, 2026.
> For current planning, see `MO57_Master_Plan.md`.

*Generated 2026-03-28 for Python editor script development*

## 1. UUserWidget / UCommonActivatableWidget Inheritance

### UCommonActivatableWidget Inheritors (16 classes)

| Class | Header Path |
|-------|-------------|
| UMOMainMenuWidget | `Plugins/MOFramework/Source/MOFramework/Public/MOMainMenuWidget.h` |
| UMOInGameMenu | `Plugins/MOFramework/Source/MOFramework/Public/MOInGameMenu.h` |
| UMOBuildingMenu | `Plugins/MOFramework/Source/MOFramework/Public/MOBuildingMenu.h` |
| UMOItemContextMenu | `Plugins/MOFramework/Source/MOFramework/Public/MOItemContextMenu.h` |
| UMOPossessionMenu | `Plugins/MOFramework/Source/MOFramework/Public/MOPossessionMenu.h` |
| UMOSurvivorTaskMenu | `Plugins/MOFramework/Source/MOFramework/Public/MOSurvivorTaskMenu.h` |
| UMOUnifiedInventoryMenu | `Plugins/MOFramework/Source/MOFramework/Public/MOUnifiedInventoryMenu.h` |
| UMOLoadPanel | `Plugins/MOFramework/Source/MOFramework/Public/MOLoadPanel.h` |
| UMOSavePanel | `Plugins/MOFramework/Source/MOFramework/Public/MOSavePanel.h` |
| UMOOptionsPanel | `Plugins/MOFramework/Source/MOFramework/Public/MOOptionsPanel.h` |
| UMONewGamePanel | `Plugins/MOFramework/Source/MOFramework/Public/MONewGamePanel.h` |
| UMOStatusPanel | `Plugins/MOFramework/Source/MOFramework/Public/MOStatusPanel.h` |
| UMOQuestLogPanel | `Plugins/MOFramework/Source/MOFramework/Public/MOQuestLogPanel.h` |
| UMOHarvestProgressWidget | `Plugins/MOFramework/Source/MOFramework/Public/MOHarvestProgressWidget.h` |
| UMOInspectionProgressWidget | `Plugins/MOFramework/Source/MOFramework/Public/MOInspectionProgressWidget.h` |
| UMOConfirmationDialog | `Plugins/MOFramework/Source/MOFramework/Public/MOConfirmationDialog.h` |

### UUserWidget Inheritors (45+ classes)

**Entry/Item Widgets:**
- UMOBuildingEntryWidget, UMOPawnEntryWidget, UMORecipeEntryWidget, UMOSkillEntryWidget
- UMOCharacterInfoEntry, UMOQuestLogEntry, UMOQuestTrackerEntry
- UMOKeyBindingEntryWidget, UMOSurvivorTaskEntryWidget, UMOSurvivorJobEntryWidget
- UMOBuildingQueueEntryWidget, UMOCraftingQueueEntryWidget

**Context Menus (inherit UMOContextMenuBase : UUserWidget):**
- UMOGhostContextMenu, UMOStationContextMenu, UMOKeepOnHarvestContextMenu
- UMOGroundContextMenu, UMOSurvivorContextMenu

**Panels:**
- UMOItemInfoPanel, UMOEquipmentPanel, UMONearbyItemsPanel
- UMORecipeDetailPanel, UMOBuildingDetailPanel, UMOSkillsPanel

**HUD Widgets:**
- UMOReticleWidget, UMOModeIndicatorWidget, UMOToolHintWidget
- UMOPlayerStatusWidget, UMONotificationWidget, UMOQuestHUDWidget, UMOFPSCounterWidget

**Other:**
- UMOInventoryMenu, UMOCraftingMenu, UMOBuildWidget
- UMOInventoryGrid, UMODragVisualWidget, UMOLoadingOverlay, UMOModalBackground, UMOIntroWidget

### CommonUI Classes

| Class | Parent | Header |
|-------|--------|--------|
| UMOCommonButton | UCommonButtonBase | `MOCommonButton.h` |
| UMOPrimaryGameLayout | UCommonUserWidget | `MOPrimaryGameLayout.h` |
| UMOStatusField | UCommonUserWidget | `MOStatusField.h` |

---

## 2. BlueprintType/Blueprintable UI Classes (40+)

All in `Plugins/MOFramework/Source/MOFramework/Public/`:

| Class | UCLASS Specifier |
|-------|------------------|
| UMOMainMenuWidget | `Abstract, Blueprintable` |
| UMONewGamePanel | `Abstract, Blueprintable` |
| UMOPrimaryGameLayout | `Abstract, Blueprintable` |
| UMOStatusPanel | `Abstract, Blueprintable` |
| UMOSkillsPanel | `Abstract, Blueprintable` |
| UMOStatusField | `Abstract, Blueprintable` |
| UMOCraftingMenu | `Abstract, Blueprintable` |
| UMOQuestLogPanel | `Abstract, Blueprintable` |
| UMOHarvestProgressWidget | `Abstract, Blueprintable` |
| UMOInspectionProgressWidget | `Abstract, Blueprintable` |
| UMOBuildingQueueWidget | `Abstract, Blueprintable` |
| UMOCraftingQueueWidget | `Abstract, Blueprintable` |
| UMOBuildingRecipeListWidget | `Abstract, Blueprintable` |
| UMOBuildWidget | `Abstract, Blueprintable` |
| UMORecipeListWidget | `Abstract, Blueprintable` |
| UMORecipeDetailPanel | `Abstract, Blueprintable` |
| UMOBuildingDetailPanel | `Abstract, Blueprintable` |
| UMODragVisualWidget | `Blueprintable` |
| UMOPossessionMenu | `Abstract, Blueprintable` |
| UMOGhostContextMenu | `Abstract, Blueprintable` |
| UMOSkillEntryWidget | `Abstract, Blueprintable` |
| UMOQuestHUDWidget | `Abstract, Blueprintable` |
| UMOBuildingQueueEntryWidget | `Abstract, Blueprintable` |
| UMOCraftingQueueEntryWidget | `Abstract, Blueprintable` |
| UMOBuildingRecipeEntryWidget | `Abstract, Blueprintable` |
| UMORecipeEntryWidget | `Abstract, Blueprintable` |
| UMOKeyBindingEntryWidget | `Abstract, Blueprintable` |
| UMOCharacterInfoEntry | `Abstract, Blueprintable` |
| UMOIntroWidget | `Abstract, Blueprintable` |
| UMOPawnEntryWidget | `Abstract, Blueprintable` |
| UMOKeepOnHarvestContextMenu | `Abstract, Blueprintable` |
| UMOStationContextMenu | `Abstract, Blueprintable` |
| UMOQuestLogEntry | `Abstract, Blueprintable` |
| UMOQuestTrackerEntry | `Abstract, Blueprintable` |

---

## 3. GameplayTags (MO.UI.*)

**File:** `Config/DefaultGameplayTags.ini`

```ini
+GameplayTagList=(Tag="MO.UI.Layer.HUD",DevComment="Always-visible HUD elements")
+GameplayTagList=(Tag="MO.UI.Layer.Game",DevComment="Switchable gameplay menus")
+GameplayTagList=(Tag="MO.UI.Layer.GameOverlay",DevComment="Overlays on gameplay menus")
+GameplayTagList=(Tag="MO.UI.Layer.Menu",DevComment="System menus")
+GameplayTagList=(Tag="MO.UI.Layer.Modal",DevComment="Modal dialogs and context menus")
```

---

## 4. Existing Python Scripts

**Location:** `Tools/`

| Script | Purpose |
|--------|---------|
| `ue_csv_utils.py` | UE DataTable CSV ↔ SQLite utility |
| `update_tool_capabilities.py` | Item tool capability migration |
| `update_items_json.py` | Item JSON schema updates |
| `update_recipes_json.py` | Recipe JSON updates |
| `migrate_items_json.py` | JSON schema migration |

**Note:** These are standalone offline utilities, not UE Python plugin scripts.

---

## 5. Build.cs Plugin References

**MOFramework.Build.cs:**
- No Python plugin dependencies
- Has Editor utilities: `UnrealEd`, `ContentBrowser`, `AssetTools`, `ToolMenus`

**MO57.Build.cs:**
- No Python plugin dependencies

**Conclusion:** Python scripting plugin is NOT currently enabled. To use Python editor scripts, enable `PythonScriptPlugin` in the .uproject.

---

## 6. Widget Blueprint Assets (WBP_)

**Location:** `Plugins/MOFramework/Content/UI/`
**Count:** 70+ WBP assets

### Key Assets by Category:

**System UI:**
- WBP_MOInGameMenu, WBP_MOOptionsPanel, WBP_LoadPanel, WBP_SavePanel, WBP_NewGamePanel

**Gameplay UI:**
- WBP_InventoryMenu, WBP_CraftingMenu, WBP_BuildingMenu, WBP_SkillsPanel, WBP_MOStatusPanel

**Context Menus:**
- WBP_ItemContextMenu, WBP_GroundContextMenu, WBP_SurvivorContextMenu

**Progress/HUD:**
- WBP_HarvestProgress, WBP_InspectionProgress, WBP_QuestHUD, WBP_MOCommonButton

### Directory Structure:
```
UI/
├── BuildSystem/       (WBP_BuildingMenu, WBP_BuildingRecipeDetail, etc.)
├── CraftingSystem/    (WBP_CraftingMenu, WBP_StationMenu, etc.)
├── InventorySystem/   (WBP_InventoryMenu, WBP_EquipmentPanel, etc.)
├── Quest/             (WBP_QuestHUD, WBP_JournalPanel, etc.)
├── SurvivorAI/        (WBP_SurvivorTaskMenu, WBP_SurvivorContextMenu, etc.)
├── MainMenu_UI/       (WBP_MOInGameMenu1, WBP_IntroVideo, WBP_NewGamePanel)
├── GroundSearch/      (WBP_GroundContextMenu)
└── KeyBindingsUI/     (WBP_KeyBindingEntry)
```

---

## Summary Statistics

| Metric | Count |
|--------|-------|
| Total UI C++ Classes | 61+ |
| UCommonActivatableWidget inheritors | 16 |
| UUserWidget inheritors | 45+ |
| Blueprintable UI classes | 40+ |
| Widget Blueprint assets | 70+ |
| Python scripts (standalone) | 5 |
| MO.UI.* GameplayTags | 5 |
| Python plugin enabled | No |

---

## Next Steps for Python Editor Script

To batch-create WBP children from C++ parents:

1. **Enable Python Plugin** - Add `"PythonScriptPlugin"` to MO57.uproject Plugins array
2. **Script Location** - Place in `Content/Python/` for auto-discovery or `Tools/Editor/`
3. **Use Unreal Python API** - `unreal.EditorAssetLibrary`, `unreal.WidgetBlueprintFactory`
4. **Pattern to follow** - Map C++ class paths (`/Script/MOFramework.MOSkillsPanel`) to output paths (`/Game/UI/WBP_SkillsPanel`)

---

## C++ Class to WBP Mapping (Existing)

For reference, here are known C++ → WBP mappings that already exist:

| C++ Class | WBP Asset |
|-----------|-----------|
| UMOCommonButton | WBP_MOCommonButton |
| UMOInGameMenu | WBP_MOInGameMenu |
| UMOStatusPanel | WBP_MOStatusPanel |
| UMOSkillsPanel | WBP_SkillsPanel |
| UMOOptionsPanel | WBP_MOOptionsPanel |
| UMOLoadPanel | WBP_LoadPanel |
| UMOSavePanel | WBP_SavePanel |
| UMONewGamePanel | WBP_NewGamePanel |
| UMOInventoryMenu | WBP_InventoryMenu |
| UMOCraftingMenu | WBP_CraftingMenu |
| UMOBuildingMenu | WBP_BuildingMenu |
| UMOPossessionMenu | WBP_PossessionMenu |
| UMOItemContextMenu | WBP_ItemContextMenu |
| UMOHarvestProgressWidget | WBP_HarvestProgress |
| UMOQuestHUDWidget | WBP_QuestHUD |
