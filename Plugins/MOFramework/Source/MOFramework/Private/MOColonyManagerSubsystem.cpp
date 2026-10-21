#include "MOColonyManagerSubsystem.h"
#include "MOPersistenceSubsystem.h"
#include "MOworldSaveGame.h"
#include "MOFramework.h"
#include "MOCharacter.h"
#include "MOContainerActor.h"
#include "MOCraftingStationActor.h"
#include "MOBuildableActor.h"
#include "MOIdentityComponent.h"
#include "MOIdentityRegistrySubsystem.h"
#include "MOAmbientEnvironmentProvider.h"
#include "NavigationSystem.h"
#include "MORecruitmentComponent.h"
#include "MOMetabolismComponent.h"
#include "MOMentalStateComponent.h"
#include "MOVitalsComponent.h"
#include "MOPersonalityComponent.h"
#include "MOCharacterHistoryComponent.h"
#include "MOInventoryComponent.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MORecipeDatabaseSettings.h"
#include "MORecipeDefinitionRow.h"
#include "MOGameClockSubsystem.h"
#include "MOSurvivorController.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOSkillsComponent.h"
#include "MOBuildProgressComponent.h"
#include "MODesignationSubsystem.h"
#include "MOAnatomyComponent.h"
#include "MOCreature.h"
#include "MOMedicalTreatmentActions.h"
#include "EngineUtils.h"
#include "TimerManager.h"

UMOColonyManagerSubsystem* UMOColonyManagerSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UMOColonyManagerSubsystem>() : nullptr;
}

void UMOColonyManagerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (UGameInstance* GI = InWorld.GetGameInstance())
	{
		if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
		{
			Persist->RegisterSaveDomain(this);
		}
	}

	// Server-only loop; clients get colony state via queries/replicated comps.
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	InWorld.GetTimerManager().SetTimer(UpkeepTimer, this,
		&UMOColonyManagerSubsystem::RunUpkeepTick, UpkeepIntervalSeconds, /*bLoop=*/true);
}

void UMOColonyManagerSubsystem::Deinitialize()
{
	if (UWorld* W = GetWorld())
	{
		if (UGameInstance* GI = W->GetGameInstance())
		{
			if (UMOPersistenceSubsystem* Persist = GI->GetSubsystem<UMOPersistenceSubsystem>())
			{
				Persist->UnregisterSaveDomain(this);
			}
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpkeepTimer);
	}
	Super::Deinitialize();
}

// ============================================================================
// SETTLEMENT
// ============================================================================

const FMOSettlementRecord& UMOColonyManagerSubsystem::FoundSettlement(const FString& Name, const FVector& Center, float Radius)
{
	if (!Settlement.IsValid())
	{
		Settlement.SettlementId = FGuid::NewGuid();
	}
	Settlement.DisplayName = FText::FromString(Name);
	Settlement.Center = Center;
	Settlement.Radius = Radius;
	UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] Settlement '%s' founded at %s (r=%.0f)"),
		*Name, *Center.ToCompactString(), Radius);
	return Settlement;
}

bool UMOColonyManagerSubsystem::IsInSettlement(const FVector& Location) const
{
	return Settlement.IsValid()
		&& FVector::DistSquared2D(Location, Settlement.Center) <= FMath::Square(Settlement.Radius);
}

TArray<APawn*> UMOColonyManagerSubsystem::GetColonyRoster() const
{
	TArray<APawn*> Roster;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return Roster;
	}
	for (TActorIterator<APawn> It(const_cast<UWorld*>(World)); It; ++It)
	{
		APawn* Pawn = *It;
		if (!IsValid(Pawn))
		{
			continue;
		}
		const UMORecruitmentComponent* Recruit = Pawn->FindComponentByClass<UMORecruitmentComponent>();
		if (Recruit && Recruit->IsPossessable() && GetPawnGuid(Pawn).IsValid())
		{
			Roster.Add(Pawn);
		}
	}
	return Roster;
}

// ============================================================================
// HOUSING
// ============================================================================

bool UMOColonyManagerSubsystem::AssignResidence(APawn* Villager, AActor* House)
{
	const FGuid PawnGuid = GetPawnGuid(Villager);
	const AMOBuildableActor* Buildable = Cast<AMOBuildableActor>(House);
	const UMOIdentityComponent* HouseId = House ? House->FindComponentByClass<UMOIdentityComponent>() : nullptr;
	if (!PawnGuid.IsValid() || !Buildable || !HouseId || !HouseId->GetGuid().IsValid())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] AssignResidence rejected: missing pawn guid / buildable / house identity"));
		return false;
	}

	// Capacity is DATA: the building recipe's HousingCapacity.
	int32 Capacity = 0;
	if (const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(Buildable->GetRecipeId()))
	{
		Capacity = Recipe->HousingCapacity;
	}
	if (Capacity <= 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] AssignResidence rejected: recipe '%s' has no HousingCapacity"),
			*Buildable->GetRecipeId().ToString());
		return false;
	}
	const FGuid HouseGuid = HouseId->GetGuid();
	if (GetHouseOccupancy(HouseGuid) >= Capacity && Residency.FindRef(PawnGuid) != HouseGuid)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] AssignResidence rejected: house full (%d/%d)"),
			GetHouseOccupancy(HouseGuid), Capacity);
		return false;
	}

	Residency.Add(PawnGuid, HouseGuid);
	VillagerUnhousedHours.Remove(PawnGuid);
	if (UMOCharacterHistoryComponent* History = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
	{
		History->AddEntry(EHistoryEntryType::Activity,
			FString::Printf(TEXT("Moved into %s"), *Buildable->GetName()));
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] %s assigned to house %s (%d/%d)"),
		*Villager->GetName(), *House->GetName(), GetHouseOccupancy(HouseGuid), Capacity);
	return true;
}

void UMOColonyManagerSubsystem::ClearResidence(const FGuid& PawnGuid)
{
	Residency.Remove(PawnGuid);
}

int32 UMOColonyManagerSubsystem::GetHouseOccupancy(const FGuid& HouseGuid) const
{
	int32 N = 0;
	for (const auto& Pair : Residency)
	{
		if (Pair.Value == HouseGuid)
		{
			++N;
		}
	}
	return N;
}

// ============================================================================
// MOOD — pure math over real-sim inputs
// ============================================================================

float UMOColonyManagerSubsystem::ComputeVillagerMood(const FMOVillagerMoodInputs& In)
{
	// Content-neutral baseline. Positive comforts are few in V1 (a home);
	// most pressure is downward, exactly like early survival.
	float Mood = 0.65f;

	float Pressure = 0.0f;
	if (In.bStarving)    { Pressure += 0.30f; }
	if (In.bDehydrated)  { Pressure += 0.20f; }
	Pressure += 0.10f * FMath::Clamp(In.Wetness, 0.0f, 1.0f);
	Pressure += 0.15f * FMath::Clamp(In.Shock / 100.0f, 0.0f, 1.0f);
	Pressure += 0.15f * FMath::Clamp(In.TraumaticStress / 100.0f, 0.0f, 1.0f);
	Pressure += 0.10f * FMath::Clamp(In.MoraleFatigue / 100.0f, 0.0f, 1.0f);

	if (In.bHasHome)
	{
		Mood += 0.10f;
	}
	else
	{
		// Homelessness bites harder the longer it lasts: ~0.02/h up to 0.25.
		Pressure += FMath::Min(In.UnhousedHours * 0.02f, 0.25f);
	}

	// Volatile personalities (modifier > 1) crash harder under the same
	// pressure; stable ones ride it out.
	Mood -= Pressure * FMath::Clamp(In.MoodVarianceModifier, 0.25f, 2.0f);

	return FMath::Clamp(Mood, 0.0f, 1.0f);
}

float UMOColonyManagerSubsystem::GetVillagerMood(const FGuid& PawnGuid) const
{
	const float* Found = VillagerMood.Find(PawnGuid);
	return Found ? *Found : 0.65f;
}

// ============================================================================
// QUOTAS / STANDING ORDERS (V2.1)
// ============================================================================

void UMOColonyManagerSubsystem::SetQuota(FName OutputItemId, FName RecipeId, int32 TargetCount, int32 Priority)
{
	Quotas.RemoveAll([&](const FMOColonyQuota& Q) { return Q.OutputItemId == OutputItemId; });
	if (TargetCount > 0)
	{
		FMOColonyQuota Q;
		Q.OutputItemId = OutputItemId;
		Q.RecipeId = RecipeId;
		Q.TargetCount = TargetCount;
		Q.Priority = Priority;
		Quotas.Add(Q);
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] Quota %s: keep %d via %s (prio %d)"),
		*OutputItemId.ToString(), TargetCount, *RecipeId.ToString(), Priority);
}

