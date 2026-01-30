# MO57

Developed with Unreal Engine 5

## MOFramework Plugin

The MOFramework plugin provides a complete set of gameplay systems including inventory, crafting, survival stats, pawn possession, and persistence.

---

## UI Widget Implementation Guide

This section documents all UI widgets in the framework, their required Widget Blueprints (WBP), and bound widget names.

### Recommended Content Folder Structure

```
Content/
└── UI/
    ├── Common/
    │   ├── WBP_MOCommonButton.uasset
    │   └── WBP_DragVisual.uasset
    │
    ├── HUD/
    │   ├── WBP_PlayerStatus.uasset
    │   ├── WBP_Reticle.uasset
    │   └── WBP_Notification.uasset
    │
    ├── Inventory/
    │   ├── WBP_InventoryMenu.uasset
    │   ├── WBP_InventoryGrid.uasset
    │   ├── WBP_InventorySlot.uasset
    │   ├── WBP_ItemInfoPanel.uasset
    │   └── WBP_ItemContextMenu.uasset
    │
    ├── Crafting/
    │   ├── WBP_CraftingMenu.uasset
    │   ├── WBP_RecipeList.uasset
    │   ├── WBP_RecipeEntry.uasset
    │   ├── WBP_RecipeDetailPanel.uasset
    │   ├── WBP_CraftingQueue.uasset
    │   └── WBP_CraftingQueueEntry.uasset
    │
    ├── Status/
    │   ├── WBP_StatusPanel.uasset
    │   ├── WBP_StatusField.uasset
    │   └── WBP_CharacterInfoEntry.uasset
    │
    ├── Possession/
    │   ├── WBP_PossessionMenu.uasset
    │   └── WBP_PawnEntry.uasset
    │
    └── Menu/
        ├── WBP_InGameMenu.uasset
        ├── WBP_OptionsPanel.uasset
        ├── WBP_SavePanel.uasset
        ├── WBP_LoadPanel.uasset
        └── WBP_SaveSlotEntry.uasset
```

---

## Widget Reference

### Legend

- **Required** (`BindWidget`): Widget MUST exist with exact name
- **Optional** (`BindWidgetOptional`): Widget is optional but uses exact name if present
- **Config**: Property set in Blueprint defaults, not a bound widget

---

## Common Widgets

### WBP_MOCommonButton
**Parent Class:** `UMOCommonButton` (extends `UCommonButtonBase`)

Base button for all MOFramework UI. Provides text label support via Blueprint event.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| *(none)* | - | - | Override `UpdateButtonText` event to set your text widget |

**Properties:**
- `ButtonLabel` (FText): The button's display text

---

### WBP_DragVisual
**Parent Class:** `UMODragVisualWidget`

Displayed during inventory drag operations. Uses pure Slate rendering.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| *(none)* | - | - | Renders via Slate, no bindings needed |

---

## HUD Widgets

### WBP_PlayerStatus
**Parent Class:** `UMOPlayerStatusWidget`

Displays survival stats (health, stamina, hunger, thirst, energy, temperature).

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `HealthBar` | UProgressBar | Optional | Health display |
| `StaminaBar` | UProgressBar | Optional | Stamina display |
| `HungerBar` | UProgressBar | Optional | Hunger display |
| `ThirstBar` | UProgressBar | Optional | Thirst display |
| `EnergyBar` | UProgressBar | Optional | Energy display |
| `TemperatureBar` | UProgressBar | Optional | Temperature display |
| `HealthText` | UTextBlock | Optional | Numeric health value |
| `StaminaText` | UTextBlock | Optional | Numeric stamina value |
| `HungerText` | UTextBlock | Optional | Numeric hunger value |
| `ThirstText` | UTextBlock | Optional | Numeric thirst value |
| `EnergyText` | UTextBlock | Optional | Numeric energy value |
| `TemperatureText` | UTextBlock | Optional | Numeric temperature value |

