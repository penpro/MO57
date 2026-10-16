// Dev verbs for the swimming unit. Output lines start with [MOQUERY] WATER.
//
//   MO.Water.Swim                        the local pawn's water state: oracle surface, immersion, movement mode, head under, oxygen reserve, SpO2, stamina
//   MO.Water.GoToLake [rank=0] [dry]     teleport the local pawn into the rank-th nearest upland lake (centre, a metre under the surface so it floats up); `dry` = onto the bank a few metres from the shore
//   MO.Water.SetBreath <0..1>            set the local pawn's oxygen reserve (tests)
//   MO.Water.Stroke [seconds=5] [up|down] hold a swimming stroke along the camera direction (or straight up / down) for a few seconds: the held keys, for tests
//   MO.Water.Meter                       the breath meter widget's visibility, opacity and on-screen rectangle
//   MO.Water.Here                        the water underfoot / under the crosshair (kind, reach), thirst, body water, sodium, conditions
//   MO.Water.LookAtWater                 turn the camera onto the nearest water within 4 m
//   MO.Water.GoToShore [sea|fresh] [m]   stand on the bank of the nearest sea / fresh water, a metre from the edge, facing it
//   MO.Water.Drink                       drink at the crosshair exactly as E does
//   MO.Medical.Contract <Type> [now]     give the local pawn a condition (with its course); now = skip the incubation
//   MO.Medical.SkipIncubation [hours]    end every incubation now; hours = also fast-forward into the course
//   MO.Survival.Advance <minutes>        run the needs (and illness effects on them) forward
//   MO.Fire.SpawnLit                     a finished, fuelled, burning campfire in front of the pawn (to test boiling)
//   MO.Item.Actions <ItemId>             the item menu's extra actions for an item the pawn carries, and its contents
//   MO.Item.Do <ItemId> <Action>         do one through the real server RPC
//   MO.Item.Menu <ItemId>                open the item context menu for it (screenshots)
//   MO.Fire.Actions / MO.Fire.Do <Action> the nearest fire's actions (add fuel, light, put out), through the real server RPC

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "MOBreathMeterWidget.h"
#include "MOCharacter.h"
#include "MOCharacterMovementComponent.h"
#include "MOFramework.h"
#include "MORespirationComponent.h"
#include "MOSurvivalStatsComponent.h"
#include "MOVitalsComponent.h"
#include "MOWaterSubsystem.h"
#include "MOFireActions.h"
#include "EngineUtils.h"
#include "MOItemActions.h"
#include "MOInventoryComponent.h"
#include "MOUIManagerComponent.h"
#include "MOCraftingStationActor.h"
#include "MORecipeDatabaseSettings.h"
#include "MOAnatomyComponent.h"
#include "MOMetabolismComponent.h"
#include "MOPlayerController.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"
#include "UObject/UObjectIterator.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "MOInteractorComponent.h"
#include "MOResourceNodeDefinitionRow.h"
#include "MOResourceDatabaseSettings.h"
#include "MOCraftingUIController.h"
#include "MOForagingSubsystem.h"
#include "MOPCGWaterMask.h"
#include "MOCoverForageSettings.h"

