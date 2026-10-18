#include "MOCraftingSubsystem.h"
#include "MOCraftingQueueComponent.h"
#include "MOCraftingStationActor.h"
#include "MOIdentifiableInterface.h"
#include "MOViewpointUtils.h"
#include "GameFramework/Pawn.h"

#include "MOQuestSubsystem.h"
#include "MOKnowledgeComponent.h"
#include "MOSkillsComponent.h"
#include "MOInventoryComponent.h"
#include "MOEquipmentComponent.h"
#include "MORecipeDiscoveryComponent.h"
#include "MORecipeDatabaseSettings.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOFramework.h"

// ============================================================================
// CENTRALIZED AVAILABILITY CHECK
// ============================================================================

FMORecipeAvailability UMOCraftingSubsystem::IsRecipeAvailable(
	FName RecipeId,
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent
) const
{
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		FMORecipeAvailability Result;
		Result.UnavailableReason = FText::FromString(TEXT("Recipe not found."));
		return Result;
	}

	return IsRecipeAvailable(Recipe, KnowledgeComponent, SkillsComponent);
}

FMORecipeAvailability UMOCraftingSubsystem::IsRecipeAvailable(
	const FMORecipeDefinitionRow* Recipe,
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent
) const
{
	FMORecipeAvailability Result;

	if (!Recipe)
	{
		Result.UnavailableReason = FText::FromString(TEXT("Invalid recipe."));
		return Result;
	}

	// --- Check Discovery Requirements ---
	if (Recipe->bRequiresDiscovery)
	{
		Result.DiscoveryKnowledgeId = Recipe->DiscoveryKnowledgeId;
		Result.DiscoveryKnowledgeLevel = Recipe->DiscoveryKnowledgeLevel;

		bool bDiscovered = false;

		// Knowledge-based discovery (e.g., WoodProperties level 2 unlocks HarvestBark)
		if (!Recipe->DiscoveryKnowledgeId.IsNone() && IsValid(SkillsComponent))
		{
			Result.CurrentDiscoveryKnowledgeLevel = SkillsComponent->GetSkillLevel(Recipe->DiscoveryKnowledgeId);
			bDiscovered = Result.CurrentDiscoveryKnowledgeLevel >= Recipe->DiscoveryKnowledgeLevel;
		}

		// Skill-based discovery (alternative unlock path)
		if (!bDiscovered && Recipe->DiscoverySkillLevel > 0 && !Recipe->RequiredSkillId.IsNone() && IsValid(SkillsComponent))
		{
			int32 SkillLevel = SkillsComponent->GetSkillLevel(Recipe->RequiredSkillId);
			bDiscovered = SkillLevel >= Recipe->DiscoverySkillLevel;
		}

		Result.bIsDiscovered = bDiscovered;

		if (!bDiscovered)
		{
			Result.UnavailableReason = FText::Format(
				NSLOCTEXT("MOCrafting", "NotDiscovered", "Requires {0} level {1} (current: {2})"),
				FText::FromName(Recipe->DiscoveryKnowledgeId),
				FText::AsNumber(Recipe->DiscoveryKnowledgeLevel),
				FText::AsNumber(Result.CurrentDiscoveryKnowledgeLevel)
			);
			return Result;
		}
	}

	// --- Check Knowledge Requirements ---
	TArray<FName> MissingKnowledge;
	if (!HasRequiredKnowledge(Recipe, KnowledgeComponent, SkillsComponent, &MissingKnowledge))
	{
		Result.MissingKnowledge = MissingKnowledge;
		Result.UnavailableReason = FText::FromString(TEXT("Missing required knowledge."));
		return Result;
	}

	// --- Check Skill Requirements ---
	int32 RequiredLevel = 0;
	int32 CurrentLevel = 0;
	if (!MeetsSkillRequirements(Recipe, SkillsComponent, &RequiredLevel, &CurrentLevel))
	{
		Result.RequiredSkillLevel = RequiredLevel;
		Result.CurrentSkillLevel = CurrentLevel;
		Result.UnavailableReason = FText::Format(
			NSLOCTEXT("MOCrafting", "SkillTooLowAvail", "Requires {0} level {1} (current: {2})"),
			FText::FromName(Recipe->RequiredSkillId),
			FText::AsNumber(RequiredLevel),
			FText::AsNumber(CurrentLevel)
		);
		return Result;
	}

	// All checks passed
	Result.bIsAvailable = true;
	return Result;
}

// ============================================================================
// RECIPE QUERIES
// ============================================================================

