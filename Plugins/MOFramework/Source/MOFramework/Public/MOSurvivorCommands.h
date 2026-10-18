// A player's orders to a recruited survivor (Follow Me, Stay Here, Go Home, Set Home, add / cancel a task): one request type, one server RPC
// (AMOPlayerController::ServerSurvivorCommand), executed on the authority. The survivor context menu and the task menu submit through Submit.
//
// Until Oct 18 2026 the menus called the survivor's AI controller and job queue directly. Those exist and accept changes only on the server, so a
// joining player's orders did nothing ("No survivor controller for Follow Me"; "EnqueueJob called on non-authority"): Wes's Steam test, "cant give
// comand".

#pragma once

#include "CoreMinimal.h"
#include "MOSurvivorJobTypes.h"
#include "MOSurvivorCommands.generated.h"

class APawn;
class APlayerController;

UENUM()
enum class EMOSurvivorCommandKind : uint8
{
	FollowMe,
	StayHere,
	GoHome,
	SetHome,
	EnqueueJob,
	CancelJob
};

/** One order (an RPC parameter). */
USTRUCT()
struct MOFRAMEWORK_API FMOSurvivorCommand
{
	GENERATED_BODY()

	UPROPERTY()
	EMOSurvivorCommandKind Kind = EMOSurvivorCommandKind::FollowMe;

	/** EnqueueJob: the job and how many times. */
	UPROPERTY()
	EMOSurvivorJobType JobType = EMOSurvivorJobType::None;

	UPROPERTY()
	int32 RepeatCount = 1;

	/** CancelJob: which one. */
	UPROPERTY()
	FGuid JobId;

	static FMOSurvivorCommand Make(EMOSurvivorCommandKind InKind)
	{
		FMOSurvivorCommand C;
		C.Kind = InKind;
		return C;
	}
};

struct MOFRAMEWORK_API FMOSurvivorCommands
{
	/** The UI's call: executes on the authority, otherwise sends the order to the server. True when it was executed / sent (not that it succeeded there). */
	static bool Submit(APlayerController* Issuer, APawn* Survivor, const FMOSurvivorCommand& Command);

	/** SERVER: check the issuer may order this survivor (CanCommand), then carry the order out. True on success. */
	static bool Execute(APlayerController* Issuer, APawn* Survivor, const FMOSurvivorCommand& Command);

	/** May Issuer's player order Survivor: a recruited survivor (UMORecruitmentComponent::IsPossessable), not the issuer's own pawn, not another player's. */
	static bool CanCommand(const APlayerController* Issuer, const APawn* Survivor);
};