**Properties:**
- `bShowPercentage` (bool): Show % or current/max values
- `HealthyColor`, `WarningColor`, `CriticalColor` (FLinearColor)
- `WarningThreshold`, `CriticalThreshold` (float)

---

### WBP_Reticle
**Parent Class:** `UMOReticleWidget`

Crosshair/targeting reticle. Renders via Slate, no Blueprint bindings needed.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| *(none)* | - | - | Pure Slate rendering |

**Properties:**
- `ReticleColor` (FLinearColor)
- `ReticleSize`, `ReticleThickness`, `ReticleGap`, `CenterDotSize` (float)
- `bShowCenterDot` (bool)

---

### WBP_Notification
**Parent Class:** `UMONotificationWidget`

Centered notification messages. Can use pure Slate or Blueprint widgets.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `MessageText` | UTextBlock | Optional | If not bound, uses Slate text |
| `BackgroundBorder` | UBorder | Optional | Background styling |

---

## Inventory Widgets

### WBP_InventoryMenu
**Parent Class:** `UMOInventoryMenu`

Main inventory screen combining grid and info panel.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `InventoryGrid` | UMOInventoryGrid | **Required** | The item grid |
| `ItemInfoPanel` | UMOItemInfoPanel | **Required** | Selected item details |

**Delegates:**
- `OnRequestClose`: Menu close requested
- `OnSlotRightClicked`: Context menu trigger

---

### WBP_InventoryGrid
**Parent Class:** `UMOInventoryGrid`

Grid of inventory slots.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `SlotsUniformGrid` | UUniformGridPanel | **Required** | Container for slot widgets |

**Properties:**
- `SlotWidgetClass` (TSubclassOf): Must be set to your WBP_InventorySlot
- `Columns` (int32): Grid column count (default 5)
- `MinimumVisibleSlotCount` (int32): Minimum slots shown (default 20)

---

### WBP_InventorySlot
**Parent Class:** `UMOInventorySlot`

Single inventory slot with drag-drop support.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `SlotButton` | UButton | **Required** | Clickable area |
| `ItemIconImage` | UImage | Optional | Item icon display |
| `QuantityText` | UTextBlock | Optional | Stack count text |
| `QuantityBox` | UWidget | Optional | Container for quantity (hidden when qty=1) |
| `SlotBorder` | UBorder | Optional | Hover/selection outline |
| `DragHandle` | UWidget | Optional | Drag initiation area |
| `DebugItemIdText` | UTextBlock | Optional | Debug display |

**Properties:**
- `bEnableDragDrop` (bool): Enable drag-drop
- `bEnableWorldDrop` (bool): Drop items into world
- `DefaultItemIcon`, `EmptySlotIcon` (UTexture2D)
- `NormalBorderColor`, `HoverBorderColor`, `DraggingBorderColor` (FLinearColor)

---

### WBP_ItemInfoPanel
**Parent Class:** `UMOItemInfoPanel`

Displays detailed info about selected item.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `ItemNameText` | UTextBlock | Optional | Item name |
| `ItemTypeText` | UTextBlock | Optional | Type (Weapon, Armor, etc.) |
| `RarityText` | UTextBlock | Optional | Rarity tier |
| `DescriptionText` | UTextBlock | Optional | Full description |
| `ShortDescriptionText` | UTextBlock | Optional | Brief description |
| `ItemIconImage` | UImage | Optional | Large item icon |
| `QuantityText` | UTextBlock | Optional | Stack quantity |
| `MaxStackText` | UTextBlock | Optional | Max stack size |
| `WeightText` | UTextBlock | Optional | Item weight |
| `ValueText` | UTextBlock | Optional | Item value |
| `FlagsText` | UTextBlock | Optional | Item flags |
| `TagsText` | UTextBlock | Optional | Gameplay tags |
| `PropertiesText` | UTextBlock | Optional | Scalar properties |
| `InfoGrid` | UPanelWidget | Optional | Container for detail widgets |
| `PlaceholderText` | UTextBlock | Optional | "Select an item" message |

