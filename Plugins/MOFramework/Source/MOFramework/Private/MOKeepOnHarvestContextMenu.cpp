#include "MOKeepOnHarvestContextMenu.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOHarvestSubsystem.h"
#include "MOResourceDatabaseSettings.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOInventoryComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
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
	UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] InitializeForTarget called. ButtonClass=%s, ButtonContainer=%s"),
		ButtonClass ? *ButtonClass->GetName() : TEXT("NULL"),
		ButtonContainer ? TEXT("bound") : TEXT("NULL"));

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

	// Get smart inspect item
	SmartInspectItemId = HarvestSubsystem->GetSmartInspectItemId(TargetTags, Knowledge, Skills);

	// Extract ResourceNodeId from tags and look up the resource definition
	CachedResourceNodeId = HarvestSubsystem->ExtractResourceNodeId(TargetTags);

	// Clear previous state
	AvailableHarvestActions.Empty();
	DestroyActionIndex = INDEX_NONE;
	bCanExecuteDestroyAction = false;

	// Debug: Log all collected tags
	UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] Collected %d tags from target:"), TargetTags.Num());
	for (const FName& Tag : TargetTags)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu]   Tag: '%s'"), *Tag.ToString());
	}

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
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOKeepOnHarvestContextMenu] No ResourceNode_ tag found in target tags. ISM components must have 'ResourceNode_<RowName>' tag."));
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
	ButtonContainer->AddChild(Button);
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

	UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] PopulateButtons: CanInspect=%s, HarvestActions=%d, DestroyActionIndex=%d"),
		CanInspect() ? TEXT("yes") : TEXT("no"),
		AvailableHarvestActions.Num(),
		DestroyActionIndex);

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
		const bool bHasKnowledge = !Action.RequiresKnowledge() ||
			(CachedKnowledge.IsValid() && CachedKnowledge->HasKnowledge(Action.RequiredKnowledgeId));
		const bool bHasTool = !Action.RequiresTool() || !Action.bToolRequired || HasToolOfType(Action.RequiredToolType);
		const bool bCanPerform = bHasKnowledge && bHasTool;

		UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Adding harvest button: %s (ActionId: %s, Knowledge=%s/%s, Tool=%s)"),
			*Action.DisplayName.ToString(), *Action.ActionId.ToString(),
			bHasKnowledge ? TEXT("ok") : TEXT("MISSING"),
			Action.RequiresKnowledge() ? *Action.RequiredKnowledgeId.ToString() : TEXT("none"),
			bHasTool ? TEXT("ok") : TEXT("missing"));

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
	UE_LOG(LogMOFramework, Log, TEXT("[MOKeepOnHarvestContextMenu] Created %d buttons"), CreatedButtons.Num());
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

bool UMOKeepOnHarvestContextMenu::CanPerformAction(const FMOResourceHarvestAction& Action) const
{
	// Check knowledge requirement first
	if (Action.RequiresKnowledge())
	{
		UMOKnowledgeComponent* Knowledge = CachedKnowledge.Get();
		if (!Knowledge || !Knowledge->HasKnowledge(Action.RequiredKnowledgeId))
		{
			return false;
		}
	}

	// If action doesn't require a tool, it can be performed
	if (!Action.RequiresTool())
	{
		return true;
	}

	// If tool is optional (bToolRequired=false), action can be performed without it (with penalties)
	if (!Action.bToolRequired)
	{
		return true;
	}

	// Tool is required - check if player has it
	return HasToolOfType(Action.RequiredToolType);
}