void UMOCraftingSubsystem::GetAvailableRecipes(
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent,
	EMOCraftingStation Station,
	TArray<FName>& OutRecipeIds
) const
{
	OutRecipeIds.Empty();

	// Get pre-filtered craftable recipes (excludes buildings) - O(1) cached lookup
	TArray<FName> CraftableRecipes;
	UMORecipeDatabaseSettings::GetCraftableRecipes(CraftableRecipes);

	OutRecipeIds.Reserve(CraftableRecipes.Num());

	for (const FName& RecipeId : CraftableRecipes)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe)
		{
			continue;
		}

		// Skip harvest recipes - they only appear in harvest context menus
		if (Recipe->bIsHarvestRecipe)
		{
			continue;
		}

		// Check station requirement (context-specific)
		if (Recipe->RequiredStation != EMOCraftingStation::None && Recipe->RequiredStation != Station)
		{
			continue;
		}

		// Use centralized availability check for discovery/knowledge/skill
		FMORecipeAvailability Availability = IsRecipeAvailable(Recipe, KnowledgeComponent, SkillsComponent);
		if (!Availability.bIsAvailable)
		{
			continue;
		}

		OutRecipeIds.Add(RecipeId);
	}
}

void UMOCraftingSubsystem::GetCraftableRecipes(
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent,
	UMOInventoryComponent* InventoryComponent,
	EMOCraftingStation Station,
	TArray<FName>& OutRecipeIds
) const
{
	// First get available recipes
	TArray<FName> AvailableRecipes;
	GetAvailableRecipes(KnowledgeComponent, SkillsComponent, Station, AvailableRecipes);

	OutRecipeIds.Empty();

	for (const FName& RecipeId : AvailableRecipes)
	{
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
		if (!Recipe)
		{
			continue;
		}

		// Check if has all ingredients
		if (HasIngredients(Recipe, InventoryComponent, KnowledgeComponent))
		{
			OutRecipeIds.Add(RecipeId);
		}
	}
}

FMOCraftingExecutionContext UMOCraftingSubsystem::ResolveExecutionContext(FName RecipeId, AActor* Crafter, AActor* StationActor) const
{
	FMOCraftingExecutionContext Context;
	auto Refuse = [&Context](EMOCraftingContextFailure Failure, const TCHAR* Reason)
	{
		Context.Failure = Failure;
		Context.FailureReason = FText::FromString(Reason);
		return Context;
	};
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe || Recipe->bIsBuilding) return Refuse(EMOCraftingContextFailure::InvalidRecipe, TEXT("This is not a crafting recipe."));
	if (Recipe->bIsHarvestRecipe) return Refuse(EMOCraftingContextFailure::HarvestActionRequired, TEXT("Gathering requires a resource target and a harvest action."));
	if (!FMath::IsFinite(Recipe->CraftTime) || Recipe->CraftTime <= 0.0f) return Refuse(EMOCraftingContextFailure::InvalidDuration, TEXT("Crafting requires a positive action duration."));
	if (!IsValid(Crafter) || !Crafter->GetWorld()) return Refuse(EMOCraftingContextFailure::InvalidCrafter, TEXT("No valid crafter."));
	if (Recipe->RequiredStation == EMOCraftingStation::None && Recipe->MinimumProcessTemperatureCelsius <= 0.0f) return Context;
	AMOCraftingStationActor* Station = Cast<AMOCraftingStationActor>(StationActor);
	if (!IsValid(Station) || Station->GetWorld() != Crafter->GetWorld()) return Refuse(EMOCraftingContextFailure::MissingStation, TEXT("A real crafting station is required."));
	Context.StationType = Station->GetStationType();
	Context.StationGuid = IMOIdentifiableInterface::Execute_GetPersistentGuid(Station);
	Context.AchievableTemperatureCelsius = Station->GetAchievableProcessTemperatureCelsius();
	if (!Context.StationGuid.IsValid()) return Refuse(EMOCraftingContextFailure::MissingStation, TEXT("The station has no persistent identity."));
	if (Recipe->RequiredStation != EMOCraftingStation::None && Context.StationType != Recipe->RequiredStation) return Refuse(EMOCraftingContextFailure::WrongStation, TEXT("Wrong crafting station."));
	if (!Station->IsComplete()) return Refuse(EMOCraftingContextFailure::IncompleteStation, TEXT("Finish building this station first."));
	if (FVector::DistSquared(Crafter->GetActorLocation(), Station->GetActorLocation()) > FMath::Square(StationReachCm)) return Refuse(EMOCraftingContextFailure::OutOfRange, TEXT("Return to the crafting station."));
	FVector View = Crafter->GetActorLocation();
	FRotator Rotation;
	if (APawn* Pawn = Cast<APawn>(Crafter)) UMOViewpointUtils::ResolveViewpointForPawn(Pawn, View, Rotation);
	UMOViewpointUtils::FLineOfSightOptions Sight;
	Sight.IgnoredActor = Crafter;
	if (!UMOViewpointUtils::HasLineOfSight(Crafter->GetWorld(), View, Station, Sight)) return Refuse(EMOCraftingContextFailure::Obstructed, TEXT("The crafting station is obstructed."));
	if (!Station->IsStationActive()) return Refuse(EMOCraftingContextFailure::InactiveStation, TEXT("The crafting station is inactive or has no fuel."));
	if (!FMath::IsFinite(Context.AchievableTemperatureCelsius) || Context.AchievableTemperatureCelsius < Recipe->MinimumProcessTemperatureCelsius)
		return Refuse(EMOCraftingContextFailure::InsufficientTemperature, TEXT("The station cannot reach this recipe's required temperature."));
	return Context;
}

