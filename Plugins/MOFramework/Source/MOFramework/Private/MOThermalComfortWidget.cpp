/**
 * MOThermalComfortWidget.cpp - HUD Thermal Comfort Indicator
 */

#include "MOThermalComfortWidget.h"
#include "MOFramework.h"
#include "MOVitalsComponent.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UMOThermalComfortWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOThermalComfortWidget] NativeConstruct on '%s' — icons configured=%d"),
		*GetName(), ComfortIcons.Num());

	// Subscribe to pawn-change FIRST so the late-spawn case wires us up.
	// The HUD root is pushed at PlayerController BeginPlay, which can fire
	// BEFORE SpawnInitialPawn finishes possessing the pawn. Without this
	// hook, AutoAttach silently bails (PC->GetPawn() == nullptr) and the
	// indicator stays blank for the rest of the session.
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &UMOThermalComfortWidget::HandlePossessedPawnChanged);
		PC->OnPossessedPawnChanged.AddDynamic(this, &UMOThermalComfortWidget::HandlePossessedPawnChanged);
	}

	// Try the fast-path attach in case the pawn already exists (e.g.
	// switching levels with a possessed pawn). If it bails the pawn-change
	// delegate above will re-trigger us once the pawn arrives.
	AutoAttachToLocalPlayerVitals();
}

void UMOThermalComfortWidget::NativeDestruct()
{
	// Detach delegate so the next world's vitals doesn't fire into a dead widget.
	if (UMOVitalsComponent* Vitals = TargetVitals.Get())
	{
		Vitals->OnThermalComfortChanged.RemoveDynamic(this, &UMOThermalComfortWidget::HandleThermalComfortChanged);
	}
	TargetVitals.Reset();

	// Unhook from pawn-change broadcasts on the PC. Safe even if NativeConstruct
	// took the no-PC branch and never subscribed.
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &UMOThermalComfortWidget::HandlePossessedPawnChanged);
	}

	Super::NativeDestruct();
}

void UMOThermalComfortWidget::HandlePossessedPawnChanged(APawn* /*OldPawn*/, APawn* NewPawn)
{
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOThermalComfortWidget] OnPossessedPawnChanged — new pawn=%s, re-attaching"),
		*GetNameSafe(NewPawn));
	AutoAttachToLocalPlayerVitals();
}

void UMOThermalComfortWidget::SetTargetVitals(UMOVitalsComponent* NewVitals)
{
	// Unbind from previous (if any) before swapping. Safe even if dead.
	if (UMOVitalsComponent* OldVitals = TargetVitals.Get())
	{
		OldVitals->OnThermalComfortChanged.RemoveDynamic(this, &UMOThermalComfortWidget::HandleThermalComfortChanged);
	}

	TargetVitals = NewVitals;

	if (NewVitals)
	{
		// AddUnique to avoid double-subscription if SetTargetVitals(SameOne)
		// is called twice (BP-side defensiveness).
		NewVitals->OnThermalComfortChanged.RemoveDynamic(this, &UMOThermalComfortWidget::HandleThermalComfortChanged);
		NewVitals->OnThermalComfortChanged.AddDynamic(this, &UMOThermalComfortWidget::HandleThermalComfortChanged);

		// Render initial state immediately — don't wait for first delegate fire.
		SetComfortLevel(NewVitals->GetThermalComfort());

		UE_LOG(LogMOFramework, Log,
			TEXT("[MOThermalComfortWidget] Attached to vitals on '%s', initial level=%s"),
			*GetNameSafe(NewVitals->GetOwner()),
			*UEnum::GetValueAsString(NewVitals->GetThermalComfort()));
	}
}

void UMOThermalComfortWidget::SetComfortLevel(EMOThermalComfort NewLevel)
{
	// Guard against redundant brush rebuilds. SetBrushFromTexture invalidates
	// the Slate prepass — skipping the no-op call keeps the HUD cheap when
	// the player sits at a comfort level for a long time.
	if (NewLevel == CurrentLevel && ThermalIcon && ThermalIcon->GetBrush().GetResourceObject())
	{
		return;
	}
	CurrentLevel = NewLevel;

	const int32 Idx = static_cast<int32>(NewLevel);
	if (!ComfortIcons.IsValidIndex(Idx) || !ComfortIcons[Idx])
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOThermalComfortWidget] No icon at index %d (%s) — fill ComfortIcons in WBP Class Defaults"),
			Idx, *UEnum::GetValueAsString(NewLevel));
		return;
	}

	if (ThermalIcon)
	{
		ThermalIcon->SetBrushFromTexture(ComfortIcons[Idx], /*bMatchSize*/ false);
	}
}

void UMOThermalComfortWidget::AutoAttachToLocalPlayerVitals()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		PC = UGameplayStatics::GetPlayerController(this, 0);
	}
	if (!PC)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOThermalComfortWidget] AutoAttach bailed — no PlayerController"));
		return;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		// Common at startup — HUD widget activates before SpawnInitialPawn
		// finishes. The OnPossessedPawnChanged hook in NativeConstruct will
		// re-call us once the pawn lands.
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOThermalComfortWidget] AutoAttach deferred — PC has no pawn yet (will re-try on possession)"));
		return;
	}

	UMOVitalsComponent* Vitals = Pawn->FindComponentByClass<UMOVitalsComponent>();
	if (!Vitals)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOThermalComfortWidget] AutoAttach bailed — pawn '%s' has no UMOVitalsComponent"),
			*Pawn->GetName());
		return;
	}

	SetTargetVitals(Vitals);
}

void UMOThermalComfortWidget::HandleThermalComfortChanged(EMOThermalComfort /*OldComfort*/, EMOThermalComfort NewComfort)
{
	SetComfortLevel(NewComfort);
}
