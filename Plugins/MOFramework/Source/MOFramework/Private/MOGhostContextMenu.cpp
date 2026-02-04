#include "MOGhostContextMenu.h"
#include "MOBuildableActor.h"
#include "MOBuildProgressComponent.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "MOContainerActor.h"
#include "MOBuildingTypes.h"
#include "MOFramework.h"
#include "Components/CheckBox.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"

UMOGhostContextMenu::UMOGhostContextMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UMOGhostContextMenu::InitializeForGhost(AMOBuildableActor* Target, UMOInventoryComponent* InBuilderInventory)
{
	if (!IsValid(Target))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] InitializeForGhost called with invalid target"));
		return;
	}

	TargetBuilding = Target;
	BuilderInventory = InBuilderInventory;
	BuildProgress = Target->BuildProgressComponent;

	// Initialize checkboxes to default state
	if (InventoryCheckbox)
	{
		InventoryCheckbox->SetIsChecked(true);
	}
	if (ContainersCheckbox)
	{
		ContainersCheckbox->SetIsChecked(true);
	}
	if (SurroundingCheckbox)
	{
		SurroundingCheckbox->SetIsChecked(true);
	}

	// Refresh the material list
	RefreshMaterialList();
	UpdateButtonState();

	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Initialized for ghost: %s"), *Target->GetName());
}

void UMOGhostContextMenu::RefreshMaterialList()
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	if (!Progress)
	{
		return;
	}

	// Get required materials
	TArray<FName> RequiredItemIds;
	Progress->GetRequiredMaterials(RequiredItemIds);

	// Build material entries
	MaterialEntries.Empty();
	for (const FName& ItemId : RequiredItemIds)
	{
		FMOGhostMaterialEntry Entry;
		Entry.ItemId = ItemId;
		Entry.Deposited = Progress->GetDepositedCount(ItemId);
		Entry.Required = Progress->GetRequiredCount(ItemId);

		// Get display name from item database
		FMOItemDefinitionRow ItemDef;
		if (UMOItemDatabaseSettings::GetItemDefinition(ItemId, ItemDef))
		{
			Entry.DisplayName = ItemDef.DisplayName;
		}
		else
		{
			Entry.DisplayName = FText::FromName(ItemId);
		}

		MaterialEntries.Add(Entry);
	}

	// Update UI
	PopulateMaterialList();
	OnMaterialListUpdated(MaterialEntries);
	UpdateButtonState();
}

// ============================================================================
// ACTIONS
// ============================================================================

void UMOGhostContextMenu::AddMaterials()
{
	if (!BuildProgress.IsValid())
	{
		return;
	}

	// Try to gather materials for each requirement that isn't complete
	bool bGatheredAny = false;
	for (FMOGhostMaterialEntry& Entry : MaterialEntries)
	{
		while (!Entry.IsComplete())
		{
			if (TryGatherMaterial(Entry.ItemId))
			{
				Entry.Deposited++;
				bGatheredAny = true;
			}
			else
			{
				// Couldn't find more of this material
				break;
			}
		}
	}

	if (bGatheredAny)
	{
		RefreshMaterialList();
	}
}

void UMOGhostContextMenu::StartBuild()
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	if (!Progress)
	{
		return;
	}

	if (!AreAllMaterialsDeposited())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] Cannot start build - materials not deposited"));
		return;
	}

	// Build options from checkbox states
	FMOBuildProgress Options;
	Options.bDrawFromInventory = InventoryCheckbox ? InventoryCheckbox->IsChecked() : true;
	Options.bDrawFromNearbyContainers = ContainersCheckbox ? ContainersCheckbox->IsChecked() : true;
	Options.bDrawFromSurroundingArea = SurroundingCheckbox ? SurroundingCheckbox->IsChecked() : true;

	// Start construction
	Progress->StartConstruction(Options, BuilderInventory.Get());

	OnBuildStarted.Broadcast();

	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Build started!"));
}