---

### WBP_ItemContextMenu
**Parent Class:** `UMOItemContextMenu` (extends `UCommonActivatableWidget`)

Right-click context menu for inventory items.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `ButtonContainer` | UPanelWidget | **Required** | Container for all buttons |
| `UseButton` | UMOCommonButton | **Required** | Use/consume action |
| `Drop1Button` | UMOCommonButton | **Required** | Drop single item |
| `DropAllButton` | UMOCommonButton | **Required** | Drop entire stack |
| `InspectButton` | UMOCommonButton | **Required** | Inspect item |
| `SplitStackButton` | UMOCommonButton | **Required** | Split stack |
| `CraftButton` | UMOCommonButton | **Required** | Open crafting filtered to item |

**Properties:**
- `AutoCloseDelay` (float): Delay before auto-close when mouse leaves

---

## Crafting Widgets

### WBP_CraftingMenu
**Parent Class:** `UMOCraftingMenu`

Main crafting interface combining recipe list, details, and queue.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `RecipeList` | UMORecipeListWidget | **Required** | Recipe browser |
| `DetailPanel` | UMORecipeDetailPanel | **Required** | Recipe details |
| `QueueWidget` | UMOCraftingQueueWidget | Optional | Crafting queue display |
| `CloseButton` | UMOCommonButton | Optional | Close menu |

**Properties:**
- `bAutoRefreshOnInventoryChange` (bool): Auto-update when inventory changes

---

### WBP_RecipeList
**Parent Class:** `UMORecipeListWidget`

Scrollable list of available recipes.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `RecipeScrollBox` | UScrollBox | Optional | Scrollable container (preferred) |
| `RecipeContainer` | UVerticalBox | Optional | Alternative container |

**Properties:**
- `RecipeEntryWidgetClass` (TSubclassOf): Must be set to your WBP_RecipeEntry

---

### WBP_RecipeEntry
**Parent Class:** `UMORecipeEntryWidget`

Single recipe row in the list.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `EntryButton` | UButton | Optional | Clickable area |
| `RecipeNameText` | UTextBlock | Optional | Recipe name |
| `RecipeIcon` | UImage | Optional | Recipe icon |
| `BackgroundBorder` | UBorder | Optional | Selection/state background |

**Properties:**
- `SelectedColor`, `CraftableColor`, `UncraftableColor` (FLinearColor)
- `TextColorCraftable`, `TextColorUncraftable` (FSlateColor)

---

### WBP_RecipeDetailPanel
**Parent Class:** `UMORecipeDetailPanel`

Detailed recipe information with craft controls.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `RecipeNameText` | UTextBlock | Optional | Recipe name |
| `RecipeDescriptionText` | UTextBlock | Optional | Recipe description |
| `RecipeIcon` | UImage | Optional | Recipe icon |
| `IngredientsContainer` | UVerticalBox | Optional | Ingredient list container |
| `OutputsContainer` | UVerticalBox | Optional | Output list container |
| `SkillRequirementText` | UTextBlock | Optional | Skill requirement display |
| `CraftTimeText` | UTextBlock | Optional | Craft duration |
| `CraftButton` | UMOCommonButton | Optional | Execute craft |
| `CraftAmountSpinBox` | USpinBox | Optional | Quantity selector |
| `CraftAmountSlider` | USlider | Optional | Alternative quantity selector |
| `CraftAmountText` | UTextBlock | Optional | Quantity display |

**Blueprint Events:**
- `OnRecipeDisplayed(RecipeId, DisplayName, Description)`
- `OnIngredientsUpdated(TArray<FMOIngredientDisplayData>)`
- `OnOutputsUpdated(TArray<FMOOutputDisplayData>)`

---

### WBP_CraftingQueue
**Parent Class:** `UMOCraftingQueueWidget`

