#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOUIManagerComponent.generated.h"

class APlayerController;
class UMOInventoryComponent;
class UMOInventoryMenu;

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOUIManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOUIManagerComponent();

	// Called by your IA_Inventory binding (or any other UI action).
	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void ToggleInventoryMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void OpenInventoryMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI")
	void CloseInventoryMenu();

	UFUNCTION(BlueprintCallable, Category="MO|UI")
	bool IsInventoryMenuOpen() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APlayerController* ResolveOwningPlayerController() const;
	bool IsLocalOwningPlayerController() const;

	UMOInventoryComponent* ResolveCurrentPawnInventoryComponent() const;

	void ApplyInputModeForMenuOpen(APlayerController* PlayerController, UMOInventoryMenu* MenuWidget) const;
	void ApplyInputModeForMenuClosed(APlayerController* PlayerController) const;

	UFUNCTION()
	void HandleInventoryMenuRequestClose();

private:
	// Set this in the component defaults (or in the BP that hosts the component).
	UPROPERTY(EditDefaultsOnly, Category="MO|UI")
	TSubclassOf<UMOInventoryMenu> InventoryMenuClass;

	UPROPERTY(EditDefaultsOnly, Category="MO|UI", meta=(ClampMin="0"))
	int32 InventoryMenuZOrder = 50;

	UPROPERTY(EditDefaultsOnly, Category="MO|UI")
	bool bShowMouseCursorWhileMenuOpen = true;

	UPROPERTY(EditDefaultsOnly, Category="MO|UI")
	bool bLockMovementWhileMenuOpen = true;

	UPROPERTY(EditDefaultsOnly, Category="MO|UI")
	bool bLockLookWhileMenuOpen = true;

	// Weak pointer so we do not keep dead widgets alive.
	UPROPERTY(Transient)
	TWeakObjectPtr<UMOInventoryMenu> InventoryMenuWidget;
};