void UMOGhostContextMenu::CancelBuild()
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	AMOBuildableActor* Building = TargetBuilding.Get();

	if (Progress && Building)
	{
		// Get deposited materials for dropping
		TMap<FName, int32> DepositedMats;
		Progress->GetDepositedMaterials(DepositedMats);

		// Drop materials into the world
		FVector DropLocation = Building->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
		for (const auto& Pair : DepositedMats)
		{
			// Spawn world items for each deposited material
			for (int32 i = 0; i < Pair.Value; ++i)
			{
				// Get item definition for the world actor class
				FMOItemDefinitionRow ItemDef;
				if (UMOItemDatabaseSettings::GetItemDefinition(Pair.Key, ItemDef))
				{
					UClass* WorldActorClass = ItemDef.WorldVisual.WorldActorClass.LoadSynchronous();
					if (WorldActorClass)
					{
						FVector SpawnLocation = DropLocation + FVector(
							FMath::RandRange(-50.0f, 50.0f),
							FMath::RandRange(-50.0f, 50.0f),
							FMath::RandRange(0.0f, 30.0f)
						);
						FActorSpawnParameters SpawnParams;
						SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

						AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(WorldActorClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
						UE_LOG(LogMOFramework, Verbose, TEXT("[MOGhostContextMenu] Dropped %s at %s"),
							*Pair.Key.ToString(), *SpawnLocation.ToString());
					}
				}
			}
		}

		// Cancel the construction
		Progress->CancelConstruction(false); // Materials already dropped

		// Destroy the ghost building
		Building->Destroy();
	}

	OnCancelled.Broadcast();
	OnRequestClose.Broadcast();
}

// ============================================================================
// STATE
// ============================================================================

bool UMOGhostContextMenu::AreAllMaterialsDeposited() const
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	return Progress ? Progress->AreAllMaterialsDeposited() : false;
}

bool UMOGhostContextMenu::IsBuildTimerActive() const
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	if (!Progress)
	{
		return false;
	}
	return Progress->GetState() == EMOBuildState::Constructing;
}

float UMOGhostContextMenu::GetBuildProgress() const
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	return Progress ? Progress->GetProgress() : 0.0f;
}

void UMOGhostContextMenu::GetMaterialEntries(TArray<FMOGhostMaterialEntry>& OutEntries) const
{
	OutEntries = MaterialEntries;
}

// ============================================================================
// OVERRIDES
// ============================================================================

void UMOGhostContextMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button handlers
	if (AddBuildButton)
	{
		AddBuildButton->OnClicked.AddDynamic(this, &UMOGhostContextMenu::HandleAddBuildClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UMOGhostContextMenu::HandleCancelClicked);
	}
}

void UMOGhostContextMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update build progress UI if building
	if (IsBuildTimerActive())
	{
		UMOBuildProgressComponent* Progress = BuildProgress.Get();
		if (Progress)
		{
			float ProgressVal = Progress->GetProgress();
			float TimeRemaining = Progress->GetTimeRemaining();

			// Update progress bar
			if (BuildProgressBar)
			{
				BuildProgressBar->SetPercent(ProgressVal);
			}

			// Update time text
			if (BuildTimeText)
			{
				int32 Minutes = FMath::FloorToInt(TimeRemaining / 60.0f);
				int32 Seconds = FMath::FloorToInt(FMath::Fmod(TimeRemaining, 60.0f));
				BuildTimeText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds)));
			}

			// Blueprint callback
			OnBuildProgressUpdated(ProgressVal, TimeRemaining);
		}
	}
}

FReply UMOGhostContextMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Close on Escape
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnRequestClose.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ============================================================================
// HANDLERS
// ============================================================================

void UMOGhostContextMenu::HandleAddBuildClicked()
{
	if (AreAllMaterialsDeposited())
	{
		StartBuild();
	}
	else
	{
		AddMaterials();
	}
}

void UMOGhostContextMenu::HandleCancelClicked()
{
	CancelBuild();
}

// ============================================================================
// INTERNAL
// ============================================================================

