/**
 * MODeathRecapWidget.cpp - Post-Death Recap Screen
 */

#include "MODeathRecapWidget.h"
#include "MOFramework.h"
#include "MOCharacter.h"
#include "MOCommonButton.h"
#include "MOGameClockSubsystem.h"
#include "MOIdentityComponent.h"
#include "Components/TextBlock.h"

void UMODeathRecapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogMOFramework, Warning, TEXT("[MODeathRecap] NativeConstruct '%s'"), *GetName());

	// Re-bind buttons defensively — same pattern other widgets use to defeat
	// duplicate bindings from BP re-construct paths.
	if (ReturnToMenuButton)
	{
		ReturnToMenuButton->OnClicked().RemoveAll(this);
		ReturnToMenuButton->OnClicked().AddUObject(this, &UMODeathRecapWidget::HandleReturnToMenuClicked);
	}
	if (ContinueButton)
	{
		ContinueButton->OnClicked().RemoveAll(this);
		ContinueButton->OnClicked().AddUObject(this, &UMODeathRecapWidget::HandleContinueClicked);
	}

	// Apply whatever data was set before construct (SetRecapData buffers it
	// to RecapData; the widgets weren't ready yet at that point).
	ApplyDataToWidgets();
}

void UMODeathRecapWidget::NativeDestruct()
{
	if (ReturnToMenuButton)
	{
		ReturnToMenuButton->OnClicked().RemoveAll(this);
	}
	if (ContinueButton)
	{
		ContinueButton->OnClicked().RemoveAll(this);
	}

	OnReturnToMenuRequested.Clear();
	OnContinueRequested.Clear();

	Super::NativeDestruct();
}

void UMODeathRecapWidget::SetRecapData(const FMODeathRecapData& InData)
{
	RecapData = InData;
	ApplyDataToWidgets();
}

void UMODeathRecapWidget::ApplyDataToWidgets()
{
	if (CharacterNameText)
	{
		CharacterNameText->SetText(RecapData.CharacterName);
	}
	if (CauseOfDeathText)
	{
		CauseOfDeathText->SetText(RecapData.CauseOfDeath);
	}
	if (LocationText)
	{
		LocationText->SetText(RecapData.LocationText);
	}
	if (TimeOfDeathText)
	{
		TimeOfDeathText->SetText(RecapData.TimeOfDeathText);
	}
}

FMODeathRecapData UMODeathRecapWidget::BuildRecapData(AMOCharacter* DeadCharacter, EMOBodyPartType CausePart)
{
	FMODeathRecapData Data;

	if (!IsValid(DeadCharacter))
	{
		// Defensive: dead-character pointer can race the recap dispatch.
		// Return a placeholder rather than crashing the UI path.
		Data.CharacterName = NSLOCTEXT("MO", "DeathRecap.UnknownName", "Unknown");
		Data.CauseOfDeath = NSLOCTEXT("MO", "DeathRecap.UnknownCause", "Cause unknown");
		return Data;
	}

	// Character name — prefer the IdentityComponent's DisplayName (set by
	// the spawn flow or save load) and fall back to the actor name so the
	// UI never shows an empty string.
	if (UMOIdentityComponent* Identity = DeadCharacter->FindComponentByClass<UMOIdentityComponent>())
	{
		if (!Identity->DisplayName.IsEmpty())
		{
			Data.CharacterName = Identity->DisplayName;
		}
	}
	if (Data.CharacterName.IsEmpty())
	{
		Data.CharacterName = FText::FromString(DeadCharacter->GetName());
	}

	// Cause text — convert the enum to its display label (UE-generated from
	// the UMETA(DisplayName=...) on EMOBodyPartType). For body parts without
	// an explicit display name UE falls back to the C++ name, which is also
	// readable ("Brain", "Heart", etc.) so we don't need a hand-maintained map.
	const UEnum* BodyPartEnum = StaticEnum<EMOBodyPartType>();
	const FText PartName = BodyPartEnum
		? BodyPartEnum->GetDisplayNameTextByValue(static_cast<int64>(CausePart))
		: FText::FromString(TEXT("Unknown"));

	Data.CauseOfDeath = FText::Format(
		NSLOCTEXT("MO", "DeathRecap.CauseFormat", "{0} destroyed"),
		PartName);

	// World location — keep it compact. Player won't memorize coordinates,
	// but a rough "where am I in the world" reading helps with respawn
	// orientation decisions later.
	const FVector Loc = DeadCharacter->GetActorLocation();
	Data.LocationText = FText::Format(
		NSLOCTEXT("MO", "DeathRecap.LocationFormat", "({0}, {1}, {2})"),
		FText::AsNumber(FMath::RoundToInt(Loc.X)),
		FText::AsNumber(FMath::RoundToInt(Loc.Y)),
		FText::AsNumber(FMath::RoundToInt(Loc.Z)));

	// In-game date/time — only populated if the game clock subsystem is up.
	// If it isn't, leave TimeOfDeathText empty so the optional UI block
	// doesn't show a "0/0/0" placeholder.
	if (UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(DeadCharacter))
	{
		const FDateTime GameTime = Clock->GetGameDateTime();
		Data.TimeOfDeathText = FText::FromString(GameTime.ToString(TEXT("%Y-%m-%d %H:%M")));
	}

	return Data;
}

void UMODeathRecapWidget::HandleReturnToMenuClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MODeathRecap] Return-to-menu clicked"));
	OnReturnToMenuRequested.Broadcast();
}

void UMODeathRecapWidget::HandleContinueClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MODeathRecap] Continue clicked"));
	OnContinueRequested.Broadcast();
}
