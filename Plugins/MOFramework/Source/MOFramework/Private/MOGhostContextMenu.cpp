#include "MOGhostContextMenu.h"
#include "MOBuildableActor.h"
#include "MOBuildProgressComponent.h"
#include "MOInventoryComponent.h"
#include "MOSkillsComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOWorldItem.h"
#include "MOItemComponent.h"
#include "MOContainerActor.h"
#include "MOBuildingTypes.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOUIUtils.h"
#include "MOMaterialSourceInterface.h"
#include "Components/CheckBox.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"

const FName UMOGhostContextMenu::BuildingSkillId = FName("Construction");

UMOGhostContextMenu::UMOGhostContextMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Mouse leave closing is disabled by default (uses click-outside)
	bCloseOnMouseLeave = false;
}

void UMOGhostContextMenu::RequestClose()
{
	// Broadcast legacy delegate for backward compatibility
	OnRequestClose.Broadcast();
	// Also call base which broadcasts OnCloseRequested
	Super::RequestClose();
}

bool UMOGhostContextMenu::ShouldCloseOnMouseLeave() const
{
	// Don't close on mouse leave while build timer is active
	return bCloseOnMouseLeave && !IsBuildTimerActive();
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

	// Get skills component from the builder's owner
	if (InBuilderInventory)
	{
		AActor* BuilderOwner = InBuilderInventory->GetOwner();
		if (BuilderOwner)
		{
			BuilderSkills = BuilderOwner->FindComponentByClass<UMOSkillsComponent>();
		}
	}

	// Clear any previous deposited materials tracking
	DepositedMaterialsForRefund.Empty();

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
		// Determine if build has started (construction in progress)
		const bool bBuildStarted = (Progress->GetState() == EMOBuildState::Constructing ||
			Progress->GetState() == EMOBuildState::Paused);

		// Calculate refund based on build state and skill
		const int32 TotalDeposited = DepositedMaterialsForRefund.Num();
		const int32 RefundCount = CalculateRefundAmount(TotalDeposited, bBuildStarted);

		// Drop refunded materials (highest rarity items are kept, lowest rarity lost first)
		FVector DropLocation = Building->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
		DropRefundedMaterials(RefundCount, DropLocation);

		UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Cancel refund: %d/%d materials returned (build started: %s)"),
			RefundCount, TotalDeposited, bBuildStarted ? TEXT("yes") : TEXT("no"));

		// Cancel the construction
		Progress->CancelConstruction(false); // Materials already handled

		// Destroy the ghost building
		Building->Destroy();
	}

	// Clear tracking
	DepositedMaterialsForRefund.Empty();

	OnCancelled.Broadcast();
	RequestClose();
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
	if (AddMaterialsButton)
	{
		AddMaterialsButton->OnClicked().AddUObject(this, &UMOGhostContextMenu::HandleAddMaterialsClicked);
	}
	if (BuildButton)
	{
		BuildButton->OnClicked().AddUObject(this, &UMOGhostContextMenu::HandleBuildClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked().AddUObject(this, &UMOGhostContextMenu::HandleCancelClicked);
	}
}

void UMOGhostContextMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	// Base class handles mouse leave timer
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Check for build completion
	if (IsBuildComplete())
	{
		// Build finished - close and update ghost visual
		RequestClose();
		return;
	}

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
				BuildTimeText->SetText(UMOUIUtils::FormatDurationAsTimeCode(TimeRemaining));
			}

			// Blueprint callback
			OnBuildProgressUpdated(ProgressVal, TimeRemaining);
		}
	}
}

// NativeOnKeyDown, NativeOnMouseEnter, NativeOnMouseLeave, SetPopupPosition
// are now inherited from UMOContextMenuBase

bool UMOGhostContextMenu::IsBuildComplete() const
{
	UMOBuildProgressComponent* Progress = BuildProgress.Get();
	return Progress && Progress->GetState() == EMOBuildState::Complete;
}

// ============================================================================
// HANDLERS
// ============================================================================

void UMOGhostContextMenu::HandleAddMaterialsClicked()
{
	AddMaterials();
}

