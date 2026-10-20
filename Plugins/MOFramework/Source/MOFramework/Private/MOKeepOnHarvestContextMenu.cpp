#include "MOKeepOnHarvestContextMenu.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOHarvestSubsystem.h"
#include "MOResourceDatabaseSettings.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOInventoryComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MOInteractorComponent.h"
#include "MOItemDatabaseSettings.h"
#include "GameFramework/Pawn.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/InstancedStaticMeshComponent.h"

UMOKeepOnHarvestContextMenu::UMOKeepOnHarvestContextMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOKeepOnHarvestContextMenu::RequestClose()
{
	// Broadcast legacy delegate for backward compatibility
	OnRequestClose.Broadcast();
	// Also call base which broadcasts OnCloseRequested
	Super::RequestClose();
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMOKeepOnHarvestContextMenu::InitializeForTarget(
	const FMOInteractionTarget& Target,
	UMOKnowledgeComponent* Knowledge,
	UMOSkillsComponent* Skills,
	UMOInventoryComponent* Inventory
)
{
	// DEBUG (MOKeepOnHarvestContextMenu, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] InitializeForTarget called. ButtonClass=%s, ButtonContainer=%s"),
	// 	ButtonClass ? *ButtonClass->GetName() : TEXT("NULL"),
	// 	ButtonContainer ? TEXT("bound") : TEXT("NULL"));

	if (!Target.IsValid() || !Target.bIsInstancedMeshTarget)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] InitializeForTarget: Invalid or non-ISM target"));
		return;
	}

	CurrentTarget = Target;
	CachedKnowledge = Knowledge;
	CachedSkills = Skills;
	CachedInventory = Inventory;

	// Get harvest subsystem
	UMOHarvestSubsystem* HarvestSubsystem = GetWorld()->GetSubsystem<UMOHarvestSubsystem>();
	if (!HarvestSubsystem)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] No harvest subsystem found"));
		return;
	}

	// Collect target tags
	TargetTags = HarvestSubsystem->CollectTargetTags(Target.ISMComponent.Get());

	// What this target lets you study (a tree: its sticks, bark, catkins -- UMOHarvestSubsystem::GetStudyableItemIds), the first with something to teach
	SmartInspectItemId = HarvestSubsystem->PickItemToStudy(HarvestSubsystem->GetStudyableItemIds(Target.ISMComponent.Get()), Knowledge, Skills);

	// Extract ResourceNodeId from tags and look up the resource definition
	CachedResourceNodeId = HarvestSubsystem->ExtractResourceNodeId(TargetTags);
	const FMOResourceNodeDefinitionRow* ResourceRow = HarvestSubsystem->GetResourceDefinitionForComponent(Target.ISMComponent.Get());
	bTargetIsTree = ResourceRow && ResourceRow->ResourceType == EMOResourceType::Tree;

	// Clear previous state
	AvailableHarvestActions.Empty();
	DestroyActionIndex = INDEX_NONE;
	bCanExecuteDestroyAction = false;

	// A ground item: the PCG item spawner tags it MOItem_<ItemId> (MOPCGMeshSpawnerSettings).
	GroundItemId = NAME_None;
	for (const FName& Tag : TargetTags)
	{
		FString TagString = Tag.ToString();
		FMOItemDefinitionRow ItemDef;
		if (TagString.RemoveFromStart(TEXT("MOItem_")) && UMOItemDatabaseSettings::GetItemDefinition(FName(*TagString), ItemDef))
		{
			GroundItemId = FName(*TagString);
			break;
		}
	}
	if (!GroundItemId.IsNone() && SmartInspectItemId.IsNone())
	{
		SmartInspectItemId = GroundItemId;
	}

	// DEBUG (MOKeepOnHarvestContextMenu, disabled Oct 18): re-enable to trace setup steps
	// // Debug: Log all collected tags
	// UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Collected %d tags from target (ground item '%s'):"), TargetTags.Num(), *GroundItemId.ToString());
	// for (const FName& Tag : TargetTags)
	// {
	// 	UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu]   Tag: '%s'"), *Tag.ToString());
	// }

	if (!CachedResourceNodeId.IsNone())
	{
		// Look up resource definition from the DataTable
		const FMOResourceNodeDefinitionRow* ResourceDef = UMOResourceDatabaseSettings::GetResourceDefinition(CachedResourceNodeId);
		if (ResourceDef)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] Found resource definition: %s (%d actions)"),
				*ResourceDef->DisplayName.ToString(), ResourceDef->HarvestActions.Num());

			// Copy all harvest actions from the resource definition
			AvailableHarvestActions = ResourceDef->HarvestActions;

			// Log each action for debugging
			for (int32 i = 0; i < AvailableHarvestActions.Num(); ++i)
			{
				const FMOResourceHarvestAction& Action = AvailableHarvestActions[i];
				UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu]   Action[%d]: %s (ActionId=%s, Destroys=%s)"),
					i, *Action.DisplayName.ToString(), *Action.ActionId.ToString(),
					Action.bDestroysResource ? TEXT("yes") : TEXT("no"));
			}

			// Find the destroy action (if any) and check if we can execute it
			for (int32 i = 0; i < AvailableHarvestActions.Num(); ++i)
			{
				if (AvailableHarvestActions[i].bDestroysResource)
				{
					DestroyActionIndex = i;
					bCanExecuteDestroyAction = CanPerformAction(AvailableHarvestActions[i]);
					UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] Found destroy action: %s (CanExecute=%s)"),
						*AvailableHarvestActions[i].ActionId.ToString(),
						bCanExecuteDestroyAction ? TEXT("yes") : TEXT("no"));
					break;
				}
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] Resource definition not found for: %s (Is DataTable configured in Project Settings?)"),
				*CachedResourceNodeId.ToString());
		}
	}
	else if (GroundItemId.IsNone())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] No ResourceNode_ or MOItem_ tag found in target tags. ISM components must have a 'ResourceNode_<RowName>' tag (a resource) or an 'MOItem_<ItemId>' tag (a ground item)."));
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] This usually means the resource was spawned before the tag system was added, or PCG needs to regenerate."));
	}

	// Refresh display
	RefreshDisplay();

	UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Initialized with %d tags, %d harvest actions, inspect='%s', resourceId='%s'"),
		TargetTags.Num(),
		AvailableHarvestActions.Num(),
		*SmartInspectItemId.ToString(),
		*CachedResourceNodeId.ToString());
}

