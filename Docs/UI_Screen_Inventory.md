# UI screen inventory

> **Generated** by `python Tools/ui_inventory.py` on 2026-10-22 (read from the project: widget Blueprints, their native
> parents, the kit specs, the C++ input configs). Regenerate after UI work; do not edit by hand. BACKLOG N-120; the UI architecture is
> `Docs/UI.md`, the kit `Docs/UI_Kit_Implementation.md`.
> **Columns:** *Kit spec* = the `Content/Python/ui_specs` script that builds it ("-" = not in the Field Ledger kit). *Input* = the
> `GetDesiredInputConfig` of its class or the nearest base that has one (Menu = UI input with the cursor; Game = the pawn keeps it;
> computed = decided at run time), and the mouse capture. *Own focus* = its class chain overrides `NativeGetDesiredFocusTarget`.
> *Used by* = C++ files, other assets and scripts that name it.

**94 widget Blueprints; 72 built by a kit spec, 22 not.** Redirectors left by renames (fix up in the editor: right-click the folder > Fix Up Redirectors): 5 -- WBP_CraftQueEntry (WBP_CraftQueueEntry), WBP_MOCraftingMenu (WBP_CraftingMenu), WBP_RecipeDetail (WBP_CraftingRecipeDetail), WBP_RecipeEntry (WBP_CraftingRecipeEntry), WBP_RecipeList (WBP_CraftingRecipeList). No live reference (only validation / fix scripts name them, if anything): 2 -- WBP_CommonButtonBase, WBP_DragVisual. Several are on Codex's UI retirement list (`Tools/validate_ui_retirement.py`, BACKLOG N-126): retire them through that, not by hand.

