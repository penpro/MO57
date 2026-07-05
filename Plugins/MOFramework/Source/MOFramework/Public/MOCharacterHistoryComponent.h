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

	UPROPERTY(BlueprintAssignable, Category="MO|History")
	FMOOnHistoryEntryAdded OnEntryAdded;

	/** Ring-buffer cap; oldest entries drop past this. */
	UPROPERTY(EditAnywhere, Category="MO|History", meta=(ClampMin="10"))
	int32 MaxEntries = 100;

private:
	UPROPERTY()
	TArray<FMOCharacterHistoryEntry> Entries;
};
