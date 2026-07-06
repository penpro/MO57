/**
 * =============================================================================
 * MOCharacterHistoryComponent.h - Per-character event log (V1 settlement loop)
 * =============================================================================
 *
 * PURPOSE:
 * The character's life record: meals eaten, mood swings, jobs done, wounds —
 * the raw material for "characters feel like people" (colony design pillar).
 * Entries use the existing FMOCharacterHistoryEntry/EHistoryEntryType from
 * MOColonyTypes.h. Ring-buffered (newest kept), server-authoritative.
 *
 * REPLICATION: server-only for V1 — the colony UI reads it through
 * server-side queries ([MOQUERY] / subsystem). Replicating a growing log is
 * a V-track follow-on (flagged), not a default cost on every pawn.
 *
 * =============================================================================
 * RELATED FILES: MOColonyTypes.h, MOColonyManagerSubsystem.h
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOColonyTypes.h"

#include "MOCharacterHistoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOOnHistoryEntryAdded, const FMOCharacterHistoryEntry&, Entry);

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOCharacterHistoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOCharacterHistoryComponent();

	/** Append an event (authority only). Timestamp = current game clock time. */
	UFUNCTION(BlueprintCallable, Category="MO|History")
	void AddEntry(EHistoryEntryType EntryType, const FString& Description);

	UFUNCTION(BlueprintPure, Category="MO|History")
	const TArray<FMOCharacterHistoryEntry>& GetEntries() const { return Entries; }

	/** Newest N entries, newest first (UI convenience). */
	UFUNCTION(BlueprintCallable, Category="MO|History")
	TArray<FMOCharacterHistoryEntry> GetRecentEntries(int32 Count) const;

	UFUNCTION(BlueprintCallable, Category="MO|History|Save")
	void BuildSaveData(FMOCharacterHistorySaveData& OutSaveData) const;

	UFUNCTION(BlueprintCallable, Category="MO|History|Save")
	bool ApplySaveDataAuthority(const FMOCharacterHistorySaveData& InSaveData);

	// =========================================================================
	// RELATIONSHIP GRAPH (V2.3) — grown by REAL shared time, not scripts.
	// =========================================================================

	/**
	 * THE bond function — pure math for headless tests. Strength approaches 1
	 * asymptotically with shared hours (fast early, slow late), and drifts
	 * back toward 0 while apart (friendships fade, slowly).
	 */
	UFUNCTION(BlueprintPure, Category="MO|History|Relationships")
	static float ComputeStrengthDelta(float CurrentStrength, float SharedGameHours,
		float ApartGameHours, float GrowPerSharedHour = 0.02f, float DriftPerApartHour = 0.001f);

	/** Accrue shared time with another character (authority). Grows Strength;
	 *  auto-types Friend past the threshold if untyped. */
	UFUNCTION(BlueprintCallable, Category="MO|History|Relationships")
	void AddSharedTime(const FGuid& OtherGuid, float GameHours);

	/** Drift all bonds NOT in the given co-located set (authority). */
	void ApplyApartDrift(const TSet<FGuid>& CoLocated, float GameHours);

	/** Author a typed relationship both ways is the caller's job (authority). */
	UFUNCTION(BlueprintCallable, Category="MO|History|Relationships")
	void SetRelationshipType(const FGuid& OtherGuid, ERelationshipType Type);

	UFUNCTION(BlueprintPure, Category="MO|History|Relationships")
	FMOCharacterRelationship GetRelationship(const FGuid& OtherGuid) const;

	UFUNCTION(BlueprintPure, Category="MO|History|Relationships")
	const TArray<FMOCharacterRelationship>& GetRelationships() const { return Relationships; }

	/** Mean Strength toward the given characters (0 when none known) —
	 *  the STANDING recruitment gates on. */
	UFUNCTION(BlueprintCallable, Category="MO|History|Relationships")
	float GetAverageStandingWith(const TArray<FGuid>& Others) const;

	/** Strength at which an untyped bond becomes Friend. */
	UPROPERTY(EditAnywhere, Category="MO|History|Relationships")
	float FriendThreshold = 0.35f;

		UPROPERTY(BlueprintAssignable, Category="MO|History")
	FMOOnHistoryEntryAdded OnEntryAdded;

	/** Ring-buffer cap; oldest entries drop past this. */
	UPROPERTY(EditAnywhere, Category="MO|History", meta=(ClampMin="10"))
	int32 MaxEntries = 100;

private:
	FMOCharacterRelationship* FindOrAddRelationship(const FGuid& OtherGuid);

	UPROPERTY()
	TArray<FMOCharacterHistoryEntry> Entries;

	UPROPERTY()
	TArray<FMOCharacterRelationship> Relationships;
};