Displays crafting queue with progress.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `QueueScrollBox` | UScrollBox | Optional | Scrollable queue container |
| `QueueContainer` | UVerticalBox | Optional | Alternative container |
| `CurrentCraftNameText` | UTextBlock | Optional | Currently crafting item |
| `CurrentProgressBar` | UProgressBar | Optional | Current craft progress |
| `ProgressText` | UTextBlock | Optional | Progress percentage |
| `TimeRemainingText` | UTextBlock | Optional | Time remaining (current) |
| `TotalTimeRemainingText` | UTextBlock | Optional | Total queue time |
| `CancelAllButton` | UMOCommonButton | Optional | Cancel entire queue |
| `EmptyQueueText` | UTextBlock | Optional | "Queue empty" message |

**Properties:**
- `QueueEntryWidgetClass` (TSubclassOf): Must be set to your WBP_CraftingQueueEntry
- `ProgressUpdateInterval` (float): Update rate in seconds

---

### WBP_CraftingQueueEntry
**Parent Class:** `UMOCraftingQueueEntryWidget`

Single entry in the crafting queue.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `RecipeNameText` | UTextBlock | Optional | Recipe being crafted |
| `RecipeIcon` | UImage | Optional | Recipe icon |
| `CountText` | UTextBlock | Optional | "2/5" progress count |
| `ProgressBar` | UProgressBar | Optional | Entry progress |
| `TimeRemainingText` | UTextBlock | Optional | Time remaining |
| `CancelButton` | UMOCommonButton | Optional | Cancel this entry |
| `CancelButtonSimple` | UButton | Optional | Alternative cancel button |

**Properties:**
- `ActiveColor`, `QueuedColor` (FLinearColor)

---

## Status Widgets

### WBP_StatusPanel
**Parent Class:** `UMOStatusPanel` (extends `UCommonActivatableWidget`)

Tab-based character status display with multiple categories.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `CategorySwitcher` | UWidgetSwitcher | **Required** | Switches between category views |
| `InfoScrollBox` | UScrollBox | **Required** | Index 0 - Character info |
| `VitalsScrollBox` | UScrollBox | **Required** | Index 1 - Vitals |
| `NutritionScrollBox` | UScrollBox | **Required** | Index 2 - Nutrition |
| `NutrientsScrollBox` | UScrollBox | **Required** | Index 3 - Nutrients |
| `FitnessScrollBox` | UScrollBox | **Required** | Index 4 - Fitness |
| `MentalScrollBox` | UScrollBox | **Required** | Index 5 - Mental |
| `WoundsScrollBox` | UScrollBox | **Required** | Index 6 - Wounds |
| `ConditionsScrollBox` | UScrollBox | **Required** | Index 7 - Conditions |
| `InfoContainer` | UVerticalBox | Optional | Container inside InfoScrollBox |
| `VitalsContainer` | UVerticalBox | Optional | Container inside VitalsScrollBox |
| `NutritionContainer` | UVerticalBox | Optional | Container inside NutritionScrollBox |
| `NutrientsContainer` | UVerticalBox | Optional | Container inside NutrientsScrollBox |
| `FitnessContainer` | UVerticalBox | Optional | Container inside FitnessScrollBox |
| `MentalContainer` | UVerticalBox | Optional | Container inside MentalScrollBox |
| `WoundsContainer` | UVerticalBox | Optional | Container inside WoundsScrollBox |
| `ConditionsContainer` | UVerticalBox | Optional | Container inside ConditionsScrollBox |
| `InfoTabButton` | UMOCommonButton | Optional | Tab button |
| `VitalsTabButton` | UMOCommonButton | Optional | Tab button |
| `NutritionTabButton` | UMOCommonButton | Optional | Tab button |
| `NutrientsTabButton` | UMOCommonButton | Optional | Tab button |
| `FitnessTabButton` | UMOCommonButton | Optional | Tab button |
| `MentalTabButton` | UMOCommonButton | Optional | Tab button |
| `WoundsTabButton` | UMOCommonButton | Optional | Tab button |
| `ConditionsTabButton` | UMOCommonButton | Optional | Tab button |
| `BackButton` | UMOCommonButton | Optional | Close panel |