void UMOKeepOnHarvestContextMenu::RefreshDisplay()
{
	// Update target name
	if (TargetNameText)
	{
		TargetNameText->SetText(GetTargetDisplayName());
	}

	// Populate all buttons
	PopulateButtons();

	// Notify Blueprint
	OnButtonStatesUpdated(CanInspect(), CanExecuteDestroyAction(), AvailableHarvestActions.Num());
}

// ============================================================================
// STATE QUERIES
// ============================================================================

FText UMOKeepOnHarvestContextMenu::GetTargetDisplayName() const
{
	if (!GroundItemId.IsNone())
	{
		return UMOItemDatabaseSettings::GetItemDisplayName(GroundItemId);
	}

	// Look for "Name " tag prefix
	for (const FName& Tag : TargetTags)
	{
		FString TagString = Tag.ToString();
		if (TagString.StartsWith(TEXT("Name ")))
		{
			return FText::FromString(TagString.RightChop(5));
		}
	}

	// Fallback to "Unknown Object"
	return NSLOCTEXT("MO", "UnknownObject", "Unknown Object");
}

// SetPopupPosition is inherited from UMOContextMenuBase

// ============================================================================
// OVERRIDES
// ============================================================================

void UMOKeepOnHarvestContextMenu::NativeConstruct()
{
	Super::NativeConstruct();
	if (StyleButton)
	{
		StyleButton->SetVisibility(ESlateVisibility::Collapsed);   // it only lends its look to the runtime buttons
	}
}

void UMOKeepOnHarvestContextMenu::NativeDestruct()
{
	ClearButtons();
	Super::NativeDestruct();
}

