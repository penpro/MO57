#include "MOCharacterHistoryComponent.h"
#include "MOFramework.h"
#include "MOGameClockSubsystem.h"
#include "MOPlayerController.h"
#include "GameFramework/Controller.h"

UMOCharacterHistoryComponent::UMOCharacterHistoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UMOCharacterHistoryComponent::BuildAwaySummary(const TArray<FMOCharacterHistoryEntry>& InEntries, const FDateTime& LeftAt, const FDateTime& Now, int32 MaxLines, FText& OutText)
{
	if (LeftAt == FDateTime::MinValue())
	{
		return false;
	}
	TArray<const FMOCharacterHistoryEntry*> Since;
	for (const FMOCharacterHistoryEntry& Entry : InEntries)
	{
		if (Entry.Timestamp > LeftAt)
		{
			Since.Add(&Entry);
		}
	}
	if (Since.Num() == 0)
	{
		return false;
	}
	const FTimespan Away = Now > LeftAt ? Now - LeftAt : FTimespan::Zero();
	const int32 Days = Away.GetDays(), Hours = Away.GetHours(), Minutes = FMath::Max(Away.GetMinutes(), Days == 0 && Hours == 0 ? 1 : 0);
	FString Span = Days > 0 ? FString::Printf(TEXT("%d d %d h"), Days, Hours) : Hours > 0 ? FString::Printf(TEXT("%d h %d min"), Hours, Minutes)
		: FString::Printf(TEXT("%d min"), Minutes);
	FString Text = FString::Printf(TEXT("While you were away (%s):"), *Span);
	const int32 Shown = FMath::Min(Since.Num(), FMath::Max(1, MaxLines));
	for (int32 i = Since.Num() - Shown; i < Since.Num(); ++i)
	{
		Text.AppendChar(TCHAR(10));   // a new line
		Text += TEXT("- ") + Since[i]->Description.ToString();
	}
	if (Since.Num() > Shown)
	{
		Text.AppendChar(TCHAR(10));
		Text += FString::Printf(TEXT("(and %d earlier)"), Since.Num() - Shown);
	}
	OutText = FText::FromString(Text);
	return true;
}

void UMOCharacterHistoryComponent::HandleControllerChange(const AController* Previous, AController* Current)
{
	const UWorld* World = GetWorld();
	const UMOGameClockSubsystem* Clock = World ? World->GetSubsystem<UMOGameClockSubsystem>() : nullptr;
	if (!Clock || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const bool bPlayerBefore = Previous && Previous->IsPlayerController();
	const bool bPlayerNow = Current && Current->IsPlayerController();
	if (bPlayerBefore && !bPlayerNow)
	{
		PlayerLeftAt = Clock->GetGameDateTime();
		UE_LOG(LogMOFramework, Log, TEXT("[MOHistory] %s: the player left at %s"), *GetOwner()->GetName(), *PlayerLeftAt.ToString());
		return;
	}
	if (!bPlayerNow || bPlayerBefore)
	{
		return;
	}
	FText Summary;
	if (BuildAwaySummary(Entries, PlayerLeftAt, Clock->GetGameDateTime(), 4, Summary))
	{
		if (AMOPlayerController* PC = Cast<AMOPlayerController>(Current))
		{
			PC->ClientShowNotification(Summary, 10.0f, EMONotificationType::Info);
		}
		UE_LOG(LogMOFramework, Log, TEXT("[MOHistory] %s: back under %s -- %s"), *GetOwner()->GetName(), *Current->GetName(), *Summary.ToString().Replace(*FString::Chr(TCHAR(10)), TEXT(" | ")));
	}
	PlayerLeftAt = FDateTime::MinValue();
}

void UMOCharacterHistoryComponent::AddEntry(EHistoryEntryType EntryType, const FString& Description)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	FMOCharacterHistoryEntry Entry;
	Entry.EntryType = EntryType;
	Entry.Description = FText::FromString(Description);
	if (UWorld* World = GetWorld())
	{
		if (UMOGameClockSubsystem* Clock = World->GetSubsystem<UMOGameClockSubsystem>())
		{
			Entry.Timestamp = Clock->GetGameDateTime();
		}
	}

	Entries.Add(Entry);
	if (Entries.Num() > MaxEntries)
	{
		Entries.RemoveAt(0, Entries.Num() - MaxEntries);
	}

	OnEntryAdded.Broadcast(Entry);
}

TArray<FMOCharacterHistoryEntry> UMOCharacterHistoryComponent::GetRecentEntries(int32 Count) const
{
	TArray<FMOCharacterHistoryEntry> Result;
	const int32 N = FMath::Min(Count, Entries.Num());
	Result.Reserve(N);
	for (int32 i = Entries.Num() - 1; i >= Entries.Num() - N; --i)
	{
		Result.Add(Entries[i]);
	}
	return Result;
}

// =============================================================================
// RELATIONSHIP GRAPH (V2.3)
// =============================================================================

