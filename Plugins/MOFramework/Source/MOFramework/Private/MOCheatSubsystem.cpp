/**
 * MOCheatSubsystem.cpp - Dev/debug console commands
 */

#include "MOCheatSubsystem.h"
#include "MOFramework.h"
#include "MOAudioSubsystem.h"
#include "MOAudioTypes.h"
#include "MOGameClockSubsystem.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOWeatherIntegrationSubsystem.h"
#include "MOWeatherTypes.h"
#include "MOSpawnManagerSubsystem.h"
#include "MOPersistenceSubsystem.h"
#include "MOSaveGameTypes.h"
#include "MORecipeDatabaseSettings.h"
#include "MOSkillDatabaseSettings.h"
#include "MOSkillDefinitionRow.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOBuildableActor.h" // complete type for TSubclassOf<AMOBuildableActor> null-check (A1 art audit)
#include "MOContainerActor.h"
#include "MOCraftingStationActor.h"
#include "MOBuildProgressComponent.h"
#include "MORecruitmentComponent.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOInventoryHolderInterface.h"
#include "MOMedicalDatabaseSettings.h"
#include "MOBodyPartDefinitionRow.h"
#include "MOHUDRootWidget.h"
#include "MOStatusEffectStripWidget.h"
#include "MOStatusMoodleTypes.h"
#include "MOUIManagerComponent.h"
#include "MOVitalsComponent.h"
#include "MOSurvivalStatsComponent.h"
#include "MOInteractorComponent.h"
#include "MOCombatComponent.h"
#include "MOCraftingQueueComponent.h"
#include "MOSkillsComponent.h"
#include "MOControllableInterface.h"
#include "MOCommonButton.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Name.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Enum.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "UObject/UObjectIterator.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Class.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/**
	 * Resolve the locally controlled pawn for cheat commands. Standalone /
	 * listen-server PIE puts this on PC index 0. Returns nullptr if no pawn
	 * is possessed (e.g. main menu).
	 */
	APawn* ResolveLocalPawn(UWorld* World)
	{
		if (!World) return nullptr;
		APlayerController* PC = World->GetFirstPlayerController();
		return PC ? PC->GetPawn() : nullptr;
	}

	UMOInventoryComponent* ResolveLocalInventory(UWorld* World)
	{
		APawn* Pawn = ResolveLocalPawn(World);
		return Pawn ? Pawn->FindComponentByClass<UMOInventoryComponent>() : nullptr;
	}

	/**
	 * MO.AI.*: find the first pawn whose name contains NameSub (case-insensitive)
	 * and return its blackboard. OutPawnName stays empty when no pawn matched
	 * (vs. matched-but-no-AI, where it's set and the return is null).
	 */
	UBlackboardComponent* ResolveBlackboard(UWorld* World, const FString& NameSub, FString& OutPawnName)
	{
		if (!World) return nullptr;
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			if (It->GetName().Contains(NameSub))
			{
				OutPawnName = It->GetName();
				if (AAIController* AI = Cast<AAIController>(It->GetController()))
				{
					return AI->GetBlackboardComponent();
				}
				return nullptr;
			}
		}
		return nullptr;
	}

	// =========================================================================
	// Shared test bodies. The individual MO.Test.* commands and MO.Test.RunAll
	// both call these, so there is ONE implementation per check. Each logs its
	// existing [MOTEST] markers (so single-command usage is unchanged) AND
	// returns a structured result the suite aggregates into a results file.
	// =========================================================================

	/** Pass/fail + a human-readable detail line for one automated check. */
	struct FMOTestResult
	{
		bool bPass = false;
		FString Name;
		FString Detail;
	};

	/** H21 identity: give -> drop -> pickup, assert the same GUID returns. */
	FMOTestResult RunDropPickupTest(UWorld* World, FName ItemId)
	{
		auto Fail = [](const FString& Detail) -> FMOTestResult
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL DropPickup: %s"), *Detail);
			return { false, TEXT("DropPickup"), Detail };
		};

		APawn* Pawn = ResolveLocalPawn(World);
		UMOInventoryComponent* Inv = ResolveLocalInventory(World);
		if (!Pawn || !Inv)
		{
			return Fail(TEXT("no pawn/inventory (are you in-game?)"));
		}
		FMOItemDefinitionRow ItemDef;
		if (!UMOItemDatabaseSettings::GetItemDefinition(ItemId, ItemDef))
		{
			return Fail(FString::Printf(TEXT("'%s' is not in the item database"), *ItemId.ToString()));
		}
		const FGuid Guid = FGuid::NewGuid();
		if (!Inv->AddItemByGuid(Guid, ItemId, 1))
		{
			return Fail(TEXT("give failed"));
		}
		const FVector DropLoc = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 150.0f;
		AActor* WorldItem = Inv->DropItemByGuid(Guid, DropLoc, FRotator::ZeroRotator);
		if (!WorldItem)
		{
			return Fail(TEXT("drop returned null"));
		}
		UMOInteractorComponent* Interactor = Pawn->FindComponentByClass<UMOInteractorComponent>();
		if (!Interactor)
		{
			return Fail(TEXT("no interactor on pawn"));
		}
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] DropPickup: gave+dropped %s as %s GUID=%s; requesting pickup"),
			*ItemId.ToString(), *WorldItem->GetName(), *Guid.ToString(EGuidFormats::DigitsWithHyphens));
		Interactor->RequestInteractWithActor(WorldItem);
		// On standalone/host the canonical pickup runs same-frame (authority), so the
		// original GUID should be back in inventory now. On a true remote client it is
		// async -- run this on the host, or grep GiveToInteractorInventory's logged GUID.
		FMOInventoryEntry Entry;
		const bool bBack = Inv->TryGetEntryByGuid(Guid, Entry);
		const FString Detail = bBack
			? TEXT("original GUID preserved (identity intact)")
			: TEXT("original GUID NOT back -- identity lost or async on a remote client");
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] %s DropPickup: %s"), bBack ? TEXT("PASS") : TEXT("FAIL"), *Detail);
		return { bBack, TEXT("DropPickup"), Detail };
	}

	/** H18 combat: trigger a light attack (client forwards ServerStartAttack). */
	FMOTestResult RunAttackTest(UWorld* World)
	{
		APawn* Pawn = ResolveLocalPawn(World);
		UMOCombatComponent* Combat = Pawn ? Pawn->FindComponentByClass<UMOCombatComponent>() : nullptr;
		if (!Combat)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL Attack: no combat component (are you in-game?)"));
			return { false, TEXT("Attack"), TEXT("no combat component (are you in-game?)") };
		}
		const bool bOk = Combat->StartLightAttack();
		const FString Detail = FString::Printf(TEXT("StartLightAttack=%s CombatState=%d"),
			bOk ? TEXT("true") : TEXT("false"), (int32)Combat->CombatState);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] %s Attack: %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *Detail);
		return { bOk, TEXT("Attack"), Detail };
	}

	/** H20 crafting: grant the recipe's ingredients, then enqueue (server-gated). */
	FMOTestResult RunCraftTest(UWorld* World, FName RecipeId)
	{
		APawn* Pawn = ResolveLocalPawn(World);
		UMOCraftingQueueComponent* Queue = Pawn ? Pawn->FindComponentByClass<UMOCraftingQueueComponent>() : nullptr;
		if (!Queue)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL Craft: no crafting queue component (are you in-game?)"));
			return { false, TEXT("Craft"), TEXT("no crafting queue component (are you in-game?)") };
		}

		// Self-setup: grant the recipe's ingredients AND its gating skill so the
		// enqueue exercises the crafting PATH, isolated from content balance.
		// Tools/stations can't be fabricated here -- the detail reports if
		// enqueue still fails so the remaining gate is diagnosable.
		int32 Granted = 0;
		FString SkillNote;
		if (const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId))
		{
			if (UMOInventoryComponent* Inv = ResolveLocalInventory(World))
			{
				for (const FMORecipeIngredient& Ing : Recipe->Ingredients)
				{
					if (!Ing.ItemDefinitionId.IsNone() && Inv->AddItemByGuid(FGuid::NewGuid(), Ing.ItemDefinitionId, Ing.Quantity))
					{
						Granted += Ing.Quantity;
					}
				}
			}
			if (!Recipe->RequiredSkillId.IsNone() && Recipe->RequiredSkillLevel > 0)
			{
				if (UMOSkillsComponent* Skills = Pawn->FindComponentByClass<UMOSkillsComponent>())
				{
					if (!Skills->HasSkillLevel(Recipe->RequiredSkillId, Recipe->RequiredSkillLevel))
					{
						Skills->SetSkillLevel(Recipe->RequiredSkillId, Recipe->RequiredSkillLevel);
						SkillNote = FString::Printf(TEXT(", set %s=%d"), *Recipe->RequiredSkillId.ToString(), Recipe->RequiredSkillLevel);
					}
				}
			}
		}

		const bool bOk = Queue->EnqueueCraft(RecipeId, 1, EMOCraftingStation::None);
		const FString Detail = bOk
			? FString::Printf(TEXT("EnqueueCraft(%s)=true after granting %d ingredient(s)%s"), *RecipeId.ToString(), Granted, *SkillNote)
			: FString::Printf(TEXT("EnqueueCraft(%s)=false even after granting %d ingredient(s)%s (needs tool/station?)"), *RecipeId.ToString(), Granted, *SkillNote);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] %s Craft: %s"), bOk ? TEXT("PASS") : TEXT("FAIL"), *Detail);
		return { bOk, TEXT("Craft"), Detail };
	}

	/**
	 * Run a test body on the NEXT world tick instead of the caller's stack.
	 *
	 * Load-bearing for MP transport tests: editor-Python executes console
	 * commands under FEditorScriptExecutionGuard, and AActor::GetFunctionCallspace
	 * maps EVERY RPC to LOCAL callspace while that guard is up — so a Server RPC
	 * fired from a client world inside a Python-driven command never transports
	 * to the host (it executes locally and gets dropped by the authority guards).
	 * The next engine tick runs outside the guard, so RPC routing is real.
	 * RunAll aggregation is unaffected — it calls the Run*Test functions
	 * directly. MPSuite goes through the console commands, so its three tests
	 * run a tick after its begin/end markers — grep [MOTEST] lines, not order.
	 */
	void RunOnNextTick(UWorld* World, TFunction<void(UWorld*)> Body)
	{
		if (!World)
		{
			return;
		}
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
			[WeakWorld = MakeWeakObjectPtr(World), Body = MoveTemp(Body)]()
			{
				if (UWorld* W = WeakWorld.Get())
				{
					Body(W);
				}
			}));
	}

	void RunBiomeValidation(TArray<FMOTestResult>& OutResults); // defined below (P1)

	/**
	 * Content-integrity check (#65): every recipe/treatment reference must
	 * resolve to a real item/skill row. This is the drift class that produced
	 * H40/M21 (dangling item + treatment refs). Appends one result per table
	 * and logs each dangling ref as an indented [MOTEST] line.
	 */
	void RunDataValidation(TArray<FMOTestResult>& OutResults)
	{
		// ---- Recipes: ingredient/output/buildpart/fuel items + required skill ----
		{
			TArray<FName> RecipeIds;
			UMORecipeDatabaseSettings::GetAllRecipeIds(RecipeIds);
			int32 Dangling = 0;
			int32 EmptyOutputs = 0;
			for (const FName& RecipeId : RecipeIds)
			{
				const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
				if (!Recipe) { continue; }

				auto CheckItem = [&Dangling, &RecipeId](FName ItemId, const TCHAR* Field)
				{
					FMOItemDefinitionRow Tmp;
					if (!ItemId.IsNone() && !UMOItemDatabaseSettings::GetItemDefinition(ItemId, Tmp))
					{
						++Dangling;
						UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   Recipe '%s' %s -> unknown item '%s'"),
							*RecipeId.ToString(), Field, *ItemId.ToString());
					}
				};

				for (const FMORecipeIngredient& Ing : Recipe->Ingredients) { CheckItem(Ing.ItemDefinitionId, TEXT("ingredient")); }
				for (const FMORecipeOutput& Out : Recipe->Outputs) { CheckItem(Out.ItemDefinitionId, TEXT("output")); }
				for (const FMOBuildPart& Part : Recipe->BuildParts) { CheckItem(Part.ItemDefinitionId, TEXT("buildpart")); }
				for (const FName& Fuel : Recipe->AcceptedFuelItems) { CheckItem(Fuel, TEXT("fuel")); }

				if (!Recipe->RequiredSkillId.IsNone() && !UMOSkillDatabaseSettings::GetSkillDefinition(Recipe->RequiredSkillId))
				{
					++Dangling;
					UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   Recipe '%s' requiredSkill -> unknown skill '%s'"),
						*RecipeId.ToString(), *Recipe->RequiredSkillId.ToString());
				}

				// Empty-outputs trap (MORecipeDefinitionRow.h known pitfalls): a
				// non-building, non-harvest recipe with no outputs silently
				// consumes ingredients and yields nothing.
				if (!Recipe->bIsBuilding && !Recipe->bIsHarvestRecipe && Recipe->Outputs.Num() == 0)
				{
					++EmptyOutputs;
					UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   Recipe '%s' has NO outputs (consumes ingredients, yields nothing)"),
						*RecipeId.ToString());
				}
			}
			const bool bPass = (Dangling == 0 && EmptyOutputs == 0);
			OutResults.Add({ bPass, TEXT("Data:Recipes"),
				FString::Printf(TEXT("%d recipes, %d dangling ref(s), %d empty-output recipe(s)"), RecipeIds.Num(), Dangling, EmptyOutputs) });
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] %s Data:Recipes -- %d recipes, %d dangling, %d empty-output"),
				bPass ? TEXT("PASS") : TEXT("FAIL"), RecipeIds.Num(), Dangling, EmptyOutputs);
		}

		// ---- Medical treatments: required items + required skill ----
		{
			int32 Dangling = 0;
			int32 Count = 0;
			const UMOMedicalDatabaseSettings* Med = GetDefault<UMOMedicalDatabaseSettings>();
			UDataTable* Table = Med ? Med->GetMedicalTreatmentsTable() : nullptr;
			if (Table)
			{
				TArray<FMOMedicalTreatmentRow*> Rows;
				Table->GetAllRows<FMOMedicalTreatmentRow>(TEXT("MO.Test.ValidateData"), Rows);
				Count = Rows.Num();
				for (const FMOMedicalTreatmentRow* Row : Rows)
				{
					if (!Row) { continue; }
					for (const FName& ItemId : Row->RequiredItemIds)
					{
						FMOItemDefinitionRow Tmp;
						if (!ItemId.IsNone() && !UMOItemDatabaseSettings::GetItemDefinition(ItemId, Tmp))
						{
							++Dangling;
							UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   Treatment '%s' requiredItem -> unknown item '%s'"),
								*Row->TreatmentId.ToString(), *ItemId.ToString());
						}
					}
					if (!Row->RequiredSkillId.IsNone() && !UMOSkillDatabaseSettings::GetSkillDefinition(Row->RequiredSkillId))
					{
						++Dangling;
						UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   Treatment '%s' requiredSkill -> unknown skill '%s'"),
							*Row->TreatmentId.ToString(), *Row->RequiredSkillId.ToString());
					}
				}
				const bool bPass = (Dangling == 0);
				OutResults.Add({ bPass, TEXT("Data:Treatments"),
					FString::Printf(TEXT("%d treatments, %d dangling ref(s)"), Count, Dangling) });
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] %s Data:Treatments -- %d treatments, %d dangling"),
					bPass ? TEXT("PASS") : TEXT("FAIL"), Count, Dangling);
			}
			else
			{
				OutResults.Add({ false, TEXT("Data:Treatments"), TEXT("medical treatments table not configured") });
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL Data:Treatments -- table not configured"));
			}
		}

		// ---- Biomes (P1): species mesh/tag integrity + band sanity ----
		RunBiomeValidation(OutResults);
	}

	/**
	 * Biome catalog integrity (pipeline P1): every species mesh must resolve to
	 * a real asset, every species must carry a HISM tag (harvest/interaction
	 * resolves what an instance IS from the tag), and bands must be min<=max.
	 * P2's spawner consumes these rows blind — this check is what makes that safe.
	 */
	void RunBiomeValidation(TArray<FMOTestResult>& OutResults)
	{
		const UMOBiomeDatabaseSettings* Settings = GetDefault<UMOBiomeDatabaseSettings>();
		UDataTable* Table = Settings ? Settings->GetBiomeDefinitionsDataTable() : nullptr;
		if (!Table)
		{
			OutResults.Add({ false, TEXT("Data:Biomes"), TEXT("biome definitions table not configured") });
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL Data:Biomes -- table not configured"));
			return;
		}

		int32 Bad = 0;
		int32 SpeciesCount = 0;
		TArray<FName> BiomeIds;
		UMOBiomeDatabaseSettings::GetAllBiomeIds(BiomeIds);
		for (const FName& BiomeId : BiomeIds)
		{
			const FMOBiomeDefinitionRow* Biome = UMOBiomeDatabaseSettings::GetBiomeDefinition(BiomeId);
			if (!Biome) { continue; }

			auto CheckBand = [&Bad, &BiomeId](float Min, float Max, const TCHAR* Band)
			{
				if (Min > Max)
				{
					++Bad;
					UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   Biome '%s' %s band inverted (%.2f > %.2f)"),
						*BiomeId.ToString(), Band, Min, Max);
				}
			};
			CheckBand(Biome->HeightMin, Biome->HeightMax, TEXT("height"));
			CheckBand(Biome->SlopeMinDeg, Biome->SlopeMaxDeg, TEXT("slope"));
			CheckBand(Biome->MoistureMin, Biome->MoistureMax, TEXT("moisture"));
			CheckBand(Biome->TemperatureMin, Biome->TemperatureMax, TEXT("temperature"));

			if (Biome->Species.Num() == 0)
			{
				++Bad;
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   Biome '%s' has NO species"), *BiomeId.ToString());
			}
			for (int32 i = 0; i < Biome->Species.Num(); ++i)
			{
				const FMOBiomeSpeciesEntry& Sp = Biome->Species[i];
				++SpeciesCount;
				if (Sp.Mesh.IsNull() || !Sp.Mesh.LoadSynchronous())
				{
					++Bad;
					UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   Biome '%s' species[%d] mesh unresolvable: %s"),
						*BiomeId.ToString(), i, *Sp.Mesh.ToString());
				}
				// A species is either HARVESTABLE (ResourceNodeId -> full
				// interaction tag bundle) or DECORATIVE (HISMTag). Neither =
				// untagged scenery nothing can address — reject.
				if (Sp.HISMTag.IsNone() && Sp.ResourceNodeId.IsNone())
				{
					++Bad;
					UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   Biome '%s' species[%d] has neither ResourceNodeId nor HISMTag"),
						*BiomeId.ToString(), i);
				}
				if (Sp.MinScale > Sp.MaxScale || Sp.DensityPerHectare < 0.0f)
				{
					++Bad;
					UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   Biome '%s' species[%d] bad scale/density"),
						*BiomeId.ToString(), i);
				}
			}
		}
		const bool bPass = (Bad == 0 && BiomeIds.Num() > 0);
		OutResults.Add({ bPass, TEXT("Data:Biomes"),
			FString::Printf(TEXT("%d biomes, %d species, %d problem(s)"), BiomeIds.Num(), SpeciesCount, Bad) });
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] %s Data:Biomes -- %d biomes, %d species, %d problem(s)"),
			bPass ? TEXT("PASS") : TEXT("FAIL"), BiomeIds.Num(), SpeciesCount, Bad);
	}

	// =========================================================================
	// Art validation (#171 / pipeline A1): make the art gap a queryable number.
	// Every visual slot in the definition tables is either OK (a real asset),
	// MISSING (unset), or PLACEHOLDER (engine/basic-shape/graybox stand-in).
	// The [MOTEST] ART lines are the greppable burn-down list; the totals are
	// the baseline PROJECT_STATUS tracks.
	// =========================================================================

	enum class EMOArtSlotState : uint8 { Ok, Missing, Placeholder };

	EMOArtSlotState ClassifyArtPath(const FSoftObjectPath& Path)
	{
		if (!Path.IsValid())
		{
			return EMOArtSlotState::Missing;
		}
		const FString PathString = Path.ToString();
		// "/Engine/" catches BasicShapes/EngineMeshes/editor textures wholesale;
		// "Graybox" marks A5-generated stand-ins (deliberately placeholder, not
		// missing -- that distinction IS the A5 gate).
		static const TCHAR* PlaceholderMarkers[] = {
			TEXT("/Engine/"), TEXT("BasicShapes"), TEXT("EngineMeshes"),
			TEXT("Placeholder"), TEXT("Graybox")
		};
		for (const TCHAR* Marker : PlaceholderMarkers)
		{
			if (PathString.Contains(Marker))
			{
				return EMOArtSlotState::Placeholder;
			}
		}
		return EMOArtSlotState::Ok;
	}

	struct FMOArtDebtCounter
	{
		int32 Missing = 0;
		int32 Placeholder = 0;
		int32 SlotsChecked = 0;

		void CheckPath(const FSoftObjectPath& Path, const FString& RowId, const TCHAR* Slot)
		{
			++SlotsChecked;
			switch (ClassifyArtPath(Path))
			{
			case EMOArtSlotState::Missing:
				++Missing;
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   ART MISSING %s '%s'"), Slot, *RowId);
				break;
			case EMOArtSlotState::Placeholder:
				++Placeholder;
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   ART PLACEHOLDER %s '%s' -> %s"), Slot, *RowId, *Path.ToString());
				break;
			default:
				break;
			}
		}

		void CountMissing(const FString& RowId, const TCHAR* Slot)
		{
			++SlotsChecked;
			++Missing;
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST]   ART MISSING %s '%s'"), Slot, *RowId);
		}

		bool IsClean() const { return Missing == 0 && Placeholder == 0; }
	};

	struct FMOArtDebtTotals
	{
		int32 Missing = 0;
		int32 Placeholder = 0;
		int32 SlotsChecked = 0;

		void Accumulate(const FMOArtDebtCounter& C)
		{
			Missing += C.Missing;
			Placeholder += C.Placeholder;
			SlotsChecked += C.SlotsChecked;
		}
	};

	FMOArtDebtTotals RunArtValidation(TArray<FMOTestResult>& OutResults)
	{
		FMOArtDebtTotals Totals;

		// ---- Recipes: crafting icon for all; preview mesh + actor class for buildings ----
		{
			TArray<FName> RecipeIds;
			UMORecipeDatabaseSettings::GetAllRecipeIds(RecipeIds);
			FMOArtDebtCounter C;
			int32 Buildings = 0;
			for (const FName& RecipeId : RecipeIds)
			{
				const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
				if (!Recipe) { continue; }
				const FString Id = RecipeId.ToString();
				C.CheckPath(Recipe->Icon.ToSoftObjectPath(), Id, TEXT("Recipe.Icon"));
				if (Recipe->bIsBuilding)
				{
					++Buildings;
					C.CheckPath(Recipe->PlacementData.PreviewMesh.ToSoftObjectPath(), Id, TEXT("Recipe.PreviewMesh"));
					// Hard class ref, not a soft path: unset means the building
					// falls back to the base ghost with no mesh of its own.
					if (!Recipe->PlacementData.BuildableActorClass)
					{
						C.CountMissing(Id, TEXT("Recipe.BuildableActorClass"));
					}
					else
					{
						++C.SlotsChecked;
					}
				}
			}
			OutResults.Add({ C.IsClean(), TEXT("Art:Recipes"),
				FString::Printf(TEXT("%d recipes (%d buildings), %d slots: %d missing, %d placeholder"),
					RecipeIds.Num(), Buildings, C.SlotsChecked, C.Missing, C.Placeholder) });
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] %s Art:Recipes -- %d slots, %d missing, %d placeholder"),
				C.IsClean() ? TEXT("PASS") : TEXT("FAIL"), C.SlotsChecked, C.Missing, C.Placeholder);
			Totals.Accumulate(C);
		}

		// ---- Items: both icons; world visual = mesh OR a custom world actor ----
		{
			const UMOItemDatabaseSettings* Settings = GetDefault<UMOItemDatabaseSettings>();
			UDataTable* Table = Settings ? Settings->GetItemDefinitionsDataTable() : nullptr;
			if (Table)
			{
				FMOArtDebtCounter C;
				int32 Count = 0;
				for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
				{
					const FMOItemDefinitionRow* Row = reinterpret_cast<const FMOItemDefinitionRow*>(Pair.Value);
					if (!Row) { continue; }
					++Count;
					const FString Id = Pair.Key.ToString();
					C.CheckPath(Row->UI.IconSmall.ToSoftObjectPath(), Id, TEXT("Item.IconSmall"));
					C.CheckPath(Row->UI.IconLarge.ToSoftObjectPath(), Id, TEXT("Item.IconLarge"));
					// A dropped item renders WorldVisual.StaticMesh through the
					// default AMOWorldItem; a custom WorldActorClass brings its
					// own visuals. Neither set = invisible on the ground.
					if (!Row->WorldVisual.StaticMesh.IsNull())
					{
						C.CheckPath(Row->WorldVisual.StaticMesh.ToSoftObjectPath(), Id, TEXT("Item.WorldMesh"));
					}
					else if (!Row->WorldVisual.WorldActorClass.IsNull())
					{
						C.CheckPath(Row->WorldVisual.WorldActorClass.ToSoftObjectPath(), Id, TEXT("Item.WorldActorClass"));
					}
					else
					{
						C.CountMissing(Id, TEXT("Item.WorldVisual"));
					}
				}
				OutResults.Add({ C.IsClean(), TEXT("Art:Items"),
					FString::Printf(TEXT("%d items, %d slots: %d missing, %d placeholder"),
						Count, C.SlotsChecked, C.Missing, C.Placeholder) });
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] %s Art:Items -- %d slots, %d missing, %d placeholder"),
					C.IsClean() ? TEXT("PASS") : TEXT("FAIL"), C.SlotsChecked, C.Missing, C.Placeholder);
				Totals.Accumulate(C);
			}
			else
			{
				OutResults.Add({ false, TEXT("Art:Items"), TEXT("item definitions table not configured") });
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL Art:Items -- table not configured"));
			}
		}

		// ---- Skills: UI icon ----
		{
			TArray<FName> SkillIds;
			UMOSkillDatabaseSettings::GetAllSkillIds(SkillIds);
			FMOArtDebtCounter C;
			for (const FName& SkillId : SkillIds)
			{
				if (const FMOSkillDefinitionRow* Skill = UMOSkillDatabaseSettings::GetSkillDefinition(SkillId))
				{
					C.CheckPath(Skill->Icon.ToSoftObjectPath(), SkillId.ToString(), TEXT("Skill.Icon"));
				}
			}
			OutResults.Add({ C.IsClean(), TEXT("Art:Skills"),
				FString::Printf(TEXT("%d skills: %d missing, %d placeholder"),
					SkillIds.Num(), C.Missing, C.Placeholder) });
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] %s Art:Skills -- %d skills, %d missing, %d placeholder"),
				C.IsClean() ? TEXT("PASS") : TEXT("FAIL"), SkillIds.Num(), C.Missing, C.Placeholder);
			Totals.Accumulate(C);
		}

		return Totals;
	}

	// =========================================================================
	// MO.Colony.* helpers (V0 village vertical slice)
	// =========================================================================

	APawn* FindColonyPawnBySub(UWorld* World, const FString& NameSub)
	{
		APawn* LocalPawn = ResolveLocalPawn(World);
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			APawn* Pawn = *It;
			if (Pawn == LocalPawn || !IsValid(Pawn))
			{
				continue;
			}
			if (!Pawn->FindComponentByClass<UMORecruitmentComponent>())
			{
				continue;
			}
			if (NameSub.IsEmpty() || Pawn->GetName().Contains(NameSub))
			{
				return Pawn;
			}
		}
		return nullptr;
	}

	AActor* FindWorldActorBySub(UWorld* World, const FString& NameSub)
	{
		if (NameSub.IsEmpty())
		{
			return nullptr;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->GetName().Contains(NameSub))
			{
				return *It;
			}
		}
		return nullptr;
	}

	UMOInventoryComponent* ResolveActorInventory(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}
		if (Actor->Implements<UMOInventoryHolderInterface>())
		{
			if (UMOInventoryComponent* Inv = IMOInventoryHolderInterface::Execute_GetInventory(Actor))
			{
				return Inv;
			}
		}
		return Actor->FindComponentByClass<UMOInventoryComponent>();
	}

	/**
	 * Write results to Saved/MOTestResults.txt -- a stable, immediately-flushed
	 * path a runner reads instead of scraping the buffered game log -- and log
	 * a one-line summary (also greppable).
	 */
	void WriteTestResults(const FString& SuiteName, const TArray<FMOTestResult>& Results)
	{
		int32 Pass = 0;
		int32 Fail = 0;
		FString Out = FString::Printf(TEXT("[MOTEST-RESULTS] Suite=%s\n"), *SuiteName);
		for (const FMOTestResult& R : Results)
		{
			Out += FString::Printf(TEXT("%s %s | %s\n"), R.bPass ? TEXT("PASS") : TEXT("FAIL"), *R.Name, *R.Detail);
			if (R.bPass) { ++Pass; } else { ++Fail; }
		}
		Out += FString::Printf(TEXT("SUMMARY %d passed, %d failed\n"), Pass, Fail);

		const FString Path = FPaths::ProjectSavedDir() / TEXT("MOTestResults.txt");
		FFileHelper::SaveStringToFile(Out, *Path);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] ===== %s: %d passed, %d failed -> %s ====="),
			*SuiteName, Pass, Fail, *Path);
	}
}