TArray<FName> UMOColonyManagerSubsystem::DecideQuotaWork(const TArray<FMOColonyQuota>& InQuotas,
	const TMap<FName, int32>& Stock, const TSet<FName>& RecipesInFlight, int32 IdleVillagers)
{
	TArray<FMOColonyQuota> Sorted = InQuotas;
	Sorted.Sort([](const FMOColonyQuota& A, const FMOColonyQuota& B) { return A.Priority > B.Priority; });

	TArray<FName> Assignments;
	for (const FMOColonyQuota& Q : Sorted)
	{
		if (Assignments.Num() >= IdleVillagers)
		{
			break;
		}
		if (Q.RecipeId.IsNone() || Q.TargetCount <= 0)
		{
			continue;
		}
		if (Stock.FindRef(Q.OutputItemId) >= Q.TargetCount)
		{
			continue;   // quota met — no busywork
		}
		if (RecipesInFlight.Contains(Q.RecipeId))
		{
			continue;   // someone is already on it — one villager per order per pass
		}
		Assignments.Add(Q.RecipeId);
	}
	return Assignments;
}

void UMOColonyManagerSubsystem::RunQuotaPass(const TArray<APawn*>& Roster)
{
	if (Quotas.Num() == 0)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Stock across settlement communal storage, for quota'd items only.
	TMap<FName, int32> Stock;
	TArray<AMOContainerActor*> Storages;
	for (TActorIterator<AMOContainerActor> It(World); It; ++It)
	{
		if (IsValid(*It) && IsInSettlement(It->GetActorLocation()))
		{
			Storages.Add(*It);
			if (const UMOInventoryComponent* Inv = It->FindComponentByClass<UMOInventoryComponent>())
			{
				for (const FMOColonyQuota& Q : Quotas)
				{
					Stock.FindOrAdd(Q.OutputItemId) += Inv->GetItemCountByDefinitionId(Q.OutputItemId);
				}
			}
		}
	}
	if (Storages.Num() == 0)
	{
		return;
	}

	// Idle AI villagers + recipes already in flight.
	TArray<APawn*> Idle;
	TSet<FName> InFlight;
	for (APawn* Villager : Roster)
	{
		const bool bAIControlled = Villager->GetController()
			&& Villager->GetController()->IsA<AMOSurvivorController>();
		if (!bAIControlled)
		{
			continue;
		}
		const UMOSurvivorJobQueueComponent* JobQueue = Villager->FindComponentByClass<UMOSurvivorJobQueueComponent>();
		if (!JobQueue)
		{
			continue;
		}
		const FMOSurvivorJobEntry Current = JobQueue->GetCurrentJob();
		if (Current.IsValid())
		{
			if (Current.JobType == EMOSurvivorJobType::CraftAtStation)
			{
				InFlight.Add(Current.CraftRecipeId);
			}
		}
		else
		{
			Idle.Add(Villager);
		}
	}
	if (Idle.Num() == 0)
	{
		return;
	}

	const TArray<FName> Assignments = DecideQuotaWork(Quotas, Stock, InFlight, Idle.Num());
	int32 IdleIndex = 0;
	for (const FName& RecipeId : Assignments)
	{
		APawn* Villager = Idle[IdleIndex++];
		UMOSurvivorJobQueueComponent* JobQueue = Villager->FindComponentByClass<UMOSurvivorJobQueueComponent>();

		// Nearest station/storage to the worker — same resolution the UI uses.
		AActor* Station = nullptr;
		float BestD = TNumericLimits<float>::Max();
		for (TActorIterator<AMOCraftingStationActor> It(World); It; ++It)
		{
			const float D = FVector::DistSquared(It->GetActorLocation(), Villager->GetActorLocation());
			if (IsValid(*It) && D < BestD) { Station = *It; BestD = D; }
		}
		AActor* Storage = Storages[0];
		BestD = TNumericLimits<float>::Max();
		for (AMOContainerActor* S : Storages)
		{
			const float D = FVector::DistSquared(S->GetActorLocation(), Villager->GetActorLocation());
			if (D < BestD) { Storage = S; BestD = D; }
		}
		if (!Station || !JobQueue)
		{
			continue;
		}
		const FGuid JobId = JobQueue->EnqueueCraftJob(RecipeId, Station, Storage, 1);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] Quota assigned: %s -> %s (ok=%d)"),
			*Villager->GetName(), *RecipeId.ToString(), JobId.IsValid() ? 1 : 0);
		if (JobId.IsValid())
		{
			if (UMOCharacterHistoryComponent* History = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
			{
				History->AddEntry(EHistoryEntryType::Activity,
					FString::Printf(TEXT("Took standing order: %s"), *RecipeId.ToString()));
			}
		}
	}
}

// ============================================================================
// SCHOOL / TEACHING (V2.2)
// ============================================================================

float UMOColonyManagerSubsystem::ComputeTeachXP(float GameHoursElapsed, float BaseXPPerGameHour)
{
	// Design pillar: being taught by a skilled pawn = 2x direct-action speed.
	return FMath::Max(GameHoursElapsed, 0.0f) * FMath::Max(BaseXPPerGameHour, 0.0f) * 2.0f;
}

void UMOColonyManagerSubsystem::RunSchoolPass(const TArray<APawn*>& Roster, float GameHoursElapsed)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Completed schools inside the settlement.
	TArray<AActor*> Schools;
	for (TActorIterator<AMOBuildableActor> It(World); It; ++It)
	{
		AMOBuildableActor* Building = *It;
		if (!IsValid(Building) || !IsInSettlement(Building->GetActorLocation()))
		{
			continue;
		}
		const FMORecipeDefinitionRow* Recipe = UMORecipeDatabaseSettings::GetRecipeDefinition(Building->GetRecipeId());
		if (!Recipe || !Recipe->bIsSchool)
		{
			continue;
		}
		const UMOBuildProgressComponent* Progress = Building->FindComponentByClass<UMOBuildProgressComponent>();
		if (Progress && Progress->GetState() == EMOBuildState::Complete)
		{
			Schools.Add(Building);
		}
	}

	// Who is IN school right now, and the maintenance flag for everyone.
	TArray<APawn*> AtSchool;
	for (APawn* Villager : Roster)
	{
		bool bInSchool = false;
		for (const AActor* School : Schools)
		{
			if (FVector::DistSquared2D(Villager->GetActorLocation(), School->GetActorLocation())
				<= FMath::Square(SchoolRadius))
			{
				bInSchool = true;
				break;
			}
		}
		if (UMOSkillsComponent* Skills = Villager->FindComponentByClass<UMOSkillsComponent>())
		{
			Skills->SetSkillMaintenance(bInSchool);
		}
		if (bInSchool)
		{
			AtSchool.Add(Villager);
		}
	}
	if (AtSchool.Num() < 2 || GameHoursElapsed <= 0.0f)
	{
		return;   // teaching needs a teacher AND a student, co-located
	}

	// Teaching: each student learns the best skill some co-located teacher
	// leads them in by the margin. Real time, real XP, real skills API.
	for (APawn* Student : AtSchool)
	{
		UMOSkillsComponent* StudentSkills = Student->FindComponentByClass<UMOSkillsComponent>();
		if (!StudentSkills)
		{
			continue;
		}
		FName BestSkill = NAME_None;
		int32 BestLead = 0;
		APawn* BestTeacher = nullptr;
		for (APawn* Teacher : AtSchool)
		{
			if (Teacher == Student)
			{
				continue;
			}
			const UMOSkillsComponent* TeacherSkills = Teacher->FindComponentByClass<UMOSkillsComponent>();
			if (!TeacherSkills)
			{
				continue;
			}
			for (const FMOSkillProgress& TS : TeacherSkills->Skills)
			{
				const int32 Lead = TS.Level - StudentSkills->GetSkillLevel(TS.SkillId);
				if (Lead >= TeachTeacherLevelMargin && Lead > BestLead)
				{
					BestLead = Lead;
					BestSkill = TS.SkillId;
					BestTeacher = Teacher;
				}
			}
		}
		if (BestSkill.IsNone() || !BestTeacher)
		{
			continue;
		}
		const float XP = ComputeTeachXP(GameHoursElapsed, TeachBaseXPPerGameHour);
		StudentSkills->AddExperience(BestSkill, XP);
		if (UMOCharacterHistoryComponent* History = Student->FindComponentByClass<UMOCharacterHistoryComponent>())
		{
			History->AddEntry(EHistoryEntryType::SkillGain,
				FString::Printf(TEXT("Taught %s by %s (+%.0f XP)"),
					*BestSkill.ToString(), *BestTeacher->GetName(), XP));
		}

		// Teaching forges a typed bond: mentor and student, both ways (V2.3).
		const FGuid StudentGuid = GetPawnGuid(Student);
		const FGuid TeacherGuid = GetPawnGuid(BestTeacher);
		if (StudentGuid.IsValid() && TeacherGuid.IsValid())
		{
			if (UMOCharacterHistoryComponent* StudentHistory = Student->FindComponentByClass<UMOCharacterHistoryComponent>())
			{
				if (StudentHistory->GetRelationship(TeacherGuid).RelationshipType == ERelationshipType::None)
				{
					StudentHistory->SetRelationshipType(TeacherGuid, ERelationshipType::Mentor);
				}
			}
			if (UMOCharacterHistoryComponent* TeacherHistory = BestTeacher->FindComponentByClass<UMOCharacterHistoryComponent>())
			{
				if (TeacherHistory->GetRelationship(StudentGuid).RelationshipType == ERelationshipType::None)
				{
					TeacherHistory->SetRelationshipType(StudentGuid, ERelationshipType::Student);
				}
			}
		}
	}
}