void UMOGhostContextMenu::UpdateButtonState()
{
	bool bShowBuild = AreAllMaterialsDeposited();

	if (AddBuildButtonText)
	{
		AddBuildButtonText->SetText(bShowBuild ? FText::FromString(TEXT("Build")) : FText::FromString(TEXT("Add")));
	}

	// Show/hide progress bar based on state
	if (BuildProgressBar)
	{
		BuildProgressBar->SetVisibility(IsBuildTimerActive() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (BuildTimeText)
	{
		BuildTimeText->SetVisibility(IsBuildTimerActive() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Blueprint callback
	OnButtonStateChanged(bShowBuild);
}

UTextBlock* UMOGhostContextMenu::CreateMaterialTextWidget(const FMOGhostMaterialEntry& Entry)
{
	UTextBlock* TextWidget = NewObject<UTextBlock>(this);
	if (TextWidget)
	{
		UpdateMaterialTextWidget(TextWidget, Entry);
	}
	return TextWidget;
}

void UMOGhostContextMenu::UpdateMaterialTextWidget(UTextBlock* Widget, const FMOGhostMaterialEntry& Entry)
{
	if (!Widget)
	{
		return;
	}

	// Format: "Material Name 3/5"
	FString Text = FString::Printf(TEXT("%s %d/%d"),
		*Entry.DisplayName.ToString(),
		Entry.Deposited,
		Entry.Required);

	Widget->SetText(FText::FromString(Text));

	// Green if complete, default otherwise
	if (Entry.IsComplete())
	{
		Widget->SetColorAndOpacity(FLinearColor(0.2f, 1.0f, 0.2f, 1.0f));
	}
	else
	{
		Widget->SetColorAndOpacity(FLinearColor::White);
	}
}

bool UMOGhostContextMenu::TryGatherMaterial(FName ItemId)
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	if (!Progress)
	{
		return false;
	}

	// Check if we still need this material
	int32 Required = Progress->GetRequiredCount(ItemId);
	int32 Deposited = Progress->GetDepositedCount(ItemId);
	if (Deposited >= Required)
	{
		return false;
	}

	// 1. Try player inventory first
	if (InventoryCheckbox && InventoryCheckbox->IsChecked())
	{
		UMOInventoryComponent* Inventory = BuilderInventory.Get();
		if (Inventory && Inventory->HasItem(ItemId, 1))
		{
			if (Inventory->RemoveItemByDefinitionId(ItemId, 1))
			{
				Progress->DepositMaterial(ItemId);
				UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Gathered %s from inventory"), *ItemId.ToString());
				return true;
			}
		}
	}

	// Get gather range from progress
	float GatherRange = Progress->GetProgressData().GatherRange;
	if (GatherRange <= 0.0f)
	{
		GatherRange = 500.0f; // Default range
	}

	AMOBuildableActor* Building = TargetBuilding.Get();
	if (!Building)
	{
		return false;
	}

	FVector Origin = Building->GetActorLocation();

	// 2. Try nearby containers
	if (ContainersCheckbox && ContainersCheckbox->IsChecked())
	{
		TArray<AActor*> Sources = Progress->FindMaterialSources(GatherRange);
		for (AActor* Source : Sources)
		{
			AMOContainerActor* Container = Cast<AMOContainerActor>(Source);
			if (Container)
			{
				UMOInventoryComponent* ContainerInv = Container->GetContainerInventory();
				if (ContainerInv && ContainerInv->HasItem(ItemId, 1))
				{
					if (ContainerInv->RemoveItemByDefinitionId(ItemId, 1))
					{
						Progress->DepositMaterial(ItemId);
						UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Gathered %s from container: %s"),
							*ItemId.ToString(), *Container->GetName());
						return true;
					}
				}
			}
		}
	}

	// 3. Try surrounding world items
	if (SurroundingCheckbox && SurroundingCheckbox->IsChecked())
	{
		TArray<AActor*> Sources = Progress->FindMaterialSources(GatherRange);
		for (AActor* Source : Sources)
		{
			AMOWorldItem* WorldItem = Cast<AMOWorldItem>(Source);
			if (WorldItem)
			{
				UMOItemComponent* ItemComp = WorldItem->GetItemComponent();
				if (ItemComp && ItemComp->ItemDefinitionId == ItemId && ItemComp->bWorldItemActive)
				{
					int32 Available = ItemComp->Quantity;
					if (Available > 0)
					{
						// Take one from the world item
						if (Available == 1)
						{
							// Remove the world item
							ItemComp->SetWorldItemActive(false);
							WorldItem->Destroy();
						}
						else
						{
							ItemComp->Quantity = Available - 1;
						}

						Progress->DepositMaterial(ItemId);
						UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Gathered %s from world item: %s"),
							*ItemId.ToString(), *WorldItem->GetName());
						return true;
					}
				}
			}
		}
	}

	return false;
}

void UMOGhostContextMenu::PopulateMaterialList()
{
	if (!MaterialListContainer)
	{
		return;
	}

	// Clear existing widgets
	MaterialListContainer->ClearChildren();
	MaterialTextWidgets.Empty();

	// Create text widgets for each material
	for (const FMOGhostMaterialEntry& Entry : MaterialEntries)
	{
		UTextBlock* TextWidget = CreateMaterialTextWidget(Entry);
		if (TextWidget)
		{
			MaterialListContainer->AddChild(TextWidget);
			MaterialTextWidgets.Add(TextWidget);
		}
	}
}