// NativeOnKeyDown is inherited from UMOContextMenuBase

// ============================================================================
// BUTTON MANAGEMENT
// ============================================================================

UMOCommonButton* UMOKeepOnHarvestContextMenu::CreateButton(const FText& Label)
{
	if (!ButtonClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] No ButtonClass set"));
		return nullptr;
	}

	UMOCommonButton* Button = CreateWidget<UMOCommonButton>(GetOwningPlayer(), ButtonClass);
	if (!Button)
	{
		return nullptr;
	}

	Button->SetButtonText(Label);
	if (StyleButton && StyleButton->GetStyle())
	{
		Button->SetStyle(StyleButton->GetStyle()->GetClass());   // the menu's own look
	}
	UVerticalBoxSlot* NewSlot = ButtonContainer->AddChildToVerticalBox(Button);
	if (const UVerticalBoxSlot* From = StyleButton ? Cast<UVerticalBoxSlot>(StyleButton->Slot) : nullptr; NewSlot && From)
	{
		NewSlot->SetPadding(From->GetPadding());
	}
	CreatedButtons.Add(Button);

	return Button;
}

void UMOKeepOnHarvestContextMenu::PopulateButtons()
{
	ClearButtons();

	if (!ButtonContainer)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] No ButtonContainer bound"));
		return;
	}

	if (!ButtonClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] No ButtonClass set"));
		return;
	}

	// DEBUG (MOKeepOnHarvestContextMenu, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] PopulateButtons: CanInspect=%s, HarvestActions=%d, DestroyActionIndex=%d"),
	// 	CanInspect() ? TEXT("yes") : TEXT("no"),
	// 	AvailableHarvestActions.Num(),
	// 	DestroyActionIndex);

	// 0. A ground item: Pick Up, the same request E sends (by mesh + location, so it works for a joining player too)
	if (!GroundItemId.IsNone())
	{
		if (UMOCommonButton* PickUpBtn = CreateButton(NSLOCTEXT("MO", "PickUpButton", "Pick Up")))
		{
			PickUpBtn->OnClicked().AddWeakLambda(this, [this]()
			{
				const APawn* Pawn = GetOwningPlayerPawn();
				UMOInteractorComponent* Interactor = Pawn ? Pawn->FindComponentByClass<UMOInteractorComponent>() : nullptr;
				const bool bSent = Interactor && Interactor->RequestInteractWithInstance(CurrentTarget.ISMComponent.Get(), CurrentTarget.InstanceIndex);
				UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Pick Up clicked (item: %s, sent: %s)"), *GroundItemId.ToString(), bSent ? TEXT("yes") : TEXT("no"));
				RequestClose();
			});
		}
	}

	// 1. Inspect button (if available)
	if (CanInspect())
	{
		UMOCommonButton* InspectBtn = CreateButton(NSLOCTEXT("MO", "InspectButton", "Inspect"));
		if (InspectBtn)
		{
			InspectBtn->OnClicked().AddWeakLambda(this, [this]()
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Inspect clicked (item: %s)"), *SmartInspectItemId.ToString());
				OnInspectClicked.Broadcast();
				RequestClose();
			});
		}
	}

	// 2. Harvest action buttons (non-destructive actions first)
	for (int32 i = 0; i < AvailableHarvestActions.Num(); ++i)
	{
		const FMOResourceHarvestAction& Action = AvailableHarvestActions[i];

		// Skip the destroy action - it's handled separately at the end
		if (Action.bDestroysResource)
		{
			continue;
		}

		// Check requirements for logging
		const bool bHasKnowledge = HasRequiredKnowledge(Action.RequiredKnowledgeId);
		const bool bHasTool = UMOHarvestSubsystem::HasToolForAction(Action, CachedInventory.Get());
		const bool bCanPerform = bHasKnowledge && bHasTool;

		// DEBUG (MOKeepOnHarvestContextMenu, disabled Oct 18): re-enable to trace setup steps
		// UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Adding harvest button: %s (ActionId: %s, Knowledge=%s/%s, Tool=%s)"),
		// 	*Action.DisplayName.ToString(), *Action.ActionId.ToString(),
		// 	bHasKnowledge ? TEXT("ok") : TEXT("MISSING"),
		// 	Action.RequiresKnowledge() ? *Action.RequiredKnowledgeId.ToString() : TEXT("none"),
		// 	bHasTool ? TEXT("ok") : TEXT("missing"));

		UMOCommonButton* ActionBtn = CreateButton(Action.DisplayName);
		if (ActionBtn)
		{
			// Enable/disable based on requirements (knowledge and tool)
			ActionBtn->SetIsEnabled(bCanPerform);

			FName CapturedActionId = Action.ActionId;
			ActionBtn->OnClicked().AddWeakLambda(this, [this, CapturedActionId]()
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Harvest clicked (action: %s)"), *CapturedActionId.ToString());
				OnHarvestClicked.Broadcast(CapturedActionId);
				RequestClose();
			});
		}
	}

	// 3. Destroy action button (Chop Down, Mine, etc.) - always shown last, disabled if no tool
	if (HasDestroyAction())
	{
		const FMOResourceHarvestAction& DestroyAction = AvailableHarvestActions[DestroyActionIndex];
		FText DestroyLabel = DestroyAction.DisplayName;
		if (DestroyLabel.IsEmpty())
		{
			DestroyLabel = NSLOCTEXT("MO", "DestroyButton", "Destroy");
		}

		UMOCommonButton* DestroyBtn = CreateButton(DestroyLabel);
		if (DestroyBtn)
		{
			// Disable if requirements not met (CanExecuteDestroyAction checks tool requirements)
			DestroyBtn->SetIsEnabled(CanExecuteDestroyAction());

			FName CapturedActionId = DestroyAction.ActionId;
			DestroyBtn->OnClicked().AddWeakLambda(this, [this, CapturedActionId]()
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Destroy clicked (action: %s)"), *CapturedActionId.ToString());
				OnHarvestClicked.Broadcast(CapturedActionId);
				RequestClose();
			});
		}
	}

	// Note: "Search for Insects" removed - future feature

	OnHarvestButtonsUpdated(CreatedButtons.Num());
	// DEBUG (MOKeepOnHarvestContextMenu, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Created %d buttons"), CreatedButtons.Num());
}