// =============================================================================
// RELATIONSHIPS / STANDING (V2.3)
// =============================================================================

void UMOColonyManagerSubsystem::RunRelationshipPass(const TArray<APawn*>& Roster, float GameHoursElapsed)
{
	UWorld* World = GetWorld();
	if (!World || GameHoursElapsed <= 0.0f)
	{
		return;
	}

	// Social circle = roster + wild recruitables lingering near any settler.
	// Strangers build standing by ACTUALLY spending time around the colony —
	// that standing is what the recruitment gate reads.
	TArray<APawn*> Social = Roster;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		if (!IsValid(Candidate) || Roster.Contains(Candidate))
		{
			continue;
		}
		const UMORecruitmentComponent* Recruit = Candidate->FindComponentByClass<UMORecruitmentComponent>();
		if (!Recruit || !Recruit->IsRecruitable() || Recruit->IsPossessable())
		{
			continue;
		}
		if (!Candidate->FindComponentByClass<UMOCharacterHistoryComponent>())
		{
			continue;
		}
		for (const APawn* Settler : Roster)
		{
			if (FVector::DistSquared2D(Candidate->GetActorLocation(), Settler->GetActorLocation())
				<= FMath::Square(FamiliarityRadius))
			{
				Social.Add(Candidate);
				break;
			}
		}
	}

	// Per pawn: who shared this slice of time with me?
	for (APawn* Pawn : Social)
	{
		UMOCharacterHistoryComponent* History = Pawn->FindComponentByClass<UMOCharacterHistoryComponent>();
		if (!History)
		{
			continue;
		}
		TSet<FGuid> CoLocated;
		for (APawn* Other : Social)
		{
			if (Other == Pawn)
			{
				continue;
			}
			const FGuid OtherGuid = GetPawnGuid(Other);
			if (!OtherGuid.IsValid())
			{
				continue;
			}
			if (FVector::DistSquared2D(Pawn->GetActorLocation(), Other->GetActorLocation())
				<= FMath::Square(FamiliarityRadius))
			{
				CoLocated.Add(OtherGuid);
				History->AddSharedTime(OtherGuid, GameHoursElapsed);
			}
		}
		History->ApplyApartDrift(CoLocated, GameHoursElapsed);
	}
}

float UMOColonyManagerSubsystem::GetColonyStanding(APawn* Candidate) const
{
	if (!Candidate)
	{
		return 0.0f;
	}
	const UMOCharacterHistoryComponent* History = Candidate->FindComponentByClass<UMOCharacterHistoryComponent>();
	if (!History)
	{
		return 0.0f;
	}
	const FGuid CandidateGuid = GetPawnGuid(Candidate);
	TArray<FGuid> RosterGuids;
	for (const APawn* Settler : GetColonyRoster())
	{
		const FGuid Guid = GetPawnGuid(Settler);
		if (Guid.IsValid() && Guid != CandidateGuid)
		{
			RosterGuids.Add(Guid);
		}
	}
	return History->GetAverageStandingWith(RosterGuids);
}

bool UMOColonyManagerSubsystem::CanRecruitByStanding(APawn* Candidate) const
{
	return Candidate && EvaluateRecruitGate(Candidate).bPasses;
}

UMOColonyManagerSubsystem::FStandingGate UMOColonyManagerSubsystem::EvaluateRecruitGate(APawn* Candidate) const
{
	FStandingGate Gate;
	Gate.Needed = RecruitStandingThreshold;
	if (!Candidate)
	{
		Gate.bPasses = false;
		return Gate;
	}
	// Bootstrap: a colony smaller than RecruitGateMinRoster (besides the candidate) has no standing to judge a stranger by.
	const FGuid CandidateGuid = GetPawnGuid(Candidate);
	int32 OthersOnRoster = 0;
	for (const APawn* Settler : GetColonyRoster())
	{
		const FGuid Guid = GetPawnGuid(Settler);
		if (Guid.IsValid() && Guid != CandidateGuid)
		{
			++OthersOnRoster;
		}
	}
	Gate.Standing = OthersOnRoster > 0 ? GetColonyStanding(Candidate) : 0.0f;
	if (!GateApplies(OthersOnRoster, RecruitGateMinRoster))
	{
		return Gate;   // bGateActive false, passes
	}
	Gate.bGateActive = true;
	Gate.bPasses = Gate.Standing >= Gate.Needed;
	Gate.HoursToPass = Gate.bPasses ? 0.0f : HoursToReachStanding(Gate.Standing, Gate.Needed);
	return Gate;
}

float UMOColonyManagerSubsystem::HoursToReachStanding(float Standing, float Needed)
{
	if (Standing >= Needed)
	{
		return 0.0f;
	}
	// Bisection on the bond function itself (never a re-derived formula that could drift from it).
	float Lo = 0.0f, Hi = 24.0f * 365.0f;
	for (int32 I = 0; I < 40; ++I)
	{
		const float Mid = 0.5f * (Lo + Hi);
		(UMOCharacterHistoryComponent::ComputeStrengthDelta(Standing, Mid, 0.0f) >= Needed ? Hi : Lo) = Mid;
	}
	return Hi;
}

bool UMOColonyManagerSubsystem::Marry(APawn* A, APawn* B)
{
	if (!A || !B || A == B)
	{
		return false;
	}
	UMOCharacterHistoryComponent* HistoryA = A->FindComponentByClass<UMOCharacterHistoryComponent>();
	UMOCharacterHistoryComponent* HistoryB = B->FindComponentByClass<UMOCharacterHistoryComponent>();
	const FGuid GuidA = GetPawnGuid(A);
	const FGuid GuidB = GetPawnGuid(B);
	if (!HistoryA || !HistoryB || !GuidA.IsValid() || !GuidB.IsValid())
	{
		return false;
	}
	HistoryA->SetRelationshipType(GuidB, ERelationshipType::Spouse);
	HistoryB->SetRelationshipType(GuidA, ERelationshipType::Spouse);
	HistoryA->AddEntry(EHistoryEntryType::RelationshipChange,
		FString::Printf(TEXT("Married %s"), *B->GetName()));
	HistoryB->AddEntry(EHistoryEntryType::RelationshipChange,
		FString::Printf(TEXT("Married %s"), *A->GetName()));
	return true;
}

bool UMOColonyManagerSubsystem::RecordParentage(APawn* Parent, APawn* Child)
{
	if (!Parent || !Child || Parent == Child)
	{
		return false;
	}
	UMOCharacterHistoryComponent* ParentHistory = Parent->FindComponentByClass<UMOCharacterHistoryComponent>();
	UMOCharacterHistoryComponent* ChildHistory = Child->FindComponentByClass<UMOCharacterHistoryComponent>();
	const FGuid ParentGuid = GetPawnGuid(Parent);
	const FGuid ChildGuid = GetPawnGuid(Child);
	if (!ParentHistory || !ChildHistory || !ParentGuid.IsValid() || !ChildGuid.IsValid())
	{
		return false;
	}
	ParentHistory->SetRelationshipType(ChildGuid, ERelationshipType::Child);
	ChildHistory->SetRelationshipType(ParentGuid, ERelationshipType::Parent);
	return true;
}

// ============================================================================
// UPKEEP
// ============================================================================