FMOCraftingValidation UMOCraftingSubsystem::CanCraftRecipeAtStation(FName RecipeId, UMOKnowledgeComponent* Knowledge,
	UMOSkillsComponent* Skills, UMOInventoryComponent* Inventory, AActor* Station) const
{
	const auto Context = ResolveExecutionContext(RecipeId, Inventory ? Inventory->GetOwner() : nullptr, Station);
	if (!Context.IsUsable())
	{
		FMOCraftingValidation Result;
		Result.bCorrectStation = false;
		Result.bHasFuel = Context.Failure != EMOCraftingContextFailure::InactiveStation;
		Result.FailureReason = Context.FailureReason;
		return Result;
	}
	return CanCraftRecipe(RecipeId, Knowledge, Skills, Inventory, Context.StationType);
}

FMOCraftingValidation UMOCraftingSubsystem::CanCraftRecipe(
	FName RecipeId,
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent,
	UMOInventoryComponent* InventoryComponent,
	EMOCraftingStation Station
) const
{
	FMOCraftingValidation Result;

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		Result.FailureReason = FText::FromString(TEXT("Recipe not found."));
		return Result;
	}

	if (Recipe->bIsHarvestRecipe || Recipe->bIsBuilding)
	{
		Result.FailureReason = FText::FromString(TEXT("This recipe requires its own world action, not the crafting queue."));
		return Result;
	}

	// --- Use centralized availability check for discovery/knowledge/skill ---
	FMORecipeAvailability Availability = IsRecipeAvailable(Recipe, KnowledgeComponent, SkillsComponent);

	// Copy availability results to validation struct
	Result.bRecipeDiscovered = Availability.bIsDiscovered;
	Result.MissingKnowledge = Availability.MissingKnowledge;
	Result.RequiredSkillLevel = Availability.RequiredSkillLevel;
	Result.CurrentSkillLevel = Availability.CurrentSkillLevel;

	if (!Availability.bIsAvailable)
	{
		Result.FailureReason = Availability.UnavailableReason;
		return Result;
	}

	// --- Station check (context-specific, not part of availability) ---
	if (Recipe->RequiredStation != EMOCraftingStation::None && Recipe->RequiredStation != Station)
	{
		Result.bCorrectStation = false;
		Result.FailureReason = FText::FromString(TEXT("Wrong crafting station."));
		return Result;
	}

	// --- Check ingredients (can craft NOW, not availability) ---
	TMap<FName, int32> MissingIngredients;
	if (!HasIngredients(Recipe, InventoryComponent, KnowledgeComponent, &MissingIngredients))
	{
		Result.MissingIngredients = MissingIngredients;
		Result.FailureReason = FText::FromString(TEXT("Missing ingredients."));
		return Result;
	}

	// Check tools - separate required vs optional
	TArray<EMOToolType> MissingRequiredTools;
	TArray<EMOToolType> MissingOptionalTools;
	TArray<FText> MissingToolNeeds;   // the refusal names each tool (UMOItemDatabaseSettings::DescribeToolNeed)
	float TimeMultiplier = 1.0f;
	float QualityMultiplier = 1.0f;

	UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] CanCraftRecipe '%s': RequiredTools.Num()=%d"),
		*RecipeId.ToString(), Recipe->RequiredTools.Num());

	const UEnum* ToolTypeEnum = StaticEnum<EMOToolType>();
	for (const FMOToolRequirement& ToolReq : Recipe->RequiredTools)
	{
		FString ToolTypeName = ToolTypeEnum->GetNameStringByValue(static_cast<int64>(ToolReq.ToolType));
		UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting]   Checking tool: Type='%s', MinQuality=%.1f, bIsRequired=%s"),
			*ToolTypeName, ToolReq.MinQuality, ToolReq.bIsRequired ? TEXT("yes") : TEXT("no"));

		FGuid ToolGuid;
		float ToolQuality;
		if (!FindBestTool(InventoryComponent, ToolReq.ToolType, ToolReq.MinQuality, ToolGuid, ToolQuality))
		{
			if (ToolReq.bIsRequired)
			{
				// Absolutely required - blocks crafting
				MissingRequiredTools.Add(ToolReq.ToolType);
				MissingToolNeeds.Add(UMOItemDatabaseSettings::DescribeToolNeed(ToolReq.ToolType, ToolReq.MinQuality));
			}
			else
			{
				// Optional - apply penalties
				MissingOptionalTools.Add(ToolReq.ToolType);
				TimeMultiplier *= ToolReq.MissingToolTimeMultiplier;
				QualityMultiplier *= ToolReq.MissingToolQualityMultiplier;
			}
		}
	}

	// Store all missing tools info
	Result.MissingRequiredTools = MissingRequiredTools;
	Result.MissingOptionalTools = MissingOptionalTools;
	Result.MissingToolTimeMultiplier = TimeMultiplier;
	Result.MissingToolQualityMultiplier = QualityMultiplier;

	// Backwards compatibility
	Result.MissingTools = MissingRequiredTools;

	// Only block if REQUIRED tools are missing
	if (MissingRequiredTools.Num() > 0)
	{
		Result.FailureReason = FText::Join(FText::FromString(TEXT(" ")), MissingToolNeeds);
		return Result;
	}

	Result.bCanCraft = true;
	return Result;
}

