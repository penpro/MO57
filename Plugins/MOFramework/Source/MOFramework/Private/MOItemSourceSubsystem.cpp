#include "MOItemSourceSubsystem.h"

#include "MOFramework.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"

UMOItemSourceSubsystem* UMOItemSourceSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UMOItemSourceSubsystem>() : nullptr;
}

void UMOItemSourceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Worked out while a game world loads (its actors initialized, the loading screen still up), not on the first panel that asks.
	FWorldDelegates::OnWorldInitializedActors.AddWeakLambda(this, [this](const FActorsInitializedParams& Params)
	{
		if (Params.World && Params.World->IsGameWorld() && Params.World->GetGameInstance() == GetGameInstance())
		{
			EnsureAnalyzed();
		}
	});
}

void UMOItemSourceSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.RemoveAll(this);
	Super::Deinitialize();
}

void UMOItemSourceSubsystem::EnsureAnalyzed()
{
	if (bAnalyzed)
	{
		return;
	}
	bAnalyzed = true;
	const double Start = FPlatformTime::Seconds();
	FMOItemSourceReport Report = FMOItemSourceAnalysis::Analyze(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	Hints = MoveTemp(Report.Hints);
	for (TPair<FName, TArray<FMOItemSourceHint>>& Item : Hints)
	{
		Item.Value.StableSort([](const FMOItemSourceHint& A, const FMOItemSourceHint& B) { return A.Tier < B.Tier; });
	}
	AnalysisMs = (FPlatformTime::Seconds() - Start) * 1000.0;
	UE_LOG(LogMOFramework, Log, TEXT("[MOItemSources] where-to-find for %d items worked out in %.0f ms"), Hints.Num(), AnalysisMs);
}

TArray<FMOItemSourceHint> UMOItemSourceSubsystem::GetWays(FName ItemId)
{
	EnsureAnalyzed();
	const TArray<FMOItemSourceHint>* Ways = Hints.Find(ItemId);
	return Ways ? *Ways : TArray<FMOItemSourceHint>();
}

FText UMOItemSourceSubsystem::GetWhereToFind(FName ItemId, int32 MaxWays)
{
	const TArray<FMOItemSourceHint> Ways = GetWays(ItemId);
	if (Ways.Num() == 0)
	{
		return FText::GetEmpty();
	}
	TArray<FText> Shown;
	for (int32 i = 0; i < Ways.Num() && i < FMath::Max(1, MaxWays); ++i)
	{
		Shown.Add(Ways[i].Label);
	}
	return FText::Format(NSLOCTEXT("MOItemSources", "FindIt", "Find it: {0}"), FText::Join(NSLOCTEXT("MOItemSources", "Sep", "; "), Shown));
}

#if !UE_BUILD_SHIPPING
// MO.Test.WhereToFind <ItemId>: what the panels would say under that ingredient, and what the one-off analysis cost.
static FAutoConsoleCommandWithWorldAndArgs GMOWhereToFindCmd(
	TEXT("MO.Test.WhereToFind"),
	TEXT("Where an item comes from, as the crafting panels say it. Usage: MO.Test.WhereToFind <ItemId>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		UMOItemSourceSubsystem* Sources = UMOItemSourceSubsystem::Get(World);
		if (!Sources || Args.Num() < 1)
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] FAIL WhereToFind: usage MO.Test.WhereToFind <ItemId> (and a game instance)"));
			return;
		}
		const FText Text = Sources->GetWhereToFind(FName(*Args[0]), 8);
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTEST] WhereToFind %s ways=%d analysisMs=%.0f text=%s"), *Args[0],
			Sources->GetWays(FName(*Args[0])).Num(), Sources->GetAnalysisMs(), *Text.ToString());
	}));
#endif