float UMOCharacterHistoryComponent::ComputeStrengthDelta(float CurrentStrength, float SharedGameHours,
	float ApartGameHours, float GrowPerSharedHour, float DriftPerApartHour)
{
	float Strength = CurrentStrength;
	if (SharedGameHours > 0.0f)
	{
		// Asymptotic growth: each shared hour closes a fixed fraction of the
		// distance to 1.0 — fast early acquaintance, slow late deepening.
		const float CloseFraction = 1.0f - FMath::Pow(1.0f - GrowPerSharedHour, SharedGameHours);
		Strength += (1.0f - Strength) * CloseFraction;
	}
	if (ApartGameHours > 0.0f)
	{
		// Drift toward 0 while apart — friendships fade, slowly, and never
		// cross zero from fading alone (hostility requires events, not absence).
		const float Drift = DriftPerApartHour * ApartGameHours;
		if (Strength > 0.0f)
		{
			Strength = FMath::Max(0.0f, Strength - Drift);
		}
		else if (Strength < 0.0f)
		{
			Strength = FMath::Min(0.0f, Strength + Drift);
		}
	}
	return FMath::Clamp(Strength, -1.0f, 1.0f);
}

FMOCharacterRelationship* UMOCharacterHistoryComponent::FindOrAddRelationship(const FGuid& OtherGuid)
{
	for (FMOCharacterRelationship& Rel : Relationships)
	{
		if (Rel.OtherCharacterGuid == OtherGuid)
		{
			return &Rel;
		}
	}
	FMOCharacterRelationship NewRel;
	NewRel.OtherCharacterGuid = OtherGuid;
	return &Relationships[Relationships.Add(NewRel)];
}

void UMOCharacterHistoryComponent::AddSharedTime(const FGuid& OtherGuid, float GameHours)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !OtherGuid.IsValid() || GameHours <= 0.0f)
	{
		return;
	}
	FMOCharacterRelationship* Rel = FindOrAddRelationship(OtherGuid);
	Rel->ProximityTime += GameHours * 3600.0f;
	const float OldStrength = Rel->Strength;
	Rel->Strength = ComputeStrengthDelta(Rel->Strength, GameHours, 0.0f);
	if (Rel->RelationshipType == ERelationshipType::None &&
		OldStrength < FriendThreshold && Rel->Strength >= FriendThreshold)
	{
		Rel->RelationshipType = ERelationshipType::Friend;
		AddEntry(EHistoryEntryType::RelationshipChange, TEXT("Became friends with a fellow settler"));
	}
}

void UMOCharacterHistoryComponent::ApplyApartDrift(const TSet<FGuid>& CoLocated, float GameHours)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || GameHours <= 0.0f)
	{
		return;
	}
	for (FMOCharacterRelationship& Rel : Relationships)
	{
		if (!CoLocated.Contains(Rel.OtherCharacterGuid))
		{
			Rel.Strength = ComputeStrengthDelta(Rel.Strength, 0.0f, GameHours);
		}
	}
}

void UMOCharacterHistoryComponent::SetRelationshipType(const FGuid& OtherGuid, ERelationshipType Type)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !OtherGuid.IsValid())
	{
		return;
	}
	FMOCharacterRelationship* Rel = FindOrAddRelationship(OtherGuid);
	Rel->RelationshipType = Type;
	// Family/romance floor: a typed bond starts meaningfully positive.
	if (Type == ERelationshipType::Spouse || Type == ERelationshipType::Parent ||
		Type == ERelationshipType::Child || Type == ERelationshipType::Romantic)
	{
		Rel->Strength = FMath::Max(Rel->Strength, 0.6f);
	}
}

void UMOCharacterHistoryComponent::StampRomantic(const FGuid& OtherGuid, double GameSeconds)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !OtherGuid.IsValid())
	{
		return;
	}
	FMOCharacterRelationship* Rel = FindOrAddRelationship(OtherGuid);
	if (Rel->RomanticSinceGameSeconds < 0.0)
	{
		Rel->RomanticSinceGameSeconds = GameSeconds;
	}
}

FMOCharacterRelationship UMOCharacterHistoryComponent::GetRelationship(const FGuid& OtherGuid) const
{
	for (const FMOCharacterRelationship& Rel : Relationships)
	{
		if (Rel.OtherCharacterGuid == OtherGuid)
		{
			return Rel;
		}
	}
	return FMOCharacterRelationship();
}

float UMOCharacterHistoryComponent::GetAverageStandingWith(const TArray<FGuid>& Others) const
{
	if (Others.Num() == 0)
	{
		return 0.0f;
	}
	float Total = 0.0f;
	for (const FGuid& Guid : Others)
	{
		Total += GetRelationship(Guid).Strength;
	}
	return Total / static_cast<float>(Others.Num());
}

void UMOCharacterHistoryComponent::BuildSaveData(FMOCharacterHistorySaveData& OutSaveData) const
{
	OutSaveData.Entries = Entries;
	OutSaveData.Relationships = Relationships;
	OutSaveData.bHasValidData = true;
}

bool UMOCharacterHistoryComponent::ApplySaveDataAuthority(const FMOCharacterHistorySaveData& InSaveData)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !InSaveData.bHasValidData)
	{
		return false;
	}
	Entries = InSaveData.Entries;
	if (Entries.Num() > MaxEntries)
	{
		Entries.RemoveAt(0, Entries.Num() - MaxEntries);
	}
	Relationships = InSaveData.Relationships;
	return true;
}