void UMOCraftingSubsystem::AddOutput(UMOInventoryComponent* Inventory, const FMORecipeOutput& Output, FMOCraftResult& Result)
{
	// Through the ingredient chokepoint: an item is a new stack; a liquid measure (clean water) pours into a container.
	const int32 Placed = Inventory->AddIngredient(Output.ItemDefinitionId, Output.Quantity);
	if (Placed > 0)
	{
		Result.ProducedItems.FindOrAdd(Output.ItemDefinitionId) += Placed;
	}
	if (Placed < Output.Quantity)
	{
		FMOItemDefinitionRow Def;
		const bool bLiquid = UMOItemDatabaseSettings::GetItemDefinition(Output.ItemDefinitionId, Def) && Def.LiquidMeasureMl > 0.0f;
		(bLiquid ? Result.SpilledLiquid : Result.FailedItems).FindOrAdd(Output.ItemDefinitionId) += Output.Quantity - Placed;
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCraftingSubsystem] Output %d x '%s' not placed: %s"), Output.Quantity - Placed, *Output.ItemDefinitionId.ToString(),
			bLiquid ? TEXT("no container with room (spilled)") : TEXT("inventory full"));
	}
}

FMOCraftResult UMOCraftingSubsystem::ExecuteCraft(FName RecipeId, UMOInventoryComponent* InventoryComponent, UMOSkillsComponent* SkillsComponent)
{
	// Legacy entry point submits timed work. It must never be an instant bypass around the authoritative queue.
	FMOCraftResult Result;
	AActor* Owner = IsValid(InventoryComponent) ? InventoryComponent->GetOwner() : nullptr;
	if (Owner)
	{
		if (UMOCraftingQueueComponent* Queue = Owner->FindComponentByClass<UMOCraftingQueueComponent>())
		{
			Result.bSuccess = Queue->EnqueueCraft(RecipeId, 1);
		}
	}
	return Result;
}

FMOCraftResult UMOCraftingSubsystem::ProduceOutputsOnly(
	FName RecipeId, UMOInventoryComponent* InventoryComponent, UMOSkillsComponent* SkillsComponent, AActor* StationActor)
{
	// Retained Blueprint signature: an unbacked output request must go through timed reservation.
	FMOCraftResult Result;
	AActor* Owner = IsValid(InventoryComponent) ? InventoryComponent->GetOwner() : nullptr;
	if (Owner)
	{
		if (auto* Queue = Owner->FindComponentByClass<UMOCraftingQueueComponent>())
			Result.bSuccess = Queue->EnqueueCraftAtStation(RecipeId, 1, StationActor);
	}
	return Result;
}

