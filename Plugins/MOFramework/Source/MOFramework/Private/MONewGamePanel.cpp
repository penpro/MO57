#include "MONewGamePanel.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "MOGameSettings.h"
#include "MOSpawnManagerSubsystem.h"
#include "MOUIUtils.h"
#include "MOWorldCreationWidgets.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOBiomeDefinitionRow.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"

UMONewGamePanel::UMONewGamePanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void UMONewGamePanel::NativeConstruct()
{
	Super::NativeConstruct();

	// DEBUG (MONewGamePanel, disabled Oct 18): re-enable to trace setup steps
	// UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] NativeConstruct called"));

	// Bind button events
	if (RandomSeedButton)
	{
		RandomSeedButton->OnClicked().RemoveAll(this);
		RandomSeedButton->OnClicked().AddUObject(this, &UMONewGamePanel::HandleRandomSeedClicked);
		// DEBUG (MONewGamePanel, disabled Oct 18): re-enable to trace setup steps
		// UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] RandomSeedButton bound"));
	}

	if (StartGameButton)
	{
		StartGameButton->OnClicked().RemoveAll(this);
		StartGameButton->OnClicked().AddUObject(this, &UMONewGamePanel::HandleStartGameClicked);
		// DEBUG (MONewGamePanel, disabled Oct 18): re-enable to trace setup steps
		// UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] StartGameButton bound"));
	}

	if (BackButton)
	{
		BackButton->OnClicked().RemoveAll(this);
		BackButton->OnClicked().AddUObject(this, &UMONewGamePanel::HandleBackClicked);
		// DEBUG (MONewGamePanel, disabled Oct 18): re-enable to trace setup steps
		// UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] BackButton bound"));
	}

	UMOUIUtils::ApplyReadableTextInputStyle(WorldNameInputBox);
	UMOUIUtils::ApplyReadableTextInputStyle(SeedInputBox);

	// Bind text box commit event
	if (SeedInputBox)
	{
		SeedInputBox->OnTextCommitted.RemoveDynamic(this, &UMONewGamePanel::HandleSeedTextCommitted);
		SeedInputBox->OnTextCommitted.AddDynamic(this, &UMONewGamePanel::HandleSeedTextCommitted);
	}

	if (ResetCreationButton)
	{
		ResetCreationButton->OnClicked().RemoveAll(this);
		ResetCreationButton->OnClicked().AddUObject(this, &UMONewGamePanel::HandleResetCreationClicked);
	}

	// Generate initial random seed and default world name
	GenerateRandomSeed();
	GenerateDefaultWorldName();

	// A new world starts as authored (every knob 1, the coast) until the player changes it.
	Creation = FMOWorldCreationSettings();
	BuildWorldCreationControls();
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
	if (ResetCreationButton)
	{
		ResetCreationButton->OnClicked().RemoveAll(this);
	}
	for (UMOWorldKnobRow* Row : KnobRows)
	{
		if (Row)
		{
			Row->OnKnobChanged.RemoveAll(this);
		}
	}
	for (UMOBiomeCard* Card : BiomeCards)
	{
		if (Card)
		{
			Card->OnPicked.RemoveAll(this);
		}
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
		SeedInputBox->SetText(UMOGameSettings::SeedToText(NewSeed));
	}

	if (SeedPreviewText)
	{
		SeedPreviewText->SetText(FText::Format(NSLOCTEXT("MONewGame", "SeedPreview", "Seed: {0}"), UMOGameSettings::SeedToText(NewSeed)));
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
		SeedInputBox->SetText(UMOGameSettings::SeedToText(Seed));
	}

	if (SeedPreviewText)
	{
		SeedPreviewText->SetText(FText::Format(NSLOCTEXT("MONewGame", "SeedPreview", "Seed: {0}"), UMOGameSettings::SeedToText(Seed)));
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
		Settings->PendingWorldCreation = Creation;   // the game mode consumes it once (MOWorldCreationTypes.h)
		UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] World creation: %s"), *FMOWorldCreation::Describe(Creation));

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
		SeedPreviewText->SetText(FText::Format(NSLOCTEXT("MONewGame", "SeedPreview", "Seed: {0}"), UMOGameSettings::SeedToText(Seed)));
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

// ============================================================================
// WORLD CREATION (MOWorldCreationTypes.h)
// ============================================================================

void UMONewGamePanel::BuildWorldCreationControls()
{
	KnobRows.Reset();
	if (ShapeKnobsBox)
	{
		ShapeKnobsBox->ClearChildren();
	}
	if (PaceKnobsBox)
	{
		PaceKnobsBox->ClearChildren();
	}
	if (UClass* RowClass = (ShapeKnobsBox || PaceKnobsBox) ? KnobRowClass.LoadSynchronous() : nullptr)
	{
		for (const FMOWorldKnob& Knob : FMOWorldCreation::GetKnobs())
		{
			UPanelWidget* Box = Knob.Group == EMOWorldKnobGroup::Shape ? ShapeKnobsBox : PaceKnobsBox;
			UMOWorldKnobRow* Row = Box ? CreateWidget<UMOWorldKnobRow>(this, RowClass) : nullptr;
			if (!Row)
			{
				continue;
			}
			if (UVerticalBoxSlot* RowSlot = Cast<UVerticalBoxSlot>(Box->AddChild(Row)))
			{
				RowSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
			}
			Row->Configure(Knob, FMOWorldCreation::GetKnob(Creation, Knob.Id));
			Row->OnKnobChanged.AddUObject(this, &UMONewGamePanel::HandleKnobChanged);
			KnobRows.Add(Row);
		}
	}

	// The starting biomes: the coast (the default), then every DT_Biomes row that has a preview picture, in the table's order.
	BiomeCards.Reset();
	UClass* CardClass = BiomeCardsBox ? BiomeCardClass.LoadSynchronous() : nullptr;
	if (CardClass)
	{
		BiomeCardsBox->ClearChildren();
		auto AddCard = [this, CardClass](FName Id, const FText& Name, UTexture2D* Image)
		{
			UMOBiomeCard* Card = CreateWidget<UMOBiomeCard>(this, CardClass);
			if (!Card)
			{
				return;
			}
			if (UHorizontalBoxSlot* CardSlot = Cast<UHorizontalBoxSlot>(BiomeCardsBox->AddChild(Card)))
			{
				CardSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
			}
			Card->Configure(Id, Name, Image);
			Card->OnPicked.AddUObject(this, &UMONewGamePanel::HandleBiomePicked);
			BiomeCards.Add(Card);
		};
		AddCard(NAME_None, NSLOCTEXT("MONewGame", "Coast", "The coast"), CoastPreviewImage.LoadSynchronous());
		TArray<FName> BiomeIds;
		UMOBiomeDatabaseSettings::GetAllBiomeIds(BiomeIds);
		for (const FName& Id : BiomeIds)
		{
			const FMOBiomeDefinitionRow* Row = UMOBiomeDatabaseSettings::GetBiomeDefinition(Id);
			if (Row && !Row->PreviewImage.IsNull())
			{
				AddCard(Id, Row->DisplayName.IsEmpty() ? FText::FromName(Id) : Row->DisplayName, Row->PreviewImage.LoadSynchronous());
			}
		}
	}
	UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] world creation: %d knob row(s), %d starting biome card(s)"), KnobRows.Num(), BiomeCards.Num());
	RefreshWorldCreationControls();
}