**Properties:**
- `StatusFieldClass` (TSubclassOf): WBP_StatusField class
- `CharacterInfoEntryClass` (TSubclassOf): WBP_CharacterInfoEntry class
- `FieldConfigs` (TArray<FMOStatusFieldConfig>): Field definitions

---

### WBP_StatusField
**Parent Class:** `UMOStatusField` (extends `UCommonUserWidget`)

Reusable stat display widget.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `TitleText` | UTextBlock | Optional | Field label |
| `ValueText` | UTextBlock | Optional | Field value |
| `ValueBar` | UProgressBar | Optional | Normalized progress |
| `IconImage` | UImage | Optional | Field icon |

**Properties:**
- `FieldId` (FName): Unique identifier for data binding
- `WarningThreshold`, `CriticalThreshold` (float)
- `HealthyColor`, `WarningColor`, `CriticalColor` (FLinearColor)
- `bInvertThresholds` (bool): Higher = worse

---

### WBP_CharacterInfoEntry
**Parent Class:** `UMOCharacterInfoEntry`

Editable character info field (name, age, gender).

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `LabelText` | UTextBlock | Optional | Field label |
| `ValueText` | UTextBlock | Optional | Current value (display mode) |
| `ChangeButton` | UMOCommonButton | Optional | Enter edit mode |
| `EditTextBox` | UEditableTextBox | Optional | Text input (edit mode) |
| `ConfirmButton` | UMOCommonButton | Optional | Save changes |
| `CancelButton` | UMOCommonButton | Optional | Cancel edit |

---

## Possession Widgets

### WBP_PossessionMenu
**Parent Class:** `UMOPossessionMenu` (extends `UCommonActivatableWidget`)

Character/pawn selection screen.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `PawnListScrollBox` | UScrollBox | **Required** | Container for pawn entries |
| `CreateCharacterButton` | UMOCommonButton | Optional | Create new character |
| `CloseButton` | UMOCommonButton | Optional | Close menu |
| `TitleText` | UTextBlock | Optional | Menu title |
| `EmptyListText` | UTextBlock | Optional | "No characters" message |

**Properties:**
- `PawnEntryWidgetClass` (TSubclassOf): Must be set to your WBP_PawnEntry

---

### WBP_PawnEntry
**Parent Class:** `UMOPawnEntryWidget`

Single pawn entry in possession menu.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `NameText` | UTextBlock | Optional | Pawn name |
| `AgeText` | UTextBlock | Optional | Pawn age |
| `GenderText` | UTextBlock | Optional | Pawn gender |
| `HealthBar` | UProgressBar | Optional | Health indicator |
| `StatusText` | UTextBlock | Optional | Current status |
| `LocationText` | UTextBlock | Optional | World location |
| `LastPlayedText` | UTextBlock | Optional | Last played time |
| `PortraitImage` | UImage | Optional | Character portrait |
| `PossessButton` | UMOCommonButton | Optional | Possess this pawn |

---

## Menu Widgets

### WBP_InGameMenu
**Parent Class:** `UMOInGameMenu` (extends `UCommonActivatableWidget`)

Pause menu with options, save, load, and exit.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `ButtonsBox` | UPanelWidget | **Required** | Left-side button container |
| `OptionsButton` | UMOCommonButton | **Required** | Open options |
| `SaveButton` | UMOCommonButton | **Required** | Open save panel |
| `LoadButton` | UMOCommonButton | **Required** | Open load panel |
| `ExitToMainMenuButton` | UMOCommonButton | **Required** | Exit to main menu |
| `ExitGameButton` | UMOCommonButton | **Required** | Exit game |
| `FocusWindowSwitcher` | UWidgetSwitcher | **Required** | Right-side panel switcher |
| `OptionsPanel` | UMOOptionsPanel | Optional | Options panel (index 1) |
| `SavePanel` | UMOSavePanel | Optional | Save panel (index 2) |
| `LoadPanel` | UMOLoadPanel | Optional | Load panel (index 3) |

