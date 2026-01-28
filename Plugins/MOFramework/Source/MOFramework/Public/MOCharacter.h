#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MOCharacter.generated.h"

class UMOIdentityComponent;
class UMOInventoryComponent;
class UMOInteractorComponent;
class UMOSurvivalStatsComponent;
class UMOSkillsComponent;
class UMOKnowledgeComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class USkeletalMesh;
class UAnimInstance;
struct FInputActionValue;
struct FSoftObjectPath;

UCLASS()
class MOFRAMEWORK_API AMOCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMOCharacter();

	// Component accessors
	UFUNCTION(BlueprintPure, Category="MO")
	UMOIdentityComponent* GetIdentityComponent() const { return IdentityComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOInteractorComponent* GetInteractorComponent() const { return InteractorComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOSurvivalStatsComponent* GetSurvivalStatsComponent() const { return SurvivalStatsComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOSkillsComponent* GetSkillsComponent() const { return SkillsComponent; }

	UFUNCTION(BlueprintPure, Category="MO")
	UMOKnowledgeComponent* GetKnowledgeComponent() const { return KnowledgeComponent; }

	UFUNCTION(BlueprintPure, Category="MO|Camera")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	UFUNCTION(BlueprintPure, Category="MO|Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	// Runtime mesh/animation swapping
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void SetCharacterMesh(USkeletalMesh* NewMesh);

	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void SetAnimationBlueprint(TSubclassOf<UAnimInstance> NewAnimClass);

	// Blueprint-callable input functions (useful for UI virtual joysticks, etc.)
	UFUNCTION(BlueprintCallable, Category="MO|Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="MO|Input")
	virtual void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="MO|Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="MO|Input")
	virtual void DoJumpEnd();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// Input callbacks
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartJump();
	void StopJump();
	void Interact();

protected:
	// Camera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// MO Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOIdentityComponent> IdentityComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOInteractorComponent> InteractorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOSurvivalStatsComponent> SurvivalStatsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOSkillsComponent> SkillsComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO")
	TObjectPtr<UMOKnowledgeComponent> KnowledgeComponent;

	// Input - using soft references to avoid constructor issues in UE 5.7
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input")
	TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input")
	TSoftObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input")
	TSoftObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input")
	TSoftObjectPtr<UInputAction> MouseLookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input")
	TSoftObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Input")
	TSoftObjectPtr<UInputAction> InteractAction;

	// Appearance - using soft references
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	TSoftObjectPtr<USkeletalMesh> DefaultMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	TSoftClassPtr<UAnimInstance> DefaultAnimBlueprint;
};