void UMOColonyManagerSubsystem::RunUpkeepTick()
{
	UWorld* World = GetWorld();
	if (!World || !Settlement.IsValid())
	{
		return;
	}

	// Game-hours since the last pass (time-scale aware) for duration effects.
	double NowGameSeconds = 0.0;
	if (UMOGameClockSubsystem* Clock = World->GetSubsystem<UMOGameClockSubsystem>())
	{
		NowGameSeconds = Clock->GetGameTimeSeconds();
	}
	const float GameHoursElapsed = (LastUpkeepGameSeconds >= 0.0)
		? static_cast<float>((NowGameSeconds - LastUpkeepGameSeconds) / 3600.0)
		: 0.0f;
	LastUpkeepGameSeconds = NowGameSeconds;

	const TArray<APawn*> Roster = GetColonyRoster();

	// The bleeding first (N-061): an idle hand dresses a wound before taking standing orders.
	RunMedicPass(Roster);

	// Standing orders next: idle hands pick up quota work this same pass.
	RunQuotaPass(Roster);

	// School: maintenance flags + teaching for co-located villagers (V2.2).
	RunSchoolPass(Roster, GameHoursElapsed);

	// Relationships: REAL shared time builds bonds; absence fades them (V2.3).
	RunRelationshipPass(Roster, GameHoursElapsed);

	// Family: courtship -> marriage -> children on real clocks (V2.5).
	RunFamilyPass(Roster, GameHoursElapsed, NowGameSeconds);

	// Winter shelter: cold villagers head home to warm up (V2.5).
	RunShelterPass(Roster);

	// Hearths: keep settlement fires fed — firewood demand is real (F1/T3).
	RunHearthPass(Roster);

	// Excavation: idle hands that are left work the designated dig zones (terraform unit 3, stage 4). Last on purpose: food, warmth
	// and fires come before moving earth.
	RunExcavationPass(Roster);

	for (APawn* Villager : Roster)
	{
		const FGuid PawnGuid = GetPawnGuid(Villager);

		// First-touch personality: villagers become PEOPLE when they join.
		if (UMOPersonalityComponent* Personality = Villager->FindComponentByClass<UMOPersonalityComponent>())
		{
			const FMOPersonalityTraits& T = Personality->GetTraits();
			if (T.Conscientiousness == 0.0f && T.Sociability == 0.0f && T.Stability == 0.0f)
			{
				Personality->RandomizeTraits();
			}
		}

		// EAT — only AI-controlled villagers self-feed; a possessed pawn is
		// the player's own stomach to manage.
		const bool bAIControlled = Villager->GetController()
			&& Villager->GetController()->IsA<AMOSurvivorController>();
		if (bAIControlled)
		{
			TryFeedVillager(Villager, PawnGuid);
		}

		// HOUSING clock.
		const bool bHasHome = Residency.Contains(PawnGuid);
		if (!bHasHome)
		{
			VillagerUnhousedHours.FindOrAdd(PawnGuid) += GameHoursElapsed;
		}
		else
		{
			VillagerUnhousedHours.Remove(PawnGuid);
		}

		// MOOD from the real sims.
		FMOVillagerMoodInputs In;
		if (const UMOMetabolismComponent* Metab = Villager->FindComponentByClass<UMOMetabolismComponent>())
		{
			In.bStarving = Metab->IsStarving();
			In.bDehydrated = Metab->IsDehydrated();
		}
		if (const UMOVitalsComponent* Vitals = Villager->FindComponentByClass<UMOVitalsComponent>())
		{
			In.Wetness = Vitals->GetWetnessLevel();
		}
		if (const UMOMentalStateComponent* Mental = Villager->FindComponentByClass<UMOMentalStateComponent>())
		{
			const FMOMentalState& MS = Mental->GetMentalState();
			In.Shock = MS.ShockAccumulation;
			In.TraumaticStress = MS.TraumaticStress;
			In.MoraleFatigue = MS.MoraleFatigue;
		}
		if (const UMOPersonalityComponent* Personality = Villager->FindComponentByClass<UMOPersonalityComponent>())
		{
			In.MoodVarianceModifier = Personality->GetMoodVarianceModifier();
		}
		In.bHasHome = bHasHome;
		In.UnhousedHours = VillagerUnhousedHours.FindRef(PawnGuid);

		const float NewMood = ComputeVillagerMood(In);
		const float OldMood = VillagerMood.FindRef(PawnGuid);
		VillagerMood.Add(PawnGuid, NewMood);

		// History + broadcast on quarter-bucket transitions only (no spam).
		if (FMath::FloorToInt(OldMood * 4.0f) != FMath::FloorToInt(NewMood * 4.0f))
		{
			OnVillagerMoodChanged.Broadcast(PawnGuid, NewMood);
			if (UMOCharacterHistoryComponent* History = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
			{
				History->AddEntry(EHistoryEntryType::MoodChange,
					FString::Printf(TEXT("Mood %s (%.2f)"),
						NewMood > OldMood ? TEXT("improved") : TEXT("worsened"), NewMood));
			}
		}
	}
}

void UMOColonyManagerSubsystem::TryFeedVillager(APawn* Villager, const FGuid& PawnGuid)
{
	UMOMetabolismComponent* Metab = Villager->FindComponentByClass<UMOMetabolismComponent>();
	if (!Metab)
	{
		return;
	}
	if (!Metab->IsStarving() && Metab->GetCalorieBalance() > EatCalorieDeficitThreshold)
	{
		return;   // not hungry — real appetite, not a schedule
	}

	FName FoodItemId;
	AMOContainerActor* Storage = FindCommunalFood(FoodItemId);
	if (!Storage || FoodItemId.IsNone())
	{
		return;   // empty larder: the metabolism sim keeps running down — that IS the failure state
	}

	FMOItemDefinitionRow FoodDef;
	if (!UMOItemDatabaseSettings::GetItemDefinition(FoodItemId, FoodDef))
	{
		return;
	}
	UMOInventoryComponent* StorageInv = Storage->FindComponentByClass<UMOInventoryComponent>();
	if (!StorageInv || !StorageInv->RemoveItemByDefinitionId(FoodItemId, 1))
	{
		return;
	}

	// The REAL eating path — same digestion the player gets.
	Metab->ConsumeFood(FoodDef.Nutrition, FoodItemId);

	if (UMOCharacterHistoryComponent* History = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
	{
		History->AddEntry(EHistoryEntryType::Activity,
			FString::Printf(TEXT("Ate %s from communal storage"), *FoodItemId.ToString()));
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] %s ate 1x %s from %s"),
		*Villager->GetName(), *FoodItemId.ToString(), *Storage->GetName());
}

AMOContainerActor* UMOColonyManagerSubsystem::FindCommunalFood(FName& OutItemId) const
{
	OutItemId = NAME_None;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AMOContainerActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		AMOContainerActor* Container = *It;
		if (!IsValid(Container) || !IsInSettlement(Container->GetActorLocation()))
		{
			continue;
		}
		const UMOInventoryComponent* Inv = Container->FindComponentByClass<UMOInventoryComponent>();
		if (!Inv)
		{
			continue;
		}
		// First item with real calories wins (menu planning is a V2 nicety).
		TArray<FMOInventoryEntry> Items;
		Inv->GetInventoryEntries(Items);
		for (const FMOInventoryEntry& Item : Items)
		{
			FMOItemDefinitionRow Def;
			if (UMOItemDatabaseSettings::GetItemDefinition(Item.ItemDefinitionId, Def)
				&& Def.Nutrition.Calories > 0.0f)
			{
				OutItemId = Item.ItemDefinitionId;
				return Container;
			}
		}
	}
	return nullptr;
}

FGuid UMOColonyManagerSubsystem::GetPawnGuid(const APawn* Pawn)
{
	const UMOIdentityComponent* Id = Pawn ? Pawn->FindComponentByClass<UMOIdentityComponent>() : nullptr;
	return Id ? Id->GetGuid() : FGuid();
}

// ============================================================================
// SAVE / LOAD
// ============================================================================

