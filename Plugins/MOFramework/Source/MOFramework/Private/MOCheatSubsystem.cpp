/**
 * MOCheatSubsystem.cpp - Dev/debug console commands
 */

#include "MOCheatSubsystem.h"
#include "MOFramework.h"
#include "MOGameClockSubsystem.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

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
}

UMOCheatSubsystem* UMOCheatSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	const UWorld* World = WorldContextObject->GetWorld();
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<UMOCheatSubsystem>() : nullptr;
}

void UMOCheatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisterConsoleCommands();
	UE_LOG(LogMOFramework, Log, TEXT("[MOCheat] Initialized — %d commands registered"), ConsoleCommands.Num());
}

void UMOCheatSubsystem::Deinitialize()
{
	UnregisterConsoleCommands();
	Super::Deinitialize();
}

void UMOCheatSubsystem::RegisterConsoleCommands()
{
	IConsoleManager& CM = IConsoleManager::Get();

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
