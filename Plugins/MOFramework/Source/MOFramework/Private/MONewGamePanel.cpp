#include "MONewGamePanel.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOGameSettings.h"
#include "MOSpawnManagerSubsystem.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"

UMONewGamePanel::UMONewGamePanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UMONewGamePanel::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] NativeConstruct called"));

	// Bind button events
	if (RandomSeedButton)
	{
		RandomSeedButton->OnClicked().RemoveAll(this);
		RandomSeedButton->OnClicked().AddUObject(this, &UMONewGamePanel::HandleRandomSeedClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] RandomSeedButton bound"));
	}

	if (StartGameButton)
	{
		StartGameButton->OnClicked().RemoveAll(this);
		StartGameButton->OnClicked().AddUObject(this, &UMONewGamePanel::HandleStartGameClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] StartGameButton bound"));
	}

	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &UMONewGamePanel::HandleBackClicked);
		UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] BackButton bound"));
	}

	// Bind text box commit event
	if (SeedInputBox)
	{
		SeedInputBox->OnTextCommitted.RemoveDynamic(this, &UMONewGamePanel::HandleSeedTextCommitted);
		SeedInputBox->OnTextCommitted.AddDynamic(this, &UMONewGamePanel::HandleSeedTextCommitted);
	}

	// Generate initial random seed and default world name
	GenerateRandomSeed();
	GenerateDefaultWorldName();
}

void UMONewGamePanel::NativeDestruct()
{
	// Clean up button bindings
	if (RandomSeedButton)
	{
		RandomSeedButton->OnClicked().RemoveAll(this);
	}
	if (StartGameButton)
	{
		StartGameButton->OnClicked().RemoveAll(this);
	}
	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
	}
	if (SeedInputBox)
	{
		SeedInputBox->OnTextCommitted.RemoveDynamic(this, &UMONewGamePanel::HandleSeedTextCommitted);
	}

	Super::NativeDestruct();
}

UWidget* UMONewGamePanel::NativeGetDesiredFocusTarget() const
{
	// Focus the world name input box first
	if (WorldNameInputBox)
	{
		return WorldNameInputBox;
	}
	if (SeedInputBox)
	{
		return SeedInputBox;
	}
	if (StartGameButton)
	{
		return StartGameButton;
	}
	return nullptr;
}

FReply UMONewGamePanel::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape never reaches this bubble-phase handler — the main menu (an
	// ancestor in the preview path) consumes it via NativeOnCloseKeyRequested
	// and closes this panel through CloseFocusPanel.

	// Enter starts game (if not focused on text input)
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		// Check if any text input has focus - if so, let it handle Enter
		if ((SeedInputBox && SeedInputBox->HasKeyboardFocus()) ||
			(WorldNameInputBox && WorldNameInputBox->HasKeyboardFocus()))
		{
			return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
		}

		HandleStartGameClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMONewGamePanel::GenerateRandomSeed()
{
	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (!Settings)
	{
		return;
	}

	const int32 NewSeed = Settings->GenerateRandomSeed();

	// Update UI
	if (SeedInputBox)
	{
		SeedInputBox->SetText(FText::AsNumber(NewSeed));
	}

	if (SeedPreviewText)
	{
		SeedPreviewText->SetText(FText::Format(NSLOCTEXT("MONewGame", "SeedPreview", "Seed: {0}"), FText::AsNumber(NewSeed)));
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] Generated random seed: %d"), NewSeed);
}

int32 UMONewGamePanel::GetCurrentSeed() const
{
	if (!SeedInputBox)
	{
		return 0;
	}

	const FString SeedText = SeedInputBox->GetText().ToString();
	return UMOGameSettings::SeedFromString(SeedText);
}

void UMONewGamePanel::SetSeed(int32 Seed)
{
	if (SeedInputBox)
	{
		SeedInputBox->SetText(FText::AsNumber(Seed));
	}

	if (SeedPreviewText)
	{
		SeedPreviewText->SetText(FText::Format(NSLOCTEXT("MONewGame", "SeedPreview", "Seed: {0}"), FText::AsNumber(Seed)));
	}

	// Update settings
	if (UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings())
	{
		Settings->PendingWorldSeed = Seed;
	}
}

void UMONewGamePanel::HandleRandomSeedClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] Random Seed button clicked"));
	GenerateRandomSeed();
}