FMOColonySaveData UMOColonyManagerSubsystem::BuildSaveData() const
{
	FMOColonySaveData Data;
	Data.Settlement = Settlement;
	Data.Quotas = Quotas;
	Data.Pregnancies = Pregnancies;
	Data.Residency = Residency;
	Data.VillagerMood = VillagerMood;
	Data.VillagerUnhousedHours = VillagerUnhousedHours;
	for (APawn* Villager : GetColonyRoster())
	{
		const FGuid PawnGuid = GetPawnGuid(Villager);
		if (const UMOCharacterHistoryComponent* History = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
		{
			FMOCharacterHistorySaveData HistoryData;
			History->BuildSaveData(HistoryData);
			Data.VillagerHistory.Add(PawnGuid, HistoryData);
		}
	}
	Data.bHasValidData = Settlement.IsValid() || Residency.Num() > 0;
	return Data;
}

bool UMOColonyManagerSubsystem::ApplySaveDataAuthority(const FMOColonySaveData& InData)
{
	if (!InData.bHasValidData)
	{
		return false;
	}
	Settlement = InData.Settlement;
	Quotas = InData.Quotas;
	Pregnancies = InData.Pregnancies;
	Residency = InData.Residency;
	VillagerMood = InData.VillagerMood;
	VillagerUnhousedHours = InData.VillagerUnhousedHours;
	// History replays onto the components once pawns are respawned; the roster
	// walk finds them by GUID.
	for (APawn* Villager : GetColonyRoster())
	{
		const FGuid PawnGuid = GetPawnGuid(Villager);
		if (const FMOCharacterHistorySaveData* HistoryData = InData.VillagerHistory.Find(PawnGuid))
		{
			if (UMOCharacterHistoryComponent* History = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
			{
				History->ApplySaveDataAuthority(*HistoryData);
			}
		}
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] Restored settlement '%s' (%d residents housed)"),
		*Settlement.DisplayName.ToString(), Residency.Num());
	return true;
}

// ---- IMOSaveDomain (Colony) — see MOSaveDomainInterface.h ----

void UMOColonyManagerSubsystem::CaptureSaveDomain(UMOWorldSaveGame& Save)
{
	Save.ColonyData = BuildSaveData();
}

void UMOColonyManagerSubsystem::ApplySaveDomain(const UMOWorldSaveGame& Save)
{
	ApplySaveDataAuthority(Save.ColonyData);
}

// =============================================================================
// FAMILY / DYNASTY (V2.5)
// =============================================================================

bool UMOColonyManagerSubsystem::ShouldBecomeRomantic(float StrengthAB, float StrengthBA, float MutualThreshold)
{
	return StrengthAB >= MutualThreshold && StrengthBA >= MutualThreshold;
}

float UMOColonyManagerSubsystem::ComputePregnancyProgress(double ConceivedGameSeconds, double NowGameSeconds, float InGestationGameHours)
{
	if (InGestationGameHours <= 0.0f || NowGameSeconds <= ConceivedGameSeconds)
	{
		return 0.0f;
	}
	return static_cast<float>((NowGameSeconds - ConceivedGameSeconds) / (InGestationGameHours * 3600.0));
}

void UMOColonyManagerSubsystem::RunFamilyPass(const TArray<APawn*>& Roster, float GameHoursElapsed, double NowGameSeconds)
{
	UWorld* World = GetWorld();
	if (!World || GameHoursElapsed <= 0.0f)
	{
		return;
	}

	// Pairwise pass over co-located roster villagers.
	for (int32 i = 0; i < Roster.Num(); ++i)
	{
		APawn* A = Roster[i];
		const FGuid GuidA = GetPawnGuid(A);
		UMOCharacterHistoryComponent* HistA = A->FindComponentByClass<UMOCharacterHistoryComponent>();
		if (!GuidA.IsValid() || !HistA)
		{
			continue;
		}
		for (int32 j = i + 1; j < Roster.Num(); ++j)
		{
			APawn* B = Roster[j];
			const FGuid GuidB = GetPawnGuid(B);
			UMOCharacterHistoryComponent* HistB = B->FindComponentByClass<UMOCharacterHistoryComponent>();
			if (!GuidB.IsValid() || !HistB)
			{
				continue;
			}
			if (FVector::DistSquared2D(A->GetActorLocation(), B->GetActorLocation())
				> FMath::Square(FamiliarityRadius))
			{
				continue;
			}
			FMOCharacterRelationship RelAB = HistA->GetRelationship(GuidB);
			FMOCharacterRelationship RelBA = HistB->GetRelationship(GuidA);

			// Spouses skip courtship but proceed to conception below; blood
			// family (parent/child) skips this pair entirely.
			if (RelAB.RelationshipType == ERelationshipType::Parent || RelAB.RelationshipType == ERelationshipType::Child
				|| RelBA.RelationshipType == ERelationshipType::Parent || RelBA.RelationshipType == ERelationshipType::Child)
			{
				continue;
			}
			auto IsMarried = [](const UMOCharacterHistoryComponent* Hist)
			{
				for (const FMOCharacterRelationship& R : Hist->GetRelationships())
				{
					if (R.RelationshipType == ERelationshipType::Spouse)
					{
						return true;
					}
				}
				return false;
			};

			// COURTSHIP: strong mutual bond, both single -> Romantic.
			const bool bTypedFree =
				(RelAB.RelationshipType == ERelationshipType::None || RelAB.RelationshipType == ERelationshipType::Friend) &&
				(RelBA.RelationshipType == ERelationshipType::None || RelBA.RelationshipType == ERelationshipType::Friend);
			if (bTypedFree && !IsMarried(HistA) && !IsMarried(HistB)
				&& ShouldBecomeRomantic(RelAB.Strength, RelBA.Strength, RomanceMutualStrengthThreshold))
			{
				HistA->SetRelationshipType(GuidB, ERelationshipType::Romantic);
				HistB->SetRelationshipType(GuidA, ERelationshipType::Romantic);
				HistA->StampRomantic(GuidB, NowGameSeconds);
				HistB->StampRomantic(GuidA, NowGameSeconds);
				HistA->AddEntry(EHistoryEntryType::RelationshipChange,
					FString::Printf(TEXT("Fell for %s"), *B->GetName()));
				HistB->AddEntry(EHistoryEntryType::RelationshipChange,
					FString::Printf(TEXT("Fell for %s"), *A->GetName()));
				continue;
			}

			// MARRIAGE: romance that lasted the courtship clock.
			if (RelAB.RelationshipType == ERelationshipType::Romantic
				&& RelBA.RelationshipType == ERelationshipType::Romantic
				&& RelAB.RomanticSinceGameSeconds >= 0.0
				&& (NowGameSeconds - RelAB.RomanticSinceGameSeconds) >= MarryAfterRomanticGameHours * 3600.0)
			{
				Marry(A, B);
				continue;
			}

			// CONCEPTION: married, sharing a residence, neither expecting.
			if (RelAB.RelationshipType == ERelationshipType::Spouse
				&& Residency.Contains(GuidA) && Residency.Contains(GuidB)
				&& Residency[GuidA] == Residency[GuidB]
				&& !Pregnancies.Contains(GuidA) && !Pregnancies.Contains(GuidB))
			{
				const float Chance = ConceptionChancePerGameDay * (GameHoursElapsed / 24.0f);
				if (FMath::FRand() < Chance)
				{
					// No biological-sex model: the lower GUID carries (abstraction).
					const bool bALower = GuidA < GuidB;
					const FGuid MotherGuid = bALower ? GuidA : GuidB;
					const FGuid FatherGuid = bALower ? GuidB : GuidA;
					FMOPregnancy P;
					P.FatherGuid = FatherGuid;
					P.ConceivedGameSeconds = NowGameSeconds;
					Pregnancies.Add(MotherGuid, P);
					(bALower ? HistA : HistB)->AddEntry(EHistoryEntryType::RelationshipChange, TEXT("Expecting a child"));
					UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] Conception: mother=%s father=%s"),
						*MotherGuid.ToString(EGuidFormats::Short), *FatherGuid.ToString(EGuidFormats::Short));
				}
			}
		}
	}

	// BIRTHS: gestation complete -> a real new villager.
	TArray<FGuid> Due;
	for (const TPair<FGuid, FMOPregnancy>& Pair : Pregnancies)
	{
		if (ComputePregnancyProgress(Pair.Value.ConceivedGameSeconds, NowGameSeconds, GestationGameHours) >= 1.0f)
		{
			Due.Add(Pair.Key);
		}
	}
	for (const FGuid& MotherGuid : Due)
	{
		APawn* Mother = nullptr;
		for (APawn* Villager : Roster)
		{
			if (GetPawnGuid(Villager) == MotherGuid)
			{
				Mother = Villager;
				break;
			}
		}
		if (Mother)
		{
			SpawnChild(Mother, MotherGuid, Pregnancies[MotherGuid]);
		}
		Pregnancies.Remove(MotherGuid);
	}
}

