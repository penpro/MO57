#pragma once

#include "CoreMinimal.h"
#include "MOItemSourceAnalysis.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MOItemSourceSubsystem.generated.h"

/**
 * Where an item comes from, in a player's words (N-255; Wes, Nov 1: "the crude chopper takes stone, idk where to get stone"). The recipe and
 * building panels show it under each ingredient the player lacks: "Find it: Pick up loose stones; Boulder: Break off a building stone".
 *
 * The answers are FMOItemSourceAnalysis's (every reachable way to get each item, from the data the game itself runs on), worked out once
 * while the first game world loads (~1.4 s: on first use it was a hitch the first time a panel showed a missing ingredient) and kept for
 * the session.
 */
UCLASS()
class MOFRAMEWORK_API UMOItemSourceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	static UMOItemSourceSubsystem* Get(const UObject* WorldContextObject);

	/** "Find it: <way>; <way>; ..." -- the easiest ways first, at most MaxWays; empty when nothing in the world gives the item. */
	UFUNCTION(BlueprintCallable, Category="MO|Crafting")
	FText GetWhereToFind(FName ItemId, int32 MaxWays = 3);

	/** The ways themselves, easiest first. */
	TArray<FMOItemSourceHint> GetWays(FName ItemId);

	/** How long the analysis took (ms), 0 before it has run. */
	double GetAnalysisMs() const { return AnalysisMs; }

private:
	void EnsureAnalyzed();

	bool bAnalyzed = false;
	double AnalysisMs = 0.0;
	TMap<FName, TArray<FMOItemSourceHint>> Hints;
};
