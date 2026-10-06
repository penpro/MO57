/**
 * =============================================================================
 * MOSessionListWidget.h - Session-Browser List (Multiplayer Join Tab)
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * Thin UMOScrollListBase specialization for the Join Game panel's session list.
 * The base class only knows FName entry ids; this widget keeps the actual
 * FMOFoundSessionInfo data on the side (keyed by the same ResultIndex-derived
 * FName each UMOSessionListEntry row uses) and hands it to each row via
 * ConfigureEntry.
 *
 * =============================================================================
 * RELATED FILES: MOScrollListBase.h, MOSessionListEntry.h, MOSessionSubsystem.h,
 *                MOJoinGamePanel.h
 * LAST UPDATED: 2026-09-22
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOScrollListBase.h"
#include "MOSessionSubsystem.h"
#include "MOSessionListWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOSessionListWidget : public UMOScrollListBase
{
	GENERATED_BODY()

public:
	/** Replace the list's contents with a fresh set of search results. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Multiplayer")
	void SetSessionResults(const TArray<FMOFoundSessionInfo>& Results);

	/** Look up the currently-selected row's full session info, if any. */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Multiplayer")
	bool GetSelectedSessionInfo(FMOFoundSessionInfo& OutInfo) const;

protected:
	virtual void ConfigureEntry_Implementation(UMOListEntryBase* Entry, FName EntryId) override;

private:
	TMap<FName, FMOFoundSessionInfo> SessionsById;
};