void UMOColonyManagerSubsystem::SpawnChild(APawn* Mother, const FGuid& MotherGuid, const FMOPregnancy& Pregnancy)
{
	UWorld* World = GetWorld();
	if (!World || !Mother)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const FVector Loc = Mother->GetActorLocation() + Mother->GetActorForwardVector() * 120.0f;
	APawn* Child = World->SpawnActor<APawn>(Mother->GetClass(), Loc, Mother->GetActorRotation(), Params);
	if (!Child)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] Birth FAILED to spawn child pawn"));
		return;
	}
	if (UMOIdentityComponent* Identity = Child->FindComponentByClass<UMOIdentityComponent>())
	{
		Identity->GetOrCreateGuid();
	}
	if (UMORecruitmentComponent* Recruit = Child->FindComponentByClass<UMORecruitmentComponent>())
	{
		Recruit->ForceRecruit();   // a child of the colony is OF the colony
	}
	RecordParentage(Mother, Child);
	APawn* Father = nullptr;
	for (APawn* Villager : GetColonyRoster())
	{
		if (GetPawnGuid(Villager) == Pregnancy.FatherGuid)
		{
			Father = Villager;
			break;
		}
	}
	if (Father)
	{
		RecordParentage(Father, Child);
	}
	if (UMOCharacterHistoryComponent* Hist = Mother->FindComponentByClass<UMOCharacterHistoryComponent>())
	{
		Hist->AddEntry(EHistoryEntryType::RelationshipChange,
			FString::Printf(TEXT("Gave birth to %s"), *Child->GetName()));
	}
	UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] BIRTH: %s born to mother=%s father=%s"),
		*Child->GetName(), *MotherGuid.ToString(EGuidFormats::Short),
		*Pregnancy.FatherGuid.ToString(EGuidFormats::Short));
	// NOTE: the child spawns as an adult-sized villager — visual childhood
	// (scaled meshes, growth) is an art/animation problem, flagged for V3+.
}

// =============================================================================
// WINTER SHELTER (V2.5)
// =============================================================================

void UMOColonyManagerSubsystem::RunShelterPass(const TArray<APawn*>& Roster)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UMOIdentityRegistrySubsystem* Registry = World->GetSubsystem<UMOIdentityRegistrySubsystem>();
	if (!Registry)
	{
		return;
	}
	for (APawn* Villager : Roster)
	{
		const FGuid PawnGuid = GetPawnGuid(Villager);
		AMOSurvivorController* AI = Cast<AMOSurvivorController>(Villager->GetController());
		const UMOVitalsComponent* Vitals = Villager->FindComponentByClass<UMOVitalsComponent>();
		if (!PawnGuid.IsValid() || !AI || !Vitals)
		{
			continue;
		}
		const float BodyTemp = Vitals->GetVitalSigns().BodyTemperature;
		const bool bSheltering = ShelteringVillagers.Contains(PawnGuid);
		if (!bSheltering && BodyTemp < ColdSeekShelterBodyTempC)
		{
			// Warm-up target, best first: a LIT hearth beats a cold doorstep,
			// and it's the only recourse a homeless villager has. Falls back
			// to the assigned house for residents when nothing burns nearby.
			TOptional<FVector> WarmTarget;
			const TCHAR* WhereWarm = TEXT("by the fire");
			if (const UMOAmbientEnvironmentRegistry* Ambient = UMOAmbientEnvironmentRegistry::Get(World))
			{
				float BestDistSq = FMath::Square(HearthSeekRadiusCm);
				for (const IMOLocalHeatSource* Fire : Ambient->GetHeatSources())
				{
					if (!Fire || !Fire->IsHeatActive())
					{
						continue;
					}
					const float DistSq = FVector::DistSquared(Villager->GetActorLocation(), Fire->GetHeatLocation());
					if (DistSq < BestDistSq)
					{
						BestDistSq = DistSq;
						// Stand at arm's length on the villager's side of the
						// fire — deep in the heat falloff, outside the pit.
						// Nav projection alone can land 3m+ out, where a
						// winter night out-pulls the warmth.
						const FVector Toward = (Villager->GetActorLocation() - Fire->GetHeatLocation()).GetSafeNormal2D();
						WarmTarget = Fire->GetHeatLocation() + Toward * 140.0f;
					}
				}
			}
			if (!WarmTarget.IsSet() && Residency.Contains(PawnGuid))
			{
				if (const AActor* House = Registry->ResolveActorOrNull(Residency[PawnGuid]))
				{
					WarmTarget = House->GetActorLocation();
					WhereWarm = TEXT("at home");
				}
			}
			if (WarmTarget.IsSet())
			{
				// DOORSTEP, not doorway: a point inside the target's hull
				// wedges the pawn in collision (three overlapping lean-tos
				// bricked an entire soak, 2026-07-07). Project to navigable
				// ground beside it; fall back to a generous offset.
				FVector StayPoint = WarmTarget.GetValue() + FVector(300.0f, 300.0f, 0.0f);
				if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
				{
					FNavLocation Projected;
					if (Nav->ProjectPointToNavigation(WarmTarget.GetValue(), Projected, FVector(500.0f, 500.0f, 300.0f)))
					{
						StayPoint = Projected.Location;
					}
				}
				// A stay ORDER suspends job processing — an in-flight job would
				// wedge forever (and its quota slot with it). Abort cleanly:
				// too cold to work is an honest job failure; the quota pass
				// reassigns once they have warmed back up.
				AI->AbortCurrentJob();
				AI->SetStayAtLocation(StayPoint);
				ShelteringVillagers.Add(PawnGuid);
				if (UMOCharacterHistoryComponent* Hist = Villager->FindComponentByClass<UMOCharacterHistoryComponent>())
				{
					Hist->AddEntry(EHistoryEntryType::Activity,
						FString::Printf(TEXT("Went to warm up %s (%.1fC)"), WhereWarm, BodyTemp));
				}
			}
		}
		else if (bSheltering && BodyTemp >= ColdShelterReleaseBodyTempC)
		{
			AI->ClearStayLocation();
			ShelteringVillagers.Remove(PawnGuid);
		}
	}
}