void UMOGhostContextMenu::HandleBuildClicked()
{
	if (AreAllMaterialsDeposited())
	{
		StartBuild();
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
	const bool bAllMaterialsReady = AreAllMaterialsDeposited();
	const bool bBuildActive = IsBuildTimerActive();

	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] UpdateButtonState: AllMaterials=%d, BuildActive=%d, HasProgressBar=%d, HasTimeText=%d"),
		bAllMaterialsReady ? 1 : 0, bBuildActive ? 1 : 0,
		BuildProgressBar != nullptr ? 1 : 0, BuildTimeText != nullptr ? 1 : 0);

	// Build button - enabled only when all materials deposited and not already building
	if (BuildButton)
	{
		BuildButton->SetIsEnabled(bAllMaterialsReady && !bBuildActive);
	}

	// Add Materials button - hidden when all materials deposited or when building
	if (AddMaterialsButton)
	{
		AddMaterialsButton->SetVisibility((bAllMaterialsReady || bBuildActive)
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}

	// Progress bar and time text are always visible
	// They show 0% / 00:00 before build starts, then update during build
	if (BuildProgressBar)
	{
		BuildProgressBar->SetVisibility(ESlateVisibility::Visible);
		if (!bBuildActive)
		{
			BuildProgressBar->SetPercent(0.0f);
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] BuildProgressBar not bound! Add a ProgressBar named 'BuildProgressBar' to the widget."));
	}

	if (BuildTimeText)
	{
		BuildTimeText->SetVisibility(ESlateVisibility::Visible);
		if (!bBuildActive)
		{
			// Show total build time before starting
			UMOBuildProgressComponent* Progress = BuildProgress.Get();
			if (Progress)
			{
				float TotalTime = Progress->GetProgressData().TotalBuildTime;
				BuildTimeText->SetText(UMOUIUtils::FormatDurationAsTimeCode(TotalTime));
			}
			else
			{
				BuildTimeText->SetText(FText::FromString(TEXT("--:--")));
			}
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] BuildTimeText not bound! Add a TextBlock named 'BuildTimeText' to the widget."));
	}

	// Blueprint callback
	OnButtonStateChanged(bAllMaterialsReady);
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

	// Get gather range from progress
	float GatherRange = Progress->GetProgressData().GatherRange;
	if (GatherRange <= 0.0f)
	{
		GatherRange = 500.0f; // Default range
	}

	// Get all material sources and sort by priority
	TArray<AActor*> Sources = Progress->FindMaterialSources(GatherRange);

	// Add builder (player) as a potential source if checkbox is checked
	if (InventoryCheckbox && InventoryCheckbox->IsChecked())
	{
		UMOInventoryComponent* Inventory = BuilderInventory.Get();
		if (Inventory)
		{
			AActor* OwnerActor = Inventory->GetOwner();
			if (OwnerActor && !Sources.Contains(OwnerActor))
			{
				Sources.Add(OwnerActor);
			}
		}
	}

	// Sort by priority (highest first) using interface
	Sources.Sort([](AActor& A, AActor& B) {
		int32 PriorityA = A.Implements<UMOMaterialSourceInterface>()
			? IMOMaterialSourceInterface::Execute_GetMaterialSourcePriority(&A) : 0;
		int32 PriorityB = B.Implements<UMOMaterialSourceInterface>()
			? IMOMaterialSourceInterface::Execute_GetMaterialSourcePriority(&B) : 0;
		return PriorityA > PriorityB;
	});

	// Filter sources based on checkbox states
	for (AActor* Source : Sources)
	{
		if (!Source->Implements<UMOMaterialSourceInterface>())
		{
			continue;
		}

		// Determine source type and check if enabled
		int32 SourcePriority = IMOMaterialSourceInterface::Execute_GetMaterialSourcePriority(Source);

		// SourcePriority 100 = Inventory (player), 50 = Containers, 25 = World Items
		bool bSourceEnabled = false;
		if (SourcePriority >= 100)
		{
			bSourceEnabled = InventoryCheckbox && InventoryCheckbox->IsChecked();
		}
		else if (SourcePriority >= 50)
		{
			bSourceEnabled = ContainersCheckbox && ContainersCheckbox->IsChecked();
		}
		else
		{
			bSourceEnabled = SurroundingCheckbox && SurroundingCheckbox->IsChecked();
		}

		if (!bSourceEnabled)
		{
			continue;
		}

		// Try to gather from this source via interface
		if (IMOMaterialSourceInterface::Execute_CanProvideMaterial(Source, ItemId, 1))
		{
			int32 Gathered = IMOMaterialSourceInterface::Execute_GatherMaterial(Source, ItemId, 1);
			if (Gathered > 0)
			{
				Progress->DepositMaterial(ItemId);

				// Track deposited material for potential refund
				FMODepositedMaterial DepositedMat;
				DepositedMat.ItemId = ItemId;

				// Get rarity from item definition
				FMOItemDefinitionRow ItemDef;
				if (UMOItemDatabaseSettings::GetItemDefinition(ItemId, ItemDef))
				{
					DepositedMat.Rarity = ItemDef.Rarity;
				}
				DepositedMaterialsForRefund.Add(DepositedMat);

				UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Gathered %s from %s (priority %d)"),
					*ItemId.ToString(), *Source->GetName(), SourcePriority);
				return true;
			}
		}
	}

	return false;
}