void UMOKeepOnHarvestContextMenu::ClearButtons()
{
	for (UMOCommonButton* Button : CreatedButtons)
	{
		if (IsValid(Button))
		{
			Button->OnClicked().RemoveAll(this);
			Button->RemoveFromParent();
		}
	}
	CreatedButtons.Empty();
}

// ============================================================================
// TOOL CHECKS
// ============================================================================

bool UMOKeepOnHarvestContextMenu::HasToolOfType(EMOToolType ToolType) const
{
	if (ToolType == EMOToolType::None)
	{
		return true;
	}

	UMOInventoryComponent* Inventory = CachedInventory.Get();
	if (!Inventory)
	{
		return false;
	}

	// Check if inventory has a tool of this type
	return Inventory->HasToolOfType(ToolType);
}

bool UMOKeepOnHarvestContextMenu::HasRequiredKnowledge(FName KnowledgeId) const
{
	// The harvest subsystem's one knowledge gate (the server's harvest refuses by the same rule).
	FMOResourceHarvestAction Probe;
	Probe.RequiredKnowledgeId = KnowledgeId;
	return UMOHarvestSubsystem::HasKnowledgeForAction(Probe, GetOwningPlayerPawn());
}

bool UMOKeepOnHarvestContextMenu::CanPerformAction(const FMOResourceHarvestAction& Action) const
{
	// Check knowledge requirement first (now checks skills and recipes too)
	if (Action.RequiresKnowledge())
	{
		if (!HasRequiredKnowledge(Action.RequiredKnowledgeId))
		{
			return false;
		}
	}

	// The one tool gate BeginHarvest also enforces: a required tool at its minimum quality (an optional one only slows the work).
	return UMOHarvestSubsystem::HasToolForAction(Action, CachedInventory.Get());
}
