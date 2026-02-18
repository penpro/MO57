#include "MOGameInstance.h"
#include "MOFramework.h"
#include "MOGameSettings.h"
#include "MoviePlayer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SOverlay.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Slate/DeferredCleanupSlateBrush.h"

// Simple loading screen Slate widget
class SMOLoadingScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMOLoadingScreen) {}
		SLATE_ARGUMENT(TSharedPtr<FDeferredCleanupSlateBrush>, DeferredBrush)
		SLATE_ARGUMENT(FText, TipText)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		// Store the deferred brush to keep it alive
		DeferredBrush = InArgs._DeferredBrush;

		// Get the actual brush pointer (or fallback)
		const FSlateBrush* BrushToUse = FDeferredCleanupSlateBrush::TrySlateBrush(DeferredBrush);
		const bool bHasBackground = (BrushToUse != nullptr);

		ChildSlot
		[
			SNew(SOverlay)
			// Background
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(bHasBackground ? BrushToUse : FCoreStyle::Get().GetBrush("GenericWhiteBox"))
				.BorderBackgroundColor(bHasBackground ? FLinearColor::White : FLinearColor(0.02f, 0.02f, 0.02f, 1.0f))
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
			]
			// Loading text
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("MOLoading", "Loading", "Loading..."))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
				.ColorAndOpacity(FLinearColor::White)
			]
			// Tip text at bottom
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(50.0f, 50.0f, 50.0f, 100.0f))
			[
				SNew(STextBlock)
				.Text(InArgs._TipText)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
			]
		];
	}

private:
	// Keep the deferred brush alive for the lifetime of the widget
	TSharedPtr<FDeferredCleanupSlateBrush> DeferredBrush;
};

void UMOGameInstance::Init()
{
	Super::Init();

	// Reset intro playback flag on fresh game launch
	// This ensures the intro plays when the game starts, but not when
	// returning to main menu from gameplay (which sets bPlayIntro = false)
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Settings->bPlayIntro = true;
		UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] Init - Reset bPlayIntro to true for fresh game launch"));
	}

	// Bind loading screen delegates
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UMOGameInstance::BeginLoadingScreen);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UMOGameInstance::EndLoadingScreen);

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] Bound loading screen delegates"));
}

void UMOGameInstance::Shutdown()
{
	// Unbind loading screen delegates
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	Super::Shutdown();
}

void UMOGameInstance::BeginLoadingScreen(const FString& MapName)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] BeginLoadingScreen for map: %s"), *MapName);

	// Set up loading screen attributes
	FLoadingScreenAttributes LoadingScreen;
	LoadingScreen.bAutoCompleteWhenLoadingCompletes = true;
	LoadingScreen.bMoviesAreSkippable = false;
	LoadingScreen.bWaitForManualStop = false;
	LoadingScreen.MinimumLoadingScreenDisplayTime = MinLoadingScreenTime;
	LoadingScreen.PlaybackType = EMoviePlaybackType::MT_Normal;

	// Create and set loading widget
	LoadingScreen.WidgetLoadingScreen = CreateLoadingScreenWidget();

	GetMoviePlayer()->SetupLoadingScreen(LoadingScreen);
	bLoadingScreenShowing = true;
}

void UMOGameInstance::EndLoadingScreen(UWorld* InLoadedWorld)
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOGameInstance] EndLoadingScreen for world: %s"),
		InLoadedWorld ? *InLoadedWorld->GetName() : TEXT("None"));

	bLoadingScreenShowing = false;
}

TSharedRef<SWidget> UMOGameInstance::CreateLoadingScreenWidget()
{
	// Load background texture if specified
	TSharedPtr<FDeferredCleanupSlateBrush> DeferredBrush;
	if (!LoadingScreenBackground.IsNull())
	{
		if (UTexture2D* Texture = LoadingScreenBackground.LoadSynchronous())
		{
			// Create a deferred cleanup brush so it persists during loading
			DeferredBrush = FDeferredCleanupSlateBrush::CreateBrush(Texture);
		}
	}

	// Pick a random loading tip
	FText TipText;
	if (LoadingTips.Num() > 0)
	{
		const int32 TipIndex = FMath::RandRange(0, LoadingTips.Num() - 1);
		TipText = LoadingTips[TipIndex];
	}

	return SNew(SMOLoadingScreen)
		.DeferredBrush(DeferredBrush)
		.TipText(TipText);
}