void UMONewGamePanel::RefreshWorldCreationControls()
{
	for (UMOWorldKnobRow* Row : KnobRows)
	{
		if (Row)
		{
			Row->SetValue(FMOWorldCreation::GetKnob(Creation, Row->GetKnobId()));
		}
	}
	FText StartName = NSLOCTEXT("MONewGame", "Coast", "The coast");
	for (UMOBiomeCard* Card : BiomeCards)
	{
		if (!Card)
		{
			continue;
		}
		const bool bPicked = Card->GetBiomeId() == Creation.StartingBiome;
		Card->SetSelected(bPicked);
		if (bPicked && !Creation.StartingBiome.IsNone())
		{
			const FMOBiomeDefinitionRow* Row = UMOBiomeDatabaseSettings::GetBiomeDefinition(Creation.StartingBiome);
			StartName = Row && !Row->DisplayName.IsEmpty() ? Row->DisplayName : FText::FromName(Creation.StartingBiome);
		}
	}
	if (StartingBiomeText)
	{
		StartingBiomeText->SetText(FText::Format(NSLOCTEXT("MONewGame", "StartingIn", "Starting in: {0}"), StartName));
	}
}

void UMONewGamePanel::ResetWorldCreation()
{
	Creation = FMOWorldCreationSettings();
	RefreshWorldCreationControls();
}

void UMONewGamePanel::HandleResetCreationClicked()
{
	ResetWorldCreation();
}

void UMONewGamePanel::HandleKnobChanged(FName KnobId, float Value)
{
	FMOWorldCreation::SetKnob(Creation, KnobId, Value);
}

void UMONewGamePanel::HandleBiomePicked(FName BiomeId)
{
	Creation.StartingBiome = BiomeId;
	UE_LOG(LogMOFramework, Log, TEXT("[MONewGamePanel] starting biome: %s"), BiomeId.IsNone() ? TEXT("the coast") : *BiomeId.ToString());
	RefreshWorldCreationControls();
}
