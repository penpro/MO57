/**
 * =============================================================================
 * MOSessionListEntry.h - One Row In The Session-Browser List
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 *
 * PURPOSE:
 * UMOListEntryBase subclass for the Join Game panel's session list. Displays one
 * FMOFoundSessionInfo (host name, player count, ping) and disables itself when
 * the session is full.
 *
 * =============================================================================
 * RELATED FILES: MOSessionListWidget.h, MOSessionSubsystem.h, MOJoinGamePanel.h
 * LAST UPDATED: 2026-09-22
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOListEntryBase.h"
#include "MOSessionSubsystem.h"
#include "MOSessionListEntry.generated.h"

class UTextBlock;

UCLASS(Abstract, Blueprintable)
class MOFRAMEWORK_API UMOSessionListEntry : public UMOListEntryBase
{
	GENERATED_BODY()

public:
	/**
	 * Bind this row to a found session. Sets the entry ID to the session's
	 * ResultIndex (stringified, matching UMOSessionListWidget's keying) and
	 * formats the display text. Disables the row when the session is full.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|UI|Multiplayer")
	void SetSessionInfo(const FMOFoundSessionInfo& InSessionInfo);

	UFUNCTION(BlueprintPure, Category="MO|UI|Multiplayer")
	const FMOFoundSessionInfo& GetSessionInfo() const { return SessionInfo; }

private:
	/** Host's display name, e.g. "Wes's Camp". */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SessionNameText;

	/** "2/4 players — 45 ms" style summary line. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SessionDetailText;

	FMOFoundSessionInfo SessionInfo;
};