namespace
{
	AMOCharacter* LocalMOPawn(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<AMOCharacter>(PC->GetPawn()) : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GSwimCmd(
		TEXT("MO.Water.Swim"),
		TEXT("Dev: log the local pawn's water state (oracle surface, immersion, mode, head under, oxygen reserve, SpO2, stamina)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			if (!Pawn)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER Swim FAILED: no local MO pawn"));
				return;
			}
			const UMOCharacterMovementComponent* Move = Cast<UMOCharacterMovementComponent>(Pawn->GetCharacterMovement());
			float Surface = 0.0f;
			const bool bHave = Move && Move->GetWaterSurfaceZ(Surface);
			const UMORespirationComponent* Breath = Pawn->GetRespirationComponent();
			const UMOVitalsComponent* Vitals = Pawn->GetVitalsComponent();
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOQUERY] WATER Swim pos=(%.0f, %.0f, %.0f) surface=%s%.0f immersion=%.2f depth=%.0fcm column=%.0fcm mode=%s swimming=%d headUnder=%d | airwayClosed=%d reserve=%.2f spo2=%.1f asphyxiating=%.1fs | stamina=%.1f speed=%.0f cm/s | bodyTemp=%.2fC vitalsImmersion=%.2f wet=%.2f"),
				Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Pawn->GetActorLocation().Z,
				bHave ? TEXT("") : TEXT("(none) "), Surface, Move ? Move->GetImmersion() : 0.0f, Move ? Move->GetWaterDepthCm() : 0.0f,
				(Move && Move->GetWaterColumnDepthCm() < 1.0e6f) ? Move->GetWaterColumnDepthCm() : -1.0f,   // the swim decision's depth (-1 = no bottom within the probe)
				Move ? *UEnum::GetValueAsString(Move->MovementMode.GetValue()) : TEXT("?"), Pawn->IsSwimming() ? 1 : 0, (Move && Move->IsHeadUnderwater()) ? 1 : 0,
				(Breath && Breath->IsAirwayClosed()) ? 1 : 0, Breath ? Breath->GetOxygenReserve() : -1.0f, Vitals ? Vitals->Vitals.SpO2 : -1.0f, Breath ? Breath->GetSecondsAsphyxiating() : 0.0f,
				Pawn->GetSurvivalStatsComponent() ? Pawn->GetSurvivalStatsComponent()->GetStatCurrent(TEXT("Stamina")) : -1.0f, Pawn->GetVelocity().Size(),
				Vitals ? Vitals->Vitals.BodyTemperature : -1.0f, Vitals ? Vitals->GetWaterImmersion() : -1.0f, Vitals ? Vitals->GetWetnessLevel() : -1.0f);
		}));

	FAutoConsoleCommandWithWorldAndArgs GGoToLakeCmd(
		TEXT("MO.Water.GoToLake"),
		TEXT("Dev: teleport the local pawn into the rank-th nearest upland lake. Usage: MO.Water.GoToLake [rank=0] [dry]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			const UMOWorldSeedSubsystem* Seeds = UMOWorldSeedSubsystem::Get(World);
			if (!Pawn || !Seeds || !Seeds->HasActiveSeed())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER GoToLake FAILED: needs a local MO pawn and an active world seed"));
				return;
			}
			int32 Rank = 0;
			bool bDry = false;
			for (const FString& A : Args)
			{
				if (A.IsNumeric())
				{
					Rank = FMath::Max(0, FCString::Atoi(*A));
				}
				else if (A.Equals(TEXT("dry"), ESearchCase::IgnoreCase))
				{
					bDry = true;
				}
			}
			const FMOWorldGenParamsRef P = FMOWorldGenParamsProvider::Get();
			const FVector Here = Pawn->GetActorLocation();
			TArray<FMOWorldGen::FLake> Lakes;
			FMOWorldGen::GetUplandLakesNear(*P, Here.X, Here.Y, 600000.0, Seeds->GetActiveTerrainSeed(), Lakes);
			Lakes.Sort([&Here](const FMOWorldGen::FLake& A, const FMOWorldGen::FLake& B)
			{
				return FVector2D::DistSquared(A.Center, FVector2D(Here.X, Here.Y)) < FVector2D::DistSquared(B.Center, FVector2D(Here.X, Here.Y));
			});
			if (!Lakes.IsValidIndex(Rank))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER GoToLake FAILED: %d lake(s) within 6 km, rank %d asked"), Lakes.Num(), Rank);
				return;
			}
			const FMOWorldGen::FLake& L = Lakes[Rank];
			FVector Dest;
			if (bDry)
			{
				// 1.15 radii out along the long axis, on the ground there + 2.5 m (the terrain streams in under the pawn)
				const double X = L.Center.X + 1.15 * L.RadiusCm * L.CosAngle, Y = L.Center.Y + 1.15 * L.RadiusCm * L.SinAngle;
				Dest = FVector(X, Y, FMOWorldGen::SampleHeightCm(*P, X, Y, Seeds->GetActiveTerrainSeed(), P->Tuning.VoxelSizeCm) + 250.0);
			}
			else
			{
				Dest = FVector(L.Center.X, L.Center.Y, L.SurfaceCm - 60.0f);   // a body length under the surface: it floats up into the swim
			}
			Pawn->SetActorLocation(Dest, false, nullptr, ETeleportType::ResetPhysics);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER GoToLake rank %d: lake at (%.0f, %.0f) radius %.0f surface %.0f depth %.0f -> pawn at (%.0f, %.0f, %.0f)%s"),
				Rank, L.Center.X, L.Center.Y, L.RadiusCm, L.SurfaceCm, L.DepthCm, Dest.X, Dest.Y, Dest.Z, bDry ? TEXT(" (bank)") : TEXT(""));
		}));

	FAutoConsoleCommandWithWorldAndArgs GStrokeCmd(
		TEXT("MO.Water.Stroke"),
		TEXT("Dev: hold a swimming stroke for a few seconds. Usage: MO.Water.Stroke [seconds=5] [up|down]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			if (!Pawn)
			{
				return;
			}
			double Seconds = 5.0;
			int32 Vertical = 0;
			for (const FString& A : Args)
			{
				if (A.IsNumeric())
				{
					Seconds = FMath::Clamp((double)FCString::Atof(*A), 0.5, 120.0);
				}
				else if (A.Equals(TEXT("up"), ESearchCase::IgnoreCase))
				{
					Vertical = 1;
				}
				else if (A.Equals(TEXT("down"), ESearchCase::IgnoreCase))
				{
					Vertical = -1;
				}
			}
			const double EndTime = FPlatformTime::Seconds() + Seconds;
			TWeakObjectPtr<AMOCharacter> Weak(Pawn);
			const double StartTime = FPlatformTime::Seconds();
			TSharedRef<double> LastLog = MakeShared<double>(0.0);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, EndTime, StartTime, LastLog, Vertical](float)
			{
				AMOCharacter* P = Weak.Get();
				if (!P || FPlatformTime::Seconds() >= EndTime)
				{
					return false;
				}
				if (FPlatformTime::Seconds() - *LastLog >= 0.5)
				{
					*LastLog = FPlatformTime::Seconds();
					const UCharacterMovementComponent* M = P->GetCharacterMovement();
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER Stroke t=%.1f z=%.0f v=(%.0f, %.0f, %.0f) mode=%s | accel=(%.0f, %.0f, %.0f) maxSpeed=%.0f maxAccel=%.0f buoyancy=%.2f immersion=%.2f input=%.2f"), FPlatformTime::Seconds() - StartTime, P->GetActorLocation().Z, P->GetVelocity().X, P->GetVelocity().Y, P->GetVelocity().Z, P->IsSwimming() ? TEXT("swim") : TEXT("other"), M->GetCurrentAcceleration().X, M->GetCurrentAcceleration().Y, M->GetCurrentAcceleration().Z, M->GetMaxSpeed(), M->GetMaxAcceleration(), M->Buoyancy, M->ImmersionDepth(), P->GetPendingMovementInputVector().Z);
				}
				if (Vertical != 0)
				{
					P->AddMovementInput(FVector::UpVector, (float)Vertical);
				}
				else if (P->GetController())
				{
					P->AddMovementInput(P->GetController()->GetControlRotation().Vector(), 1.0f);
				}
				return true;
			}), 0.0f);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER Stroke for %.1f s (%s)"), Seconds, Vertical > 0 ? TEXT("up") : Vertical < 0 ? TEXT("down") : TEXT("forward along the camera"));
		}));

	FAutoConsoleCommandWithWorldAndArgs GSetBreathCmd(
		TEXT("MO.Water.SetBreath"),
		TEXT("Dev: set the local pawn's oxygen reserve (0..1). Usage: MO.Water.SetBreath <reserve>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			if (Pawn && Pawn->GetRespirationComponent() && Args.Num() > 0 && Args[0].IsNumeric())
			{
				Pawn->GetRespirationComponent()->DebugSetOxygenReserve(FCString::Atof(*Args[0]));
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER SetBreath %.2f"), Pawn->GetRespirationComponent()->GetOxygenReserve());
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs GMeterCmd(
		TEXT("MO.Water.Meter"),
		TEXT("Dev: report the breath meter widgets' state (visibility, opacity, on-screen rectangle). Usage: MO.Water.Meter"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			int32 Count = 0;
			for (TObjectIterator<UMOBreathMeterWidget> It; It; ++It)
			{
				if (It->GetWorld() != World)
				{
					continue;
				}
				++Count;
				const FGeometry& Geo = It->GetCachedGeometry();
				const FVector2D Abs = Geo.GetAbsolutePosition();
				const FVector2D Size = Geo.GetAbsoluteSize();
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER Meter visibility=%d opacity=%.2f inViewport=%d rect=(%.0f, %.0f) size=(%.0f x %.0f) desired=(%.0f x %.0f)"),
					(int32)It->GetVisibility(), It->GetRenderOpacity(), It->IsInViewport() ? 1 : 0, Abs.X, Abs.Y, Size.X, Size.Y, It->GetDesiredSize().X, It->GetDesiredSize().Y);
			}
			if (Count == 0)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER Meter: no breath meter widget in this world"));
			}
		}));


	// ---- Drinking and illness (Docs/Water_And_Illness.md) ----

	FString ConditionsLine(const AMOCharacter* Pawn)
	{
		const UMOAnatomyComponent* Anatomy = Pawn ? Pawn->FindComponentByClass<UMOAnatomyComponent>() : nullptr;
		FString Out;
		for (const FMOCondition& C : Anatomy ? Anatomy->GetConditions() : TArray<FMOCondition>())
		{
			Out += FString::Printf(TEXT(" %s(sev=%.1f incubation=%.0fs course=%.0f/%.0fs)"), *UEnum::GetValueAsString(C.ConditionType).RightChop(18), C.Severity,
				C.IncubationRemaining, C.CourseElapsed, C.CourseSeconds);
		}
		return Out.IsEmpty() ? FString(TEXT(" none")) : Out;
	}

	FAutoConsoleCommandWithWorldAndArgs GWaterHereCmd(
		TEXT("MO.Water.Here"),
		TEXT("Dev: what water the local pawn stands in / sees under the crosshair (kind, reach), its thirst, body water, sodium and conditions. Lines start with [MOQUERY] WATER Here."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			AMOPlayerController* PC = World ? Cast<AMOPlayerController>(World->GetFirstPlayerController()) : nullptr;
			const UMOWaterSubsystem* Water = UMOWaterSubsystem::Get(World);
			if (!Pawn || !PC || !Water)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER Here FAILED: needs a local MO pawn and the water subsystem"));
				return;
			}
			const FVector P = Pawn->GetActorLocation();
			FVector Point = FVector::ZeroVector;
			EMOWaterBodyKind Seen = EMOWaterBodyKind::None;
			bool bReach = false;
			const bool bSees = PC->FindWaterAtCrosshair(Point, Seen, bReach);
			const UMOSurvivalStatsComponent* Stats = Pawn->GetSurvivalStatsComponent();
			const UMOMetabolismComponent* Metabolism = Pawn->FindComponentByClass<UMOMetabolismComponent>();
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER Here underfoot=%s crosshair=%s%s at (%.0f, %.0f, %.0f) dist=%.0f | thirst=%.1f hydration=%.1f%% sodium=%.0f%% | conditions:%s"),
				*UEnum::GetValueAsString(Water->ClassifyWaterAt(P.X, P.Y)), bSees ? *UEnum::GetValueAsString(Seen) : TEXT("none"), bSees ? (bReach ? TEXT(" (in reach)") : TEXT(" (too far)")) : TEXT(""),
				Point.X, Point.Y, Point.Z, bSees ? FVector::Dist(P, Point) : -1.0f, Stats ? Stats->GetStatCurrent(TEXT("Thirst")) : -1.0f,
				Metabolism ? Metabolism->GetNutrientLevels().HydrationLevel : -1.0f, Metabolism ? Metabolism->GetNutrientLevels().Sodium : -1.0f, *ConditionsLine(Pawn));
		}));

	FAutoConsoleCommandWithWorldAndArgs GWaterLookCmd(
		TEXT("MO.Water.LookAtWater"),
		TEXT("Dev: turn the local camera onto the nearest open water within 4 m of the pawn (so E / MO.Water.Drink can be tested from a bank)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			const UMOWaterSubsystem* Water = UMOWaterSubsystem::Get(World);
			if (!Pawn || !PC || !PC->PlayerCameraManager || !Water)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER LookAtWater FAILED: no pawn / camera / water subsystem"));
				return;
			}
			const FVector P = Pawn->GetActorLocation();
			FVector Best = FVector::ZeroVector;
			float BestD = TNumericLimits<float>::Max();
			for (int32 Ring = 1; Ring <= 8; ++Ring)
			{
				for (int32 K = 0; K < 24; ++K)
				{
					const float A = K * 2.0f * UE_PI / 24.0f, R = Ring * 50.0f;
					const FVector Q(P.X + R * FMath::Cos(A), P.Y + R * FMath::Sin(A), 0.0);
					float Surface = 0.0f;
					if (Water->ClassifyWaterAt(Q.X, Q.Y) != EMOWaterBodyKind::None && Water->GetSurfaceZ(Q.X, Q.Y, Surface) && R < BestD)
					{
						Best = FVector(Q.X, Q.Y, Surface);
						BestD = R;
					}
				}
				if (BestD < TNumericLimits<float>::Max())
				{
					break;
				}
			}
			if (BestD == TNumericLimits<float>::Max())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER LookAtWater: no water within 4 m"));
				return;
			}
			PC->SetControlRotation((Best - PC->PlayerCameraManager->GetCameraLocation()).Rotation());
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER LookAtWater: water %.0f cm away at (%.0f, %.0f, %.0f)"), BestD, Best.X, Best.Y, Best.Z);
		}));

	FAutoConsoleCommandWithWorldAndArgs GWaterDrinkCmd(
		TEXT("MO.Water.Drink"),
		TEXT("Dev: drink from the water under the crosshair, exactly as E does (through the server)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOPlayerController* PC = World ? Cast<AMOPlayerController>(World->GetFirstPlayerController()) : nullptr;
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER Drink %s"), PC && PC->TryDrinkAtCrosshair() ? TEXT("asked") : TEXT("FAILED: no water under the crosshair"));
		}));

	FAutoConsoleCommandWithWorldAndArgs GContractCmd(
		TEXT("MO.Medical.Contract"),
		TEXT("Dev (server): give the local pawn a condition, with its course if it has one. Usage: MO.Medical.Contract <WaterborneDisease|FoodPoisoning|...> [now] -- now skips the incubation."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			UMOAnatomyComponent* Anatomy = Pawn ? Pawn->FindComponentByClass<UMOAnatomyComponent>() : nullptr;
			const UEnum* Enum = StaticEnum<EMOConditionType>();
			const int64 Value = Args.Num() > 0 && Enum ? Enum->GetValueByNameString(Args[0]) : INDEX_NONE;
			if (!Anatomy || Value == INDEX_NONE)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] MEDICAL Contract FAILED: needs a local pawn (server) and a condition name"));
				return;
			}
			const bool bAdded = Anatomy->AddCondition(static_cast<EMOConditionType>(Value));
			if (bAdded && Args.Num() > 1 && Args[1].Equals(TEXT("now"), ESearchCase::IgnoreCase))
			{
				IConsoleManager::Get().ProcessUserConsoleInput(TEXT("MO.Medical.SkipIncubation"), *GLog, World);
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] MEDICAL Contract %s: %s |%s"), *Args[0], bAdded ? TEXT("added") : TEXT("not added (already has it?)"), *ConditionsLine(Pawn));
		}));

	FAutoConsoleCommandWithWorldAndArgs GSkipIncubationCmd(
		TEXT("MO.Medical.SkipIncubation"),
		TEXT("Dev (server): end every incubating illness's incubation now (symptoms start on the next medical tick). Optional: [hours] also fast-forwards that many game hours into the course."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			UMOAnatomyComponent* Anatomy = Pawn ? Pawn->FindComponentByClass<UMOAnatomyComponent>() : nullptr;
			if (!Anatomy || !Anatomy->ForEachConditionForDebug([&Args](FMOCondition& C)
				{
					C.IncubationRemaining = 0.0f;
					if (Args.Num() > 0 && C.FollowsCourse())
					{
						C.CourseElapsed = FMath::Min(C.CourseSeconds * 0.999f, C.CourseElapsed + FCString::Atof(*Args[0]) * 3600.0f);
					}
				}))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] MEDICAL SkipIncubation FAILED: no local pawn on the server"));
				return;
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] MEDICAL SkipIncubation done |%s"), *ConditionsLine(Pawn));
		}));

	FAutoConsoleCommandWithWorldAndArgs GAdvanceBodyCmd(
		TEXT("MO.Survival.Advance"),
		TEXT("Dev (server): run the local pawn's survival needs (and every illness's effects on them) forward by N game minutes. Usage: MO.Survival.Advance <minutes>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			UMOSurvivalStatsComponent* Stats = Pawn ? Pawn->GetSurvivalStatsComponent() : nullptr;
			if (!Stats || Args.Num() == 0)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] SURVIVAL Advance FAILED: needs a local pawn and minutes"));
				return;
			}
			const float Minutes = FCString::Atof(*Args[0]);
			for (float T = 0.0f; T < Minutes * 60.0f; T += 10.0f)
			{
				Stats->AdvanceStats(10.0f);
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] SURVIVAL Advance %.0f min: thirst=%.1f energy=%.1f |%s"), Minutes, Stats->GetStatCurrent(TEXT("Thirst")),
				Stats->GetStatCurrent(TEXT("Energy")), *ConditionsLine(Pawn));
		}));


	FAutoConsoleCommandWithWorldAndArgs GGoToShoreCmd(
		TEXT("MO.Water.GoToShore"),
		TEXT("Dev: put the local pawn on the bank of the nearest water of a kind (sea | fresh = river, lake, pond, upland lake), a metre from the edge, facing it. Usage: MO.Water.GoToShore [sea|fresh] [maxMetres=400]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			const UMOWaterSubsystem* Water = UMOWaterSubsystem::Get(World);
			if (!Pawn || !PC || !Water)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER GoToShore FAILED: no pawn / water subsystem"));
				return;
			}
			const bool bSea = Args.Num() == 0 || !Args[0].Equals(TEXT("fresh"), ESearchCase::IgnoreCase);
			const float MaxCm = (Args.Num() > 1 ? FCString::Atof(*Args[1]) : 400.0f) * 100.0f;
			auto Wanted = [bSea](EMOWaterBodyKind K) { return bSea ? K == EMOWaterBodyKind::Sea : (K != EMOWaterBodyKind::None && K != EMOWaterBodyKind::Sea); };
			const FVector P = Pawn->GetActorLocation();
			FVector2D Hit = FVector2D::ZeroVector;
			bool bFound = false;
			for (float R = 200.0f; R <= MaxCm && !bFound; R += 200.0f)
			{
				const int32 N = FMath::Clamp(FMath::CeilToInt(2.0f * UE_PI * R / 300.0f), 16, 720);
				for (int32 K = 0; K < N && !bFound; ++K)
				{
					const float A = K * 2.0f * UE_PI / N;
					const FVector2D Q(P.X + R * FMath::Cos(A), P.Y + R * FMath::Sin(A));
					if (Wanted(Water->ClassifyWaterAt(Q.X, Q.Y)))
					{
						Hit = Q;
						bFound = true;
					}
				}
			}
			if (!bFound)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER GoToShore: no %s water within %.0f m"), bSea ? TEXT("sea") : TEXT("fresh"), MaxCm / 100.0f);
				return;
			}
			// Walk back toward where the pawn stood until the ground is dry, then a metre further onto the bank.
			const FVector2D Back = (FVector2D(P) - Hit).GetSafeNormal();
			FVector2D Edge = Hit;
			for (float T = 0.0f; T < 5000.0f; T += 25.0f)
			{
				const FVector2D Q = Hit + Back * T;
				if (Water->ClassifyWaterAt(Q.X, Q.Y) == EMOWaterBodyKind::None)
				{
					Edge = Q;
					break;
				}
			}
			const FVector2D Stand = Edge + Back * 100.0f;
			float Ground = 0.0f, Surface = 0.0f;
			Water->GetGroundZ(Stand.X, Stand.Y, Ground);
			Water->GetSurfaceZ(Hit.X, Hit.Y, Surface);
			Pawn->SetActorLocation(FVector(Stand.X, Stand.Y, Ground + 120.0f), false, nullptr, ETeleportType::ResetPhysics);
			PC->SetControlRotation(FRotator(-35.0f, FMath::RadiansToDegrees(FMath::Atan2(-Back.Y, -Back.X)), 0.0f));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] WATER GoToShore %s: water at (%.0f, %.0f) %s, standing at (%.0f, %.0f, %.0f), %.0f m from where the pawn was"),
				bSea ? TEXT("sea") : TEXT("fresh"), Hit.X, Hit.Y, *UEnum::GetValueAsString(Water->ClassifyWaterAt(Hit.X, Hit.Y)), Stand.X, Stand.Y, Ground, FVector2D::Distance(FVector2D(P), Stand) / 100.0f);
		}));


	FAutoConsoleCommandWithWorldAndArgs GFireSpawnLitCmd(
		TEXT("MO.Fire.SpawnLit"),
		TEXT("Dev (server): a finished, fuelled, burning campfire (the BuildCampfire recipe's actor) 1.5 m in front of the local pawn -- for testing what needs a fire (boiling). Usage: MO.Fire.SpawnLit [unlit]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(FName(TEXT("BuildCampfire")));
			UClass* FireClass = Recipe ? Recipe->PlacementData.BuildableActorClass.Get() : nullptr;
			if (!Pawn || !FireClass || !Pawn->HasAuthority())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FIRE SpawnLit FAILED: needs a local pawn on the server and the BuildCampfire recipe's actor class"));
				return;
			}
			FVector Spot = Pawn->GetActorLocation() + FRotator(0.0f, Pawn->GetActorRotation().Yaw, 0.0f).Vector() * 150.0f;
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(MOFireSpawn), false, Pawn);
			if (World->LineTraceSingleByChannel(Hit, Spot + FVector(0, 0, 300), Spot - FVector(0, 0, 600), ECC_WorldStatic, Params))
			{
				Spot = Hit.ImpactPoint;
			}
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AMOCraftingStationActor* Fire = World->SpawnActor<AMOCraftingStationActor>(FireClass, Spot, FRotator::ZeroRotator, SpawnParams);
			if (!Fire)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FIRE SpawnLit FAILED: %s is not a crafting station / did not spawn"), *FireClass->GetName());
				return;
			}
			static_cast<AMOBuildableActor*>(Fire)->InitializeBuilding(FName(TEXT("BuildCampfire")));   // public on the base, the station overrides it protected
			// "unlit": an empty, cold campfire, to light the real way (MO.Fire.Do Light).
			if (!(Args.Num() > 0 && Args[0].Equals(TEXT("unlit"), ESearchCase::IgnoreCase)))
			{
				Fire->CurrentFuel = Fire->MaxFuel;
				Fire->SetStationActive(true);
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FIRE SpawnLit: %s at (%.0f, %.0f, %.0f), fuel %.0f, active %d, heat %.1f C over %.0f cm"), *Fire->GetName(), Spot.X, Spot.Y, Spot.Z,
				Fire->CurrentFuel, Fire->IsStationActive() ? 1 : 0, Fire->HeatOutputCelsius, Fire->HeatRadiusCm);
		}));


	// ---- Item actions (FMOItemActions): what the item menu offers, done through the real server path ----

	bool FirstEntryOf(AMOCharacter* Pawn, const FString& ItemId, FMOInventoryEntry& Out, int32& OutSlot)
	{
		UMOInventoryComponent* Inv = Pawn ? Pawn->FindComponentByClass<UMOInventoryComponent>() : nullptr;
		if (!Inv)
		{
			return false;
		}
		for (int32 Slot = 0; Slot < Inv->GetSlotCount(); ++Slot)
		{
			if (Inv->TryGetSlotEntry(Slot, Out) && Out.ItemDefinitionId == FName(*ItemId))
			{
				OutSlot = Slot;
				return true;
			}
		}
		return false;
	}

	FAutoConsoleCommandWithWorldAndArgs GItemActionsCmd(
		TEXT("MO.Item.Actions"),
		TEXT("Dev: the extra actions the item menu offers for the first <ItemId> the local pawn carries (FMOItemActions::GetActions), and its contents. Usage: MO.Item.Actions <ItemId>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			FMOInventoryEntry Entry;
			int32 Slot = INDEX_NONE;
			if (Args.Num() == 0 || !FirstEntryOf(Pawn, Args[0], Entry, Slot))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] ITEM Actions FAILED: no %s carried"), Args.Num() > 0 ? *Args[0] : TEXT("<ItemId>"));
				return;
			}
			TArray<FMOItemActionOption> Options;
			FMOItemActions::GetActions(Pawn, Pawn->FindComponentByClass<UMOInventoryComponent>(), Entry.ItemGuid, Options);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] ITEM Actions %s slot %d: %.0f ml (salinity %.1f, illness %.2f %%/L, boiled %d, source %s), %d action(s)"), *Args[0], Slot,
				Entry.Liquid.Millilitres, Entry.Liquid.SalinityGPerL, Entry.Liquid.IllnessChancePerLitre * 100.0f, Entry.Liquid.bBoiled ? 1 : 0, *UEnum::GetValueAsString(Entry.Liquid.Source), Options.Num());
			for (const FMOItemActionOption& O : Options)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] ITEM   %s '%s' %s%s"), *O.Id.ToString(), *O.Label.ToString(), O.bEnabled ? TEXT("enabled") : TEXT("DISABLED: "),
					O.bEnabled ? TEXT("") : *O.DisabledReason.ToString());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GItemDoCmd(
		TEXT("MO.Item.Do"),
		TEXT("Dev: do an item action on the first <ItemId> the local pawn carries through the real server RPC (as the menu button does). Usage: MO.Item.Do <ItemId> <Drink|Fill|PourOut|BoilOverFire|BoilWithStones|Use>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			AMOPlayerController* PC = World ? Cast<AMOPlayerController>(World->GetFirstPlayerController()) : nullptr;
			FMOInventoryEntry Entry;
			int32 Slot = INDEX_NONE;
			if (Args.Num() < 2 || !PC || !FirstEntryOf(Pawn, Args[0], Entry, Slot))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] ITEM Do FAILED: usage MO.Item.Do <ItemId> <Action>, and the item must be carried"));
				return;
			}
			PC->ServerItemAction(Entry.ItemGuid, FName(*Args[1]));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] ITEM Do %s %s: asked"), *Args[0], *Args[1]);
		}));

	FAutoConsoleCommandWithWorldAndArgs GItemMenuCmd(
		TEXT("MO.Item.Menu"),
		TEXT("Dev: open the item context menu (as a right-click in the inventory does) for the first <ItemId> the local pawn carries, at the screen centre. Usage: MO.Item.Menu <ItemId>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			UMOUIManagerComponent* UI = PC ? PC->FindComponentByClass<UMOUIManagerComponent>() : nullptr;
			FMOInventoryEntry Entry;
			int32 Slot = INDEX_NONE;
			if (Args.Num() == 0 || !UI || !FirstEntryOf(Pawn, Args[0], Entry, Slot))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] ITEM Menu FAILED: no UI manager or no %s carried"), Args.Num() > 0 ? *Args[0] : TEXT("<ItemId>"));
				return;
			}
			int32 X = 0, Y = 0;
			PC->GetViewportSize(X, Y);
			UI->ShowItemContextMenu(Pawn->FindComponentByClass<UMOInventoryComponent>(), Entry.ItemGuid, Slot, FVector2D(X * 0.5f, Y * 0.5f));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] ITEM Menu %s slot %d: opened"), *Args[0], Slot);
		}));


	AMOCraftingStationActor* NearestFire(UWorld* World, const AActor* From)
	{
		AMOCraftingStationActor* Best = nullptr;
		float BestD = 1000.0f;
		for (TActorIterator<AMOCraftingStationActor> It(World); It; ++It)
		{
			const float D = FVector::Dist(It->GetActorLocation(), From->GetActorLocation());
			if (It->bRequiresFuel && D < BestD)
			{
				Best = *It;
				BestD = D;
			}
		}
		return Best;
	}

	FAutoConsoleCommandWithWorldAndArgs GFireActionsCmd(
		TEXT("MO.Fire.Actions"),
		TEXT("Dev: the fire actions (FMOFireActions::GetActions) the local pawn has at the nearest fuelled station within 10 m, and its state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			AMOCraftingStationActor* Fire = Pawn ? NearestFire(World, Pawn) : nullptr;
			if (!Fire)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FIRE Actions FAILED: no fuelled station within 10 m"));
				return;
			}
			TArray<FMOItemActionOption> Options;
			FMOFireActions::GetActions(Pawn, Fire, Options);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FIRE Actions %s: fuel %.0f / %.0f, active %d, %.0f cm away, %d action(s)"), *Fire->GetName(), Fire->CurrentFuel, Fire->MaxFuel,
				Fire->IsStationActive() ? 1 : 0, FVector::Dist(Fire->GetActorLocation(), Pawn->GetActorLocation()), Options.Num());
			for (const FMOItemActionOption& O : Options)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FIRE   %s '%s' %s%s"), *O.Id.ToString(), *O.Label.ToString(), O.bEnabled ? TEXT("enabled") : TEXT("DISABLED: "), O.bEnabled ? TEXT("") : *O.DisabledReason.ToString());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GFireDoCmd(
		TEXT("MO.Fire.Do"),
		TEXT("Dev: do a fire action at the nearest fuelled station through the real server RPC. Usage: MO.Fire.Do <AddFuel.<ItemId>|Light|PutOut>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			AMOPlayerController* PC = World ? Cast<AMOPlayerController>(World->GetFirstPlayerController()) : nullptr;
			AMOCraftingStationActor* Fire = Pawn ? NearestFire(World, Pawn) : nullptr;
			if (!Fire || !PC || Args.Num() == 0)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FIRE Do FAILED: usage MO.Fire.Do <Action>, a fuelled station within 10 m"));
				return;
			}
			PC->ServerStationAction(Fire, FName(*Args[0]));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FIRE Do %s: asked"), *Args[0]);
		}));


	FAutoConsoleCommandWithWorldAndArgs GFireMenuCmd(
		TEXT("MO.Fire.Menu"),
		TEXT("Dev: open the station menu (as E on it does) for the nearest fuelled station within 10 m."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			UMOUIManagerComponent* UI = PC ? PC->FindComponentByClass<UMOUIManagerComponent>() : nullptr;
			AMOCraftingStationActor* Fire = Pawn ? NearestFire(World, Pawn) : nullptr;
			if (!Fire || !UI)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FIRE Menu FAILED: no fuelled station within 10 m or no UI manager"));
				return;
			}
			UI->ShowStationContextMenu(Fire, Fire->GetActorLocation());
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] FIRE Menu %s: opened"), *Fire->GetName());
		}));

	/** The nearest live instance of a placed resource (a HISM tagged ResourceNode_<Id>) within RadiusCm that offers ActionId (None = any), of OnlyResource
	 *  when that is set. */
	bool NearestResourceInstance(UWorld* World, const FVector& From, float RadiusCm, FName ActionId, FMOInteractionTarget& OutTarget, FName& OutResourceId,
		FName OnlyResource = NAME_None)
	{
		float BestDistSq = FMath::Square(RadiusCm);
		bool bFound = false;
		for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
		{
			UInstancedStaticMeshComponent* Ism = *It;
			if (!Ism || Ism->GetWorld() != World || Ism->GetInstanceCount() == 0)
			{
				continue;
			}
			FName ResourceId;
			for (const FName& Tag : Ism->ComponentTags)
			{
				FString S = Tag.ToString();
				if (S.RemoveFromStart(TEXT("ResourceNode_")))
				{
					ResourceId = FName(*S);
					break;
				}
			}
			const FMOResourceNodeDefinitionRow* Row = ResourceId.IsNone() ? nullptr : UMOResourceDatabaseSettings::GetResourceDefinition(ResourceId);
			if (!Row || (!ActionId.IsNone() && !Row->FindAction(ActionId)) || (!OnlyResource.IsNone() && ResourceId != OnlyResource))
			{
				continue;
			}
			for (const int32 Index : Ism->GetInstancesOverlappingSphere(From, RadiusCm, /*bSphereInWorldSpace=*/true))
			{
				FTransform Xf;
				if (!Ism->GetInstanceTransform(Index, Xf, /*bWorldSpace=*/true) || Xf.GetScale3D().IsNearlyZero())
				{
					continue;   // removed (felled / picked) instances are scaled to nothing
				}
				const float DistSq = FVector::DistSquared(From, Xf.GetLocation());
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					OutTarget = FMOInteractionTarget();
					OutTarget.TargetActor = Ism->GetOwner();
					OutTarget.bIsInstancedMeshTarget = true;
					OutTarget.ISMComponent = Ism;
					OutTarget.HISMComponent = Cast<UHierarchicalInstancedStaticMeshComponent>(Ism);
					OutTarget.InstanceIndex = Index;
					OutTarget.HitResult.Component = Ism;
					OutTarget.HitResult.Item = Index;
					OutTarget.HitResult.ImpactPoint = Xf.GetLocation();
					OutTarget.HitResult.Location = Xf.GetLocation();
					OutTarget.HitResult.bBlockingHit = true;
					OutResourceId = ResourceId;
					bFound = true;
				}
			}
		}
		return bFound;
	}

	FAutoConsoleCommandWithWorldAndArgs GHarvestDoCmd(
		TEXT("MO.Harvest.Do"),
		TEXT("Dev: harvest the nearest placed resource offering ActionId as the harvest menu does (the menu's target + StartHarvestOperation: timed, then the "
			"server's yield). `list` = name the nearest instance of every resource within the radius and its actions. Usage: MO.Harvest.Do <ActionId|list> [radius_m=40]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			UMOCraftingUIController* CraftUI = PC ? PC->FindComponentByClass<UMOCraftingUIController>() : nullptr;
			const float RadiusCm = (Args.Num() > 1 ? FCString::Atof(*Args[1]) : 40.0f) * 100.0f;
			if (!Pawn || !CraftUI || Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] HARVEST Do FAILED: no pawn / crafting UI controller, or no ActionId"));
				return;
			}
			if (Args[0].Equals(TEXT("list"), ESearchCase::IgnoreCase))
			{
				TSet<FName> Seen;
				for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
				{
					for (const FName& Tag : (*It && (*It)->GetWorld() == World) ? (*It)->ComponentTags : TArray<FName>())
					{
						FString S = Tag.ToString();
						if (S.RemoveFromStart(TEXT("ResourceNode_")))
						{
							Seen.Add(FName(*S));
						}
					}
				}
				for (const FName& Id : Seen)
				{
					FMOInteractionTarget OfKind;
					FName KindId;
					const FMOResourceNodeDefinitionRow* Row = UMOResourceDatabaseSettings::GetResourceDefinition(Id);
					if (!Row || !NearestResourceInstance(World, Pawn->GetActorLocation(), RadiusCm, NAME_None, OfKind, KindId, Id))
					{
						continue;
					}
					FString Actions;
					for (const FMOResourceHarvestAction& Action : Row->HarvestActions)
					{
						Actions += (Actions.IsEmpty() ? TEXT("") : TEXT(", ")) + Action.ActionId.ToString();
					}
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] HARVEST list %s at %.0f m: %s"), *Id.ToString(),
						FVector::Dist(Pawn->GetActorLocation(), OfKind.HitResult.ImpactPoint) / 100.0f, *Actions);
				}
				return;
			}
			const FName ActionId(*Args[0]);
			FMOInteractionTarget Target;
			FName ResourceId;
			if (!NearestResourceInstance(World, Pawn->GetActorLocation(), RadiusCm, ActionId, Target, ResourceId))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] HARVEST Do FAILED: no placed resource offering %s within %.0f m"), *ActionId.ToString(), RadiusCm / 100.0f);
				return;
			}
			// As a click does: the menu is open on the target, the action starts (the progress widget takes the target), then the menu closes.
			CraftUI->ShowKeepOnHarvestContextMenu(Target);
			CraftUI->StartHarvestOperation(ActionId);
			CraftUI->HideKeepOnHarvestContextMenu();
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] HARVEST Do %s on %s #%d at %.0f m: started"), *ActionId.ToString(), *ResourceId.ToString(), Target.InstanceIndex,
				FVector::Dist(Pawn->GetActorLocation(), Target.HitResult.ImpactPoint) / 100.0f);
		}));

	FAutoConsoleCommandWithWorldAndArgs GCoverCmd(
		TEXT("MO.Cover.Gather"),
		TEXT("Dev: list every kind of gatherable ground cover (UMOCoverForageSettings) within radius_m of the pawn, as the Forage menu offers them; `go [n]` "
			"gathers the n-th (default 0) through the real server RPC. Usage: MO.Cover.Gather [radius_m=3] [go [n]]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalMOPawn(World);
			AMOPlayerController* PC = World ? Cast<AMOPlayerController>(World->GetFirstPlayerController()) : nullptr;
			UMOForagingSubsystem* Foraging = World ? World->GetSubsystem<UMOForagingSubsystem>() : nullptr;
			const float RadiusCm = (Args.Num() > 0 ? FCString::Atof(*Args[0]) : 3.0f) * 100.0f;
			TArray<FMOGatherableCover> Kinds;
			if (Pawn && Foraging)
			{
				Foraging->FindGatherableCoverKinds(Pawn->GetActorLocation(), RadiusCm, Kinds, Pawn);
			}
			if (!PC || Kinds.Num() == 0)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COVER none within %.0f m"), RadiusCm / 100.0f);
				return;
			}
			for (int32 N = 0; N < Kinds.Num(); ++N)
			{
				const FMOGatherableCover& Cover = Kinds[N];
				FString Yields;
				for (const FMODigDropEntry& Entry : Cover.Yields())
				{
					Yields += FString::Printf(TEXT("%s%s %d-%d @%.0f%%"), Yields.IsEmpty() ? TEXT("") : TEXT(", "), *Entry.ItemId.ToString(), Entry.MinQuantity, Entry.MaxQuantity,
						Entry.DropChance * 100.0f);
				}
				const FMOPCGWaterMask Mask = FMOPCGWaterMask::ForActor(Cover.Component->GetOwner());
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COVER %d '%s' (%s #%d%s) z %.0f (mask %s, in water %d, shore %.2f)%s: %s"), N, *Cover.Label().ToString(),
					*GetNameSafe(Cover.Component->GetStaticMesh()), Cover.InstanceIndex, Cover.bAtShore ? TEXT(", at the water's edge") : TEXT(""), Cover.Location.Z,
					Mask.IsActive() ? TEXT("on") : TEXT("OFF"), Mask.IsInWater(Cover.Location.X, Cover.Location.Y, Cover.Location.Z) ? 1 : 0,
					Mask.ShoreFactor(Cover.Location.X, Cover.Location.Y, Cover.Location.Z, 300.0f),
					Cover.Entry->CanBeWorkedBy(Pawn->FindComponentByClass<UMOInventoryComponent>()) ? TEXT("") : TEXT(" [needs a tool]"), *Yields);
			}
			const int32 GoAt = Args.IndexOfByKey(TEXT("go"));
			if (GoAt != INDEX_NONE)
			{
				const int32 Which = Args.IsValidIndex(GoAt + 1) ? FCString::Atoi(*Args[GoAt + 1]) : 0;
				if (Kinds.IsValidIndex(Which))
				{
					PC->ServerGatherCover(Kinds[Which].Location, Kinds[Which].EntryIndex);
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] COVER gather %d asked"), Which);
				}
			}
		}));

}

#endif // !UE_BUILD_SHIPPING