bool UMOCraftingSubsystem::PrepareCraftOutputs(FName RecipeId, FMOIngredientReservation& Outputs, FMOCraftResult& Result) const
{
	const auto* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe || Recipe->bIsHarvestRecipe || Recipe->bIsBuilding) return false;
	TMap<FName, int32> Quantities;
	for (const auto& Output : Recipe->Outputs)
	{
		FMOItemDefinitionRow Item;
		if (Output.ItemDefinitionId.IsNone() || Output.Quantity <= 0 || !FMath::IsFinite(Output.Chance)
			|| Output.Chance < 0 || Output.Chance > 1 || !UMOItemDatabaseSettings::GetItemDefinition(Output.ItemDefinitionId, Item)) return false;
		if (Output.Chance < 1.0f && FMath::FRand() >= Output.Chance) continue;
		const int64 Total = int64(Quantities.FindRef(Output.ItemDefinitionId)) + Output.Quantity;
		if (Total > MAX_int32) return false;
		Quantities.Add(Output.ItemDefinitionId, int32(Total));
	}
	if (!MOIngredientPlans::CreateProducts(Quantities, Outputs, UMOItemDatabaseSettings::GetItemDefinition).IsComplete()) return false;
	Result.ProducedItems = MoveTemp(Quantities);
	return true;
}

void UMOCraftingSubsystem::FinalizeReservedCraft(FName RecipeId, UMOInventoryComponent* Inventory, UMOSkillsComponent* Skills, FMOCraftResult& Result)
{
	const auto* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe || !Inventory || !Inventory->GetOwner() || !Inventory->GetOwner()->HasAuthority()) return;
	// Snapshot rewards before tool/XP delegates can change recipe caches.
	const FName SkillId = Recipe->RequiredSkillId;
	const float XP = Recipe->SkillXPReward;
	DegradeToolsForRecipe(RecipeId, Inventory);
	if (IsValid(Skills) && !SkillId.IsNone() && FMath::IsFinite(XP) && XP > 0)
	{
		Skills->AddExperience(SkillId, XP);
		Result.XPGranted.Add(SkillId, XP);
	}
	Result.bSuccess = true;
	OnCraftCompleted.Broadcast(RecipeId, Result);
	for (const auto& Produced : Result.ProducedItems)
		UMOQuestSubsystem::ReportForActor(Inventory->GetOwner(), EMOObjectiveType::ItemCraft, Produced.Key, Produced.Value);
}

float UMOCraftingSubsystem::GetRecipeCraftTime(FName RecipeId) const
{
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	return Recipe ? Recipe->CraftTime : 0.0f;
}

bool UMOCraftingSubsystem::HasRequiredKnowledge(
	const FMORecipeDefinitionRow* Recipe,
	UMOKnowledgeComponent* KnowledgeComponent,
	UMOSkillsComponent* SkillsComponent,
	TArray<FName>* OutMissingKnowledge
) const
{
	if (!Recipe || Recipe->RequiredKnowledge.Num() == 0)
	{
		return true;
	}

	// Try to get RecipeDiscoveryComponent from the component owner
	UMORecipeDiscoveryComponent* DiscoveryComponent = nullptr;
	if (IsValid(SkillsComponent))
	{
		if (AActor* Owner = SkillsComponent->GetOwner())
		{
			DiscoveryComponent = Owner->FindComponentByClass<UMORecipeDiscoveryComponent>();
		}
	}
	else if (IsValid(KnowledgeComponent))
	{
		if (AActor* Owner = KnowledgeComponent->GetOwner())
		{
			DiscoveryComponent = Owner->FindComponentByClass<UMORecipeDiscoveryComponent>();
		}
	}

	bool bHasAll = true;
	for (const FName& KnowledgeId : Recipe->RequiredKnowledge)
	{
		bool bHasKnowledge = false;

		// Check KnowledgeComponent first (legacy system)
		if (IsValid(KnowledgeComponent) && KnowledgeComponent->HasKnowledge(KnowledgeId))
		{
			bHasKnowledge = true;
		}

		// Fallback: Check if player has this as a skill with level >= 1
		// This bridges the gap between the unused KnowledgeComponent and active SkillsComponent
		if (!bHasKnowledge && IsValid(SkillsComponent))
		{
			if (SkillsComponent->HasSkillLevel(KnowledgeId, 1))
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] Knowledge '%s' satisfied via SkillsComponent (level >= 1)"),
					*KnowledgeId.ToString());
				bHasKnowledge = true;
			}
		}

		// Fallback: Check if there's a recipe with this ID that's been discovered
		if (!bHasKnowledge && IsValid(DiscoveryComponent))
		{
			if (DiscoveryComponent->IsRecipeDiscovered(KnowledgeId))
			{
				UE_LOG(LogMOFramework, Log, TEXT("[MOCrafting] Knowledge '%s' satisfied via RecipeDiscoveryComponent"),
					*KnowledgeId.ToString());
				bHasKnowledge = true;
			}
		}

		if (!bHasKnowledge)
		{
			bHasAll = false;
			if (OutMissingKnowledge)
			{
				OutMissingKnowledge->Add(KnowledgeId);
			}
		}
	}

	return bHasAll;
}

