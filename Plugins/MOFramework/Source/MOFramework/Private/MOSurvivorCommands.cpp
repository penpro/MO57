#include "MOSurvivorCommands.h"
#include "MOPlayerController.h"
#include "MOSurvivorController.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MORecruitmentComponent.h"
#include "MOFramework.h"
#include "GameFramework/Pawn.h"

bool FMOSurvivorCommands::Submit(APlayerController* Issuer, APawn* Survivor, const FMOSurvivorCommand& Command)
{
	if (!IsValid(Issuer) || !IsValid(Survivor))
	{
		return false;
	}
	if (Issuer->HasAuthority())
	{
		return Execute(Issuer, Survivor, Command);   // host / single player: no round trip
	}
	if (AMOPlayerController* MOPC = Cast<AMOPlayerController>(Issuer))
	{
		MOPC->ServerSurvivorCommand(Survivor, Command);
		return true;   // the effect arrives by replication (the job queue's FastArray, the survivor's movement); the menus refresh on it
	}
	return false;
}

bool FMOSurvivorCommands::CanCommand(const APlayerController* Issuer, const APawn* Survivor)
{
	if (!IsValid(Issuer) || !IsValid(Survivor) || Survivor == Issuer->GetPawn() || Survivor->IsPlayerControlled())
	{
		return false;
	}
	const UMORecruitmentComponent* Recruitment = Survivor->FindComponentByClass<UMORecruitmentComponent>();
	return Recruitment && Recruitment->IsPossessable();
}

bool FMOSurvivorCommands::Execute(APlayerController* Issuer, APawn* Survivor, const FMOSurvivorCommand& Command)
{
	if (!IsValid(Issuer) || !Issuer->HasAuthority() || !CanCommand(Issuer, Survivor))
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOSurvivorCommand] %s: order %d to %s refused (not a recruited survivor free of players)"), *GetNameSafe(Issuer),
			static_cast<int32>(Command.Kind), *GetNameSafe(Survivor));
		return false;
	}
	AMOSurvivorController* AI = Cast<AMOSurvivorController>(Survivor->GetController());
	UMOSurvivorJobQueueComponent* Jobs = Survivor->FindComponentByClass<UMOSurvivorJobQueueComponent>();
	bool bDone = false;
	switch (Command.Kind)
	{
	case EMOSurvivorCommandKind::FollowMe:
		if (AI && Issuer->GetPawn())
		{
			AI->SetFollowTarget(Issuer->GetPawn());
			bDone = true;
		}
		break;
	case EMOSurvivorCommandKind::StayHere:
		if (AI)
		{
			AI->SetStayAtLocation(Survivor->GetActorLocation());
			bDone = true;
		}
		break;
	case EMOSurvivorCommandKind::GoHome:
		if (AI && Jobs && Jobs->HasHome())
		{
			AI->GoToHome();
			bDone = true;
		}
		break;
	case EMOSurvivorCommandKind::SetHome:
		if (Jobs)
		{
			Jobs->SetHome(Survivor->GetActorLocation());
			bDone = true;
		}
		break;
	case EMOSurvivorCommandKind::EnqueueJob:
		bDone = Jobs && Command.JobType != EMOSurvivorJobType::None && Jobs->EnqueueJob(Command.JobType, FMath::Clamp(Command.RepeatCount, 1, 99)).IsValid();
		break;
	case EMOSurvivorCommandKind::CancelJob:
		bDone = Jobs && Command.JobId.IsValid() && Jobs->CancelJob(Command.JobId);
		break;
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MOSurvivorCommand] %s -> %s: order %d (job %d) %s"), *GetNameSafe(Issuer), *Survivor->GetName(), static_cast<int32>(Command.Kind),
		static_cast<int32>(Command.JobType), bDone ? TEXT("done") : TEXT("FAILED (no survivor AI / job queue / home)"));
	return bDone;
}