**Switcher Indices:**
- 0: Empty/None
- 1: Options panel
- 2: Save panel
- 3: Load panel

---

### WBP_OptionsPanel
**Parent Class:** `UMOOptionsPanel` (extends `UCommonActivatableWidget`)

Settings/options panel. Override in Blueprint to add your options.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `BackButton` | UCommonButtonBase | **Required** | Close panel |
| `ApplyButton` | UCommonButtonBase | Optional | Apply settings |
| `ResetButton` | UCommonButtonBase | Optional | Reset to defaults |

---

### WBP_SavePanel
**Parent Class:** `UMOSavePanel` (extends `UCommonActivatableWidget`)

Save game panel with slot list.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `SaveSlotsScrollBox` | UScrollBox | **Required** | Container for save entries |
| `NewSaveButton` | UMOCommonButton | **Required** | Create new save |
| `BackButton` | UMOCommonButton | **Required** | Close panel |

**Properties:**
- `SaveSlotEntryClass` (TSubclassOf): Must be set to your WBP_SaveSlotEntry

---

### WBP_LoadPanel
**Parent Class:** `UMOLoadPanel` (extends `UCommonActivatableWidget`)

Load game panel with slot list.

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `SaveSlotsScrollBox` | UScrollBox | **Required** | Container for save entries |
| `BackButton` | UMOCommonButton | **Required** | Close panel |

**Properties:**
- `SaveSlotEntryClass` (TSubclassOf): Must be set to your WBP_SaveSlotEntry
- `bFilterToCurrentWorld` (bool): Only show saves from current world

---

### WBP_SaveSlotEntry
**Parent Class:** `UMOSaveSlotEntry` (extends `UCommonButtonBase`)

Single save slot entry (clickable button).

| Widget | Type | Required | Notes |
|--------|------|----------|-------|
| `SaveNameText` | UTextBlock | Optional | Save display name |
| `TimestampText` | UTextBlock | Optional | Save date/time |
| `PlayTimeText` | UTextBlock | Optional | Total playtime |
| `WorldNameText` | UTextBlock | Optional | World/level name |
| `CharacterInfoText` | UTextBlock | Optional | Character info |
| `ScreenshotImage` | UImage | Optional | Save thumbnail |
| `AutosaveIndicator` | UWidget | Optional | Autosave marker |

---

## Creating Widget Blueprints

### Basic Setup Process

1. **Create the Blueprint**
   - Right-click in Content Browser > User Interface > Widget Blueprint
   - Select the appropriate parent class (e.g., `UMOInventoryMenu`)
   - Name it following the `WBP_` convention

2. **Add Required Widgets**
   - Check the widget reference table for required bindings
   - Add widgets to the Designer with **exact names** from the table
   - Mark each as "Is Variable" in the Details panel

3. **Set Class References**
   - In Class Defaults, set any `*WidgetClass` properties to your sub-widgets
   - Example: `InventoryGrid.SlotWidgetClass` = `WBP_InventorySlot`

4. **Configure Properties**
   - Set colors, thresholds, and other properties in Class Defaults
   - Override BlueprintImplementableEvents for custom behavior

### Example: Creating WBP_InventorySlot

1. Create Widget Blueprint with parent `UMOInventorySlot`
2. Add these widgets in Designer:
   ```
   [Canvas Panel]
   └── SlotButton (Button) - REQUIRED
       └── SlotBorder (Border) - Optional, for hover outline
           └── [Overlay]
               ├── ItemIconImage (Image) - Optional
               └── QuantityBox (SizeBox) - Optional
                   └── QuantityText (TextBlock) - Optional
   ```