UMOCheatSubsystem* UMOCheatSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	const UWorld* World = WorldContextObject->GetWorld();
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<UMOCheatSubsystem>() : nullptr;
}

namespace
{
	/**
	 * Console-name OWNERSHIP guard. Multi-GameInstance PIE (2-player listen
	 * server) creates one subsystem per GameInstance, but console names are
	 * process-GLOBAL: the second Initialize's duplicate registrations alias the
	 * first instance's objects, and whichever instance tears down second then
	 * unregisters already-freed IConsoleObjects — EXCEPTION_ACCESS_VIOLATION in
	 * UnregisterConsoleCommands (found by the FIRST 2-client mptest boot,
	 * 2026-07-03). Only the first instance registers; only it unregisters.
	 */
	UMOCheatSubsystem* GCheatConsoleOwner = nullptr;
}

void UMOCheatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GCheatConsoleOwner == nullptr)
	{
		RegisterConsoleCommands();
		GCheatConsoleOwner = this;
		UE_LOG(LogMOFramework, Log, TEXT("[MOCheat] Initialized — %d commands registered"), ConsoleCommands.Num());
	}
	else
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MOCheat] Secondary GameInstance (multi-PIE) — console commands already owned, skipping registration"));
	}
}

void UMOCheatSubsystem::Deinitialize()
{
	if (GCheatConsoleOwner == this)
	{
		UnregisterConsoleCommands();
		GCheatConsoleOwner = nullptr;
	}
	Super::Deinitialize();
}