bool UMOCraftingSubsystem::MeetsSkillRequirements(
	const FMORecipeDefinitionRow* Recipe,
	UMOSkillsComponent* SkillsComponent,
	int32* OutRequiredLevel,
	int32* OutCurrentLevel
) const
{
	if (!Recipe || Recipe->RequiredSkillId.IsNone() || Recipe->RequiredSkillLevel <= 0)
	{
		return true;
	}

	const int32 RequiredLevel = Recipe->RequiredSkillLevel;
	const int32 CurrentLevel = IsValid(SkillsComponent) ? SkillsComponent->GetSkillLevel(Recipe->RequiredSkillId) : 0;

	if (OutRequiredLevel)
	{
		*OutRequiredLevel = RequiredLevel;
	}
	if (OutCurrentLevel)
	{
		*OutCurrentLevel = CurrentLevel;
	}

	return CurrentLevel >= RequiredLevel;
}

bool UMOCraftingSubsystem::HasIngredients(
	const FMORecipeDefinitionRow* Recipe,
	UMOInventoryComponent* InventoryComponent,
	UMOKnowledgeComponent* KnowledgeComponent,
	TMap<FName, int32>* OutMissingIngredients
) const
{
	if (!Recipe || !IsValid(InventoryComponent))
	{
		return false;
	}

	bool bHasAll = true;
	for (const FMORecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		// Check knowledge requirement for ingredient
		if (Ingredient.bRequiresKnowledge && IsValid(KnowledgeComponent))
		{
			// The ingredient's item must have been inspected/learned
			// For simplicity, we check if any knowledge from that item has been learned
			// A more sophisticated check would verify specific knowledge IDs
			FMOItemKnowledgeProgress Progress;
			if (!KnowledgeComponent->GetInspectionProgress(Ingredient.ItemDefinitionId, Progress) || Progress.InspectionCount == 0)
			{
				bHasAll = false;
				if (OutMissingIngredients)
				{
					// Mark as missing - quantity 0 indicates knowledge issue
					OutMissingIngredients->Add(Ingredient.ItemDefinitionId, Ingredient.Quantity);
				}
				continue;
			}
		}

		const int32 Available = InventoryComponent->CountIngredient(Ingredient.ItemDefinitionId);
		if (Available < Ingredient.Quantity)
		{
			bHasAll = false;
			if (OutMissingIngredients)
			{
				OutMissingIngredients->Add(Ingredient.ItemDefinitionId, Ingredient.Quantity - Available);
			}
		}
	}

	return bHasAll;
}

bool UMOCraftingSubsystem::HasRequiredTools(
	const FMORecipeDefinitionRow* Recipe,
	UMOInventoryComponent* Inventory,
	TArray<EMOToolType>* OutMissingTools
) const
{
	if (!Recipe || Recipe->RequiredTools.Num() == 0)
	{
		return true;
	}

	if (!IsValid(Inventory))
	{
		if (OutMissingTools)
		{
			for (const FMOToolRequirement& Req : Recipe->RequiredTools)
			{
				// Only report truly required tools as missing
				if (Req.bIsRequired)
				{
					OutMissingTools->Add(Req.ToolType);
				}
			}
		}
		return false;
	}

	bool bHasAllRequired = true;
	for (const FMOToolRequirement& ToolReq : Recipe->RequiredTools)
	{
		FGuid FoundGuid;
		float FoundQuality;
		if (!FindBestTool(Inventory, ToolReq.ToolType, ToolReq.MinQuality, FoundGuid, FoundQuality))
		{
			// Only fail for required tools, optional tools just get penalties
			if (ToolReq.bIsRequired)
			{
				bHasAllRequired = false;
				if (OutMissingTools)
				{
					OutMissingTools->Add(ToolReq.ToolType);
				}
			}
		}
	}

	return bHasAllRequired;
}