void UMOGhostContextMenu::PopulateMaterialList()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] PopulateMaterialList called, MaterialEntries: %d"), MaterialEntries.Num());

	if (!MaterialListContainer)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOGhostContextMenu] MaterialListContainer is NULL - widget not bound! Make sure widget is named 'MaterialListContainer' and marked as variable."));
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
			UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Added material: %s (%d/%d)"),
				*Entry.ItemId.ToString(), Entry.Deposited, Entry.Required);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] PopulateMaterialList complete, added %d widgets"), MaterialTextWidgets.Num());
}

int32 UMOGhostContextMenu::CalculateRefundAmount(int32 TotalDeposited, bool bBuildStarted) const
{
	if (TotalDeposited <= 0)
	{
		return 0;
	}

	// Before build starts: 100% refund
	if (!bBuildStarted)
	{
		return TotalDeposited;
	}

	// After build starts: 50% base + skill bonus (0-45% for skill 0-100)
	constexpr float BaseRefundPercent = 0.50f;
	constexpr float MaxSkillBonus = 0.45f;

	float SkillBonus = 0.0f;
	if (UMOSkillsComponent* Skills = BuilderSkills.Get())
	{
		const int32 SkillLevel = Skills->GetSkillLevel(BuildingSkillId);
		// Clamp to 0-100 and calculate bonus
		const float NormalizedSkill = FMath::Clamp(static_cast<float>(SkillLevel), 0.0f, 100.0f) / 100.0f;
		SkillBonus = NormalizedSkill * MaxSkillBonus;
	}

	const float TotalRefundPercent = BaseRefundPercent + SkillBonus;
	const int32 RefundAmount = FMath::FloorToInt(TotalDeposited * TotalRefundPercent);

	UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Refund calc: %d total, %.1f%% base + %.1f%% skill = %d returned"),
		TotalDeposited, BaseRefundPercent * 100.0f, SkillBonus * 100.0f, RefundAmount);

	return RefundAmount;
}

void UMOGhostContextMenu::DropRefundedMaterials(int32 RefundCount, const FVector& DropLocation)
{
	if (RefundCount <= 0 || DepositedMaterialsForRefund.Num() == 0)
	{
		return;
	}

	// Sort by rarity (lowest first) so we lose common items first, keep rarer items
	DepositedMaterialsForRefund.Sort([](const FMODepositedMaterial& A, const FMODepositedMaterial& B) {
		return static_cast<uint8>(A.Rarity) < static_cast<uint8>(B.Rarity);
	});

	// Materials to lose are at the front (lowest rarity)
	// Materials to refund are at the back (highest rarity)
	const int32 TotalDeposited = DepositedMaterialsForRefund.Num();
	const int32 LoseCount = TotalDeposited - RefundCount;

	// Spawn refunded materials (skip the first LoseCount items)
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 i = LoseCount; i < TotalDeposited; ++i)
	{
		const FMODepositedMaterial& Mat = DepositedMaterialsForRefund[i];

		FMOItemDefinitionRow ItemDef;
		if (!UMOItemDatabaseSettings::GetItemDefinition(Mat.ItemId, ItemDef))
		{
			continue;
		}

		UClass* WorldActorClass = ItemDef.WorldVisual.WorldActorClass.LoadSynchronous();
		if (!WorldActorClass)
		{
			continue;
		}

		// Scatter spawn locations slightly
		FVector SpawnLocation = DropLocation + FVector(
			FMath::RandRange(-50.0f, 50.0f),
			FMath::RandRange(-50.0f, 50.0f),
			FMath::RandRange(0.0f, 30.0f)
		);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActor* SpawnedActor = World->SpawnActor<AActor>(WorldActorClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

		if (SpawnedActor)
		{
			// Apply item definition visuals if it's a world item
			if (AMOWorldItem* WorldItem = Cast<AMOWorldItem>(SpawnedActor))
			{
				WorldItem->ApplyItemDefinitionToWorldMesh();
				WorldItem->EnableDropPhysics();
			}

			UE_LOG(LogMOFramework, Verbose, TEXT("[MOGhostContextMenu] Refunded %s (%s rarity)"),
				*Mat.ItemId.ToString(),
				*UEnum::GetValueAsString(Mat.Rarity));
		}
	}

	// Log what was lost
	if (LoseCount > 0)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOGhostContextMenu] Lost %d materials (lowest rarity first)"), LoseCount);
	}
}