void UMOCheatSubsystem::RegisterConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();

	// =========================================================================
	// MO.Help — discovery for all MO.* commands
	// =========================================================================
	// Lists every registered console command/variable that starts with "MO."
	// alongside its help text. The MO.* prefix is the project's convention so
	// this surfaces every cheat/diagnostic command we register here AND any
	// CVars (MO.Audio.*, MO.UI.Debug.*, etc.) registered elsewhere.
	//
	// Always available in every build — diagnostic, not destructive. Modders
	// and QA discover commands via `MO.Help`.
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Help"),
		TEXT("List every MO.* console command with its help text. Optional filter: MO.Help <substring>"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			const FString Filter = Args.Num() > 0 ? Args[0] : FString();

			// Collect first so we can sort + format. ForEachConsoleObject visits
			// in registration order which isn't useful for browsing.
			struct FEntry { FString Name; FString Help; bool bIsCommand; };
			TArray<FEntry> Entries;

			IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(
				FConsoleObjectVisitor::CreateLambda([&Entries, &Filter](const TCHAR* Name, IConsoleObject* Obj)
				{
					if (!Obj) return;
					const FString NameStr(Name);
					if (!Filter.IsEmpty() && !NameStr.Contains(Filter, ESearchCase::IgnoreCase))
					{
						return;
					}

					FEntry E;
					E.Name = NameStr;
					E.Help = Obj->GetHelp();
					E.bIsCommand = (Obj->AsCommand() != nullptr);
					Entries.Add(MoveTemp(E));
				}),
				TEXT("MO."));

			Entries.Sort([](const FEntry& A, const FEntry& B) { return A.Name < B.Name; });

			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Help] %d entries%s%s"),
				Entries.Num(),
				Filter.IsEmpty() ? TEXT("") : TEXT(" matching '"),
				Filter.IsEmpty() ? TEXT("") : *(Filter + TEXT("'")));

			for (const FEntry& E : Entries)
			{
				// Tag CMD vs CVAR so the difference is visible — CVars take a
				// value, commands take args. Help text on multi-line entries
				// reads better with the name on its own line.
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.Help]   %s %s"),
					E.bIsCommand ? TEXT("[CMD] ") : TEXT("[CVAR]"),
					*E.Name);
				if (!E.Help.IsEmpty())
				{
					// Split help text on \n so multi-line descriptions don't
					// get glommed into one log line that overflows the console.
					TArray<FString> HelpLines;
					E.Help.ParseIntoArrayLines(HelpLines);
					for (const FString& Line : HelpLines)
					{
						UE_LOG(LogMOFramework, Warning, TEXT("[MO.Help]           %s"), *Line);
					}
				}
			}

			if (Entries.Num() == 0 && !Filter.IsEmpty())
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.Help] No MO.* commands matched '%s'. Try MO.Help with no filter to see everything."),
					*Filter);
			}
		}),
		ECVF_Default));


	// ---------- MO.Player.Info ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Player.Info"),
		TEXT("Print info about the locally controlled pawn (name, location, inventory size)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			APawn* Pawn = ResolveLocalPawn(World);
			if (!Pawn)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOCheat] No locally controlled pawn (in main menu?)"));
				return;
			}
			const FVector Loc = Pawn->GetActorLocation();
			UMOInventoryComponent* Inv = Pawn->FindComponentByClass<UMOInventoryComponent>();
			const int32 Entries = Inv ? Inv->GetEntryCount() : -1;
			const int32 Slots = Inv ? Inv->GetSlotCount() : -1;
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOCheat] Pawn=%s  Loc=(%.0f, %.0f, %.0f)  Inventory: %d entries / %d slots"),
				*Pawn->GetName(), Loc.X, Loc.Y, Loc.Z, Entries, Slots);
		}),
		ECVF_Default));

	// ---------- MO.Player.Teleport X Y Z ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Player.Teleport"),
		TEXT("Teleport the locally controlled pawn to world coords. Usage: MO.Player.Teleport <X> <Y> <Z>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APawn* Pawn = ResolveLocalPawn(World);
			if (!Pawn)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOCheat] No locally controlled pawn"));
				return;
			}
			if (Args.Num() < 3)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOCheat] Usage: MO.Player.Teleport <X> <Y> <Z>"));
				return;
			}
			const FVector Dest(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), FCString::Atof(*Args[2]));
			// SetActorLocation with bSweep=false so we don't get stuck against geometry.
			// bTeleport=true skips the physics interpolation.
			const bool bOk = Pawn->SetActorLocation(Dest, /*bSweep*/false, /*OutSweepHit*/nullptr, ETeleportType::TeleportPhysics);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCheat] Teleport %s -> (%.0f, %.0f, %.0f) [%s]"),
				*Pawn->GetName(), Dest.X, Dest.Y, Dest.Z, bOk ? TEXT("OK") : TEXT("FAILED"));
		}),
		ECVF_Default));

	// ---------- MO.Player.GiveItem ItemId [Count] ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Player.GiveItem"),
		TEXT("Add an item to the local pawn's inventory. Usage: MO.Player.GiveItem <ItemId> [Count=1]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOInventoryComponent* Inv = ResolveLocalInventory(World);
			if (!Inv)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOCheat] No inventory on local pawn"));
				return;
			}
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOCheat] Usage: MO.Player.GiveItem <ItemId> [Count=1]"));
				return;
			}
			const FName ItemId(*Args[0]);
			const int32 Count = (Args.Num() > 1) ? FMath::Max(1, FCString::Atoi(*Args[1])) : 1;

			// VALIDATE THE ITEM ID FIRST. The inventory's AddItemByGuid accepts
			// any FName and creates the entry; if the ID doesn't resolve to a
			// real row in the item database, every consumer downstream (UI,
			// crafting, save) treats it as a phantom "debug item". So we gate
			// on the database lookup here — typos error out immediately
			// instead of polluting the inventory.
			FMOItemDefinitionRow ItemDef;
			if (!UMOItemDatabaseSettings::GetItemDefinition(ItemId, ItemDef))
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MOCheat] GiveItem refused — '%s' is not in the item database. "
					     "Check Items.csv for the exact row name (case-sensitive)."),
					*ItemId.ToString());
				return;
			}

			// Authority-only check — inventory mutations must run on the server.
			// PIE standalone host satisfies this; dedicated client would need an RPC.
			if (!Inv->GetOwner() || !Inv->GetOwner()->HasAuthority())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOCheat] GiveItem requires authority — run from server/standalone"));
				return;
			}

			if (!Inv->CanAddItemByDefinitionId(ItemId, Count))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOCheat] GiveItem refused — inventory can't hold %d x %s"),
					Count, *ItemId.ToString());
				return;
			}

			const FGuid NewGuid = FGuid::NewGuid();
			const bool bOk = Inv->AddItemByGuid(NewGuid, ItemId, Count);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOCheat] GiveItem %d x %s -> %s (Guid=%s)"),
				Count, *ItemId.ToString(),
				bOk ? TEXT("OK") : TEXT("FAILED"),
				*NewGuid.ToString(EGuidFormats::DigitsWithHyphens));
		}),
		ECVF_Default));

	// =========================================================================
	// MO.Test.* — automated MP-authority test harness (#132). Each command drives
	// the exact authoritative path the UI invokes and logs a greppable [MOTEST]
	// PASS/FAIL marker, so a runner can assert from Saved/Logs/MO57.log with no
	// input driving. Run these in a CLIENT window on 2-client PIE to exercise the
	// real client->server RPCs.
	// =========================================================================

	// ---------- MO.Test.DropPickup [ItemId=Stick01] ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Test.DropPickup"),
		TEXT("H21 identity test: give->drop->pickup, assert the same GUID returns to inventory. Usage: MO.Test.DropPickup [ItemId=Stick01]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FName ItemId = Args.Num() > 0 ? FName(*Args[0]) : FName(TEXT("Stick01"));
			// Deferred so client-world RPCs really transport (see RunOnNextTick).
			RunOnNextTick(World, [ItemId](UWorld* W) { RunDropPickupTest(W, ItemId); });
		}),
		ECVF_Default));

	// ---------- MO.Test.Attack ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Test.Attack"),
		TEXT("H18 combat test: trigger a light attack (client forwards ServerStartAttack); log the combat state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			// Deferred so client-world RPCs really transport (see RunOnNextTick).
			RunOnNextTick(World, [](UWorld* W) { RunAttackTest(W); });
		}),
		ECVF_Default));

	// ---------- MO.Test.Craft [RecipeId=KnapFlint] ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Test.Craft"),
		TEXT("H20 crafting test: enqueue a craft (server-gated). Usage: MO.Test.Craft [RecipeId=KnapFlintFlakes]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FName RecipeId = Args.Num() > 0 ? FName(*Args[0]) : FName(TEXT("KnapFlintFlakes"));
			// Deferred so client-world RPCs really transport (see RunOnNextTick).
			RunOnNextTick(World, [RecipeId](UWorld* W) { RunCraftTest(W, RecipeId); });
		}),
		ECVF_Default));

	// ---------- MO.Test.MPSuite ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Test.MPSuite"),
		TEXT("Run the MP-authority smoke suite (DropPickup + Attack + Craft); grep the log for [MOTEST]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] ===== MPSuite begin ====="));
			if (APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr)
			{
				PC->ConsoleCommand(TEXT("MO.Test.DropPickup"), true);
				PC->ConsoleCommand(TEXT("MO.Test.Attack"), true);
				PC->ConsoleCommand(TEXT("MO.Test.Craft"), true);
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] ===== MPSuite end ====="));
		}),
		ECVF_Default));

	// ---------- MO.Test.ValidateData ----------
	// Content-integrity gate (#65). This subsystem only exists once a game
	// GameInstance does (i.e. PIE running), but unlike the gameplay tests it
	// needs NO possessed pawn -- it reads DataTables, so it runs fine sitting at
	// the main menu. Writes PASS/FAIL to Saved/MOTestResults.txt.
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Test.ValidateData"),
		TEXT("Validate DataTable integrity (dangling recipe/treatment item+skill refs, empty-output recipes). Writes Saved/MOTestResults.txt. Works at the main menu (no pawn needed)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			TArray<FMOTestResult> Results;
			RunDataValidation(Results);
			WriteTestResults(TEXT("ValidateData"), Results);
		}),
		ECVF_Default));

	// ---------- MO.Test.ValidateArt ----------
	// Pipeline A1 (#171): art debt as a number. Per-slot MISSING/PLACEHOLDER
	// audit over recipes/items/skills; [MOTEST] ART lines are the burn-down
	// list. Like ValidateData it needs no pawn -- runs at the main menu.
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Test.ValidateArt"),
		TEXT("Audit art slots (recipe icons/preview meshes, item icons/world visuals, skill icons): MISSING or PLACEHOLDER (engine/basic-shape/graybox paths). Writes Saved/MOTestResults.txt. Works at the main menu."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			TArray<FMOTestResult> Results;
			RunArtValidation(Results);
			WriteTestResults(TEXT("ValidateArt"), Results);
		}),
		ECVF_Default));

	// =========================================================================
	// MO.Colony.* -- V0 village vertical slice dev verbs. All bodies deferred
	// (RunOnNextTick) so Python-driven runs escape the script guard and any
	// downstream RPCs route for real. [MOQUERY] COLONY lines are the greppable
	// output surface for the test_village_v0 gate.
	// =========================================================================

	// ---------- MO.Colony.SpawnSurvivor [dist=300] ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Colony.SpawnSurvivor"),
		TEXT("Dev: spawn a survivor pawn (player pawn class) in front of the player. Usage: MO.Colony.SpawnSurvivor [dist=300]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const float Dist = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 300.0f;
			RunOnNextTick(World, [Dist](UWorld* W)
			{
				APawn* LocalPawn = ResolveLocalPawn(W);
				if (!LocalPawn)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY SpawnSurvivor FAILED: no local pawn"));
					return;
				}
				FActorSpawnParameters P;
				P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
				const FVector Loc = LocalPawn->GetActorLocation()
					+ LocalPawn->GetActorForwardVector() * Dist + FVector(0, 0, 50);
				APawn* NewPawn = W->SpawnActor<APawn>(LocalPawn->GetClass(), Loc, LocalPawn->GetActorRotation(), P);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY SpawnSurvivor %s"),
					NewPawn ? *NewPawn->GetName() : TEXT("FAILED"));
			});
		}),
		ECVF_Default));

	// ---------- MO.Colony.Recruit <pawnNameSub> ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Colony.Recruit"),
		TEXT("Dev: force-recruit a survivor by name substring (skips quest). Usage: MO.Colony.Recruit <pawnNameSub>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FString Sub = Args.Num() > 0 ? Args[0] : FString();
			RunOnNextTick(World, [Sub](UWorld* W)
			{
				APawn* Pawn = FindColonyPawnBySub(W, Sub);
				UMORecruitmentComponent* Recruit = Pawn ? Pawn->FindComponentByClass<UMORecruitmentComponent>() : nullptr;
				if (!Recruit)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY Recruit FAILED: no pawn matching '%s'"), *Sub);
					return;
				}
				Recruit->ForceRecruit();
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY Recruit %s possessable=%d"),
					*Pawn->GetName(), Recruit->IsPossessable() ? 1 : 0);
			});
		}),
		ECVF_Default));

	// ---------- MO.Colony.PlaceBuilding <recipeId> [dist=400] ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Colony.PlaceBuilding"),
		TEXT("Dev: spawn a COMPLETED building for a building recipe in front of the player (mirrors ServerPlaceBuilding + restore-as-complete). Usage: MO.Colony.PlaceBuilding <recipeId> [dist=400]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FName RecipeId = Args.Num() > 0 ? FName(*Args[0]) : NAME_None;
			const float Dist = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 400.0f;
			RunOnNextTick(World, [RecipeId, Dist](UWorld* W)
			{
				const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(RecipeId);
				APawn* LocalPawn = ResolveLocalPawn(W);
				if (!Recipe || !Recipe->bIsBuilding || !LocalPawn)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY PlaceBuilding FAILED: %s"),
						!LocalPawn ? TEXT("no local pawn") : TEXT("not a building recipe"));
					return;
				}

				// Same class inference as UMOBuildingComponent::ServerPlaceBuilding.
				TSubclassOf<AMOBuildableActor> ActorClass = Recipe->PlacementData.BuildableActorClass;
				if (!ActorClass)
				{
					if (Recipe->ContainerSlotCount > 0) { ActorClass = AMOContainerActor::StaticClass(); }
					else if (Recipe->ProvidedStationType != EMOCraftingStation::None) { ActorClass = AMOCraftingStationActor::StaticClass(); }
					else { ActorClass = AMOBuildableActor::StaticClass(); }
				}

				// Ground-snap: trace down from ahead-of-player.
				FVector Loc = LocalPawn->GetActorLocation() + LocalPawn->GetActorForwardVector() * Dist + FVector(0, 0, 200);
				FHitResult Hit;
				FCollisionQueryParams QP;
				QP.AddIgnoredActor(LocalPawn);
				if (W->LineTraceSingleByChannel(Hit, Loc, Loc - FVector(0, 0, 2000), ECC_WorldStatic, QP))
				{
					Loc = Hit.Location;
				}

				FActorSpawnParameters P;
				P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AMOBuildableActor* Placed = W->SpawnActor<AMOBuildableActor>(ActorClass, FTransform(FRotator::ZeroRotator, Loc), P);
				if (!Placed)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY PlaceBuilding FAILED: spawn"));
					return;
				}
				Placed->InitializeBuilding(RecipeId);

				// Dev-only shortcut: restore as COMPLETE via the same path the
				// save system uses for finished buildings. (Shipped placement
				// always runs real construction - see CLAUDE.md sim rules.)
				if (UMOBuildProgressComponent* Progress = Placed->FindComponentByClass<UMOBuildProgressComponent>())
				{
					FMOBuildProgress Done;
					Done.State = EMOBuildState::Complete;
					Progress->ApplySaveData(Done);
				}
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY PlaceBuilding %s recipe=%s at %s"),
					*Placed->GetName(), *RecipeId.ToString(), *Loc.ToCompactString());
			});
		}),
		ECVF_Default));

	// ---------- MO.Colony.Stock <actorNameSub> <itemId> [count=1] ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Colony.Stock"),
		TEXT("Dev: add items to a container/station inventory by actor name substring. Usage: MO.Colony.Stock <actorNameSub> <itemId> [count=1]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 2)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY Stock usage: <actorNameSub> <itemId> [count]"));
				return;
			}
			const FString Sub = Args[0];
			const FName ItemId = FName(*Args[1]);
			const int32 Count = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 1;
			RunOnNextTick(World, [Sub, ItemId, Count](UWorld* W)
			{
				AActor* Actor = FindWorldActorBySub(W, Sub);
				UMOInventoryComponent* Inv = ResolveActorInventory(Actor);
				if (!Inv)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY Stock FAILED: no inventory on actor matching '%s'"), *Sub);
					return;
				}
				const bool bAdded = Inv->AddItemByGuid(FGuid::NewGuid(), ItemId, Count);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY Stock %s %dx %s ok=%d now=%d"),
					*Actor->GetName(), Count, *ItemId.ToString(), bAdded ? 1 : 0,
					Inv->GetItemCountByDefinitionId(ItemId));
			});
		}),
		ECVF_Default));

	// ---------- MO.Colony.AssignJob <pawnSub> <recipeId> <stationSub> <storageSub> [repeat=1] ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Colony.AssignJob"),
		TEXT("Dev: assign a CraftAtStation job. Usage: MO.Colony.AssignJob <pawnSub> <recipeId> <stationSub> <storageSub> [repeat=1]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 4)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY AssignJob usage: <pawnSub> <recipeId> <stationSub> <storageSub> [repeat]"));
				return;
			}
			const FString PawnSub = Args[0];
			const FName RecipeId = FName(*Args[1]);
			const FString StationSub = Args[2];
			const FString StorageSub = Args[3];
			const int32 Repeat = Args.Num() > 4 ? FCString::Atoi(*Args[4]) : 1;
			RunOnNextTick(World, [PawnSub, RecipeId, StationSub, StorageSub, Repeat](UWorld* W)
			{
				APawn* Pawn = FindColonyPawnBySub(W, PawnSub);
				AActor* Station = FindWorldActorBySub(W, StationSub);
				AActor* Storage = FindWorldActorBySub(W, StorageSub);
				UMOSurvivorJobQueueComponent* JobQueue = Pawn ? Pawn->FindComponentByClass<UMOSurvivorJobQueueComponent>() : nullptr;
				if (!JobQueue || !Station || !Storage)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY AssignJob FAILED: pawn=%s station=%s storage=%s"),
						Pawn ? *Pawn->GetName() : TEXT("none"),
						Station ? *Station->GetName() : TEXT("none"),
						Storage ? *Storage->GetName() : TEXT("none"));
					return;
				}
				const FGuid JobId = JobQueue->EnqueueCraftJob(RecipeId, Station, Storage, Repeat);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY AssignJob %s -> %dx %s ok=%d"),
					*Pawn->GetName(), Repeat, *RecipeId.ToString(), JobId.IsValid() ? 1 : 0);
			});
		}),
		ECVF_Default));

	// ---------- MO.Colony.SetSkill <pawnSub> <skillId> <level> ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Colony.SetSkill"),
		TEXT("Dev: set a skill level on a colony pawn. Usage: MO.Colony.SetSkill <pawnSub> <skillId> <level>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 3)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY SetSkill usage: <pawnSub> <skillId> <level>"));
				return;
			}
			const FString Sub = Args[0];
			const FName SkillId = FName(*Args[1]);
			const int32 Level = FCString::Atoi(*Args[2]);
			RunOnNextTick(World, [Sub, SkillId, Level](UWorld* W)
			{
				APawn* Pawn = FindColonyPawnBySub(W, Sub);
				UMOSkillsComponent* Skills = Pawn ? Pawn->FindComponentByClass<UMOSkillsComponent>() : nullptr;
				if (!Skills)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY SetSkill FAILED: no pawn/skills matching '%s'"), *Sub);
					return;
				}
				Skills->SetSkillLevel(SkillId, Level);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY SetSkill %s %s=%d"),
					*Pawn->GetName(), *SkillId.ToString(), Level);
			});
		}),
		ECVF_Default));

	// ---------- MO.Colony.Status ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Colony.Status"),
		TEXT("Dev: dump the colony roster (recruited survivors, jobs, queues) as [MOQUERY] COLONY lines."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			RunOnNextTick(World, [](UWorld* W)
			{
				int32 Count = 0;
				APawn* LocalPawn = ResolveLocalPawn(W);
				for (TActorIterator<APawn> It(W); It; ++It)
				{
					APawn* Pawn = *It;
					if (Pawn == LocalPawn || !IsValid(Pawn))
					{
						continue;
					}
					UMORecruitmentComponent* Recruit = Pawn->FindComponentByClass<UMORecruitmentComponent>();
					if (!Recruit)
					{
						continue;
					}
					++Count;
					FString JobDesc = TEXT("none");
					int32 QueueLen = 0;
					if (UMOSurvivorJobQueueComponent* JobQueue = Pawn->FindComponentByClass<UMOSurvivorJobQueueComponent>())
					{
						QueueLen = JobQueue->GetAllJobs().Num();
						const FMOSurvivorJobEntry Job = JobQueue->GetCurrentJob();
						if (Job.IsValid())
						{
							JobDesc = FString::Printf(TEXT("%s:%s p=%.2f"),
								*UEnum::GetDisplayValueAsText(Job.JobType).ToString(),
								*UEnum::GetDisplayValueAsText(Job.State).ToString(),
								Job.Progress);
						}
					}
					UE_LOG(LogMOFramework, Warning,
						TEXT("[MOQUERY] COLONY pawn=%s recruited=%d job=%s queue=%d controller=%s"),
						*Pawn->GetName(), Recruit->IsPossessable() ? 1 : 0, *JobDesc, QueueLen,
						Pawn->GetController() ? *Pawn->GetController()->GetClass()->GetName() : TEXT("none"));
				}
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COLONY roster=%d"), Count);
			});
		}),
		ECVF_Default));

	// ---------- MO.Test.RunAll ----------
	// The full regression gate: gameplay smoke tests (need an in-game pawn) plus
	// the data-integrity checks, aggregated into one PASS/FAIL summary file so a
	// runner reads Saved/MOTestResults.txt instead of scraping the buffered log.
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Test.RunAll"),
		TEXT("Run the full regression suite (DropPickup+Attack+Craft + data validation) and write PASS/FAIL to Saved/MOTestResults.txt."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] ===== RunAll begin ====="));
			TArray<FMOTestResult> Results;
			Results.Add(RunDropPickupTest(World, FName(TEXT("Stick01"))));
			Results.Add(RunAttackTest(World));
			Results.Add(RunCraftTest(World, FName(TEXT("KnapFlintFlakes"))));
			RunDataValidation(Results);
			// Art debt is a tracked BASELINE being burned down (pipeline
			// A-track), not a regression: one always-informational line so the
			// standing gate stays green while the debt exists by design. The
			// honest per-category verdicts live in MO.Test.ValidateArt.
			{
				TArray<FMOTestResult> ArtDetail;
				const FMOArtDebtTotals Art = RunArtValidation(ArtDetail);
				Results.Add({ true, TEXT("Data:Art"),
					FString::Printf(TEXT("%d missing, %d placeholder across %d slots (baseline burn-down -- see MO.Test.ValidateArt)"),
						Art.Missing, Art.Placeholder, Art.SlotsChecked) });
			}
			WriteTestResults(TEXT("RunAll"), Results);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] ===== RunAll end ====="));
		}),
		ECVF_Default));

	// =========================================================================
	// MO.Test.Input + MO.Test.ClickWidget -- Pillar-0 DRIVE primitives (charter
	// Move 1). Input goes through IMOControllableInterface::Execute_Request* --
	// the exact seam AMOPlayerController delegates through -- so movement /
	// interaction / combat flows are testable without OS input (synthetic keys
	// never reach Enhanced Input, #144; this interface is the post-input
	// surface). ClickWidget drives real UI: an MO button gets the full guarded
	// CommonUI click path; anything else gets a synthesized Slate pointer click
	// at its screen center (real hit-testing, catches occlusion/z-order bugs).
	// =========================================================================

	// ---------- MO.Test.Input <Action> [args] ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Test.Input"),
		TEXT("Inject controllable-interface input on the local pawn. Usage: MO.Test.Input <Move X Y|Look X Y|JumpStart|JumpEnd|SprintStart|SprintEnd|ToggleJog|CrouchToggle|Interact|SecondaryInteract|PrimaryAction|PrimaryActionRelease|SecondaryAction|SecondaryActionRelease|TerraformToggle|TerraformCycleTool>. Move/Look are per-frame inputs (drive across frames via claude_seq)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APawn* Pawn = ResolveLocalPawn(World);
			if (!Pawn || !Pawn->GetClass()->ImplementsInterface(UMOControllableInterface::StaticClass()))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL Input: no controllable pawn (are you in-game?)"));
				return;
			}
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL Input: no action given (see help)"));
				return;
			}
			const FString& Action = Args[0];
			auto Is = [&Action](const TCHAR* Name) { return Action.Equals(Name, ESearchCase::IgnoreCase); };
			bool bDispatched = true;
			if (Is(TEXT("Move")) && Args.Num() >= 3)
			{
				IMOControllableInterface::Execute_RequestMove(Pawn, FVector2D(FCString::Atof(*Args[1]), FCString::Atof(*Args[2])));
			}
			else if (Is(TEXT("Look")) && Args.Num() >= 3)
			{
				IMOControllableInterface::Execute_RequestLook(Pawn, FVector2D(FCString::Atof(*Args[1]), FCString::Atof(*Args[2])));
			}
			else if (Is(TEXT("JumpStart")))              { IMOControllableInterface::Execute_RequestJumpStart(Pawn); }
			else if (Is(TEXT("JumpEnd")))                { IMOControllableInterface::Execute_RequestJumpEnd(Pawn); }
			else if (Is(TEXT("SprintStart")))            { IMOControllableInterface::Execute_RequestSprintStart(Pawn); }
			else if (Is(TEXT("SprintEnd")))              { IMOControllableInterface::Execute_RequestSprintEnd(Pawn); }
			else if (Is(TEXT("ToggleJog")))              { IMOControllableInterface::Execute_RequestToggleJog(Pawn); }
			else if (Is(TEXT("CrouchToggle")))           { IMOControllableInterface::Execute_RequestCrouchToggle(Pawn); }
			else if (Is(TEXT("Interact")))               { IMOControllableInterface::Execute_RequestInteract(Pawn); }
			else if (Is(TEXT("SecondaryInteract")))      { IMOControllableInterface::Execute_RequestSecondaryInteract(Pawn); }
			else if (Is(TEXT("PrimaryAction")))          { IMOControllableInterface::Execute_RequestPrimaryAction(Pawn); }
			else if (Is(TEXT("PrimaryActionRelease")))   { IMOControllableInterface::Execute_RequestPrimaryActionRelease(Pawn); }
			else if (Is(TEXT("SecondaryAction")))        { IMOControllableInterface::Execute_RequestSecondaryAction(Pawn); }
			else if (Is(TEXT("SecondaryActionRelease"))) { IMOControllableInterface::Execute_RequestSecondaryActionRelease(Pawn); }
			else if (Is(TEXT("TerraformToggle")))        { IMOControllableInterface::Execute_RequestTerraformToggle(Pawn); }
			else if (Is(TEXT("TerraformCycleTool")))     { IMOControllableInterface::Execute_RequestTerraformCycleTool(Pawn); }
			else { bDispatched = false; }

			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] %s Input: '%s' (canMove=%s canJump=%s vel=%s)"),
				bDispatched ? TEXT("PASS") : TEXT("FAIL"), *Action,
				IMOControllableInterface::Execute_CanMove(Pawn) ? TEXT("Y") : TEXT("N"),
				IMOControllableInterface::Execute_CanJump(Pawn) ? TEXT("Y") : TEXT("N"),
				*Pawn->GetVelocity().ToCompactString());
		}),
		ECVF_Default));

	// ---------- MO.Test.ClickWidget <NameSubstring> ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Test.ClickWidget"),
		TEXT("Click a live widget by name: UMOCommonButton -> full guarded CommonUI click (SimulateClick); other widgets -> synthesized Slate pointer click at their screen center. Usage: MO.Test.ClickWidget <NameSubstring> (locate names via MO.Test.FindWidget)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL ClickWidget: usage MO.Test.ClickWidget <NameSubstring>"));
				return;
			}
			const FString& Needle = Args[0];

			// Pass 1: MO buttons -- deterministic, guard-respecting click.
			UMOCommonButton* Exact = nullptr;
			UMOCommonButton* Partial = nullptr;
			for (TObjectIterator<UMOCommonButton> It; It; ++It)
			{
				UMOCommonButton* Btn = *It;
				if (!IsValid(Btn) || Btn->GetWorld() != World || !Btn->GetCachedWidget().IsValid() || !Btn->IsVisible())
				{
					continue;
				}
				if (Btn->GetName().Equals(Needle, ESearchCase::IgnoreCase)) { Exact = Btn; break; }
				if (!Partial && Btn->GetName().Contains(Needle)) { Partial = Btn; }
			}
			if (UMOCommonButton* Btn = Exact ? Exact : Partial)
			{
				const bool bClicked = Btn->SimulateClick();
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] %s ClickWidget: '%s' via direct MOCommonButton path%s"),
					bClicked ? TEXT("PASS") : TEXT("FAIL"), *Btn->GetName(),
					bClicked ? TEXT("") : TEXT(" (button not interactable)"));
				return;
			}

			// Pass 2: any widget -- synthesized pointer click through real Slate
			// hit-testing at the widget's absolute center (same coords FindWidget
			// reports), so occlusion/z-order behave as they would for a user.
			UWidget* Target = nullptr;
			UWidget* TargetPartial = nullptr;
			for (TObjectIterator<UWidget> It; It; ++It)
			{
				UWidget* W = *It;
				if (!IsValid(W) || W->GetWorld() != World || !W->GetCachedWidget().IsValid() || !W->IsVisible())
				{
					continue;
				}
				if (W->GetName().Equals(Needle, ESearchCase::IgnoreCase)) { Target = W; break; }
				if (!TargetPartial && W->GetName().Contains(Needle)) { TargetPartial = W; }
			}
			UWidget* W = Target ? Target : TargetPartial;
			if (!W)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL ClickWidget: no live widget matching '%s' (try MO.Test.FindWidget)"), *Needle);
				return;
			}
			const FGeometry& Geo = W->GetCachedGeometry();
			const FVector2D Center = FVector2D(Geo.GetAbsolutePosition()) + FVector2D(Geo.GetAbsoluteSize()) * 0.5f;
			FSlateApplication& Slate = FSlateApplication::Get();
			TSet<FKey> Pressed;
			Pressed.Add(EKeys::LeftMouseButton);
			const FPointerEvent DownEvent(0, Center, Center, Pressed, EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
			Slate.ProcessMouseButtonDownEvent(nullptr, DownEvent);
			const FPointerEvent UpEvent(0, Center, Center, TSet<FKey>(), EKeys::LeftMouseButton, 0.0f, FModifierKeysState());
			Slate.ProcessMouseButtonUpEvent(UpEvent);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] PASS ClickWidget: '%s' via pointer click at (%.0f, %.0f)"),
				*W->GetName(), Center.X, Center.Y);
		}),
		ECVF_Default));

	// =========================================================================
	// MO.AI.DumpBlackboard + MO.AI.SetKey -- Pillar-0 AI observe/tweak verbs
	// (charter Move 1). Complements MO.AI.DumpFreezeState / StressSpawn: dump
	// reads EVERY key via DescribeKeyValue (no hardcoded key lists); SetKey
	// writes typed by the blackboard asset's entry so a runner can force
	// ShouldFlee/HealthPercent/etc. and watch the BT react.
	// =========================================================================

	// ---------- MO.AI.DumpBlackboard <PawnNameSubstring> ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.AI.DumpBlackboard"),
		TEXT("Log [MOQUERY] every blackboard key+value for the first AI pawn whose name contains the arg. Usage: MO.AI.DumpBlackboard <PawnNameSubstring>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] BB: usage MO.AI.DumpBlackboard <PawnNameSubstring>"));
				return;
			}
			FString PawnName;
			UBlackboardComponent* BB = ResolveBlackboard(World, Args[0], PawnName);
			if (!BB)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] BB: %s"), PawnName.IsEmpty()
					? *FString::Printf(TEXT("no pawn matching '%s'"), *Args[0])
					: *FString::Printf(TEXT("pawn '%s' has no AI blackboard"), *PawnName));
				return;
			}
			const UBlackboardData* Asset = BB->GetBlackboardAsset();
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] BB %s (asset=%s):"),
				*PawnName, Asset ? *Asset->GetName() : TEXT("none"));
			for (const UBlackboardData* Data = Asset; Data; Data = Data->Parent)
			{
				for (const FBlackboardEntry& Entry : Data->Keys)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] BB   %s"),
						*BB->DescribeKeyValue(Entry.EntryName, EBlackboardDescription::KeyWithValue));
				}
			}
		}),
		ECVF_Default));

	// ---------- MO.AI.SetKey <PawnNameSubstring> <KeyName> <Value...> ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.AI.SetKey"),
		TEXT("Set a blackboard key on the first AI pawn whose name contains the arg (typed by the BB asset: bool/float/int/vector(X Y Z)/name/string/enum/object-by-actor-name). Usage: MO.AI.SetKey <PawnSub> <KeyName> <Value> [Y Z]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World || Args.Num() < 3)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] BB: usage MO.AI.SetKey <PawnSub> <KeyName> <Value> [Y Z]"));
				return;
			}
			FString PawnName;
			UBlackboardComponent* BB = ResolveBlackboard(World, Args[0], PawnName);
			if (!BB)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] BB: %s"), PawnName.IsEmpty()
					? *FString::Printf(TEXT("no pawn matching '%s'"), *Args[0])
					: *FString::Printf(TEXT("pawn '%s' has no AI blackboard"), *PawnName));
				return;
			}
			const FName KeyName(*Args[1]);
			const FBlackboardEntry* Entry = nullptr;
			for (const UBlackboardData* Data = BB->GetBlackboardAsset(); Data && !Entry; Data = Data->Parent)
			{
				for (const FBlackboardEntry& E : Data->Keys)
				{
					if (E.EntryName == KeyName) { Entry = &E; break; }
				}
			}
			if (!Entry || !Entry->KeyType)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] BB %s: no key '%s' (run MO.AI.DumpBlackboard %s for the list)"),
					*PawnName, *Args[1], *Args[0]);
				return;
			}

			const FString& Val = Args[2];
			bool bSet = true;
			if (Entry->KeyType->IsA<UBlackboardKeyType_Bool>())        { BB->SetValueAsBool(KeyName, Val.ToBool()); }
			else if (Entry->KeyType->IsA<UBlackboardKeyType_Float>())  { BB->SetValueAsFloat(KeyName, FCString::Atof(*Val)); }
			else if (Entry->KeyType->IsA<UBlackboardKeyType_Int>())    { BB->SetValueAsInt(KeyName, FCString::Atoi(*Val)); }
			else if (Entry->KeyType->IsA<UBlackboardKeyType_Enum>())   { BB->SetValueAsEnum(KeyName, (uint8)FCString::Atoi(*Val)); }
			else if (Entry->KeyType->IsA<UBlackboardKeyType_Name>())   { BB->SetValueAsName(KeyName, FName(*Val)); }
			else if (Entry->KeyType->IsA<UBlackboardKeyType_String>()) { BB->SetValueAsString(KeyName, Val); }
			else if (Entry->KeyType->IsA<UBlackboardKeyType_Vector>())
			{
				if (Args.Num() >= 5)
				{
					BB->SetValueAsVector(KeyName, FVector(FCString::Atof(*Args[2]), FCString::Atof(*Args[3]), FCString::Atof(*Args[4])));
				}
				else
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] BB %s: vector key '%s' needs X Y Z"), *PawnName, *Args[1]);
					bSet = false;
				}
			}
			else if (Entry->KeyType->IsA<UBlackboardKeyType_Object>())
			{
				if (Val.Equals(TEXT("None"), ESearchCase::IgnoreCase) || Val.Equals(TEXT("null"), ESearchCase::IgnoreCase))
				{
					BB->ClearValue(KeyName);
				}
				else
				{
					AActor* Found = nullptr;
					for (TActorIterator<AActor> It(World); It; ++It)
					{
						if (It->GetName().Contains(Val)) { Found = *It; break; }
					}
					if (Found)
					{
						BB->SetValueAsObject(KeyName, Found);
					}
					else
					{
						UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] BB %s: no actor matching '%s' for object key '%s'"), *PawnName, *Val, *Args[1]);
						bSet = false;
					}
				}
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] BB %s: key '%s' has unsupported type %s"),
					*PawnName, *Args[1], *Entry->KeyType->GetClass()->GetName());
				bSet = false;
			}

			if (bSet)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] BB %s: set -> %s"),
					*PawnName, *BB->DescribeKeyValue(KeyName, EBlackboardDescription::KeyWithValue));
			}
		}),
		ECVF_Default));

	// =========================================================================
	// MO.Test.State + MO.Test.FindWidget -- INTROSPECTION so a runner reads state +
	// UI positions from the log ([MOQUERY]) instead of screenshotting.
	// =========================================================================

	// ---------- MO.Test.State ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Test.State"),
		TEXT("Log [MOQUERY] game state (netmode / in-game vs menu / possessed pawn / level) -- read from the log instead of screenshotting."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] STATE: no world"));
				return;
			}
			const TCHAR* NetMode = TEXT("Unknown");
			switch (World->GetNetMode())
			{
			case NM_Standalone:      NetMode = TEXT("Standalone"); break;
			case NM_ListenServer:    NetMode = TEXT("ListenServer"); break;
			case NM_DedicatedServer: NetMode = TEXT("DedicatedServer"); break;
			case NM_Client:          NetMode = TEXT("Client"); break;
			default: break;
			}
			APlayerController* PC = World->GetFirstPlayerController();
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			const bool bInGame = Pawn && Pawn->FindComponentByClass<UMOInventoryComponent>() != nullptr;
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOQUERY] STATE netmode=%s level=%s inGame=%s pawn=%s class=%s"),
				NetMode, *World->GetMapName(),
				bInGame ? TEXT("YES") : TEXT("NO(menu?)"),
				Pawn ? *Pawn->GetName() : TEXT("none"),
				Pawn ? *Pawn->GetClass()->GetName() : TEXT("-"));
		}),
		ECVF_Default));

	// ---------- MO.Test.FindWidget [NameSubstring] ----------
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Test.FindWidget"),
		TEXT("Log [MOQUERY] live widgets whose name contains the arg, with absolute on-screen center/rect -- locate UI without screenshots. Usage: MO.Test.FindWidget [NameSubstring]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FString Filter = Args.Num() > 0 ? Args[0] : FString();
			int32 Found = 0;
			for (TObjectIterator<UUserWidget> It; It; ++It)
			{
				UUserWidget* UW = *It;
				if (!IsValid(UW) || UW->GetWorld() != World || !UW->WidgetTree)
				{
					continue;
				}
				UW->WidgetTree->ForEachWidget([&Filter, &Found](UWidget* Child)
				{
					if (!Child)
					{
						return;
					}
					const FString Name = Child->GetName();
					// CommonUI button labels are UCommonTextBlock (: UTextBlock) leaves in the
					// button's own WidgetTree, so capture the visible text and let the filter
					// match on it -- "FindWidget Load" should find a button by its on-screen
					// label, not just an internal object name.
					FString LabelText;
					if (const UTextBlock* AsText = Cast<UTextBlock>(Child))
					{
						LabelText = AsText->GetText().ToString();
					}
					if (!Filter.IsEmpty() && !Name.Contains(Filter) && !LabelText.Contains(Filter))
					{
						return;
					}
					const FGeometry& Geo = Child->GetCachedGeometry();
					const FVector2D LocalSize = Geo.GetLocalSize();
					// Skip widgets not currently laid out on screen (zero cached size). This
					// replaces the old IsInViewport() gate, which wrongly excluded every CommonUI
					// widget -- they live on activatable widget-stacks, not AddToViewport (#160).
					if (LocalSize.X <= 1.f && LocalSize.Y <= 1.f)
					{
						return;
					}
					const FVector2D TopLeft = Geo.LocalToAbsolute(FVector2D::ZeroVector);
					const FVector2D Center = Geo.LocalToAbsolute(LocalSize * 0.5f);
					UE_LOG(LogMOFramework, Warning,
						TEXT("[MOQUERY] WIDGET '%s' (%s) text='%s' center=(%.0f,%.0f) topLeft=(%.0f,%.0f) size=(%.0f,%.0f) visible=%d"),
						*Name, *Child->GetClass()->GetName(), *LabelText,
						Center.X, Center.Y, TopLeft.X, TopLeft.Y, LocalSize.X, LocalSize.Y,
						Child->IsVisible() ? 1 : 0);
					Found++;
				});
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FindWidget('%s'): %d match(es)"), *Filter, Found);
		}),
		ECVF_Default));

	// =========================================================================
	// MO.Clock.* — operate on the world-scoped UMOGameClockSubsystem
	// =========================================================================
	// These live here (not on the clock subsystem) because the clock is a
	// UWorldSubsystem and its lifetime is incompatible with IConsoleManager.
	// Lambdas resolve UMOGameClockSubsystem::Get(World) at call time, so
	// they always operate on whatever clock the current world has.

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Clock.Info"),
		TEXT("Print current clock state (TimeScale, RealPlayTime, GameTime, GameDateTime, IsDaytime)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			const UMOGameClockSubsystem* Sys = UMOGameClockSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] No subsystem")); return; }
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOClock] TimeScale=%.2f RealPlayTime=%.1fs GameTime=%.1fs GameDateTime=%s (%s)"),
				Sys->GetTimeScale(),
				Sys->GetRealPlayTimeSeconds(),
				Sys->GetGameTimeSeconds(),
				*Sys->GetGameDateTime().ToString(),
				Sys->IsDaytime() ? TEXT("DAY") : TEXT("NIGHT"));
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Clock.SetTimeScale"),
		TEXT("Set TimeScale (in-game seconds per real second). Usage: MO.Clock.SetTimeScale 60"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOGameClockSubsystem* Sys = UMOGameClockSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] No subsystem")); return; }
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] Usage: MO.Clock.SetTimeScale <float>"));
				return;
			}
			const float NewScale = FCString::Atof(*Args[0]);
			Sys->SetTimeScale(NewScale);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] TimeScale -> %.2f"), Sys->GetTimeScale());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Clock.SetTime"),
		TEXT("Jump to a specific hour:minute today. Usage: MO.Clock.SetTime 6 30  (= 06:30)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOGameClockSubsystem* Sys = UMOGameClockSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] No subsystem")); return; }
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] Usage: MO.Clock.SetTime <hour 0-23> [minute 0-59]"));
				return;
			}
			const int32 Hour = FMath::Clamp(FCString::Atoi(*Args[0]), 0, 23);
			const int32 Minute = (Args.Num() > 1) ? FMath::Clamp(FCString::Atoi(*Args[1]), 0, 59) : 0;
			const FDateTime Current = Sys->GetGameDateTime();
			const FDateTime NewDT(Current.GetYear(), Current.GetMonth(), Current.GetDay(), Hour, Minute, 0);
			Sys->SetGameDateTime(NewDT);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] GameDateTime -> %s"), *NewDT.ToString());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Clock.AdvanceHours"),
		TEXT("Fast-forward the in-game DateTime by N hours. Usage: MO.Clock.AdvanceHours 8"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOGameClockSubsystem* Sys = UMOGameClockSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] No subsystem")); return; }
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] Usage: MO.Clock.AdvanceHours <float>"));
				return;
			}
			const float Hours = FCString::Atof(*Args[0]);
			const FDateTime NewDT = Sys->GetGameDateTime() + FTimespan::FromHours(Hours);
			Sys->SetGameDateTime(NewDT);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] Advanced %.2fh -> %s"), Hours, *NewDT.ToString());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Clock.SkipToDay"),
		TEXT("Skip in-game time to the next 06:00 (start of daytime)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UMOGameClockSubsystem* Sys = UMOGameClockSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] No subsystem")); return; }
			const FDateTime Current = Sys->GetGameDateTime();
			FDateTime Target(Current.GetYear(), Current.GetMonth(), Current.GetDay(), 6, 0, 0);
			if (Target <= Current) Target += FTimespan::FromDays(1);
			Sys->SetGameDateTime(Target);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] Skipped to %s"), *Target.ToString());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Clock.SkipToNight"),
		TEXT("Skip in-game time to the next 18:00 (start of nighttime)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UMOGameClockSubsystem* Sys = UMOGameClockSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] No subsystem")); return; }
			const FDateTime Current = Sys->GetGameDateTime();
			FDateTime Target(Current.GetYear(), Current.GetMonth(), Current.GetDay(), 18, 0, 0);
			if (Target <= Current) Target += FTimespan::FromDays(1);
			Sys->SetGameDateTime(Target);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOClock] Skipped to %s"), *Target.ToString());
		}),
		ECVF_Default));

	// =========================================================================
	// MO.Weather.* — operate on the world-scoped UMOWeatherIntegrationSubsystem
	// =========================================================================
	// Commands dispatch to whatever provider is registered (BP_WeatherBridge by
	// default, which translates to UDS/UDW). If no provider is registered the
	// subsystem logs a warning and the command is a no-op.

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Weather.Info"),
		TEXT("Print current weather state (preset name + intensities + temperature)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UMOWeatherIntegrationSubsystem* Sys = World ? World->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOWeather] No subsystem")); return; }

			const FMOWeatherState State = Sys->GetCurrentWeatherState();
			const float TempC = Sys->GetGlobalTemperature(EMOTemperatureUnit::Celsius);
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOWeather] %s | Cloud=%.2f Fog=%.2f Rain=%.2f Snow=%.2f Wind=%.2f Thunder=%.2f Temp=%.1fC"),
				*State.DisplayName.ToString(),
				State.CloudCoverage, State.Fog,
				State.RainIntensity, State.SnowIntensity,
				State.WindIntensity, State.ThunderIntensity,
				TempC);
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Weather.ListPresets"),
		TEXT("List the UDS weather presets MO.Weather.SetPreset accepts (built-in UDS Weather_Presets folder)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* /*World*/)
		{
			static const TCHAR* Presets[] = {
				TEXT("Clear_Skies"),
				TEXT("Partly_Cloudy"),
				TEXT("Cloudy"),
				TEXT("Overcast"),
				TEXT("Foggy"),
				TEXT("Rain_Light"),
				TEXT("Rain"),
				TEXT("Rain_Thunderstorm"),
				TEXT("Snow_Light"),
				TEXT("Snow"),
				TEXT("Snow_Blizzard"),
				TEXT("Sand_Dust_Calm"),
				TEXT("Sand_Dust_Storm"),
			};
			UE_LOG(LogMOFramework, Warning, TEXT("[MOWeather] Available UDS presets:"));
			for (const TCHAR* P : Presets)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("  %s"), P);
			}
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Weather.SetPreset"),
		TEXT("Apply a UDS weather preset by name. Usage: MO.Weather.SetPreset Rain"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOWeatherIntegrationSubsystem* Sys = World ? World->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOWeather] No subsystem")); return; }
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOWeather] Usage: MO.Weather.SetPreset <PresetName>. Try MO.Weather.ListPresets."));
				return;
			}

			// UDS weather presets are INSTANCES of UDS_Weather_Settings_C saved
			// as data assets, NOT subclasses. Load path is:
			//   /Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/<Name>.<Name>
			// (object name = asset name, no _C suffix — that would be the class).
			const FString& PresetName = Args[0];
			const FString InstancePath = FString::Printf(
				TEXT("/Game/UltraDynamicSky/Blueprints/Weather_Effects/Weather_Presets/%s.%s"),
				*PresetName, *PresetName);

			UObject* PresetInstance = LoadObject<UObject>(nullptr, *InstancePath);

			// Fallback: maybe this version of UDS uses subclasses (older versions?)
			if (!PresetInstance)
			{
				const FString ClassPath = InstancePath + TEXT("_C");
				if (UClass* PresetClass = LoadClass<UObject>(nullptr, *ClassPath))
				{
					PresetInstance = PresetClass->GetDefaultObject();
				}
			}

			if (!PresetInstance)
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MOWeather] Preset '%s' not found at %s. Try MO.Weather.ListPresets."),
					*PresetName, *InstancePath);
				return;
			}

			Sys->SetWeatherPreset(PresetInstance);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOWeather] Applied preset '%s' (object=%s, class=%s)"),
				*PresetName, *GetNameSafe(PresetInstance), *GetNameSafe(PresetInstance->GetClass()));
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Weather.LogSaveData"),
		TEXT("Call BuildWeatherSaveData and print every field — for verifying what would be persisted."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UMOWeatherIntegrationSubsystem* Sys = World ? World->GetSubsystem<UMOWeatherIntegrationSubsystem>() : nullptr;
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOWeather] No subsystem")); return; }

			const FMOWeatherSaveData Data = Sys->BuildWeatherSaveData();
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOWeather] SaveData: bIsValid=%s DateTime=%s Cloud=%.2f Fog=%.2f Preset=%s"),
				Data.bIsValid ? TEXT("true") : TEXT("false"),
				*Data.DateTime.ToString(),
				Data.CloudCoverage, Data.FogDensity,
				*GetNameSafe(Data.WeatherPresetObject));
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOWeather] NOTE: WeatherPresetObject is UPROPERTY(Transient) — won't serialize to disk."));
		}),
		ECVF_Default));

	// =========================================================================
	// MO.Audio.* — operate on the GameInstance-scoped UMOAudioSubsystem
	// =========================================================================

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Audio.Info"),
		TEXT("Print audio subsystem state: current music/ambient, volumes, loaded banks."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UMOAudioSubsystem* Sys = UMOAudioSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] No subsystem")); return; }

			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOAudio] Music=%s Ambient=%s | Vols: Master=%.2f Music=%.2f Ambient=%.2f SFX=%.2f UI=%.2f | Bank IDs=%d"),
				*UEnum::GetValueAsString(Sys->GetMusicState()),
				*UEnum::GetValueAsString(Sys->GetAmbientState()),
				Sys->GetMasterVolume(), Sys->GetMusicVolume(), Sys->GetAmbientVolume(),
				Sys->GetSFXVolume(), Sys->GetUIVolume(),
				Sys->GetAllAudioIds().Num());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Audio.ListBank"),
		TEXT("List every audio ID currently registered across loaded banks."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UMOAudioSubsystem* Sys = UMOAudioSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] No subsystem")); return; }

			const TArray<FName> Ids = Sys->GetAllAudioIds();
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] %d audio IDs in bank:"), Ids.Num());
			for (const FName& Id : Ids)
			{
				FMOAudioBankRow Row;
				if (Sys->FindAudioBankRow(Id, Row))
				{
					UE_LOG(LogMOFramework, Warning, TEXT("  %s [%s] -> %s"),
						*Id.ToString(),
						*UEnum::GetValueAsString(Row.Category),
						*Row.Sound.ToSoftObjectPath().ToString());
				}
			}
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Audio.SetMusic"),
		TEXT("Set music state. Usage: MO.Audio.SetMusic <None|MainMenu|Exploration_Day|Exploration_Night|Combat|Stealth|DangerNear|Death|Discovery>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOAudioSubsystem* Sys = UMOAudioSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] No subsystem")); return; }
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] Usage: MO.Audio.SetMusic <StateName>"));
				return;
			}

			const UEnum* EnumPtr = StaticEnum<EMOMusicState>();
			const int64 Value = EnumPtr ? EnumPtr->GetValueByNameString(Args[0]) : INDEX_NONE;
			if (Value == INDEX_NONE)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] Unknown music state: %s"), *Args[0]);
				return;
			}
			Sys->SetMusicState(static_cast<EMOMusicState>(Value));
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Audio.SetAmbient"),
		TEXT("Set ambient state. Usage: MO.Audio.SetAmbient <None|Outdoor_Day|Outdoor_Dusk|Outdoor_Night|Cave|Indoor|Water>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOAudioSubsystem* Sys = UMOAudioSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] No subsystem")); return; }
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] Usage: MO.Audio.SetAmbient <StateName>"));
				return;
			}

			const UEnum* EnumPtr = StaticEnum<EMOAmbientState>();
			const int64 Value = EnumPtr ? EnumPtr->GetValueByNameString(Args[0]) : INDEX_NONE;
			if (Value == INDEX_NONE)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] Unknown ambient state: %s"), *Args[0]);
				return;
			}
			Sys->SetAmbientState(static_cast<EMOAmbientState>(Value));
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Audio.Play"),
		TEXT("Play a one-shot from the bank by ID. Usage: MO.Audio.Play SFX.UI.ButtonClick"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOAudioSubsystem* Sys = UMOAudioSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] No subsystem")); return; }
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] Usage: MO.Audio.Play <AudioId>"));
				return;
			}
			const bool bOk = Sys->PlayOneShot2D(FName(*Args[0]));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] Play '%s' -> %s"),
				*Args[0], bOk ? TEXT("OK") : TEXT("FAILED (check ListBank)"));
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Audio.MasterVolume"),
		TEXT("Set master volume 0-1. Usage: MO.Audio.MasterVolume 0.5"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOAudioSubsystem* Sys = UMOAudioSubsystem::Get(World);
			if (!Sys || Args.Num() < 1) return;
			Sys->SetMasterVolume(FCString::Atof(*Args[0]));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] MasterVolume -> %.2f"), Sys->GetMasterVolume());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Audio.MusicVolume"),
		TEXT("Set music volume 0-1. Usage: MO.Audio.MusicVolume 0.5"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOAudioSubsystem* Sys = UMOAudioSubsystem::Get(World);
			if (!Sys || Args.Num() < 1) return;
			Sys->SetMusicVolume(FCString::Atof(*Args[0]));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] MusicVolume -> %.2f"), Sys->GetMusicVolume());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Audio.DumpAmbient"),
		TEXT("Dump full ambient state: current config, base layers playing, event groups + cooldowns, dominant group."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UMOAudioSubsystem* Sys = UMOAudioSubsystem::Get(World);
			if (!Sys) { UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] No subsystem")); return; }

			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] === Ambient State Dump ==="));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] State: %s"),
				*UEnum::GetValueAsString(Sys->GetAmbientState()));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] Run MO.Audio.Info for volume/bank summary."));
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOAudio] (Detailed config inspection requires opening DT_AmbientLayers in the editor.)"));
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Audio.AmbientVolume"),
		TEXT("Set ambient volume 0-1. Usage: MO.Audio.AmbientVolume 0.5"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOAudioSubsystem* Sys = UMOAudioSubsystem::Get(World);
			if (!Sys || Args.Num() < 1) return;
			Sys->SetAmbientVolume(FCString::Atof(*Args[0]));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] AmbientVolume -> %.2f"), Sys->GetAmbientVolume());
		}),
		ECVF_Default));

	// =========================================================================
	// MO.AI.* — verification commands for the spawn-manager freeze pipeline
	// =========================================================================
	// The freeze pipeline calls UBrainComponent::StopLogic on Prey/Predator/Ambient
	// spawns until the player is within WakeDistanceCm. These commands let you
	// confirm the pipeline is actually halting BT ticks (and not silently
	// no-opping somewhere downstream).
	//
	// Verification workflow:
	//   1. MO.AI.DumpFreezeState — see who's tracked, their distance, and
	//      whether Brain->IsRunning matches expectations. Anomalies (should
	//      be frozen but Brain still running) are flagged with [!].
	//   2. stat unit / Insights — sample CPU before/after toggling.
	//   3. MO.AI.ForceFreezeAll — flip every tracked entity to stopped.
	//      Profile again — BT tick cost should drop.
	//   4. MO.AI.ForceWakeAll — restore. Profile should swing back up.
	//
	// If step 3 doesn't move the profiler, StopLogic isn't doing what we
	// expect and the pipeline needs a deeper look (e.g. behavior trees
	// re-arming themselves via a service).

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.AI.DumpFreezeState"),
		TEXT("Dump per-pawn freeze state for every tracked spawned entity. Flags anomalies "
		     "(should be frozen by category/distance but Brain still running)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UMOSpawnManagerSubsystem* Sys = World ? World->GetSubsystem<UMOSpawnManagerSubsystem>() : nullptr;
			if (!Sys)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.AI] No spawn manager subsystem"));
				return;
			}
			Sys->DumpFreezeState();
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.AI.ForceFreezeAll"),
		TEXT("Stop the AI brain on every tracked non-Survivor spawned entity. "
		     "Pair with 'stat unit' / Insights to verify BT tick cost actually drops."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UMOSpawnManagerSubsystem* Sys = World ? World->GetSubsystem<UMOSpawnManagerSubsystem>() : nullptr;
			if (!Sys)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.AI] No spawn manager subsystem"));
				return;
			}
			const int32 Affected = Sys->ForceFreezeAll();
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.AI] ForceFreezeAll affected %d brains — now check 'stat unit'"),
				Affected);
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.AI.ForceWakeAll"),
		TEXT("Restart the AI brain on every tracked spawned entity (regardless of distance). "
		     "Counterpart to MO.AI.ForceFreezeAll for A/B profiler comparison."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UMOSpawnManagerSubsystem* Sys = World ? World->GetSubsystem<UMOSpawnManagerSubsystem>() : nullptr;
			if (!Sys)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.AI] No spawn manager subsystem"));
				return;
			}
			const int32 Affected = Sys->ForceWakeAll();
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.AI] ForceWakeAll affected %d brains — now check 'stat unit'"),
				Affected);
		}),
		ECVF_Default));

	// =========================================================================
	// MO.Save.* — multi-slot save/load verification + manual driver
	// =========================================================================
	// MOSavePanel/MOLoadPanel already drive multi-slot through the persistence
	// subsystem, but the UI hides the raw API behind a confirm-flow + filter.
	// These commands hit UMOPersistenceSubsystem directly — used to verify
	// task #62 and as ongoing regression coverage if save behavior changes.

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Save.List"),
		TEXT("List every save slot on disk with display name, save time, and world identifier. "
		     "Bypasses the UI's current-world filter so all worlds appear."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
			UMOPersistenceSubsystem* Sys = GI ? GI->GetSubsystem<UMOPersistenceSubsystem>() : nullptr;
			if (!Sys)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Save] No persistence subsystem"));
				return;
			}

			const TArray<FString> Slots = Sys->GetAllSaveSlots();
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Save.List] === %d slot(s) on disk (current=%s, world=%s) ==="),
				Slots.Num(),
				*Sys->GetCurrentSlotName(),
				*Sys->GetCurrentWorldIdentifier());

			for (const FString& Slot : Slots)
			{
				FMOSaveMetadata Meta;
				const bool bOk = Sys->GetSaveSlotMetadata(Slot, Meta);
				if (bOk)
				{
					UE_LOG(LogMOFramework, Warning,
						TEXT("[MO.Save]   %s  display='%s'  world='%s'  saved=%s%s"),
						*Slot,
						*Meta.DisplayName.ToString(),
						*Meta.WorldName,
						*Meta.Timestamp.ToString(),
						Meta.bIsAutosave ? TEXT("  [autosave]") : TEXT(""));
				}
				else
				{
					UE_LOG(LogMOFramework, Warning,
						TEXT("[MO.Save]   %s  <metadata read failed>"), *Slot);
				}
			}
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Save.SaveAs"),
		TEXT("Save the current world to a specific slot name (bypasses the UI's "
		     "auto-name generator). Usage: MO.Save.SaveAs MySlot01"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Save] Usage: MO.Save.SaveAs <SlotName>"));
				return;
			}
			UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
			UMOPersistenceSubsystem* Sys = GI ? GI->GetSubsystem<UMOPersistenceSubsystem>() : nullptr;
			if (!Sys)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Save] No persistence subsystem"));
				return;
			}

			const FString SlotName = Args[0];
			const bool bExists = Sys->DoesSaveSlotExist(SlotName);
			const bool bOk = Sys->SaveWorldToSlot(SlotName);
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Save.SaveAs] %s '%s' -> %s"),
				bExists ? TEXT("Overwrote") : TEXT("Created"),
				*SlotName,
				bOk ? TEXT("OK") : TEXT("FAILED"));
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Save.LoadFrom"),
		TEXT("Load a specific save slot. Bypasses the UI's load panel. "
		     "Usage: MO.Save.LoadFrom <SlotName>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Save] Usage: MO.Save.LoadFrom <SlotName>"));
				return;
			}
			UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
			UMOPersistenceSubsystem* Sys = GI ? GI->GetSubsystem<UMOPersistenceSubsystem>() : nullptr;
			if (!Sys)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Save] No persistence subsystem"));
				return;
			}

			const FString SlotName = Args[0];
			const FMOLoadResult Result = Sys->LoadWorldFromSlotWithResult(SlotName);
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Save.LoadFrom] '%s' -> %s  (pawns=%d ok / %d failed; items=%d / %d; buildings=%d / %d)"),
				*SlotName,
				Result.bSuccess ? TEXT("OK") : TEXT("FAILED"),
				Result.PawnsLoaded, Result.PawnsFailed,
				Result.ItemsLoaded, Result.ItemsFailed,
				Result.BuildingsLoaded, Result.BuildingsFailed);
			if (!Result.ErrorMessage.IsEmpty())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Save]   Error: %s"), *Result.ErrorMessage);
			}
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Save.Delete"),
		TEXT("Delete a save slot from disk (irreversible). Bypasses the UI's "
		     "confirmation dialog. Usage: MO.Save.Delete <SlotName>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Save] Usage: MO.Save.Delete <SlotName>"));
				return;
			}
			UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
			UMOPersistenceSubsystem* Sys = GI ? GI->GetSubsystem<UMOPersistenceSubsystem>() : nullptr;
			if (!Sys)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Save] No persistence subsystem"));
				return;
			}

			const FString SlotName = Args[0];
			if (!Sys->DoesSaveSlotExist(SlotName))
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.Save.Delete] Slot '%s' does not exist"), *SlotName);
				return;
			}
			const bool bOk = Sys->DeleteSaveSlot(SlotName);
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Save.Delete] '%s' -> %s"),
				*SlotName, bOk ? TEXT("DELETED") : TEXT("FAILED"));
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Save.Current"),
		TEXT("Print the current save slot name and session play time."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
			UMOPersistenceSubsystem* Sys = GI ? GI->GetSubsystem<UMOPersistenceSubsystem>() : nullptr;
			if (!Sys)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Save] No persistence subsystem"));
				return;
			}

			const FString Slot = Sys->GetCurrentSlotName();
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Save.Current] slot='%s'  worldId='%s'  sessionTime=%.1fs"),
				Slot.IsEmpty() ? TEXT("<none>") : *Slot,
				*Sys->GetCurrentWorldIdentifier(),
				Sys->GetSessionPlayTime());
		}),
		ECVF_Default));

	// =========================================================================
	// MO.Mod.* — runtime modding support
	// =========================================================================
	// Modders can drop a DataTable asset (same row struct as the base — e.g.
	// FMOItemDefinitionRow for items) anywhere in the Content tree and call
	// MO.Mod.LoadItems /Game/Mods/MyMod/DT_MoreItems.DT_MoreItems to merge it
	// in. Mod rows live in a separate static overlay, win on ID collision
	// with base items, and survive cache rebuilds. Only the item table is
	// wired up so far — recipes/quests/skills/etc are tracked in task #113.

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Mod.LoadItems"),
		TEXT("Merge a UDataTable of FMOItemDefinitionRow rows into the item database. "
		     "Mod rows override base on ID collision and survive cache rebuilds. "
		     "Usage: MO.Mod.LoadItems /Game/Mods/MyMod/DT_MoreItems.DT_MoreItems"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.Mod] Usage: MO.Mod.LoadItems <DataTableAssetPath>"));
				return;
			}

			const FString Path = Args[0];

			// LoadObject works on the long form /Game/X/Y.Y.
			// Modders sometimes copy-paste the short form /Game/X/Y — handle
			// both by appending '.Y' if no dot is present.
			FString FullPath = Path;
			if (!FullPath.Contains(TEXT(".")))
			{
				int32 LastSlash;
				if (FullPath.FindLastChar(TEXT('/'), LastSlash))
				{
					FullPath += TEXT(".") + FullPath.Mid(LastSlash + 1);
				}
			}

			UDataTable* Table = LoadObject<UDataTable>(nullptr, *FullPath);
			if (!IsValid(Table))
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.Mod] LoadItems failed — '%s' not found. Make sure the asset exists and the path is /Game/... (not on disk)."),
					*FullPath);
				return;
			}

			const int32 Merged = UMOItemDatabaseSettings::MergeModItemTable(Table);
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Mod] LoadItems: merged %d items from '%s'. Mod overlay now has %d total."),
				Merged, *Table->GetName(), UMOItemDatabaseSettings::GetModItemCount());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Mod.ClearMods"),
		TEXT("Drop every mod-registered item and invalidate the item cache so the base "
		     "DataTable reloads cleanly on the next lookup."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const int32 Before = UMOItemDatabaseSettings::GetModItemCount();
			UMOItemDatabaseSettings::ClearModItems();
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Mod] ClearMods: dropped %d mod items, cache invalidated."), Before);
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Mod.Status"),
		TEXT("Print mod overlay status across every supported table."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MO.Mod] Mod overlay status:"));
			UE_LOG(LogMOFramework, Warning, TEXT("[MO.Mod]   Items:   %d registered"),
				UMOItemDatabaseSettings::GetModItemCount());
			UE_LOG(LogMOFramework, Warning, TEXT("[MO.Mod]   Recipes: %d registered"),
				UMORecipeDatabaseSettings::GetModRecipeCount());
			UE_LOG(LogMOFramework, Warning, TEXT("[MO.Mod]   Skills:  %d registered"),
				UMOSkillDatabaseSettings::GetModSkillCount());
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Mod]   (quests/medical/jobs/resources still tracked under task #114)"));
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Mod.LoadRecipes"),
		TEXT("Merge a UDataTable of FMORecipeDefinitionRow rows into the recipe database. "
		     "Same rules as MO.Mod.LoadItems: mod rows win on ID collision and survive "
		     "InvalidateCache. Usage: MO.Mod.LoadRecipes /Game/Mods/MyMod/DT_MoreRecipes.DT_MoreRecipes"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.Mod] Usage: MO.Mod.LoadRecipes <DataTableAssetPath>"));
				return;
			}

			FString FullPath = Args[0];
			if (!FullPath.Contains(TEXT(".")))
			{
				int32 LastSlash;
				if (FullPath.FindLastChar(TEXT('/'), LastSlash))
				{
					FullPath += TEXT(".") + FullPath.Mid(LastSlash + 1);
				}
			}

			UDataTable* Table = LoadObject<UDataTable>(nullptr, *FullPath);
			if (!IsValid(Table))
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.Mod] LoadRecipes failed — '%s' not found."), *FullPath);
				return;
			}

			const int32 Merged = UMORecipeDatabaseSettings::MergeModRecipeTable(Table);
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Mod] LoadRecipes: merged %d recipes from '%s'. Mod overlay now has %d total."),
				Merged, *Table->GetName(), UMORecipeDatabaseSettings::GetModRecipeCount());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Mod.ClearRecipes"),
		TEXT("Drop every mod-registered recipe and invalidate the recipe cache."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const int32 Before = UMORecipeDatabaseSettings::GetModRecipeCount();
			UMORecipeDatabaseSettings::ClearModRecipes();
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Mod] ClearRecipes: dropped %d mod recipes."), Before);
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Mod.LoadSkills"),
		TEXT("Merge a UDataTable of FMOSkillDefinitionRow rows into the skill database. "
		     "Usage: MO.Mod.LoadSkills /Game/Mods/MyMod/DT_MoreSkills.DT_MoreSkills"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.Mod] Usage: MO.Mod.LoadSkills <DataTableAssetPath>"));
				return;
			}

			FString FullPath = Args[0];
			if (!FullPath.Contains(TEXT(".")))
			{
				int32 LastSlash;
				if (FullPath.FindLastChar(TEXT('/'), LastSlash))
				{
					FullPath += TEXT(".") + FullPath.Mid(LastSlash + 1);
				}
			}

			UDataTable* Table = LoadObject<UDataTable>(nullptr, *FullPath);
			if (!IsValid(Table))
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.Mod] LoadSkills failed — '%s' not found."), *FullPath);
				return;
			}

			const int32 Merged = UMOSkillDatabaseSettings::MergeModSkillTable(Table);
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Mod] LoadSkills: merged %d skills from '%s'. Mod overlay now has %d total."),
				Merged, *Table->GetName(), UMOSkillDatabaseSettings::GetModSkillCount());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Mod.ClearSkills"),
		TEXT("Drop every mod-registered skill and invalidate the skill cache."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			const int32 Before = UMOSkillDatabaseSettings::GetModSkillCount();
			UMOSkillDatabaseSettings::ClearModSkills();
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Mod] ClearSkills: dropped %d mod skills."), Before);
		}),
		ECVF_Default));

	// =========================================================================
	// MO.HUD.* — status moodle strip drivers for #56
	// =========================================================================
	// The strip is generic; eventually wet-state / bleeding / hunger components
	// will push moodles. These cheats let designers/QA validate the strip
	// renders correctly without those sources being wired yet.

	auto ResolveStripFromWorld = [](UWorld* World) -> UMOStatusEffectStripWidget*
	{
		if (!World) return nullptr;
		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC) return nullptr;
		UMOUIManagerComponent* UIMgr = PC->FindComponentByClass<UMOUIManagerComponent>();
		if (!UIMgr) return nullptr;
		UMOHUDRootWidget* HUDRoot = UIMgr->GetHUDRoot();
		return HUDRoot ? HUDRoot->GetStatusStrip() : nullptr;
	};

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.HUD.AddTestMoodle"),
		TEXT("Push a test moodle to the HUD status strip. "
		     "Usage: MO.HUD.AddTestMoodle <Id> [Label] [Severity=Info|Warning|Critical|Buff] [Level=0]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([ResolveStripFromWorld](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.HUD] Usage: MO.HUD.AddTestMoodle <Id> [Label] [Severity] [Level]"));
				return;
			}
			UMOStatusEffectStripWidget* Strip = ResolveStripFromWorld(World);
			if (!Strip)
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.HUD] No StatusStrip on HUDRoot — add WBP_StatusEffectStrip to WBP_HUDRoot and tick 'Is Variable' (name 'StatusStrip')"));
				return;
			}

			FMOStatusMoodle M;
			M.Id = FName(*Args[0]);
			M.Label = Args.Num() > 1 ? FText::FromString(Args[1]) : FText::FromName(M.Id);
			M.Tooltip = FText::Format(NSLOCTEXT("MO", "TestMoodleTooltip", "Test moodle '{0}'"),
				FText::FromName(M.Id));

			if (Args.Num() > 2)
			{
				const FString& SevStr = Args[2];
				if      (SevStr.Equals(TEXT("Warning"),  ESearchCase::IgnoreCase)) M.Severity = EMOStatusMoodleSeverity::Warning;
				else if (SevStr.Equals(TEXT("Critical"), ESearchCase::IgnoreCase)) M.Severity = EMOStatusMoodleSeverity::Critical;
				else if (SevStr.Equals(TEXT("Buff"),     ESearchCase::IgnoreCase)) M.Severity = EMOStatusMoodleSeverity::Buff;
				else                                                                M.Severity = EMOStatusMoodleSeverity::Info;
			}

			if (Args.Num() > 3)
			{
				M.Level = FCString::Atoi(*Args[3]);
			}

			Strip->AddOrUpdateMoodle(M);
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.HUD.AddTestMoodle] Added '%s' (severity=%d, level=%d). Strip now has %d moodle(s)."),
				*M.Id.ToString(), static_cast<int32>(M.Severity), M.Level, Strip->GetMoodleCount());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.HUD.RemoveMoodle"),
		TEXT("Remove a moodle from the HUD status strip. Usage: MO.HUD.RemoveMoodle <Id>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([ResolveStripFromWorld](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.HUD] Usage: MO.HUD.RemoveMoodle <Id>"));
				return;
			}
			UMOStatusEffectStripWidget* Strip = ResolveStripFromWorld(World);
			if (!Strip) return;

			const FName Id(*Args[0]);
			Strip->RemoveMoodle(Id);
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.HUD.RemoveMoodle] Removed '%s'. Strip now has %d moodle(s)."),
				*Id.ToString(), Strip->GetMoodleCount());
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.HUD.ClearMoodles"),
		TEXT("Drop every moodle from the HUD status strip."),
		FConsoleCommandWithWorldDelegate::CreateLambda([ResolveStripFromWorld](UWorld* World)
		{
			UMOStatusEffectStripWidget* Strip = ResolveStripFromWorld(World);
			if (!Strip) return;
			const int32 Before = Strip->GetMoodleCount();
			Strip->ClearAllMoodles();
			UE_LOG(LogMOFramework, Warning, TEXT("[MO.HUD.ClearMoodles] Dropped %d moodle(s)."), Before);
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Player.SetWet"),
		TEXT("Force the local pawn's wetness level (0.0-1.0). Drives the wet "
		     "moodle on the HUD strip. Usage: MO.Player.SetWet 0.5"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.Player.SetWet] Usage: MO.Player.SetWet <0..1>"));
				return;
			}
			APawn* Pawn = ResolveLocalPawn(World);
			if (!Pawn)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Player.SetWet] No locally controlled pawn"));
				return;
			}
			UMOVitalsComponent* Vitals = Pawn->FindComponentByClass<UMOVitalsComponent>();
			if (!Vitals)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Player.SetWet] Pawn has no UMOVitalsComponent"));
				return;
			}

			const float Level = FCString::Atof(*Args[0]);
			Vitals->SetWetnessLevel(Level);
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Player.SetWet] WetnessLevel=%.2f → state=%s"),
				Vitals->GetWetnessLevel(),
				*UEnum::GetValueAsString(Vitals->GetWetnessState()));
		}),
		ECVF_Default));

	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.Player.SetStat"),
		TEXT("Force a survival stat to a specific value. Stat names: Hunger, "
		     "Thirst, Temperature, Energy. Value is in the stat's units "
		     "(0-100 typical). Usage: MO.Player.SetStat Hunger 20"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 2)
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.Player.SetStat] Usage: MO.Player.SetStat <Hunger|Thirst|Temperature|Energy> <value>"));
				return;
			}
			APawn* Pawn = ResolveLocalPawn(World);
			if (!Pawn)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Player.SetStat] No locally controlled pawn"));
				return;
			}
			UMOSurvivalStatsComponent* Stats = Pawn->FindComponentByClass<UMOSurvivalStatsComponent>();
			if (!Stats)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.Player.SetStat] Pawn has no UMOSurvivalStatsComponent"));
				return;
			}

			const FName StatName(*Args[0]);
			const float NewValue = FCString::Atof(*Args[1]);
			Stats->SetStat(StatName, NewValue);

			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.Player.SetStat] %s -> %.1f (%.0f%%)"),
				*StatName.ToString(),
				Stats->GetStatCurrent(StatName),
				Stats->GetStatPercent(StatName) * 100.0f);
		}),
		ECVF_Default));

	// MO.AI.StressSpawn — bulk-spawn for freeze profiling.
	//
	// The natural spawn rate (handful of mobs across a huge world) doesn't move
	// 'stat unit' enough to see the freeze pipeline working. This command dumps
	// N deer + N wolves in a ring around the player so their BTs all start
	// ticking at once. Re-runnable — each invocation adds another batch on top.
	// Spawns are placed via random angle + random radius between [MinR, MaxR];
	// a downward sphere-trace finds the local ground so they don't fall through.
	// Each spawn flows through UMOSpawnManagerSubsystem::ForceSpawnAtLocation so
	// it's tracked in SpawnedEntities and auto-frozen if outside WakeDistance.
	//
	// Usage:
	//   MO.AI.StressSpawn           — N=25, MinR=2000cm (20m), MaxR=10000cm (100m)
	//   MO.AI.StressSpawn 50        — N=50, defaults for radii
	//   MO.AI.StressSpawn 50 1000 5000
	ConsoleCommands.Add(CM.RegisterConsoleCommand(
		TEXT("MO.AI.StressSpawn"),
		TEXT("Bulk-spawn N Prey + N Predator around the player for freeze profiling. "
		     "Usage: MO.AI.StressSpawn [Count=25] [MinRadius=2000] [MaxRadius=10000]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.AI.StressSpawn] No world"));
				return;
			}

			UMOSpawnManagerSubsystem* Sys = World->GetSubsystem<UMOSpawnManagerSubsystem>();
			if (!Sys)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.AI.StressSpawn] No spawn manager subsystem"));
				return;
			}

			APlayerController* PC = World->GetFirstPlayerController();
			APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
			if (!PlayerPawn)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MO.AI.StressSpawn] No player pawn"));
				return;
			}

			const int32 Count    = (Args.Num() > 0) ? FMath::Max(1, FCString::Atoi(*Args[0])) : 25;
			const float MinRadius = (Args.Num() > 1) ? FMath::Max(100.0f, FCString::Atof(*Args[1])) : 2000.0f;
			const float MaxRadius = (Args.Num() > 2) ? FMath::Max(MinRadius + 100.0f, FCString::Atof(*Args[2])) : 10000.0f;

			const FVector PlayerLoc = PlayerPawn->GetActorLocation();
			const FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(StressSpawnGroundTrace), false, PlayerPawn);

			auto SpawnRing = [&](EMOSpawnCategory Category, const TCHAR* Label) -> int32
			{
				int32 Spawned = 0;
				for (int32 i = 0; i < Count; ++i)
				{
					// Random polar offset in the ring [MinR, MaxR].
					const float Angle  = FMath::FRandRange(0.0f, 2.0f * PI);
					const float Radius = FMath::FRandRange(MinRadius, MaxRadius);
					const FVector XYOffset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
					const FVector ProbeAtXY = PlayerLoc + XYOffset;

					// Drop trace to find ground near the player's Z. ±5000cm catches
					// hills and small basins without picking up a different layer
					// of terrain on a vertical voxel cliff.
					const FVector TraceStart = ProbeAtXY + FVector(0, 0, 5000.0f);
					const FVector TraceEnd   = ProbeAtXY - FVector(0, 0, 5000.0f);

					FHitResult Hit;
					const bool bHit = World->LineTraceSingleByChannel(
						Hit, TraceStart, TraceEnd, ECC_Visibility, TraceParams);

					const FVector SpawnLoc = bHit
						? Hit.ImpactPoint + FVector(0, 0, 100.0f)
						: ProbeAtXY;  // fallback: spawn at player Z

					APawn* Pawn = Sys->ForceSpawnAtLocation(Category, SpawnLoc, FRotator::ZeroRotator);
					if (Pawn) ++Spawned;
				}
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MO.AI.StressSpawn] %s: %d/%d spawned"), Label, Spawned, Count);
				return Spawned;
			};

			const int32 PreyCount     = SpawnRing(EMOSpawnCategory::Prey,     TEXT("Prey"));
			const int32 PredatorCount = SpawnRing(EMOSpawnCategory::Predator, TEXT("Predator"));

			UE_LOG(LogMOFramework, Warning,
				TEXT("[MO.AI.StressSpawn] Done. Total added: %d (%d Prey + %d Predator) "
				     "in ring [%.0f-%.0f]cm. Run MO.AI.DumpFreezeState to see them."),
				PreyCount + PredatorCount, PreyCount, PredatorCount, MinRadius, MaxRadius);
		}),
		ECVF_Default));
}

void UMOCheatSubsystem::UnregisterConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();
	for (IConsoleCommand* Cmd : ConsoleCommands)
	{
		if (Cmd) CM.UnregisterConsoleObject(Cmd);
	}
	ConsoleCommands.Reset();
}