bool UMOCraftingSubsystem::FindBestTool(
	UMOInventoryComponent* Inventory,
	EMOToolType ToolType,
	float MinEffectiveness,
	FGuid& OutItemGuid,
	float& OutEffectiveness
) const
{
	OutItemGuid.Invalidate();
	OutEffectiveness = 0.0f;

	const UEnum* ToolTypeEnum = StaticEnum<EMOToolType>();
	FString ToolTypeName = ToolTypeEnum->GetNameStringByValue(static_cast<int64>(ToolType));

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: Looking for ToolType='%s', MinEffectiveness=%.2f"),
		*ToolTypeName, MinEffectiveness);

	if (!IsValid(Inventory) || ToolType == EMOToolType::None)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCrafting] FindBestTool: Invalid inventory or empty tool type"));
		return false;
	}

	// Helper lambda to check if an item matches tool requirements
	auto CheckToolItem = [&](FName ItemDefinitionId, const FGuid& ItemGuid, int32 CurrentDurability, const TCHAR* Source) -> bool
	{
		FMOItemDefinitionRow ItemDef;
		if (!UMOItemDatabaseSettings::GetItemDefinition(ItemDefinitionId, ItemDef))
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: [%s] Item '%s' not in database"), Source, *ItemDefinitionId.ToString());
			return false;
		}

		// Check if this item has the required tool capability
		float ItemEffectiveness = ItemDef.GetToolEffectiveness(ToolType);

		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: [%s] Checking '%s' - bIsTool=%s, HasCapability=%s, Effectiveness=%.2f, Durability=%d"),
			Source,
			*ItemDefinitionId.ToString(),
			ItemDef.bIsTool ? TEXT("yes") : TEXT("no"),
			ItemEffectiveness > 0.0f ? TEXT("yes") : TEXT("no"),
			ItemEffectiveness,
			CurrentDurability);

		// Check if this item can function as the required tool type
		if (!ItemDef.bIsTool || ItemEffectiveness <= 0.0f)
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: [%s] '%s' - no capability for '%s'"),
				Source, *ItemDefinitionId.ToString(), *ToolTypeName);
			return false;
		}

		// Check effectiveness threshold
		if (ItemEffectiveness < MinEffectiveness)
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: [%s] '%s' - effectiveness too low (%.2f < %.2f)"),
				Source, *ItemDefinitionId.ToString(), ItemEffectiveness, MinEffectiveness);
			return false;
		}

		// Check if tool has durability remaining (if applicable)
		if (CurrentDurability == 0)
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: [%s] '%s' - broken (durability=0)"), Source, *ItemDefinitionId.ToString());
			return false; // Tool is broken
		}

		// Track the best effectiveness tool
		if (ItemEffectiveness > OutEffectiveness)
		{
			OutItemGuid = ItemGuid;
			OutEffectiveness = ItemEffectiveness;
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: [%s] '%s' - MATCHED! Effectiveness=%.2f"),
				Source, *ItemDefinitionId.ToString(), ItemEffectiveness);
			return true;
		}
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: [%s] '%s' - effectiveness not better than current (%.2f vs %.2f)"),
			Source, *ItemDefinitionId.ToString(), ItemEffectiveness, OutEffectiveness);
		return false;
	};

	// First, check equipped items (hand slots) - these should have priority
	AActor* Owner = Inventory->GetOwner();
	if (IsValid(Owner))
	{
		UMOEquipmentComponent* EquipComp = Owner->FindComponentByClass<UMOEquipmentComponent>();
		if (IsValid(EquipComp))
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: Found equipment component on '%s'"), *Owner->GetName());

			// Check left hand
			FMOEquippedItem LeftHand = EquipComp->GetEquippedItem(EMOEquipmentSlot::LeftHand);
			if (LeftHand.IsValid())
			{
				CheckToolItem(LeftHand.ItemDefinitionId, LeftHand.ItemGuid, LeftHand.CurrentDurability, TEXT("LeftHand"));
			}
			else
			{
				UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: LeftHand empty"));
			}

			// Check right hand
			FMOEquippedItem RightHand = EquipComp->GetEquippedItem(EMOEquipmentSlot::RightHand);
			if (RightHand.IsValid())
			{
				CheckToolItem(RightHand.ItemDefinitionId, RightHand.ItemGuid, RightHand.CurrentDurability, TEXT("RightHand"));
			}
			else
			{
				UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: RightHand empty"));
			}
		}
		else
		{
			UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: No equipment component on '%s'"), *Owner->GetName());
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOCrafting] FindBestTool: Inventory has no valid owner"));
	}

	// Then check inventory for potentially better tools
	TArray<FMOInventoryEntry> Entries;
	Inventory->GetInventoryEntries(Entries);

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: Checking %d inventory entries"), Entries.Num());

	for (const FMOInventoryEntry& Entry : Entries)
	{
		CheckToolItem(Entry.ItemDefinitionId, Entry.ItemGuid, Entry.CurrentDurability, TEXT("Inventory"));
	}

	UE_LOG(LogMOFramework, Verbose, TEXT("[MOCrafting] FindBestTool: Result - Found=%s, BestEffectiveness=%.2f"),
		OutItemGuid.IsValid() ? TEXT("yes") : TEXT("no"), OutEffectiveness);

	return OutItemGuid.IsValid();
}

