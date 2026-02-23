#include "MOGroundContextMenu.h"
#include "MOForagingSubsystem.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogMOGroundMenu, Log, All);

UMOGroundContextMenu::UMOGroundContextMenu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMOGroundContextMenu::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button clicks
	BindButtonClick(SearchNearbyButton, this, &UMOGroundContextMenu::HandleSearchNearbyClicked);
	BindButtonClick(DigForSuppliesButton, this, &UMOGroundContextMenu::HandleDigForSuppliesClicked);

	// Reset timer and start mouse position checking
	MouseOutsideTimer = 0.0f;
	StartMouseCheckTimer();

	UE_LOG(LogMOGroundMenu, Verbose, TEXT("[MOGroundContextMenu] NativeConstruct"));
}

void UMOGroundContextMenu::NativeDestruct()
{
	StopMouseCheckTimer();
	Super::NativeDestruct();
}

void UMOGroundContextMenu::InitializeForLocation(FVector WorldLocation, APawn* InForagingPawn)
{
	TargetLocation = WorldLocation;
	ForagingPawn = InForagingPawn;

	// Get foraging subsystem
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogMOGroundMenu, Warning, TEXT("[MOGroundContextMenu] No world available"));
		return;
	}

	UMOForagingSubsystem* ForagingSubsystem = World->GetSubsystem<UMOForagingSubsystem>();
	if (!ForagingSubsystem)
	{
		UE_LOG(LogMOGroundMenu, Warning, TEXT("[MOGroundContextMenu] No foraging subsystem available"));
		return;
	}

	// Get foraging level and calculate radius
	ForagingLevel = ForagingSubsystem->GetForagingLevel(InForagingPawn);
	SearchRadius = ForagingSubsystem->CalculateSearchRadius(ForagingLevel);

	// Update display
	UpdateDisplayText();

	UE_LOG(LogMOGroundMenu, Log, TEXT("[MOGroundContextMenu] Initialized at %s with radius %.0f (skill %d)"),
		*WorldLocation.ToString(), SearchRadius, ForagingLevel);
}

void UMOGroundContextMenu::HandleSearchNearbyClicked()
{
	UE_LOG(LogMOGroundMenu, Log, TEXT("[MOGroundContextMenu] Search Nearby clicked"));

	UWorld* World = GetWorld();
	if (!World)
	{
		RequestClose();
		return;
	}

	UMOForagingSubsystem* ForagingSubsystem = World->GetSubsystem<UMOForagingSubsystem>();
	if (!ForagingSubsystem)
	{
		UE_LOG(LogMOGroundMenu, Warning, TEXT("[MOGroundContextMenu] No foraging subsystem"));
		RequestClose();
		return;
	}

	// Perform the search (pawn passed for XP award)
	TArray<AMOWorldItem*> RevealedItems = ForagingSubsystem->RevealHISMInstancesInRadius(
		TargetLocation, SearchRadius, ForagingPawn.Get());

	// Broadcast completion
	OnSearchComplete.Broadcast(RevealedItems);

	// Close the menu
	RequestClose();
}

void UMOGroundContextMenu::HandleDigForSuppliesClicked()
{
	UE_LOG(LogMOGroundMenu, Log, TEXT("[MOGroundContextMenu] Dig for Supplies clicked"));

	UWorld* World = GetWorld();
	if (!World)
	{
		RequestClose();
		return;
	}

	UMOForagingSubsystem* ForagingSubsystem = World->GetSubsystem<UMOForagingSubsystem>();
	if (!ForagingSubsystem)
	{
		UE_LOG(LogMOGroundMenu, Warning, TEXT("[MOGroundContextMenu] No foraging subsystem"));
		RequestClose();
		return;
	}

	// Perform the dig (pawn passed for XP award)
	TArray<AMOWorldItem*> DugItems = ForagingSubsystem->DigForSupplies(
		TargetLocation, ForagingLevel, ForagingPawn.Get());

	// Broadcast completion
	OnDigComplete.Broadcast(DugItems);

	// Close the menu
	RequestClose();
}

void UMOGroundContextMenu::UpdateDisplayText()
{
	if (RadiusText)
	{
		RadiusText->SetText(FText::Format(
			NSLOCTEXT("MOForaging", "RadiusFormat", "Range: {0}m"),
			FText::AsNumber(FMath::RoundToInt(SearchRadius / 100.0f)) // Convert to meters
		));
	}

	if (SkillLevelText)
	{
		SkillLevelText->SetText(FText::Format(
			NSLOCTEXT("MOForaging", "SkillFormat", "Foraging: {0}"),
			FText::AsNumber(ForagingLevel)
		));
	}
}

void UMOGroundContextMenu::SetMenuPosition(FVector2D ScreenPosition)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// Get viewport size and place menu at center (where the reticle is)
	int32 ViewportX, ViewportY;
	PC->GetViewportSize(ViewportX, ViewportY);

	// Center of screen, offset slightly so cursor is inside the menu
	const float CenterX = ViewportX / 2.0f;
	const float CenterY = ViewportY / 2.0f;
	const float OffsetX = -50.0f;  // Offset left so menu appears to the right of center
	const float OffsetY = -30.0f;  // Offset up so menu appears below center

	// Get the DPI scale to convert viewport coords to widget coords
	const float DPIScale = UWidgetLayoutLibrary::GetViewportScale(PC);

	if (DPIScale > 0.0f)
	{
		const FVector2D AdjustedPosition = FVector2D(CenterX + OffsetX, CenterY + OffsetY) / DPIScale;
		SetPositionInViewport(AdjustedPosition, false);
	}
	else
	{
		SetPositionInViewport(FVector2D(CenterX + OffsetX, CenterY + OffsetY), false);
	}
}

bool UMOGroundContextMenu::IsMouseOverMenu() const
{
	if (!ButtonContainer)
	{
		return false;
	}

	// Get absolute cursor position (screen space)
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	const FVector2D AbsoluteMousePos = FSlateApplication::Get().GetCursorPos();

	// Get the widget's geometry and check if mouse is inside
	const FGeometry& Geometry = ButtonContainer->GetCachedGeometry();
	const FVector2D LocalMousePos = Geometry.AbsoluteToLocal(AbsoluteMousePos);
	const FVector2D LocalSize = Geometry.GetLocalSize();

	return LocalMousePos.X >= 0.0f && LocalMousePos.X <= LocalSize.X &&
	       LocalMousePos.Y >= 0.0f && LocalMousePos.Y <= LocalSize.Y;
}

void UMOGroundContextMenu::StartMouseCheckTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Check mouse position every 0.05 seconds (20 times per second)
	World->GetTimerManager().SetTimer(
		MouseCheckTimerHandle,
		this,
		&UMOGroundContextMenu::CheckMousePosition,
		0.05f,
		true  // Looping
	);

	UE_LOG(LogMOGroundMenu, Verbose, TEXT("[MOGroundContextMenu] Started mouse check timer"));
}

void UMOGroundContextMenu::StopMouseCheckTimer()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(MouseCheckTimerHandle);
	}
}

void UMOGroundContextMenu::CheckMousePosition()
{
	const bool bMouseOver = IsMouseOverMenu();

	if (!bMouseOver)
	{
		MouseOutsideTimer += 0.05f;  // Timer interval
		if (MouseOutsideTimer >= AutoCloseDelay)
		{
			UE_LOG(LogMOGroundMenu, Log, TEXT("[MOGroundContextMenu] Auto-closing - mouse outside for %.2f seconds"), MouseOutsideTimer);
			RequestClose();
		}
	}
	else
	{
		MouseOutsideTimer = 0.0f;
	}
}
