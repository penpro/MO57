#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "MOAnatomyComponent.h"
#include "MOBodyPartDefinitionRow.h"
#include "MOFramework.h"
#include "MOIdentityRegistrySubsystem.h"
#include "MOMedicalSubsystem.h"
#include "MOTimedActionComponent.h"
#include "MOPlayerController.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

// These verbs submit the normal owning-client request; they do not supply skill, skip time or inject materials.
static FAutoConsoleCommandWithWorldAndArgs GMOMedicalTreatCommand(
	TEXT("MO.Medical.Treat"), TEXT("MO.Medical.Treat <TreatmentId> <WoundGuid> [PatientGuid]; omit PatientGuid for self."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		auto* PC = World ? Cast<AMOPlayerController>(World->GetFirstPlayerController()) : nullptr;
		FGuid WoundId;
		if (!PC || !PC->IsLocalController() || Args.Num() < 2 || !FGuid::Parse(Args[1], WoundId))
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] TREAT usage: MO.Medical.Treat <TreatmentId> <WoundGuid> [PatientGuid]"));
			return;
		}
		AActor* Patient = PC->GetPawn();
		if (Args.Num() > 2)
		{
			FGuid PatientId;
			auto* Registry = World->GetSubsystem<UMOIdentityRegistrySubsystem>();
			if (!Registry || !FGuid::Parse(Args[2], PatientId) || !Registry->TryResolveActor(PatientId, Patient))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] TREAT patient GUID is not loaded"));
				return;
			}
		}
		// The dev bridge forces local callspace during its synchronous console invocation.
		// Submit on the next game tick, as MO.Item.Do / MO.Fire.Do, so a joiner reaches the real server RPC.
		const TWeakObjectPtr<AMOPlayerController> WeakPC(PC);
		const TWeakObjectPtr<AActor> WeakPatient(Patient);
		const FName TreatmentId(*Args[0]);
		World->GetTimerManager().SetTimerForNextTick([WeakPC, WeakPatient, WoundId, TreatmentId]()
		{
			if (auto* Controller = WeakPC.Get()) Controller->ServerTreatWound(WeakPatient.Get(), WoundId, TreatmentId);
		});
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] TREAT requested %s wound=%s"), *TreatmentId.ToString(), *WoundId.ToString());
	}));

static FAutoConsoleCommandWithWorldAndArgs GMOMedicalTreatmentStatusCommand(
	TEXT("MO.Medical.TreatmentStatus"), TEXT("Lists the local pawn's action, wound GUIDs and authored treatment requirements; 'progress' prints only action state."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const auto* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (const auto* Timed = Pawn ? Pawn->FindComponentByClass<UMOTimedActionComponent>() : nullptr)
		{
			const auto& State = Timed->GetState();
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] TREAT progress action=%s active=%d seconds=%.3f remaining=%.3f label=%s"),
				*State.ActionId.ToString(), State.bActive, State.RealSeconds, Timed->GetRemainingRealSeconds(), *State.Label.ToString());
		}
		if (Args.Contains(TEXT("progress"))) return;
		const auto* Anatomy = Pawn ? Pawn->FindComponentByClass<UMOAnatomyComponent>() : nullptr;
		if (Anatomy)
			for (const auto& Wound : Anatomy->GetAllWounds())
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] TREAT wound=%s type=%d part=%d bleed=%.4f infectionRisk=%.4f"),
					*Wound.WoundId.ToString(), int32(Wound.WoundType), int32(Wound.BodyPart), Wound.BleedRate, Wound.InfectionRisk);
		const auto* GI = World ? World->GetGameInstance() : nullptr;
		const auto* Medical = GI ? GI->GetSubsystem<UMOMedicalSubsystem>() : nullptr;
		if (!Medical) return;
		for (const auto& Treatment : Medical->GetAllTreatmentDefinitions())
		{
			FString Supplies;
			for (FName Id : Treatment.RequiredItemIds) Supplies += Id.ToString() + TEXT(" ");
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] TREAT definition=%s seconds=%.2f skill=%s min=%d woundTypes=%d supplies=%s"),
				*Treatment.TreatmentId.ToString(), Treatment.TreatmentDuration, *Treatment.RequiredSkillId.ToString(), Treatment.MinimumSkillLevel,
				Treatment.TreatsWoundTypes.Num(), *Supplies);
		}
	}));
#endif