void UMOColonyManagerSubsystem::RunMedicPass(const TArray<APawn*>& Roster)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Patients: anyone in the settlement (villager, player, guest) with a wound bleeding enough to need a dressing, worst first.
	TArray<TPair<float, AMOCharacter*>> Patients;
	for (TActorIterator<AMOCharacter> It(World); It; ++It)
	{
		AMOCharacter* Patient = *It;
		// People only: an animal is an AMOCharacter too, and a villager does not walk up to a wounded wolf with a bandage.
		if (!IsValid(Patient) || Patient->IsDead() || Patient->IsA<AMOCreature>() || !IsInSettlement(Patient->GetActorLocation()))
		{
			continue;
		}
		const UMOAnatomyComponent* Anatomy = Patient->FindComponentByClass<UMOAnatomyComponent>();
		float Worst = 0.0f;
		if (Anatomy)
		{
			for (const FMOWound& Wound : Anatomy->GetAllWounds())
			{
				Worst = FMath::Max(Worst, Wound.BleedRate);
			}
		}
		if (Worst >= MedicBleedThresholdMlPerSecond)
		{
			Patients.Emplace(Worst, Patient);
		}
	}
	if (Patients.Num() == 0)
	{
		return;
	}
	Patients.Sort([](const TPair<float, AMOCharacter*>& A, const TPair<float, AMOCharacter*>& B) { return A.Key > B.Key; });

	// Idle villagers + patients already being tended (one medic per patient).
	TArray<APawn*> Idle;
	TSet<const AActor*> Tended;
	for (APawn* Villager : Roster)
	{
		if (!Villager->GetController() || !Villager->GetController()->IsA<AMOSurvivorController>())
		{
			continue;
		}
		const UMOSurvivorJobQueueComponent* JobQueue = Villager->FindComponentByClass<UMOSurvivorJobQueueComponent>();
		if (!JobQueue)
		{
			continue;
		}
		const FMOSurvivorJobEntry Current = JobQueue->GetCurrentJob();
		if (Current.IsValid())
		{
			if (Current.JobType == EMOSurvivorJobType::TreatWound && Current.TargetActor.IsValid())
			{
				Tended.Add(Current.TargetActor.Get());
			}
		}
		else if (!ShelteringVillagers.Contains(GetPawnGuid(Villager)))
		{
			Idle.Add(Villager);   // a villager sheltering from the cold is not idle; the shelter pass owns them
		}
	}

	// Settlement stores and what they hold: a medic with an empty pack fetches the dressing first.
	TArray<TPair<AMOContainerActor*, TArray<FMOInventoryEntry>>> Stores;
	for (TActorIterator<AMOContainerActor> It(World); It; ++It)
	{
		const UMOInventoryComponent* Inv = IsValid(*It) && IsInSettlement(It->GetActorLocation()) ? It->FindComponentByClass<UMOInventoryComponent>() : nullptr;
		if (Inv)
		{
			auto& Store = Stores.AddDefaulted_GetRef();
			Store.Key = *It;
			Inv->GetInventoryEntries(Store.Value);
		}
	}

	for (const TPair<float, AMOCharacter*>& Entry : Patients)
	{
		AMOCharacter* Patient = Entry.Value;
		if (Idle.Num() == 0)
		{
			break;
		}
		if (Tended.Contains(Patient))
		{
			continue;
		}
		// The nearest idle hand that can do it; tending one's own wound comes last (self-treatment is worse and can't reach everywhere).
		const FVector At = Patient->GetActorLocation();
		Idle.Sort([&At, Patient](const APawn& A, const APawn& B)
		{
			const double DA = &A == Patient ? TNumericLimits<double>::Max() : FVector::DistSquared(A.GetActorLocation(), At);
			const double DB = &B == Patient ? TNumericLimits<double>::Max() : FVector::DistSquared(B.GetActorLocation(), At);
			return DA < DB;
		});
		Stores.Sort([&At](const TPair<AMOContainerActor*, TArray<FMOInventoryEntry>>& A, const TPair<AMOContainerActor*, TArray<FMOInventoryEntry>>& B)
		{
			return FVector::DistSquared(A.Key->GetActorLocation(), At) < FVector::DistSquared(B.Key->GetActorLocation(), At);
		});
		for (int32 Index = 0; Index < Idle.Num(); ++Index)
		{
			APawn* Medic = Idle[Index];
			FGuid WoundId;
			FName TreatmentId;
			AMOContainerActor* From = nullptr;
			bool bCan = FMOMedicalTreatmentActions::PickForMedic(Medic, Patient, MedicBleedThresholdMlPerSecond, {}, WoundId, TreatmentId);
			for (int32 S = 0; !bCan && S < Stores.Num(); ++S)
			{
				bCan = FMOMedicalTreatmentActions::PickForMedic(Medic, Patient, MedicBleedThresholdMlPerSecond, Stores[S].Value, WoundId, TreatmentId);
				From = bCan ? Stores[S].Key : nullptr;
			}
			if (!bCan)
			{
				continue;
			}
			UMOSurvivorJobQueueComponent* JobQueue = Medic->FindComponentByClass<UMOSurvivorJobQueueComponent>();
			if (JobQueue && JobQueue->EnqueueTreatJob(Patient, WoundId, TreatmentId, From).IsValid())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOColony] Medic %s goes to treat %s (%s, bleeding %.2f mL/s)%s"),
					*Medic->GetName(), *Patient->GetName(), *TreatmentId.ToString(), Entry.Key,
					From ? *FString::Printf(TEXT(", supplies from %s"), *From->GetName()) : TEXT(""));
				Idle.RemoveAt(Index);
				Tended.Add(Patient);
			}
			break;
		}
		if (!Tended.Contains(Patient))
		{
			UE_LOG(LogMOFramework, Log, TEXT("[MOColony] %s is bleeding %.2f mL/s but no idle villager has the skill and supplies to treat it"),
				*Patient->GetName(), Entry.Key);
		}
	}
}

void UMOColonyManagerSubsystem::RunHearthPass(const TArray<APawn*>& Roster)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// Settlement fuel-burning heat stations running low — includes burned-out
	// fires (a refueling villager relights them). Iterate stations directly:
	// the hearth pass tends STATIONS; the heat-source interface is for
	// thermal queries, and a plain C++ base can't be downcast under UE's
	// no-RTTI builds anyway.
	TArray<AMOCraftingStationActor*> Hungry;
	for (TActorIterator<AMOCraftingStationActor> It(World); It; ++It)
	{
		AMOCraftingStationActor* Station = *It;
		if (!IsValid(Station) || !Station->bRequiresFuel || Station->HeatOutputCelsius <= 0.0f)
		{
			continue;
		}
		if (!IsInSettlement(Station->GetActorLocation()))
		{
			continue;
		}
		if (Station->GetFuelPercent() <= HearthRefuelBelowPercent)
		{
			Hungry.Add(Station);
		}
	}
	if (Hungry.Num() == 0)
	{
		return;
	}

	// Idle villagers + stations already being tended (one tender per fire).
	TArray<APawn*> Idle;
	TSet<const AActor*> Tended;
	for (APawn* Villager : Roster)
	{
		if (!Villager->GetController() || !Villager->GetController()->IsA<AMOSurvivorController>())
		{
			continue;
		}
		if (ShelteringVillagers.Contains(GetPawnGuid(Villager)))
		{
			continue; // too cold to work — the shelter pass owns them
		}
		const UMOSurvivorJobQueueComponent* JobQueue = Villager->FindComponentByClass<UMOSurvivorJobQueueComponent>();
		if (!JobQueue)
		{
			continue;
		}
		const FMOSurvivorJobEntry Current = JobQueue->GetCurrentJob();
		if (Current.IsValid())
		{
			if (Current.JobType == EMOSurvivorJobType::RefuelStation && Current.TargetActor.IsValid())
			{
				Tended.Add(Current.TargetActor.Get());
			}
		}
		else
		{
			Idle.Add(Villager);
		}
	}

	int32 IdleIndex = 0;
	for (AMOCraftingStationActor* Station : Hungry)
	{
		if (IdleIndex >= Idle.Num())
		{
			break;
		}
		if (Tended.Contains(Station))
		{
			continue;
		}

		// Fuel source: nearest settlement container actually holding an item
		// the station accepts. No fuel in storage = an honest unmet demand
		// (the colony log will want this once B1 reasons land).
		AMOContainerActor* Storage = nullptr;
		FName FuelItem = NAME_None;
		float BestD = TNumericLimits<float>::Max();
		for (TActorIterator<AMOContainerActor> It(World); It; ++It)
		{
			if (!IsValid(*It) || !IsInSettlement(It->GetActorLocation()))
			{
				continue;
			}
			const UMOInventoryComponent* Inv = It->FindComponentByClass<UMOInventoryComponent>();
			if (!Inv)
			{
				continue;
			}
			for (const FName& Accepted : Station->AcceptedFuelItems)
			{
				if (Inv->GetItemCountByDefinitionId(Accepted) > 0)
				{
					const float D = FVector::DistSquared(It->GetActorLocation(), Station->GetActorLocation());
					if (D < BestD)
					{
						BestD = D;
						Storage = *It;
						FuelItem = Accepted;
					}
					break;
				}
			}
		}
		if (!Storage)
		{
			UE_LOG(LogMOFramework, Log,
				TEXT("[MOColony] Hearth %s is low (%.0f%%) but no settlement storage holds accepted fuel"),
				*Station->GetName(), Station->GetFuelPercent() * 100.0f);
			continue;
		}

		APawn* Tender = Idle[IdleIndex++];
		if (UMOSurvivorJobQueueComponent* JobQueue = Tender->FindComponentByClass<UMOSurvivorJobQueueComponent>())
		{
			JobQueue->EnqueueRefuelJob(Station, Storage, FuelItem, HearthFuelWithdrawCount);
			if (UMOCharacterHistoryComponent* Hist = Tender->FindComponentByClass<UMOCharacterHistoryComponent>())
			{
				Hist->AddEntry(EHistoryEntryType::Activity,
					FString::Printf(TEXT("Went to tend the fire at %s"), *Station->GetName()));
			}
		}
	}
}

// ============================================================================
// EXCAVATION (terraform unit 3, stage 4)
// ============================================================================