void UMOCraftingSubsystem::DegradeToolsForRecipe(FName RecipeId, UMOInventoryComponent* Inventory)
{
	if (!IsValid(Inventory))
	{
		return;
	}

	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe || Recipe->RequiredTools.Num() == 0)
	{
		return;
	}

	const UEnum* ToolTypeEnum = StaticEnum<EMOToolType>();
	for (const FMOToolRequirement& ToolReq : Recipe->RequiredTools)
	{
		if (ToolReq.DurabilityConsumed <= 0)
		{
			continue;
		}

		FGuid ToolGuid;
		float ToolQuality;
		if (FindBestTool(Inventory, ToolReq.ToolType, ToolReq.MinQuality, ToolGuid, ToolQuality))
		{
			bool bDestroyed = false;
			Inventory->ReduceDurability(ToolGuid, ToolReq.DurabilityConsumed, bDestroyed);

			if (bDestroyed)
			{
				FString ToolTypeName = ToolTypeEnum->GetNameStringByValue(static_cast<int64>(ToolReq.ToolType));
				UE_LOG(LogMOFramework, Log, TEXT("[MOCraftingSubsystem] Tool '%s' was destroyed during crafting"), *ToolTypeName);
			}
		}
	}
}

float UMOCraftingSubsystem::GetAdjustedCraftTime(FName RecipeId, UMOInventoryComponent* Inventory) const
{
	const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
	if (!Recipe)
	{
		return 0.0f;
	}

	float BaseCraftTime = Recipe->CraftTime;

	// No tools required means no adjustment
	if (Recipe->RequiredTools.Num() == 0 || !IsValid(Inventory))
	{
		return BaseCraftTime;
	}

	// Calculate tool quality multiplier and missing optional tool penalty
	// Higher quality = faster crafting
	// Missing optional tools = time penalty (MissingToolTimeMultiplier)
	float TotalQuality = 0.0f;
	int32 ToolsWithQuality = 0;
	float MissingToolPenalty = 1.0f;

	for (const FMOToolRequirement& ToolReq : Recipe->RequiredTools)
	{
		FGuid ToolGuid;
		float ToolQuality;
		if (FindBestTool(Inventory, ToolReq.ToolType, ToolReq.MinQuality, ToolGuid, ToolQuality))
		{
			// Tool found - factor in its quality
			TotalQuality += ToolQuality;
			ToolsWithQuality++;
		}
		else if (!ToolReq.bIsRequired)
		{
			// Optional tool missing - apply time penalty
			MissingToolPenalty *= ToolReq.MissingToolTimeMultiplier;
		}
		// Required tools missing would have blocked crafting in CanCraftRecipe
	}

	// Calculate quality multiplier from tools that were found
	float QualityMultiplier = 1.0f;
	if (ToolsWithQuality > 0)
	{
		const float AverageQuality = TotalQuality / ToolsWithQuality;
		// Quality of 1.0 = base time, Quality of 2.0 = half time, etc.
		// Clamp minimum to 0.5 to prevent unreasonably fast crafting
		QualityMultiplier = (AverageQuality > KINDA_SMALL_NUMBER)
			? FMath::Max(0.5f, 1.0f / AverageQuality)
			: 1.0f;
	}

	// Apply both quality bonus and missing tool penalty
	// Example: Good knife (quality 1.5) but no shovel (5x penalty)
	// Result: BaseCraftTime * (1/1.5) * 5.0 = BaseCraftTime * 3.33
	return BaseCraftTime * QualityMultiplier * MissingToolPenalty;
}
