// MOWorldActionProvider.h -- a thing in the world that offers a list of things a pawn can do to it (a carcass: butcher each part, or all of them).
//
// ONE path for every such target (Steam playtest, Oct 16 2026: "right clicking deer starts butchering immediately instead of giving option to click"):
//   right-click -> the server's interaction -> the target calls IMOUIContractInterface::RequestShowWorldActionMenu -> the owning client opens
//   UMOWorldActionMenu, which lists GetWorldActions as buttons (FMOItemActionOption: the option type the item and fire menus already use)
//   a click   -> FMOWorldActions::Submit -> AMOPlayerController::ServerWorldAction -> FMOWorldActions::Execute (reach) -> PerformWorldAction (server)
// PerformWorldAction normally starts the pawn's UMOTimedActionComponent, so the wait is the simulation's: replicated progress bar, cancelled by moving or
// being hurt. A survivor's job calls PerformWorldAction directly, no UI. A new kind of target = implement this interface; no menu, controller or RPC change.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOItemActions.h"
#include "MOWorldActionProvider.generated.h"

class AActor;
class APawn;
class APlayerController;

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UMOWorldActionProvider : public UInterface
{
	GENERATED_BODY()
};

class MOFRAMEWORK_API IMOWorldActionProvider
{
	GENERATED_BODY()

public:
	/** The menu's heading ("Deer Carcass"). */
	virtual FText GetWorldActionTitle() const = 0;

	/** What this pawn can do here now (any machine, from replicated state): what the menu lists. A disabled option carries its reason. */
	virtual void GetWorldActions(const APawn* Pawn, TArray<FMOItemActionOption>& Out) const = 0;

	/** Server: re-check everything GetWorldActions checked, then do it (normally by starting the pawn's timed action). False if refused. */
	virtual bool PerformWorldAction(APawn* Pawn, FName ActionId) = 0;
};

struct MOFRAMEWORK_API FMOWorldActions
{
	/** The UI's call: on the authority (host / single player) Execute at once, otherwise AMOPlayerController::ServerWorldAction. */
	static bool Submit(APlayerController* PC, AActor* Target, FName ActionId);

	/** Server: the requester's pawn must be within ReachCm of the target's bounds and the target a provider; then its PerformWorldAction. */
	static bool Execute(APlayerController* PC, AActor* Target, FName ActionId);

	/** Reach from the pawn to the target's bounds, cm. */
	static constexpr float ReachCm = 400.0f;
};
