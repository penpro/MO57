/**
 * MOPersonalityComponent.cpp - Character Personality Traits Implementation
 */

#include "MOPersonalityComponent.h"
#include "Net/UnrealNetwork.h"

UMOPersonalityComponent::UMOPersonalityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UMOPersonalityComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UMOPersonalityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMOPersonalityComponent, Traits);
}

void UMOPersonalityComponent::OnRep_Traits()
{
	OnPersonalityChanged.Broadcast(Traits);
}

// ============================================================================
// TRAIT ACCESS
// ============================================================================

void UMOPersonalityComponent::SetTraits(const FMOPersonalityTraits& NewTraits)
{
	Traits = NewTraits;
	OnPersonalityChanged.Broadcast(Traits);
}

void UMOPersonalityComponent::SetTraitValue(EPersonalityAxis Axis, float Value)
{
	Traits.SetTraitValue(Axis, Value);
	OnPersonalityChanged.Broadcast(Traits);
}

void UMOPersonalityComponent::RandomizeTraits()
{
	Traits = FMOPersonalityTraits::Random();
	OnPersonalityChanged.Broadcast(Traits);
}

// ============================================================================
// PERSONALITY EFFECTS
// ============================================================================

float UMOPersonalityComponent::GetTaskSpeedModifier() const
{
	// Conscientiousness: -1 (Diligent) = 0.8x speed, +1 (Adaptable) = 1.2x speed
	return 1.0f + (Traits.Conscientiousness * 0.2f);
}

float UMOPersonalityComponent::GetTaskQualityModifier() const
{
	// Conscientiousness: -1 (Diligent) = 1.2x quality, +1 (Adaptable) = 0.8x quality
	return 1.0f - (Traits.Conscientiousness * 0.2f);
}

float UMOPersonalityComponent::GetSocialPresenceModifier(int32 NearbyCharacterCount) const
{
	if (NearbyCharacterCount <= 0)
	{
		// Alone: Reserved characters get bonus, Social characters get penalty
		return -Traits.Sociability * 0.1f;
	}
	else
	{
		// With others: Social characters get bonus, Reserved characters get penalty
		// Scales slightly with number of nearby characters (capped at 3)
		const float Scale = FMath::Min(NearbyCharacterCount, 3) / 3.0f;
		return Traits.Sociability * 0.15f * Scale;
	}
}

float UMOPersonalityComponent::GetMoodVarianceModifier() const
{
	// Stability: -1 (Volatile) = 1.5x variance, +1 (Stable) = 0.5x variance
	return 1.0f - (Traits.Stability * 0.5f);
}

// ============================================================================
// DESCRIPTORS
// ============================================================================

FText UMOPersonalityComponent::GetTraitDescription(EPersonalityAxis Axis) const
{
	const float Value = Traits.GetTraitValue(Axis);

	FString NegativeLabel, PositiveLabel;
	switch (Axis)
	{
	case EPersonalityAxis::Conscientiousness:
		NegativeLabel = TEXT("Diligent");
		PositiveLabel = TEXT("Adaptable");
		break;
	case EPersonalityAxis::Sociability:
		NegativeLabel = TEXT("Reserved");
		PositiveLabel = TEXT("Social");
		break;
	case EPersonalityAxis::Stability:
		NegativeLabel = TEXT("Volatile");
		PositiveLabel = TEXT("Stable");
		break;
	default:
		return FText::GetEmpty();
	}

	FString Intensity;
	const float AbsValue = FMath::Abs(Value);
	if (AbsValue < 0.2f)
	{
		return FText::FromString(TEXT("Neutral"));
	}
	else if (AbsValue < 0.5f)
	{
		Intensity = TEXT("Somewhat");
	}
	else if (AbsValue < 0.8f)
	{
		Intensity = TEXT("");
	}
	else
	{
		Intensity = TEXT("Very");
	}

	const FString Label = Value < 0.0f ? NegativeLabel : PositiveLabel;
	if (Intensity.IsEmpty())
	{
		return FText::FromString(Label);
	}
	return FText::FromString(FString::Printf(TEXT("%s %s"), *Intensity, *Label));
}

FText UMOPersonalityComponent::GetPersonalitySummary() const
{
	TArray<FString> Descriptors;

	// Only include non-neutral traits
	if (FMath::Abs(Traits.Conscientiousness) >= 0.2f)
	{
		Descriptors.Add(GetTraitDescription(EPersonalityAxis::Conscientiousness).ToString());
	}
	if (FMath::Abs(Traits.Sociability) >= 0.2f)
	{
		Descriptors.Add(GetTraitDescription(EPersonalityAxis::Sociability).ToString());
	}
	if (FMath::Abs(Traits.Stability) >= 0.2f)
	{
		Descriptors.Add(GetTraitDescription(EPersonalityAxis::Stability).ToString());
	}

	if (Descriptors.Num() == 0)
	{
		return FText::FromString(TEXT("Balanced personality"));
	}

	return FText::FromString(FString::Join(Descriptors, TEXT(", ")));
}

// ============================================================================
// SAVE/LOAD
// ============================================================================

void UMOPersonalityComponent::BuildSaveData(FMOPersonalityTraits& OutSaveData) const
{
	OutSaveData = Traits;
}

bool UMOPersonalityComponent::ApplySaveDataAuthority(const FMOPersonalityTraits& InSaveData)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}

	SetTraits(InSaveData);
	return true;
}
