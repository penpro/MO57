#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "MOSpectatorPawn.generated.h"

/**
 * Custom spectator pawn that can be locked in place when no gameplay pawn is possessed.
 * Assign this in your GameMode's SpectatorClass property.
 */
UCLASS()
class MOFRAMEWORK_API AMOSpectatorPawn : public ASpectatorPawn
{
	GENERATED_BODY()

public:
	AMOSpectatorPawn();

	// Lock/unlock spectator movement
	UFUNCTION(BlueprintCallable, Category="MO|Spectator")
	void SetMovementLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category="MO|Spectator")
	bool IsMovementLocked() const { return bMovementLocked; }

	// Teleport to a location looking down
	UFUNCTION(BlueprintCallable, Category="MO|Spectator")
	void SetViewAboveLocation(FVector TargetLocation, float Height = 2000.0f, float PitchAngle = -60.0f);

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void MoveForward(float Val) override;
	virtual void MoveRight(float Val) override;
	virtual void MoveUp_World(float Val) override;
	virtual void TurnAtRate(float Rate) override;
	virtual void LookUpAtRate(float Rate) override;

private:
	/** Whether spectator movement is locked (camera stays fixed). */
	UPROPERTY()
	bool bMovementLocked = true;
};