| WBP | Folder | Native parent | Kit spec | Input | Own focus | Used by |
|---|---|---|---|---|---|---|
| WBP_BugReportPanel | /MOFramework/UI | UMOBugReportPanel | kit_bugreport, kit_menus | Menu, NoCapture (UMOActivatableWidget) | yes | MOBugReportPanel.h, WBP_MOInGameMenu |
| WBP_BuildContext | /MOFramework/UI/BuildSystem | UMOGhostContextMenu | kit_build_context | - | - | BP_MOPlayerController |
| WBP_BuildQueueEntry | /MOFramework/UI/BuildSystem | UMOBuildingQueueEntryWidget | kit_building, kit_recipes | - | - | MOBuildingQueueEntryWidget.h, MOQueueRowWidgetBase.h, WBP_BuildingQueue, validate_ui_stage1.py |
| WBP_BuildingMenu | /MOFramework/UI/BuildSystem | UMOBuildingMenu | kit_building, kit_recipes | Menu, NoCapture (UMOMenuWidget) | yes | BP_MOPlayerController, validate_ui_stage1.py |
| WBP_BuildingQueue | /MOFramework/UI/BuildSystem | UMOBuildingQueueWidget | kit_building, kit_recipes | - | - | MOBuildingQueueWidget.h, WBP_BuildingMenu, validate_ui_stage1.py |
| WBP_BuildingRecipeDetail | /MOFramework/UI/BuildSystem | UMOBuildingDetailPanel | kit_building, kit_recipes | - | - | WBP_BuildingMenu, validate_ui_stage1.py |
| WBP_BuildingRecipeEntry | /MOFramework/UI/BuildSystem | UMOBuildingRecipeEntryWidget | kit_building, kit_recipes | - | - | WBP_BuildingRecipeList, validate_ui_stage1.py |
| WBP_BuildingRecipeList | /MOFramework/UI/BuildSystem | UMOBuildingRecipeListWidget | kit_building, kit_recipes | - | - | WBP_BuildingMenu, validate_ui_stage1.py |
| WBP_CharacterCreatorPanel | /MOFramework/UI/MainMenu_UI | UMOCharacterCreatorPanel | kit_character_creator | Menu, NoCapture (UMOActivatableWidget) | yes | MOSystemMenuUIController.cpp, MOSystemMenuUIController.h |
| WBP_CommonButtonBase | /MOFramework/UI | CommonButtonBase | - | - | - | - |
| WBP_ConfirmationWidget | /MOFramework/UI | UMOConfirmationDialog | - | Menu, NoCapture (UMOConfirmationBase) | yes | BP_MOPlayerController, WBP_LoadPanel, WBP_SavePanel |
| WBP_CraftQueEntry | /MOFramework/UI | redirector -> WBP_CraftQueueEntry | - | - | - | - |
| WBP_CraftQueueEntry | /MOFramework/UI/CraftingSystem | UMOCraftingQueueEntryWidget | kit_crafting, kit_recipes | - | - | MOCraftingQueueEntryWidget.h, MOQueueRowWidgetBase.h, WBP_CraftQueEntry, WBP_CraftingQueue, validate_ui_stage1.py |
| WBP_CraftingMenu | /MOFramework/UI/CraftingSystem | UMOCraftingMenu | kit_crafting, kit_recipes | Menu, NoCapture (UMOMenuWidget) | yes | BP_MOPlayerController, WBP_MOCraftingMenu, inspect_widget_blueprints.py, ui_tree.py, validate_ui_stage1.py |
| WBP_CraftingQueue | /MOFramework/UI/CraftingSystem | UMOCraftingQueueWidget | kit_crafting, kit_recipes | - | - | MOCraftingQueueWidget.h, WBP_CraftingMenu, validate_ui_retirement.py, validate_ui_stage1.py |
| WBP_CraftingRecipeDetail | /MOFramework/UI/CraftingSystem | UMORecipeDetailPanel | kit_crafting, kit_recipes | - | - | WBP_CraftingMenu, WBP_RecipeDetail, validate_ui_stage1.py |
| WBP_CraftingRecipeEntry | /MOFramework/UI/CraftingSystem | UMORecipeEntryWidget | kit_crafting, kit_recipes | - | - | WBP_CraftingRecipeList, WBP_RecipeEntry, validate_ui_stage1.py |
| WBP_CraftingRecipeList | /MOFramework/UI/CraftingSystem | UMORecipeListWidget | kit_crafting, kit_recipes | - | - | WBP_CraftingMenu, WBP_RecipeList, validate_ui_stage1.py |
| WBP_DeathRecap | /MOFramework/UI | UMODeathRecapWidget | kit_death | Menu, NoCapture (UMOModalWidget) | yes | MODeathRecapWidget.h, MOSystemMenuUIController.cpp, MOSystemMenuUIController.h, ue_inst.py |
| WBP_DragVisual | /MOFramework/UI | UMODragVisualWidget | - | - | - | - |
| WBP_EquipmentPanel | /MOFramework/UI/InventorySystem | UMOEquipmentPanel | kit_inventory | - | - | WBP_UnifiedInventory |
| WBP_FPSCounter | /MOFramework/UI | UMOFPSCounterWidget | kit_hud | - | - | MOFPSCounterWidget.h, BP_MOPlayerController |
| WBP_GroundContextMenu | /MOFramework/UI/GroundSearch | UMOGroundContextMenu | kit_ground_menu | - | - | MOGroundContextMenu.h, BP_MOPlayerController |
| WBP_HUDRoot | /MOFramework/UI/HUDElements | UMOHUDRootWidget | kit_hud, kit_hud_moodles | computed | yes | MOCheatSubsystem.cpp, MOHUDRootWidget.cpp, MOHUDRootWidget.h, MOStatusEffectStripWidget.h, MOUIManagerComponent.h, MOWindDirectionWidget.h, BP_MOPlayerControlle |
| WBP_HarvestContextMenu | /MOFramework/UI | UMOKeepOnHarvestContextMenu | kit_harvest_menu | - | - | BP_MOPlayerController |
| WBP_HarvestProgress | /MOFramework/UI | UMOHarvestProgressWidget | kit_hud | Menu, NoCapture (UMOActivatableWidget) | yes | BP_MOPlayerController, fix_widget_bindings_complete.py, fix_widget_variables.py, fix_widget_variables_final.py, fix_widget_variables_v2.py, fix_widget_variables |
| WBP_HostGamePanel | /MOFramework/UI/MainMenu_UI | UMOHostGamePanel | kit_mainmenu | Menu, NoCapture (UMOActivatableWidget) | yes | MOHostGamePanel.h, MOMainMenuWidget.cpp, MOMainMenuWidget.h, WBP_MOInGameMenu1 |
| WBP_InspectionProgress | /MOFramework/UI | UMOInspectionProgressWidget | kit_hud | Menu, NoCapture (UMOActivatableWidget) | yes | MOInspectionActions.cpp, BP_MOPlayerController, explore_pin_type.py, explore_widget_tree.py, fix_inspection_widget.py, fix_widget_bindings_complete.py, fix_widg |
| WBP_IntroVideo | /MOFramework/UI/MainMenu_UI | UMOIntroWidget | kit_mainmenu | Menu, NoCapture (UMOActivatableWidget) | yes | BP_MainMenuPlayerController |
| WBP_InventoryGrid | /MOFramework/UI/InventorySystem | UMOInventoryGrid | kit_inventory | - | - | MOInventoryGrid.cpp, MOInventoryGrid.h, WBP_InventoryMenu, WBP_NearbyItems, WBP_UnifiedInventory |
| WBP_InventoryMenu | /MOFramework/UI/InventorySystem | UMOInventoryMenu | - | Menu, NoCapture (UMOMenuWidget) | yes | BP_MOPlayerController, BP_ThirdPersonPlayerController, inspect_widget_blueprints.py |
| WBP_InventorySlot | /MOFramework/UI/InventorySystem | UMOInventorySlot | kit_slot | - | - | MOInventoryGrid.cpp, WBP_EquipmentPanel, WBP_InventoryGrid |
| WBP_ItemContextMenu | /MOFramework/UI/InventorySystem | UMOItemContextMenu | - | - | - | MOItemContextMenu.h, BP_MOPlayerController, BP_ThirdPersonPlayerController |
| WBP_ItemInfoPanel | /MOFramework/UI | UMOItemInfoPanel | kit_inventory, kit_item_info | - | - | WBP_InventoryMenu, WBP_UnifiedInventory |
| WBP_JobQueueEntry | /MOFramework/UI/SurvivorAI | UMOSurvivorJobEntryWidget | kit_survivor_tasks | - | - | WBP_SurvivorTaskMenu |
| WBP_JoinGamePanel | /MOFramework/UI/MainMenu_UI | UMOJoinGamePanel | kit_mainmenu | Menu, NoCapture (UMOActivatableWidget) | yes | MOJoinGamePanel.h, MOMainMenuWidget.cpp, MOMainMenuWidget.h, WBP_MOInGameMenu1, mo_ui.py, mo_ui_contract.py |
| WBP_JournalPanel | /MOFramework/UI/Quest | UMOQuestLogPanel | kit_journal | Menu, NoCapture | yes | BP_MOPlayerController |
| WBP_KeyBindingEntry | /MOFramework/UI/KeyBindingsUI | UMOKeyBindingEntryWidget | - | - | - | WBP_MOOptionsPanel |
| WBP_KitGallery | /MOFramework/UI/Kit | UserWidget | kit_gallery | - | - | MOUICheats.cpp, ui_shoot.py |
| WBP_LoadPanel | /MOFramework/UI | UMOLoadPanel | kit_mainmenu, kit_menus | Menu, NoCapture (UMOActivatableWidget) | yes | MOJoinGamePanel.h, MOSaveSlotListPanel.h, WBP_MOInGameMenu, WBP_MOInGameMenu1 |
| WBP_LoadingOverlay | /MOFramework/UI | UMOLoadingOverlay | kit_mainmenu | - | - | MOGameInstance.h, BP_MOGameInstance |
| WBP_MOCharacterInfoEntry | /MOFramework/UI | UMOCharacterInfoEntry | kit_status | - | - | WBP_MOStatusPanel |
| WBP_MOColonyPortrait | /MOFramework/UI/Colony | UMOColonyPortrait | - | - | - | configure_all_widgets.py, create_generic_widgets.py, fix_missing_buttons.py, setup_widget_bindings.py |
| WBP_MOCommonButton | /MOFramework/UI | UMOCommonButton | kit_button | - | - | MOCheatSubsystem.cpp, MOCommonButton.h, MOKeepOnHarvestContextMenu.h, MOQuickPickDialog.h, MOTabBarWidget.h, MOUIUtils.cpp, MOUIUtils.h, WBP_BugReportPanel, WBP |
| WBP_MOConfirmation | /MOFramework/UI/CommonUI | UMOConfirmationBase | - | Menu, NoCapture | yes | configure_all_widgets.py, configure_widget_properties.py, create_generic_widgets.py, fix_missing_buttons.py, fix_widget_bindings_complete.py, fix_widget_variabl |
| WBP_MOCraftingMenu | /MOFramework/UI | redirector -> WBP_CraftingMenu | - | - | - | - |
| WBP_MODetailPanel | /MOFramework/UI/CommonUI | UMOCommonButton | - | - | - | MODetailPanelBase.h, configure_all_widgets.py, create_generic_widgets.py, fix_missing_buttons.py, fix_widget_bindings_complete.py, fix_widget_variables.py, fix_ |
| WBP_MOInGameMenu | /MOFramework/UI | UMOInGameMenu | kit_menus | Menu, NoCapture (UMOModalWidget) | yes | MOInGameMenu.cpp, MOMainMenuWidget.cpp, MOMainMenuWidget.h, BP_MOPlayerController, BP_MainMenuPlayerController, WBP_MOInGameMenu1 |
| WBP_MOInGameMenu1 | /MOFramework/UI/MainMenu_UI | UMOMainMenuWidget | kit_mainmenu | Menu, NoCapture (UMOActivatableWidget) | yes | MOMainMenuWidget.cpp, MOMainMenuWidget.h, BP_MainMenuPlayerController |
| WBP_MOItemPreview | /MOFramework/UI | UMOItemPreviewWidget | kit_item_info | - | - | MOItemInfoPanel.h, MOItemPreviewWidget.h, WBP_ItemInfoPanel |
| WBP_MOListEntry | /MOFramework/UI/CommonUI | UMOCommonButton | - | - | - | configure_all_widgets.py, create_generic_widgets.py, fix_missing_buttons.py, fix_widget_bindings_complete.py, fix_widget_variables.py, fix_widget_variables_fina |
| WBP_MOOptionsPanel | /MOFramework/UI | UMOOptionsPanel | kit_mainmenu, kit_menus, kit_options | Menu, NoCapture (UMOActivatableWidget) | yes | WBP_MOInGameMenu, WBP_MOInGameMenu1 |
| WBP_MOPawnEntryWidget | /MOFramework/UI | UMOPawnEntryWidget | kit_possession | - | - | BP_MOPlayerController, WBP_MOPossessionMenu |
| WBP_MOPlayerStatus | /MOFramework/UI | UMOPlayerStatusWidget | - | - | - | BP_ThirdPersonPlayerController |
| WBP_MOPossessionMenu | /MOFramework/UI | UMOPossessionMenu | kit_possession | Menu, NoCapture (UMOMenuWidget) | yes | BP_MOPlayerController |
| WBP_MOPrimaryGameLayout | /MOFramework/UI/CommonUI | UMOPrimaryGameLayout | - | - | - | MOUITestSubsystem.cpp, DefaultGame.ini, create_primary_layout.py |
| WBP_MOProgressWidget | /MOFramework/UI/CommonUI | UMOProgressWidgetBase | - | Menu, NoCapture (UMOActivatableWidget) | yes | add_missing_actionname.py, add_widget_bindings.py, configure_all_widgets.py, create_generic_widgets.py, explore_wbp_api.py, explore_wbp_properties.py, fix_widge |
| WBP_MOSaveSlot | /MOFramework/UI | UMOSaveSlotEntry | kit_gallery, kit_menus | - | - | WBP_KitGallery, WBP_LoadPanel, WBP_SavePanel |
| WBP_MOScrollList | /MOFramework/UI/CommonUI | UMOScrollListBase | - | - | - | configure_all_widgets.py, create_generic_widgets.py, fix_widget_bindings_complete.py, fix_widget_variables.py, fix_widget_variables_final.py, fix_widget_variabl |
| WBP_MOStatusField | /MOFramework/UI | UMOStatusField | kit_status | - | - | WBP_MOStatusPanel |
| WBP_MOStatusPanel | /MOFramework/UI | UMOStatusPanel | kit_status | Menu, NoCapture (UMOMenuWidget) | yes | MOStatusPanel.cpp, BP_MOPlayerController |
| WBP_ModeIndicator | /MOFramework/UI | UMOModeIndicatorWidget | kit_hud | - | - | BP_MOPlayerController |
| WBP_NearbyItems | /MOFramework/UI/InventorySystem | UMONearbyItemsPanel | kit_inventory | - | - | WBP_UnifiedInventory |
| WBP_NewGamePanel | /MOFramework/UI/MainMenu_UI | UMONewGamePanel | kit_mainmenu | Menu, NoCapture (UMOActivatableWidget) | yes | MOHostGamePanel.h, MONewGamePanel.h, WBP_MOInGameMenu1 |
| WBP_Notification | /MOFramework/UI | UMONotificationWidget | kit_hud | - | - | BP_MOPlayerController |
| WBP_QuestHUD | /MOFramework/UI/Quest | UMOQuestHUDWidget | kit_hud | - | - | BP_MOPlayerController |
| WBP_QuestLogEntry | /MOFramework/UI/Quest | UMOQuestLogEntry | kit_journal | - | - | WBP_JournalPanel |
| WBP_QuestTrackerEntry | /MOFramework/UI/Quest | UMOQuestTrackerEntry | kit_hud | - | - | WBP_QuestHUD |
| WBP_QuickPickModal | /MOFramework/UI | UMOQuickPickDialog | - | Menu, NoCapture | yes | MOPlayerController.h, MOQuickPickDialog.h |
| WBP_RecipeDetail | /MOFramework/UI | redirector -> WBP_CraftingRecipeDetail | - | - | - | validate_ui_retirement.py, validate_ui_stage1.py |
| WBP_RecipeEntry | /MOFramework/UI | redirector -> WBP_CraftingRecipeEntry | - | - | - | validate_ui_retirement.py, validate_ui_stage1.py |
| WBP_RecipeList | /MOFramework/UI | redirector -> WBP_CraftingRecipeList | - | - | - | validate_ui_retirement.py, validate_ui_stage1.py |
| WBP_SavePanel | /MOFramework/UI | UMOSavePanel | kit_menus | Menu, NoCapture (UMOActivatableWidget) | yes | MOSavePanel.h, MOSaveSlotListPanel.h, WBP_MOInGameMenu |
| WBP_SessionList | /MOFramework/UI/MainMenu_UI | UMOSessionListWidget | kit_mainmenu | - | - | MOJoinGamePanel.h, WBP_JoinGamePanel, WBP_SessionListEntry |
| WBP_SessionListEntry | /MOFramework/UI/MainMenu_UI | UMOSessionListEntry | kit_mainmenu | - | - | MOJoinGamePanel.h, WBP_SessionList |
| WBP_SkillEntry | /MOFramework/UI | UMOSkillEntryWidget | kit_journal, kit_skill_toast | - | - | MOSkillToastWidget.h, BP_MOPlayerController, WBP_SkillsPanel |
| WBP_SkillToast | /MOFramework/UI | UMOSkillToastWidget | kit_skill_toast | - | - | MONotificationComponent.cpp, MONotificationComponent.h, MOSkillToastWidget.h |
| WBP_SkillsPanel | /MOFramework/UI | UMOSkillsPanel | kit_journal | Menu, NoCapture (UMOMenuWidget) | yes | BP_MOPlayerController, fix_widget_variables.py, inspect_widget_blueprints.py |
| WBP_SplashSequence | /MOFramework/UI/MainMenu_UI | UMOSplashWidget | kit_splash | Menu, NoCapture (UMOActivatableWidget) | yes | MOSplashSettings.cpp, MOSplashWidget.h |
| WBP_StationMenu | /MOFramework/UI/CraftingSystem | UMOStationContextMenu | kit_station_menu | - | - | BP_MOPlayerController |
| WBP_StatusEffectStrip | /MOFramework/UI/HUDElements | UMOStatusEffectStripWidget | kit_hud_moodles | - | - | MOCheatSubsystem.cpp, MOHUDRootWidget.cpp, MOStatusEffectStripWidget.cpp, MOStatusEffectStripWidget.h, WBP_HUDRoot |
| WBP_StatusMoodle | /MOFramework/UI/HUDElements | UMOStatusMoodleWidget | kit_hud_moodles | - | - | MOStatusEffectStripWidget.cpp, MOStatusEffectStripWidget.h, MOStatusMoodleWidget.h, WBP_StatusEffectStrip |
| WBP_SurvivorContextMenu | /MOFramework/UI/SurvivorAI | UMOSurvivorContextMenu | - | - | - | BP_MOPlayerController |
| WBP_SurvivorTaskMenu | /MOFramework/UI/SurvivorAI | UMOSurvivorTaskMenu | kit_survivor_tasks | Menu, NoCapture | yes | BP_MOPlayerController |
| WBP_TabBar | /MOFramework/UI/Kit | UMOTabBarWidget | kit_menus, kit_options | - | - | MOTabBarWidget.h, WBP_MOOptionsPanel |
| WBP_TaskEntry | /MOFramework/UI/SurvivorAI | UMOSurvivorTaskEntryWidget | kit_survivor_tasks | - | - | WBP_SurvivorTaskMenu |
| WBP_TextInputModal | /MOFramework/UI | UMOTextInputDialog | - | Menu, NoCapture (UMOConfirmationBase) | yes | MOPlayerController.h, WBP_LoadPanel, WBP_SavePanel |
| WBP_TimeOfDayDialog | /MOFramework/UI | UMOTimeOfDayDialog | kit_time_dialog | Menu, NoCapture | yes | MOPlayerController.h, MOTimeOfDayDialog.h |
| WBP_ToolNotif | /MOFramework/UI | UMOToolHintWidget | kit_hud | - | - | BP_MOPlayerController |
| WBP_TutorialHint | /MOFramework/UI/Quest | UMOTutorialHintWidget | kit_hud | Game, CapturePermanently | yes | MOQuestUIController.h, MOTutorialHintWidget.h, BP_MOPlayerController |
| WBP_UnifiedInventory | /MOFramework/UI/InventorySystem | UMOUnifiedInventoryMenu | kit_inventory | Menu, NoCapture (UMOMenuWidget) | yes | BP_MOPlayerController |
| WBP_WindDirection | /MOFramework/UI/HUDElements | UMOWindDirectionWidget | kit_hud | - | - | MOHUDRootWidget.cpp, MOWindDirectionWidget.h, WBP_HUDRoot |
| WBP_WorldActionMenu | /MOFramework/UI/WorldActions | UMOWorldActionMenu | kit_world_action_menu | - | - | MOCraftingUIController.h, MOWorldActionMenu.h |
| WBP_WorldClock | /MOFramework/UI/Kit | UMOWorldClockWidget | kit_menus | - | - | WBP_MOInGameMenu |