TArray<FMOExcavationAssignment> UMOColonyManagerSubsystem::DecideExcavationWork(const TArray<FMODesignationZone>& Zones,
	const TSet<FGuid>& ClaimedDigZones, int32 IdleVillagers, bool bHaveContainer, const TSet<FGuid>& FlattenZonesWithWork)
{
	TArray<FMOExcavationAssignment> Work;
	const auto HasRoom = [](const FMODesignationZone& Z) { return Z.IsValidZone() && Z.Kind == EMODesignationKind::Dump && Z.RemainingVolumeM3 > 0.0f; };
	for (const FMODesignationZone& Dig : Zones)
	{
		if (Work.Num() >= IdleVillagers)
		{
			break;
		}
		if (Dig.Kind == EMODesignationKind::Flatten)
		{
			// Its own source and sink: high ground to low ground inside the zone (conservation by construction).
			if (Dig.IsValidZone() && Dig.RemainingVolumeM3 > 0.0f && FlattenZonesWithWork.Contains(Dig.ZoneId) && !ClaimedDigZones.Contains(Dig.ZoneId))
			{
				FMOExcavationAssignment A;
				A.DigZoneId = Dig.ZoneId;
				A.DumpMode = EMOExcavateDumpMode::FillZone;
				A.DumpZoneId = Dig.ZoneId;
				Work.Add(A);
			}
			continue;
		}
		if (!Dig.IsValidZone() || Dig.Kind != EMODesignationKind::Dig || Dig.RemainingVolumeM3 <= 0.0f || ClaimedDigZones.Contains(Dig.ZoneId))
		{
			continue;   // not a dig zone, finished, or someone is already on it: one worker per zone
		}
		FMOExcavationAssignment A;
		A.DigZoneId = Dig.ZoneId;

		// 1. The zone's own default target, when it is a fill zone with room.
		const FMODesignationZone* Fill = Dig.DefaultDumpTargetGuid.IsValid()
			? Zones.FindByPredicate([&](const FMODesignationZone& Z) { return Z.ZoneId == Dig.DefaultDumpTargetGuid; }) : nullptr;
		const bool bDefaultIsZone = Fill != nullptr;
		if (Fill && !HasRoom(*Fill))
		{
			Fill = nullptr;
		}
		// 2. Else the nearest fill zone with room.
		if (!Fill)
		{
			double BestD = TNumericLimits<double>::Max();
			for (const FMODesignationZone& Z : Zones)
			{
				const double D = FVector::DistSquared(Z.Center, Dig.Center);
				if (HasRoom(Z) && D < BestD)
				{
					BestD = D;
					Fill = &Z;
				}
			}
		}
		if (Fill)
		{
			A.DumpMode = EMOExcavateDumpMode::FillZone;
			A.DumpZoneId = Fill->ZoneId;
		}
		// 3. Else a container: the default target when it names one (resolved by the caller), else any settlement container.
		else if (Dig.DefaultDumpTargetGuid.IsValid() && !bDefaultIsZone)
		{
			A.DumpMode = EMOExcavateDumpMode::Container;
			A.DumpContainerGuid = Dig.DefaultDumpTargetGuid;
		}
		else if (bHaveContainer)
		{
			A.DumpMode = EMOExcavateDumpMode::Container;
		}
		else
		{
			continue;   // nowhere to put the earth: the zone waits
		}
		Work.Add(A);
	}
	return Work;
}

void UMOColonyManagerSubsystem::RunExcavationPass(const TArray<APawn*>& Roster)
{
	UWorld* World = GetWorld();
	UMODesignationSubsystem* Desig = UMODesignationSubsystem::Get(this);
	if (!World || !Desig || Desig->GetZoneCount() == 0)
	{
		return;
	}
	// Settlement-scoped (locked design): a zone outside the settlement is not colony work. The live ground decides two things the zone
	// records cannot: a fill zone already at its target height is full, and a flatten zone has work while some ground is off target by more
	// than the tolerance AND there is ground on both sides of the target to move (excess or missing earth would need an outside source or
	// sink: it waits).
	TArray<FMODesignationZone> Zones;
	TSet<FGuid> FlattenWithWork;
	for (const FMODesignationZone& Z : Desig->GetZones())
	{
		if (!IsInSettlement(Z.Center))
		{
			continue;
		}
		FMODesignationZone Copy = Z;
		FVector P;
		int32 Samples = 0;
		if (Z.Kind == EMODesignationKind::Dump && !Desig->FindBitePoint(Z, /*bDig=*/false, P, Samples) && Samples > 0)
		{
			Copy.RemainingVolumeM3 = 0.0f;   // at its target: no room, whatever its budget says
		}
		else if (Z.Kind == EMODesignationKind::Flatten)
		{
			FVector Fill;
			if (Desig->FindFlattenWork(Z, P, Fill))
			{
				FlattenWithWork.Add(Z.ZoneId);
			}
		}
		Zones.Add(Copy);
	}
	if (FlattenWithWork.Num() == 0 && !Zones.ContainsByPredicate([](const FMODesignationZone& Z) { return Z.Kind == EMODesignationKind::Dig && Z.RemainingVolumeM3 > 0.0f; }))
	{
		return;
	}

	// Idle AI villagers, and every dig zone a queued or running trip already holds (one worker per zone).
	TArray<APawn*> Idle;
	TSet<FGuid> Claimed;
	for (APawn* Villager : Roster)
	{
		if (!Villager->GetController() || !Villager->GetController()->IsA<AMOSurvivorController>())
		{
			continue;
		}
		const UMOSurvivorJobQueueComponent* JobQueue = Villager->FindComponentByClass<UMOSurvivorJobQueueComponent>();
		if (!JobQueue)
		{
			continue;
		}
		for (const FMOSurvivorJobEntry& Job : JobQueue->GetAllJobs())
		{
			if (Job.JobType == EMOSurvivorJobType::ExcavateAndHaul && Job.DigZoneId.IsValid() && !Job.IsComplete() && !Job.IsFailed())
			{
				Claimed.Add(Job.DigZoneId);
			}
		}
		if (ShelteringVillagers.Contains(GetPawnGuid(Villager)))
		{
			continue;   // too cold to work: the shelter pass owns them
		}
		if (!JobQueue->GetCurrentJob().IsValid())
		{
			Idle.Add(Villager);
		}
	}
	if (Idle.Num() == 0)
	{
		return;
	}

	TArray<AMOContainerActor*> Storages;
	for (TActorIterator<AMOContainerActor> It(World); It; ++It)
	{
		if (IsValid(*It) && IsInSettlement(It->GetActorLocation()))
		{
			Storages.Add(*It);
		}
	}

	const TArray<FMOExcavationAssignment> Work = DecideExcavationWork(Zones, Claimed, Idle.Num(), Storages.Num() > 0, FlattenWithWork);
	const UMOIdentityRegistrySubsystem* Registry = World->GetSubsystem<UMOIdentityRegistrySubsystem>();
	for (const FMOExcavationAssignment& A : Work)
	{
		const FMODesignationZone* Dig = Zones.FindByPredicate([&](const FMODesignationZone& Z) { return Z.ZoneId == A.DigZoneId; });
		if (!Dig || Idle.Num() == 0)
		{
			continue;
		}
		// The idle villager nearest the dig.
		int32 Best = 0;
		for (int32 I = 1; I < Idle.Num(); ++I)
		{
			if (FVector::DistSquared(Idle[I]->GetActorLocation(), Dig->Center) < FVector::DistSquared(Idle[Best]->GetActorLocation(), Dig->Center))
			{
				Best = I;
			}
		}
		AActor* Container = nullptr;
		if (A.DumpMode == EMOExcavateDumpMode::Container)
		{
			Container = (A.DumpContainerGuid.IsValid() && Registry) ? Registry->ResolveActorOrNull(A.DumpContainerGuid) : nullptr;
			if (!Container)   // no default container (or it is gone): the settlement container nearest the dig
			{
				double BestD = TNumericLimits<double>::Max();
				for (AMOContainerActor* S : Storages)
				{
					const double D = FVector::DistSquared(S->GetActorLocation(), Dig->Center);
					if (D < BestD)
					{
						BestD = D;
						Container = S;
					}
				}
			}
			if (!Container)
			{
				continue;   // the zone's container is gone and the settlement has none
			}
		}
		APawn* Worker = Idle[Best];
		UMOSurvivorJobQueueComponent* JobQueue = Worker->FindComponentByClass<UMOSurvivorJobQueueComponent>();
		if (!JobQueue || !JobQueue->EnqueueExcavateJob(A.DigZoneId, A.DumpMode, A.DumpZoneId, Container, NAME_None).IsValid())
		{
			continue;
		}
		Idle.RemoveAtSwap(Best);
		UE_LOG(LogMOFramework, Log, TEXT("[MOColony] Excavation: %s digs zone %s (%.2f m3 left) -> %s"), *Worker->GetName(),
			*A.DigZoneId.ToString(EGuidFormats::Short), Dig->RemainingVolumeM3,
			A.DumpMode == EMOExcavateDumpMode::FillZone ? *FString::Printf(TEXT("fill zone %s"), *A.DumpZoneId.ToString(EGuidFormats::Short)) : *GetNameSafe(Container));
		if (UMOCharacterHistoryComponent* Hist = Worker->FindComponentByClass<UMOCharacterHistoryComponent>())
		{
			Hist->AddEntry(EHistoryEntryType::Activity, TEXT("Went to dig at the excavation site"));
		}
	}
}