3. Mark `SlotButton`, `ItemIconImage`, `QuantityBox`, `QuantityText` as "Is Variable"
4. In Class Defaults, set `DefaultItemIcon` and `EmptySlotIcon` textures
5. Implement `OnVisualDataUpdated` event for custom visuals

---

## Initialization Flow

### PlayerController Setup Example

```cpp
void AMyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Create and initialize inventory menu
    InventoryMenu = CreateWidget<UMOInventoryMenu>(this, InventoryMenuClass);
    if (InventoryMenu && GetPawn())
    {
        auto* Inventory = GetPawn()->FindComponentByClass<UMOInventoryComponent>();
        InventoryMenu->InitializeMenu(Inventory);
        InventoryMenu->OnRequestClose.AddDynamic(this, &AMyPlayerController::HandleInventoryClose);
    }

    // Create and initialize crafting menu
    CraftingMenu = CreateWidget<UMOCraftingMenu>(this, CraftingMenuClass);
    if (CraftingMenu && GetPawn())
    {
        auto* Inventory = GetPawn()->FindComponentByClass<UMOInventoryComponent>();
        auto* Skills = GetPawn()->FindComponentByClass<UMOSkillsComponent>();
        auto* Knowledge = GetPawn()->FindComponentByClass<UMOKnowledgeComponent>();
        auto* CraftingQueue = GetPawn()->FindComponentByClass<UMOCraftingQueueComponent>();
        auto* Discovery = GetPawn()->FindComponentByClass<UMORecipeDiscoveryComponent>();

        CraftingMenu->InitializeMenu(Inventory, Skills, Knowledge, CraftingQueue, Discovery);
    }
}
```

---

## Blueprint Events Reference

Many widgets provide `BlueprintImplementableEvent` functions for customization:

| Widget | Event | Purpose |
|--------|-------|---------|
| `UMOInventorySlot` | `OnVisualDataUpdated` | Custom slot visuals |
| `UMORecipeEntryWidget` | `OnVisualsUpdated` | Custom entry styling |
| `UMORecipeDetailPanel` | `OnRecipeDisplayed` | Custom header display |
| `UMORecipeDetailPanel` | `OnIngredientsUpdated` | Custom ingredient list |
| `UMORecipeDetailPanel` | `OnOutputsUpdated` | Custom output list |
| `UMOCraftingQueueWidget` | `OnQueueUpdated` | Queue state changes |
| `UMOCraftingQueueWidget` | `OnProgressUpdated` | Progress bar updates |
| `UMOCraftingQueueEntryWidget` | `OnVisualsUpdated` | Custom entry styling |
| `UMOStatusField` | `OnFieldDataChanged` | Custom field display |
| `UMOCharacterInfoEntry` | `OnEntryInitialized` | Custom entry setup |
| `UMOPawnEntryWidget` | `OnEntryInitialized` | Custom pawn display |
| `UMOSaveSlotEntry` | `OnMetadataUpdated` | Custom save display |
| `UMOOptionsPanel` | `OnRefreshSettings` | Load current settings |
| `UMOSavePanel` | `OnSaveListUpdated` | Custom save list |
| `UMOLoadPanel` | `OnSaveListUpdated` | Custom load list |
| `UMOCommonButton` | `UpdateButtonText` | Set button text widget |

---

## Tips

1. **Widget Names Matter**: Bound widgets must have exact names. `HealthBar` not `Health_Bar`.

2. **Is Variable Checkbox**: Always enable "Is Variable" for bound widgets.

3. **Optional Bindings**: Optional widgets are gracefully handled - the code checks for null.

4. **Subclass Properties**: Set `*WidgetClass` properties in Blueprint defaults, not at runtime.

5. **CommonUI**: Several widgets extend CommonUI classes for gamepad/keyboard support.

6. **Delegates**: Connect to `BlueprintAssignable` delegates in your controller or game code.