void UMONewGamePanel::HandleStartGameClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] Start Game button clicked"));

	// Get world name and seed from input and store in settings
	const FString WorldName = GetWorldName();
	const int32 Seed = GetCurrentSeed();

	UMOGameSettings* Settings = UMOGameSettings::GetMOGameSettings();
	if (Settings)
	{
		Settings->PendingWorldName = WorldName;
		Settings->PendingWorldSeed = Seed;

		// Pre-generate the starting survivor's name so the save slot can use
		// it (e.g. "Alex_Smith-01"). Spawn manager consumes
		// Settings->PendingSurvivorName when the first survivor pawn spawns.
		const FString SurvivorName = UMOSpawnManagerSubsystem::GenerateRandomSurvivorName();
		Settings->PendingSurvivorName = SurvivorName;

		// Build slot name from the survivor name (sanitized) + iteration suffix.
		// Format: "<First>_<Last>-01". Filesystem-safe characters only.
		auto Sanitize = [](FString In) -> FString
		{
			In = In.Replace(TEXT(" "), TEXT("_"));
			In = In.Replace(TEXT("/"), TEXT("_"));
			In = In.Replace(TEXT("\\"), TEXT("_"));
			In = In.Replace(TEXT(":"), TEXT("_"));
			In = In.Replace(TEXT("*"), TEXT("_"));
			In = In.Replace(TEXT("?"), TEXT("_"));
			In = In.Replace(TEXT("\""), TEXT("_"));
			In = In.Replace(TEXT("<"), TEXT("_"));
			In = In.Replace(TEXT(">"), TEXT("_"));
			In = In.Replace(TEXT("|"), TEXT("_"));
			return In;
		};

		const FString SanitizedSurvivor = Sanitize(SurvivorName);
		const FString BaseSlot = FString::Printf(TEXT("%s-01"), *SanitizedSurvivor);

		// If "-01" already exists in the save directory, bump to -02, -03, etc.
		// Iteration is local to MONewGamePanel — the save subsystem will
		// overwrite-on-confirm if the player picks the same name twice.
		FString SlotName = BaseSlot;
		int32 Iter = 2;
		while (UGameplayStatics::DoesSaveGameExist(SlotName, 0) && Iter < 100)
		{
			SlotName = FString::Printf(TEXT("%s-%02d"), *SanitizedSurvivor, Iter++);
		}

		Settings->PendingNewGameSlot = SlotName;

		UE_LOG(LogMOFramework, Log,
			TEXT("[MONewGamePanel] World='%s' Seed=%d Survivor='%s' Slot='%s'"),
			*WorldName, Seed, *SurvivorName, *SlotName);
	}

	// Broadcast start game request
	OnStartGameRequested.Broadcast();
}

void UMONewGamePanel::HandleBackClicked()
{
	UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] Back button clicked"));
	OnRequestClose.Broadcast();
}

void UMONewGamePanel::HandleSeedTextCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// Update preview when text is committed
	const int32 Seed = UMOGameSettings::SeedFromString(Text.ToString());

	if (SeedPreviewText)
	{
		SeedPreviewText->SetText(FText::Format(NSLOCTEXT("MONewGame", "SeedPreview", "Seed: {0}"), FText::AsNumber(Seed)));
	}

	// If committed via Enter, optionally start game
	if (CommitMethod == ETextCommit::OnEnter)
	{
		UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] Seed committed via Enter: %d"), Seed);
		// Don't auto-start - let user click Start Game button
	}
}

FString UMONewGamePanel::GetWorldName() const
{
	if (!WorldNameInputBox)
	{
		return TEXT("New World");
	}

	FString Name = WorldNameInputBox->GetText().ToString().TrimStartAndEnd();
	if (Name.IsEmpty())
	{
		return TEXT("New World");
	}
	return Name;
}

void UMONewGamePanel::SetWorldName(const FString& Name)
{
	if (WorldNameInputBox)
	{
		WorldNameInputBox->SetText(FText::FromString(Name));
	}
}

void UMONewGamePanel::GenerateDefaultWorldName()
{
	// Generate a default name like "World 1", "World 2", etc.
	// For now just use a timestamp-based name
	static int32 WorldCounter = 1;
	const FString DefaultName = FString::Printf(TEXT("World %d"), WorldCounter++);

	SetWorldName(DefaultName);
	UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] Generated default world name: %s"), *DefaultName);
}
