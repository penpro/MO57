/**
 * MOAudioSubsystem.cpp - Audio System Coordinator
 */

#include "MOAudioSubsystem.h"
#include "MOAudioSettings.h"
#include "MOFramework.h"
#include "MOGameClockSubsystem.h"
#include "MOMainMenuGameMode.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"

// =============================================================================
// LIFECYCLE
// =============================================================================

UMOAudioSubsystem* UMOAudioSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	const UWorld* World = WorldContextObject->GetWorld();
	if (!World) return nullptr;
	const UGameInstance* GI = World->GetGameInstance();
	return GI ? GI->GetSubsystem<UMOAudioSubsystem>() : nullptr;
}

void UMOAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UMOAudioSettings* Settings = GetDefault<UMOAudioSettings>();
	if (Settings)
	{
		MasterVolume = Settings->DefaultMasterVolume;
		MusicVolume = Settings->DefaultMusicVolume;
		AmbientVolume = Settings->DefaultAmbientVolume;
		SFXVolume = Settings->DefaultSFXVolume;
		UIVolume = Settings->DefaultUIVolume;
	}

	// Try to load the default audio bank if configured. Lazy — if it's
	// not set, we just don't have a bank; playback no-ops cleanly.
	if (Settings && !Settings->DefaultAudioBank.IsNull())
	{
		if (UDataTable* DefaultBank = Cast<UDataTable>(Settings->DefaultAudioBank.TryLoad()))
		{
			LoadBank(DefaultBank);
			UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] Loaded default bank '%s' (%d rows)"),
				*DefaultBank->GetName(), DefaultBank->GetRowMap().Num());
		}
		else
		{
			UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] Default audio bank failed to load: %s"),
				*Settings->DefaultAudioBank.ToString());
		}
	}

	// Cache the ambient layers table if configured. Subsystem looks up
	// FMOAmbientLayerConfig from this when ambient state changes — multi-
	// layer playback. Falls back to single-track if unset / row missing.
	if (Settings && !Settings->AmbientLayersTable.IsNull())
	{
		CachedAmbientLayersTable = Cast<UDataTable>(Settings->AmbientLayersTable.TryLoad());
		if (CachedAmbientLayersTable)
		{
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOAudio] Loaded ambient layers table '%s' (%d configs) — LAYERED PLAYBACK ENABLED"),
				*CachedAmbientLayersTable->GetName(),
				CachedAmbientLayersTable->GetRowMap().Num());
		}
		else
		{
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOAudio] AmbientLayersTable path set but asset failed to load: %s — falling back to SINGLE-TRACK"),
				*Settings->AmbientLayersTable.ToString());
		}
	}
	else
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOAudio] AmbientLayersTable NOT SET in Project Settings — ambient will play single-track only. "
			     "Create DT_AmbientLayers (row struct = MOAmbientLayerConfig), import the generated JSON, "
			     "and assign it in Project Settings > MO Framework > Audio > AmbientLayersTable."));
	}

	// Apply initial volumes to whatever SoundClasses exist. If the soft refs
	// haven't been set up yet (fresh project), these are silent no-ops.
	ApplyVolumeToSoundClass(GetSoundClassForCategory(EMOAudioCategory::Music), MusicVolume * MasterVolume);
	ApplyVolumeToSoundClass(GetSoundClassForCategory(EMOAudioCategory::Ambient), AmbientVolume * MasterVolume);
	ApplyVolumeToSoundClass(GetSoundClassForCategory(EMOAudioCategory::SFX), SFXVolume * MasterVolume);
	ApplyVolumeToSoundClass(GetSoundClassForCategory(EMOAudioCategory::UI), UIVolume * MasterVolume);

	// Build a sensible default attenuation for spatialized ambient events.
	// Used unless the user assigns a custom USoundAttenuation in settings.
	// Profile chosen:
	//   - Sphere shape (omni listener model — ambient events are non-directional)
	//   - Inner radius = 1m (full volume close up)
	//   - Falloff out to settings.DefaultAttenuationFalloffDistance
	//   - NaturalSound distance algorithm (logarithmic, sounds real)
	//   - Low-pass filter that ramps in with distance — distant sounds get
	//     muffled, like through-the-trees cues.
	//   - Air absorption + spread enabled for extra realism
	{
		DefaultEventAttenuation = NewObject<USoundAttenuation>(this, TEXT("DefaultEventAttenuation"));
		FSoundAttenuationSettings& A = DefaultEventAttenuation->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(100.0f, 0.0f, 0.0f); // 1m inner radius
		A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		A.FalloffDistance = Settings ? Settings->DefaultAttenuationFalloffDistance : 6000.0f;
		A.dBAttenuationAtMax = -60.0f;

		// LPF — distant sounds lose high frequencies. Strong sell on "this is far".
		A.bAttenuateWithLPF = true;
		A.LPFRadiusMin = 1000.0f;  // start filtering at 10m
		A.LPFRadiusMax = A.FalloffDistance;
		A.LPFFrequencyAtMin = 22000.0f;
		A.LPFFrequencyAtMax = 1500.0f; // heavy muffle at max distance

		// Air absorption (additional realism — high frequencies attenuate faster).
		A.bEnableListenerFocus = false;
	}

	// PRE-LOAD hook — stop all audio when a level is about to unload. Without
	// this, music + ambient survive across map transitions (because the
	// AudioComponents were spawned with bPersistAcrossLevelTransition=true)
	// and you'd hear in-game music keep playing on the main menu.
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddLambda(
		[this](const FString& MapName)
		{
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOAudio] PreLoadMap('%s') — stopping all audio for transition"),
				*MapName);
			StopAllAudio();
		});

	// POST-LOAD hook — set up new ambient/music based on the new world's
	// context. Delegates to HandleWorldAudioContext so the same code path
	// runs from PostLoadMapWithWorld AND from the "world already loaded
	// when Initialize ran" path below.
	if (Settings && Settings->bAutoSwapDayNightAmbient)
	{
		WorldBeginPlayHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddLambda(
			[this](UWorld* World) { HandleWorldAudioContext(World); });

		// First-launch case: only safe to run if World has actually begun
		// play (otherwise GameMode hasn't been spawned yet and the
		// IsA<AMOMainMenuGameMode> check returns false → silent). When
		// world isn't past BeginPlay, AMOGameMode::BeginPlay calls
		// HandleWorldAudioContext directly — that's the reliable signal.
		if (UWorld* AlreadyLoadedWorld = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		{
			if (AlreadyLoadedWorld->HasBegunPlay())
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MOAudio] World '%s' already past BeginPlay at Initialize — running audio context now"),
					*AlreadyLoadedWorld->GetName());
				HandleWorldAudioContext(AlreadyLoadedWorld);
			}
			else
			{
				UE_LOG(LogMOFramework, Log,
					TEXT("[MOAudio] World '%s' exists but BeginPlay not yet fired — GameMode::BeginPlay will trigger audio context"),
					*AlreadyLoadedWorld->GetName());
			}
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] Initialized — banks=%d, masterVol=%.2f"),
		LoadedBanks.Num(), MasterVolume);
}

void UMOAudioSubsystem::Deinitialize()
{
	if (WorldBeginPlayHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(WorldBeginPlayHandle);
		WorldBeginPlayHandle.Reset();
	}

	if (PreLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
		PreLoadMapHandle.Reset();
	}

	StopAllAudio();

	LoadedBanks.Reset();
	AudioIdLookup.Reset();

	Super::Deinitialize();
}

void UMOAudioSubsystem::HandleWorldAudioContext(UWorld* World)
{
	if (!World)
	{
		return;
	}

	// Filter to gameplay/PIE worlds — skip editor preview / asset edit worlds.
	if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
	{
		return;
	}

	// Detect main menu via game mode CLASS. Clock subsystem exists in every
	// world so it can't distinguish — game mode is the authoritative signal.
	const AGameModeBase* GM = World->GetAuthGameMode();
	const bool bIsMainMenu = GM && GM->IsA<AMOMainMenuGameMode>();

	if (bIsMainMenu)
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOAudio] World audio context: main menu '%s' — main menu music"),
			*World->GetName());
		SetMusicState(EMOMusicState::MainMenu);
	}
	else if (UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(World))
	{
		Clock->OnDayNightChanged.AddUniqueDynamic(this, &UMOAudioSubsystem::HandleClockDayNightChanged);

		const bool bIsDaytime = Clock->IsDaytime();
		const EMOAmbientState InitialAmbient = bIsDaytime
			? EMOAmbientState::Outdoor_Day
			: EMOAmbientState::Outdoor_Night;
		SetAmbientState(InitialAmbient);

		const EMOMusicState InitialMusic = bIsDaytime
			? EMOMusicState::Exploration_Day
			: EMOMusicState::Exploration_Night;
		SetMusicState(InitialMusic);

		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOAudio] World audio context: gameplay '%s' — ambient=%s music=%s"),
			*World->GetName(),
			*UEnum::GetValueAsString(InitialAmbient),
			*UEnum::GetValueAsString(InitialMusic));
	}
	else
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOAudio] World audio context: '%s' has no MOGameMode/MOMainMenuGameMode — leaving audio silent"),
			*World->GetName());
	}
}

void UMOAudioSubsystem::StopAllAudio()
{
	// Music — stop immediately (no fade), clear component reference.
	if (MusicComponent)
	{
		MusicComponent->Stop();
		MusicComponent = nullptr;
	}

	// Ambient — stop every active layer component.
	for (UAudioComponent* Layer : AmbientLayerComponents)
	{
		if (Layer) Layer->Stop();
	}
	AmbientLayerComponents.Reset();

	// Cancel all event/dominance/breathe/log timers tied to ambient layers.
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		FTimerManager& TM = World->GetTimerManager();
		TM.ClearTimer(EventTimerHandle);
		TM.ClearTimer(DominanceShiftTimerHandle);
		TM.ClearTimer(BaseLayerBreatheTimerHandle);
		TM.ClearTimer(BaseLayerStatusLogTimerHandle);
	}

	// Reset internal state so PostLoadMap starts fresh, and so delayed
	// base-layer respawn timers see bAmbientStreamActive=false and bail.
	bAmbientStreamActive = false;
	CurrentLayerConfig = FMOAmbientLayerConfig();
	CurrentDominantGroupIdx = -1;
	GroupLastFireTimes.Reset();
	CurrentMusicState = EMOMusicState::None;
	CurrentAmbientState = EMOAmbientState::None;
}

// =============================================================================
// STATE MACHINES
// =============================================================================

void UMOAudioSubsystem::SetMusicState(EMOMusicState NewState)
{
	if (NewState == CurrentMusicState)
	{
		return;
	}

	const UMOAudioSettings* Settings = GetDefault<UMOAudioSettings>();
	const float FadeOut = Settings ? Settings->MusicFadeOutSeconds : 2.0f;
	const float FadeIn = Settings ? Settings->MusicFadeInSeconds : 2.0f;

	const EMOMusicState OldState = CurrentMusicState;
	CurrentMusicState = NewState;

	UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] Music state: %s -> %s"),
		*UEnum::GetValueAsString(OldState), *UEnum::GetValueAsString(NewState));

	// Fade out current music regardless of whether we'll have a new track.
	StopMusicWithFade(FadeOut);

	if (NewState == EMOMusicState::None)
	{
		OnMusicStateChanged.Broadcast(OldState, NewState);
		return;
	}

	// MainMenu has a direct settings-driven override (Cedar_Smoke_Psalm by
	// default) — bypass the bank so the menu track doesn't depend on a
	// "Music.MainMenu" row existing in DT_MOSound.
	if (NewState == EMOMusicState::MainMenu && Settings && !Settings->MainMenuMusicOverride.IsNull())
	{
		const FSoftObjectPath OverridePath = Settings->MainMenuMusicOverride;
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOAudio] MainMenu music override active: %s"),
			*OverridePath.ToString());

		FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
		Streamable.RequestAsyncLoad(OverridePath,
			FStreamableDelegate::CreateLambda([this, OverridePath, FadeIn, OldState, NewState]()
			{
				USoundBase* Sound = Cast<USoundBase>(OverridePath.ResolveObject());
				if (Sound)
				{
					// Build a synthetic bank row so StartMusic gets default
					// volume/pitch values.
					FMOAudioBankRow Row;
					Row.Sound = Sound;
					Row.Category = EMOAudioCategory::Music;
					Row.VolumeMultiplier = 1.0f;
					Row.PitchMultiplier = 1.0f;
					StartMusic(Sound, Row, FadeIn);
				}
				else
				{
					UE_LOG(LogMOFramework, Warning,
						TEXT("[MOAudio] MainMenu override asset failed to load: %s"),
						*OverridePath.ToString());
				}
				OnMusicStateChanged.Broadcast(OldState, NewState);
			}));
		return;
	}

	// Standard path — bank lookup. Bank lookup miss = silence (logged), not an error.
	const FName AudioId = MakeMusicAudioId(NewState);
	ResolveAndLoadAsync(AudioId, [this, FadeIn, OldState, NewState](USoundBase* Sound, const FMOAudioBankRow& Row)
	{
		if (Sound)
		{
			StartMusic(Sound, Row, FadeIn);
		}
		OnMusicStateChanged.Broadcast(OldState, NewState);
	});
}

void UMOAudioSubsystem::SetAmbientState(EMOAmbientState NewState)
{
	if (NewState == CurrentAmbientState)
	{
		return;
	}

	const UMOAudioSettings* Settings = GetDefault<UMOAudioSettings>();
	const float FadeOut = Settings ? Settings->AmbientFadeOutSeconds : 3.0f;
	const float FadeIn = Settings ? Settings->AmbientFadeInSeconds : 3.0f;

	const EMOAmbientState OldState = CurrentAmbientState;
	CurrentAmbientState = NewState;

	UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] Ambient state: %s -> %s"),
		*UEnum::GetValueAsString(OldState), *UEnum::GetValueAsString(NewState));

	StopAmbientWithFade(FadeOut);

	if (NewState == EMOAmbientState::None)
	{
		OnAmbientStateChanged.Broadcast(OldState, NewState);
		return;
	}

	// Prefer the layered config if present in the layers table — otherwise
	// fall back to single-track playback against the main audio bank.
	if (FindAmbientLayerConfig(NewState) != nullptr)
	{
		StartAmbientLayered(NewState, FadeIn);
		OnAmbientStateChanged.Broadcast(OldState, NewState);
	}
	else
	{
		const FName AudioId = MakeAmbientAudioId(NewState);
		ResolveAndLoadAsync(AudioId, [this, FadeIn, OldState, NewState](USoundBase* Sound, const FMOAudioBankRow& Row)
		{
			if (Sound)
			{
				StartAmbientSingle(Sound, Row, FadeIn);
			}
			OnAmbientStateChanged.Broadcast(OldState, NewState);
		});
	}
}

// =============================================================================
// ONE-SHOT PLAYBACK
// =============================================================================

bool UMOAudioSubsystem::PlayOneShot2D(FName AudioId)
{
	const FMOAudioBankRow* Row = AudioIdLookup.FindRef(AudioId);
	if (!Row)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] PlayOneShot2D: AudioId '%s' not in bank"),
			*AudioId.ToString());
		return false;
	}

	// Sync-load the asset — one-shots are typically small (UI clicks, stings).
	// Music/ambient go through async load in their state setters.
	USoundBase* Sound = Row->Sound.LoadSynchronous();
	if (!Sound)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] PlayOneShot2D: AudioId '%s' has empty/invalid sound asset"),
			*AudioId.ToString());
		return false;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	UGameplayStatics::PlaySound2D(World, Sound, Row->VolumeMultiplier, Row->PitchMultiplier);
	return true;
}

bool UMOAudioSubsystem::PlayOneShotAtLocation(const UObject* WorldContextObject, FName AudioId, FVector Location)
{
	const FMOAudioBankRow* Row = AudioIdLookup.FindRef(AudioId);
	if (!Row)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] PlayOneShotAtLocation: AudioId '%s' not in bank"),
			*AudioId.ToString());
		return false;
	}

	USoundBase* Sound = Row->Sound.LoadSynchronous();
	if (!Sound)
	{
		return false;
	}

	UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Sound, Location,
		Row->VolumeMultiplier, Row->PitchMultiplier);
	return true;
}

// =============================================================================
// VOLUME CONTROLS
// =============================================================================

void UMOAudioSubsystem::SetMasterVolume(float Volume)
{
	MasterVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyVolumeToSoundClass(GetSoundClassForCategory(EMOAudioCategory::Music), MusicVolume * MasterVolume);
	ApplyVolumeToSoundClass(GetSoundClassForCategory(EMOAudioCategory::Ambient), AmbientVolume * MasterVolume);
	ApplyVolumeToSoundClass(GetSoundClassForCategory(EMOAudioCategory::SFX), SFXVolume * MasterVolume);
	ApplyVolumeToSoundClass(GetSoundClassForCategory(EMOAudioCategory::UI), UIVolume * MasterVolume);
	UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] MasterVolume -> %.2f"), MasterVolume);
}

void UMOAudioSubsystem::SetMusicVolume(float Volume)
{
	MusicVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	USoundClass* SC = GetSoundClassForCategory(EMOAudioCategory::Music);
	ApplyVolumeToSoundClass(SC, MusicVolume * MasterVolume);
	UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] MusicVolume -> %.2f (final=%.2f, soundclass=%s)"),
		MusicVolume, MusicVolume * MasterVolume,
		SC ? *SC->GetName() : TEXT("(none — assets must have Sound Class set)"));
}

void UMOAudioSubsystem::SetAmbientVolume(float Volume)
{
	AmbientVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	USoundClass* SC = GetSoundClassForCategory(EMOAudioCategory::Ambient);
	ApplyVolumeToSoundClass(SC, AmbientVolume * MasterVolume);
	UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] AmbientVolume -> %.2f (final=%.2f, soundclass=%s)"),
		AmbientVolume, AmbientVolume * MasterVolume,
		SC ? *SC->GetName() : TEXT("(none — assets must have Sound Class set)"));
}

void UMOAudioSubsystem::SetSFXVolume(float Volume)
{
	SFXVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	USoundClass* SC = GetSoundClassForCategory(EMOAudioCategory::SFX);
	ApplyVolumeToSoundClass(SC, SFXVolume * MasterVolume);
	UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] SFXVolume -> %.2f (final=%.2f, soundclass=%s)"),
		SFXVolume, SFXVolume * MasterVolume,
		SC ? *SC->GetName() : TEXT("(none — assets must have Sound Class set)"));
}

void UMOAudioSubsystem::SetUIVolume(float Volume)
{
	UIVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	USoundClass* SC = GetSoundClassForCategory(EMOAudioCategory::UI);
	ApplyVolumeToSoundClass(SC, UIVolume * MasterVolume);
	UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] UIVolume -> %.2f (final=%.2f, soundclass=%s)"),
		UIVolume, UIVolume * MasterVolume,
		SC ? *SC->GetName() : TEXT("(none — assets must have Sound Class set)"));
}

void UMOAudioSubsystem::SetWeatherVolume(float Volume)
{
	WeatherVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	// UDW lives outside our SoundClass hierarchy. Broadcast so the bridge BP
	// (or any other listener) can apply the value to UDW's audio.
	OnWeatherVolumeChanged.Broadcast(WeatherVolume * MasterVolume);
	UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] WeatherVolume -> %.2f (final=%.2f)"),
		WeatherVolume, WeatherVolume * MasterVolume);
}

// =============================================================================
// BANK MANAGEMENT
// =============================================================================

void UMOAudioSubsystem::LoadBank(UDataTable* Bank)
{
	if (!Bank)
	{
		return;
	}
	if (Bank->GetRowStruct() != FMOAudioBankRow::StaticStruct())
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOAudio] LoadBank: '%s' row struct is %s, expected FMOAudioBankRow"),
			*Bank->GetName(),
			Bank->GetRowStruct() ? *Bank->GetRowStruct()->GetName() : TEXT("null"));
		return;
	}

	LoadedBanks.AddUnique(Bank);

	// Merge rows into the lookup. Later additions override earlier ones —
	// duplicates resolve to the most-recently-loaded bank.
	const TMap<FName, uint8*>& Rows = Bank->GetRowMap();
	for (const TPair<FName, uint8*>& Pair : Rows)
	{
		const FMOAudioBankRow* Row = reinterpret_cast<const FMOAudioBankRow*>(Pair.Value);
		AudioIdLookup.Add(Pair.Key, Row);
	}
}

void UMOAudioSubsystem::UnloadBank(UDataTable* Bank)
{
	if (!Bank || !LoadedBanks.Contains(Bank))
	{
		return;
	}
	LoadedBanks.Remove(Bank);

	// Rebuild the lookup from remaining banks (simplest correct approach —
	// banks are small, infrequent op).
	AudioIdLookup.Reset();
	for (UDataTable* RemainingBank : LoadedBanks)
	{
		if (!RemainingBank) continue;
		const TMap<FName, uint8*>& Rows = RemainingBank->GetRowMap();
		for (const TPair<FName, uint8*>& Pair : Rows)
		{
			AudioIdLookup.Add(Pair.Key, reinterpret_cast<const FMOAudioBankRow*>(Pair.Value));
		}
	}
}

bool UMOAudioSubsystem::FindAudioBankRow(FName AudioId, FMOAudioBankRow& OutRow) const
{
	if (const FMOAudioBankRow* Row = AudioIdLookup.FindRef(AudioId))
	{
		OutRow = *Row;
		return true;
	}
	return false;
}

TArray<FName> UMOAudioSubsystem::GetAllAudioIds() const
{
	TArray<FName> Ids;
	AudioIdLookup.GetKeys(Ids);
	return Ids;
}

// =============================================================================
// INTERNAL — clock hook
// =============================================================================

void UMOAudioSubsystem::HandleClockDayNightChanged(bool bNewIsDaytime, const FDateTime& /*NewDateTime*/)
{
	// Ambient: flip Outdoor_Day <-> Outdoor_Night, but leave Cave/Indoor/etc alone.
	const EMOAmbientState NewAmbientState = bNewIsDaytime
		? EMOAmbientState::Outdoor_Day
		: EMOAmbientState::Outdoor_Night;

	const bool bCurrentlyOutdoor =
		CurrentAmbientState == EMOAmbientState::Outdoor_Day ||
		CurrentAmbientState == EMOAmbientState::Outdoor_Night ||
		CurrentAmbientState == EMOAmbientState::Outdoor_Dusk ||
		CurrentAmbientState == EMOAmbientState::None;

	if (bCurrentlyOutdoor)
	{
		SetAmbientState(NewAmbientState);
	}

	// Music: same flip for exploration tracks. Don't override Combat/Stealth/etc
	// if those are the current state — only flip when the player is in default
	// exploration mode.
	const bool bCurrentlyExploring =
		CurrentMusicState == EMOMusicState::Exploration_Day ||
		CurrentMusicState == EMOMusicState::Exploration_Night ||
		CurrentMusicState == EMOMusicState::None;

	if (bCurrentlyExploring)
	{
		const EMOMusicState NewMusicState = bNewIsDaytime
			? EMOMusicState::Exploration_Day
			: EMOMusicState::Exploration_Night;
		SetMusicState(NewMusicState);
	}
}

// =============================================================================
// INTERNAL — playback helpers
// =============================================================================

void UMOAudioSubsystem::ResolveAndLoadAsync(FName AudioId,
	TFunction<void(USoundBase*, const FMOAudioBankRow&)> OnLoaded)
{
	const FMOAudioBankRow* Row = AudioIdLookup.FindRef(AudioId);
	if (!Row)
	{
		UE_LOG(LogMOFramework, Log,
			TEXT("[MOAudio] AudioId '%s' not in bank (this is fine if track isn't authored yet)"),
			*AudioId.ToString());
		return;
	}

	if (Row->Sound.IsNull())
	{
		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOAudio] AudioId '%s' has empty soft ref"), *AudioId.ToString());
		return;
	}

	// Already loaded? Skip the async dance.
	if (USoundBase* Loaded = Row->Sound.Get())
	{
		OnLoaded(Loaded, *Row);
		return;
	}

	// Async load. Capture by value (row pointer + audio id) so the lambda
	// stays valid even if banks rearrange mid-flight.
	FSoftObjectPath Path = Row->Sound.ToSoftObjectPath();
	const FMOAudioBankRow RowCopy = *Row;

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	Streamable.RequestAsyncLoad(Path,
		FStreamableDelegate::CreateLambda([Path, RowCopy, OnLoaded]()
		{
			USoundBase* Sound = Cast<USoundBase>(Path.ResolveObject());
			OnLoaded(Sound, RowCopy);
		}));
}

void UMOAudioSubsystem::StopMusicWithFade(float FadeOutSeconds)
{
	if (MusicComponent && MusicComponent->IsPlaying())
	{
		MusicComponent->FadeOut(FadeOutSeconds, 0.0f);
	}
	MusicComponent = nullptr; // Component auto-destroys after fade if bAutoDestroy=true
}

void UMOAudioSubsystem::StopAmbientWithFade(float FadeOutSeconds)
{
	// Fade out every active base layer.
	for (UAudioComponent* Layer : AmbientLayerComponents)
	{
		if (Layer && Layer->IsPlaying())
		{
			Layer->FadeOut(FadeOutSeconds, 0.0f);
		}
	}
	AmbientLayerComponents.Reset();

	// Cancel pending event timers so they don't fire after the state change.
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		FTimerManager& TM = World->GetTimerManager();
		TM.ClearTimer(EventTimerHandle);
		TM.ClearTimer(DominanceShiftTimerHandle);
		TM.ClearTimer(BaseLayerBreatheTimerHandle);
		TM.ClearTimer(BaseLayerStatusLogTimerHandle);
	}

	bAmbientStreamActive = false;
	CurrentLayerConfig = FMOAmbientLayerConfig();
	CurrentDominantGroupIdx = -1;
	GroupLastFireTimes.Reset();
}

void UMOAudioSubsystem::StartMusic(USoundBase* Sound, const FMOAudioBankRow& Row, float FadeInSeconds)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || !Sound) return;

	MusicComponent = UGameplayStatics::SpawnSound2D(World, Sound,
		/*VolumeMultiplier*/ Row.VolumeMultiplier,
		/*PitchMultiplier*/ Row.PitchMultiplier,
		/*StartTime*/ 0.0f,
		/*ConcurrencySettings*/ nullptr,
		/*bPersistAcrossLevelTransition*/ true,
		/*bAutoDestroy*/ true);

	if (MusicComponent)
	{
		MusicComponent->FadeIn(FadeInSeconds, Row.VolumeMultiplier);
		UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] Music started: %s (fade %.1fs)"),
			*Sound->GetName(), FadeInSeconds);
	}
}

void UMOAudioSubsystem::StartAmbientSingle(USoundBase* Sound, const FMOAudioBankRow& Row, float FadeInSeconds)
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || !Sound) return;

	UAudioComponent* Component = UGameplayStatics::SpawnSound2D(World, Sound,
		Row.VolumeMultiplier, Row.PitchMultiplier, 0.0f, nullptr, true, true);

	if (Component)
	{
		Component->FadeIn(FadeInSeconds, Row.VolumeMultiplier);
		AmbientLayerComponents.Add(Component);
		UE_LOG(LogMOFramework, Log, TEXT("[MOAudio] Ambient started (single): %s (fade %.1fs)"),
			*Sound->GetName(), FadeInSeconds);
	}
}

void UMOAudioSubsystem::StartAmbientLayered(EMOAmbientState NewState, float FadeInSeconds)
{
	const FMOAmbientLayerConfig* Config = FindAmbientLayerConfig(NewState);
	if (!Config)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio] StartAmbientLayered: no config for state %s"),
			*UEnum::GetValueAsString(NewState));
		return;
	}

	CurrentLayerConfig = *Config;

	// Reset cooldown tracking. All groups start "available" (last fire = -inf
	// for cooldown comparison).
	GroupLastFireTimes.Init(-1e6f, Config->EventGroups.Num());

	// Pick initial dominant group at random.
	CurrentDominantGroupIdx = Config->EventGroups.Num() > 0
		? FMath::RandRange(0, Config->EventGroups.Num() - 1)
		: -1;

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOAudio] Ambient layered start: %s | %d base / %d event groups | dominant=%s"),
		*UEnum::GetValueAsString(NewState),
		Config->BaseLayers.Num(),
		Config->EventGroups.Num(),
		(CurrentDominantGroupIdx >= 0)
			? *Config->EventGroups[CurrentDominantGroupIdx].GroupName.ToString()
			: TEXT("(none)"));

	// Sentinel: any pending delayed-restart callbacks check this flag before
	// firing — set true here, cleared in StopAmbientWithFade. Without this,
	// a delayed layer's restart timer could fire AFTER the ambient state has
	// changed, spawning a stale loop on top of the new state.
	bAmbientStreamActive = true;

	// Pre-size the slot array so each base layer has a stable index — needed
	// because delayed layers respawn their AudioComponent and we want to keep
	// the breathing logic indexing consistent.
	AmbientLayerComponents.SetNum(Config->BaseLayers.Num());

	// Spawn each base layer. Continuous vs delayed mode is decided per-layer
	// (DelayBetweenLoopsMax > 0 = delayed; 0 = continuous).
	for (int32 LayerIdx = 0; LayerIdx < Config->BaseLayers.Num(); ++LayerIdx)
	{
		StartBaseLayer(Config->BaseLayers[LayerIdx], LayerIdx,
			Config->BaseLayerVolume, FadeInSeconds);
	}

	// Kick off event picker + dominance shift timers.
	if (Config->EventGroups.Num() > 0)
	{
		ScheduleNextEvent();
		ScheduleNextDominanceShift();
	}

	// Start the base layer "breathing" — slow volume drift on random base
	// layers so the soundscape feels alive. Only if we have base layers.
	if (Config->BaseLayers.Num() > 0)
	{
		ScheduleNextBaseLayerBreathe();

		// Also start periodic base layer status logging (every 10s).
		if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		{
			World->GetTimerManager().SetTimer(
				BaseLayerStatusLogTimerHandle, this,
				&UMOAudioSubsystem::HandleBaseLayerStatusLog,
				10.0f, /*bLoop=*/true);
		}
	}
}

const FMOAmbientLayerConfig* UMOAudioSubsystem::FindAmbientLayerConfig(EMOAmbientState State) const
{
	if (!CachedAmbientLayersTable) return nullptr;

	const FName RowName = MakeAmbientAudioId(State);
	if (RowName.IsNone()) return nullptr;

	return CachedAmbientLayersTable->FindRow<FMOAmbientLayerConfig>(RowName, TEXT("FindAmbientLayerConfig"));
}

void UMOAudioSubsystem::StartBaseLayer(const FMOAmbientBaseLayer& Layer, int32 SlotIdx,
	float Volume, float FadeInSeconds)
{
	if (Layer.AudioId.IsNone())
	{
		return;
	}

	// Mix hierarchy: config-level BaseLayerVolume * per-layer Volume * asset
	// VolumeMultiplier. The downstream helpers don't know about layer-level
	// volume — we just bake it into the value they receive.
	const float EffectiveVolume = Volume * Layer.Volume;

	const bool bDelayed = Layer.DelayBetweenLoopsMax > 0.0f;
	if (bDelayed)
	{
		// Short asset → play once → wait → respawn cycle. Used for breeze swells,
		// rustles, etc. where back-to-back looping would be obvious.
		StartBaseLayerDelayed(Layer.AudioId, SlotIdx, EffectiveVolume, FadeInSeconds,
			Layer.DelayBetweenLoopsMin, Layer.DelayBetweenLoopsMax);
	}
	else
	{
		// Continuous bed loop — relies on SoundWave's bLooping flag.
		StartBaseLayerContinuous(Layer.AudioId, SlotIdx, EffectiveVolume, FadeInSeconds);
	}
}

void UMOAudioSubsystem::StartBaseLayerContinuous(FName AudioId, int32 SlotIdx,
	float Volume, float FadeInSeconds)
{
	ResolveAndLoadAsync(AudioId,
		[this, Volume, FadeInSeconds, AudioId, SlotIdx](USoundBase* Sound, const FMOAudioBankRow& Row)
		{
			if (!Sound)
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MOAudio]   base layer FAILED to load: %s"), *AudioId.ToString());
				return;
			}

			// Bail if ambient state changed while we were loading.
			if (!bAmbientStreamActive) return;

			UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
			if (!World) return;

			const float FinalVol = Volume * Row.VolumeMultiplier;
			UAudioComponent* Component = UGameplayStatics::SpawnSound2D(World, Sound,
				FinalVol, Row.PitchMultiplier, 0.0f, nullptr, true, true);
			if (Component)
			{
				Component->FadeIn(FadeInSeconds, FinalVol);

				// Write into pre-sized slot if valid; otherwise append.
				if (AmbientLayerComponents.IsValidIndex(SlotIdx))
				{
					AmbientLayerComponents[SlotIdx] = Component;
				}
				else
				{
					AmbientLayerComponents.Add(Component);
				}

				// Log asset duration too — short loops (<5s) repeat enough times
				// per minute that they're perceived as "tape loop" repetition.
				// Use this to identify which files are too short / poorly looped.
				const float Duration = Sound->GetDuration();
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MOAudio]   + base layer started [continuous, slot %d]: %s (asset=%s vol=%.2f fade=%.1fs duration=%.1fs %s)"),
					SlotIdx, *AudioId.ToString(), *Sound->GetName(), FinalVol, FadeInSeconds, Duration,
					Duration < 5.0f  ? TEXT("[VERY SHORT — will loop obviously, consider delayed mode!]") :
					Duration < 15.0f ? TEXT("[short]") :
					Duration < 60.0f ? TEXT("[ok]") :
					                   TEXT("[long, good]"));
			}
		});
}

void UMOAudioSubsystem::StartBaseLayerDelayed(FName AudioId, int32 SlotIdx,
	float Volume, float FadeInSeconds, float DelayMin, float DelayMax)
{
	ResolveAndLoadAsync(AudioId,
		[this, Volume, FadeInSeconds, AudioId, SlotIdx, DelayMin, DelayMax]
		(USoundBase* Sound, const FMOAudioBankRow& Row)
		{
			if (!Sound)
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MOAudio]   delayed base layer FAILED to load: %s"), *AudioId.ToString());
				return;
			}

			// Bail if ambient state changed while we were loading.
			if (!bAmbientStreamActive) return;

			UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
			if (!World) return;

			const float FinalVol = Volume * Row.VolumeMultiplier;

			// Spawn the component without persistent looping (we'll restart it
			// manually after a delay). bAutoDestroy=true so the comp goes away
			// when it finishes — the timer below will spawn a fresh one.
			UAudioComponent* Component = UGameplayStatics::SpawnSound2D(World, Sound,
				FinalVol, Row.PitchMultiplier, 0.0f, nullptr, true, true);
			if (!Component) return;

			Component->FadeIn(FadeInSeconds, FinalVol);

			if (AmbientLayerComponents.IsValidIndex(SlotIdx))
			{
				AmbientLayerComponents[SlotIdx] = Component;
			}
			else
			{
				AmbientLayerComponents.Add(Component);
			}

			// Figure out how long this play lasts. For looping SoundWaves
			// GetDuration returns sentinel ~10000s; Wave->Duration gives the
			// actual sample length.
			float PlayDuration = 5.0f;
			if (USoundWave* Wave = Cast<USoundWave>(Sound))
			{
				if (Wave->Duration > 0.0f && Wave->Duration < 60.0f)
				{
					PlayDuration = Wave->Duration;
				}
			}
			else
			{
				const float D = Sound->GetDuration();
				if (D > 0.0f && D < 60.0f) PlayDuration = D;
			}

			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOAudio]   + base layer started [delayed, slot %d]: %s (asset=%s vol=%.2f playLen=%.1fs gap=%.1f-%.1fs)"),
				SlotIdx, *AudioId.ToString(), *Sound->GetName(), FinalVol,
				PlayDuration, DelayMin, DelayMax);

			// Schedule the next iteration: wait one full play + a random gap,
			// then re-enter StartBaseLayerDelayed. Sentinel check prevents
			// the cycle from firing on a stale state.
			const float NextDelay = PlayDuration + FMath::FRandRange(DelayMin, DelayMax);
			TWeakObjectPtr<UAudioComponent> WeakComp = Component;

			FTimerHandle RestartHandle;
			World->GetTimerManager().SetTimer(RestartHandle,
				FTimerDelegate::CreateLambda(
					[this, AudioId, SlotIdx, Volume, DelayMin, DelayMax, WeakComp]()
					{
						if (!bAmbientStreamActive)
						{
							return; // State changed — silently drop.
						}

						// Stop the current iteration (might still be playing if
						// natural duration overshot, e.g. SoundCue with tail).
						if (UAudioComponent* C = WeakComp.Get())
						{
							C->Stop();
						}

						// Respawn for the next iteration. Use 0 fade-in for
						// loop continuity — only the first play of the state
						// gets the original FadeInSeconds.
						StartBaseLayerDelayed(AudioId, SlotIdx, Volume,
							/*FadeInSeconds*/ 0.0f, DelayMin, DelayMax);
					}),
				NextDelay, /*bLoop=*/false);
		});
}

// =============================================================================
// EVENT PICKER TIMERS
// =============================================================================
//
// Frequent & rare events follow the same pattern: timer fires, we pick a
// random AudioId from the pool, play it as a one-shot 2D sound (no component
// retained — auto-destroys when finished), then reschedule. The timer is
// cancelled when ambient state changes (in StopAmbientWithFade).

void UMOAudioSubsystem::ScheduleNextEvent()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return;

	const float Delay = FMath::FRandRange(
		CurrentLayerConfig.EventIntervalMin,
		CurrentLayerConfig.EventIntervalMax);

	World->GetTimerManager().SetTimer(
		EventTimerHandle, this,
		&UMOAudioSubsystem::HandleEventTimer,
		Delay, /*bLoop=*/false);
}

void UMOAudioSubsystem::ScheduleNextDominanceShift()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return;

	const float Delay = FMath::FRandRange(
		CurrentLayerConfig.DominanceShiftIntervalMin,
		CurrentLayerConfig.DominanceShiftIntervalMax);

	World->GetTimerManager().SetTimer(
		DominanceShiftTimerHandle, this,
		&UMOAudioSubsystem::HandleDominanceShiftTimer,
		Delay, /*bLoop=*/false);
}

void UMOAudioSubsystem::HandleDominanceShiftTimer()
{
	const int32 GroupCount = CurrentLayerConfig.EventGroups.Num();
	if (GroupCount < 2)
	{
		// With 0 or 1 group there's nothing to shift between.
		ScheduleNextDominanceShift();
		return;
	}

	// Pick a different group than the current dominant one.
	int32 NewIdx;
	do
	{
		NewIdx = FMath::RandRange(0, GroupCount - 1);
	} while (NewIdx == CurrentDominantGroupIdx);

	const FName OldName = (CurrentDominantGroupIdx >= 0)
		? CurrentLayerConfig.EventGroups[CurrentDominantGroupIdx].GroupName
		: NAME_None;
	CurrentDominantGroupIdx = NewIdx;

	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOAudio] Dominance shift: %s -> %s"),
		*OldName.ToString(),
		*CurrentLayerConfig.EventGroups[NewIdx].GroupName.ToString());

	ScheduleNextDominanceShift();
}

int32 UMOAudioSubsystem::PickWeightedGroup() const
{
	const int32 GroupCount = CurrentLayerConfig.EventGroups.Num();
	if (GroupCount == 0) return -1;

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	// Build a weighted list of group indices that are NOT on cooldown.
	TArray<int32> Candidates;
	TArray<float> Weights;
	Candidates.Reserve(GroupCount);
	Weights.Reserve(GroupCount);

	for (int32 i = 0; i < GroupCount; ++i)
	{
		const FMOAmbientEventGroup& Group = CurrentLayerConfig.EventGroups[i];
		const float LastFire = GroupLastFireTimes.IsValidIndex(i) ? GroupLastFireTimes[i] : -1e6f;
		const float TimeSinceFire = Now - LastFire;

		if (TimeSinceFire < Group.CooldownSeconds)
		{
			continue; // Group still on cooldown, skip.
		}

		const float Weight = (i == CurrentDominantGroupIdx)
			? Group.DominantWeight
			: Group.BackgroundWeight;

		if (Weight > 0.0f)
		{
			Candidates.Add(i);
			Weights.Add(Weight);
		}
	}

	if (Candidates.Num() == 0)
	{
		return -1; // Everyone on cooldown — silence this fire.
	}

	// Weighted random pick.
	float TotalWeight = 0.0f;
	for (float W : Weights) TotalWeight += W;

	const float Roll = FMath::FRandRange(0.0f, TotalWeight);
	float Cumulative = 0.0f;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		Cumulative += Weights[i];
		if (Roll <= Cumulative)
		{
			return Candidates[i];
		}
	}

	return Candidates.Last();
}

void UMOAudioSubsystem::HandleEventTimer()
{
	const int32 GroupIdx = PickWeightedGroup();
	if (GroupIdx < 0)
	{
		// Either no groups or all on cooldown. Schedule a quiet hold.
		UE_LOG(LogMOFramework, Warning, TEXT("[MOAudio]   event SKIPPED (all groups on cooldown)"));
		ScheduleNextEvent();
		return;
	}

	const FMOAmbientEventGroup& Group = CurrentLayerConfig.EventGroups[GroupIdx];
	if (Group.AudioIds.Num() == 0)
	{
		ScheduleNextEvent();
		return;
	}

	// Pick variant from within the group.
	const int32 VariantIdx = FMath::RandRange(0, Group.AudioIds.Num() - 1);
	const FName AudioId = Group.AudioIds[VariantIdx];

	// Mark group's cooldown timestamp NOW (before async load returns) so
	// fast-fire scenarios don't double-dip.
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World && GroupLastFireTimes.IsValidIndex(GroupIdx))
	{
		GroupLastFireTimes[GroupIdx] = World->GetTimeSeconds();
	}

	// Wide per-fire jitter.
	const float VolJitter = FMath::FRandRange(
		CurrentLayerConfig.VolumeJitterMin,
		CurrentLayerConfig.VolumeJitterMax);
	const float PitchJitter = FMath::FRandRange(
		CurrentLayerConfig.PitchJitterMin,
		CurrentLayerConfig.PitchJitterMax);
	const float BaseVol = CurrentLayerConfig.EventVolume;

	const bool bIsDominant = (GroupIdx == CurrentDominantGroupIdx);
	const FName GroupName = Group.GroupName;

	ResolveAndLoadAsync(AudioId,
		[this, BaseVol, VolJitter, PitchJitter, AudioId, GroupName, bIsDominant]
		(USoundBase* Sound, const FMOAudioBankRow& Row)
		{
			if (!Sound) return;
			UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
			if (!World) return;

			const float FinalVol = BaseVol * VolJitter * Row.VolumeMultiplier;
			const float FinalPitch = PitchJitter * Row.PitchMultiplier;

			const UMOAudioSettings* Settings = GetDefault<UMOAudioSettings>();
			const bool bSpatialize = Settings ? Settings->bSpatializeAmbientEvents : true;

			// IMPORTANT: events MUST NOT play as orphaned one-shots. If the
			// source SoundWave has bLooping=true (which all our ambient assets
			// do after the bulk-edit), PlaySound2D would leave it looping in
			// place forever. Use the Spawn* variants which return a managed
			// component we can Stop() on a timer.
			UAudioComponent* Comp = nullptr;
			FVector EventLoc = FVector::ZeroVector;
			float DistFromListener = 0.0f;

			if (bSpatialize)
			{
				// Spatial path: random world location around the listener,
				// engine handles panning + distance falloff via attenuation.
				EventLoc = PickRandomEventLocation();
				DistFromListener = FVector::Dist(EventLoc, GetListenerLocation());

				Comp = UGameplayStatics::SpawnSoundAtLocation(
					World, Sound, EventLoc, FRotator::ZeroRotator,
					FinalVol, FinalPitch,
					/*StartTime*/ 0.0f,
					/*AttenuationSettings*/ GetAmbientEventAttenuation(),
					/*ConcurrencySettings*/ nullptr,
					/*bAutoDestroy*/ true);
			}
			else
			{
				// 2D fallback path.
				Comp = UGameplayStatics::SpawnSound2D(
					World, Sound, FinalVol, FinalPitch,
					/*StartTime*/ 0.0f, /*Concurrency*/ nullptr,
					/*bPersistAcrossLevel*/ false, /*bAutoDestroy*/ true);
			}

			// Determine actual playback duration. For looping SoundWaves,
			// GetDuration returns 10000 (sentinel) — use Wave->Duration which
			// gives the actual sample length regardless of loop flag. Clamp
			// to sane range for safety.
			float PlayDuration = 5.0f; // default fallback
			if (USoundWave* Wave = Cast<USoundWave>(Sound))
			{
				if (Wave->Duration > 0.0f && Wave->Duration < 30.0f)
				{
					PlayDuration = Wave->Duration;
				}
			}
			else
			{
				const float D = Sound->GetDuration();
				if (D > 0.0f && D < 30.0f)
				{
					PlayDuration = D;
				}
			}

			if (Comp)
			{
				// Capture weak ref so the lambda doesn't extend the component
				// past its useful life.
				TWeakObjectPtr<UAudioComponent> WeakComp = Comp;
				FTimerHandle StopHandle;
				World->GetTimerManager().SetTimer(StopHandle,
					[WeakComp]()
					{
						if (UAudioComponent* C = WeakComp.Get())
						{
							C->Stop();
						}
					},
					PlayDuration + 0.1f, /*bLoop=*/false);
			}

			if (bSpatialize)
			{
				// Direction string for quick mental mapping in logs:
				// compute relative bearing in listener space (roughly).
				const FVector Listener = GetListenerLocation();
				const FVector Delta = EventLoc - Listener;
				const float Bearing = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MOAudio]   event: [%s%s] %s @ %.0fm bearing=%.0f° height=%+.1fm (vol=%.2f pitch=%.2f playLen=%.1fs)"),
					*GroupName.ToString(),
					bIsDominant ? TEXT("*") : TEXT(""),
					*AudioId.ToString(),
					DistFromListener / 100.0f, Bearing, Delta.Z / 100.0f,
					FinalVol, FinalPitch, PlayDuration);
			}
			else
			{
				UE_LOG(LogMOFramework, Warning,
					TEXT("[MOAudio]   event: [%s%s] %s (2D, vol=%.2f pitch=%.2f playLen=%.1fs)"),
					*GroupName.ToString(),
					bIsDominant ? TEXT("*") : TEXT(""),
					*AudioId.ToString(), FinalVol, FinalPitch, PlayDuration);
			}
		});

	ScheduleNextEvent();
}

void UMOAudioSubsystem::ScheduleNextBaseLayerBreathe()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return;

	// Fire every 4-8 seconds — slow enough that the drift is subliminal,
	// fast enough that the mix never feels static.
	const float Delay = FMath::FRandRange(4.0f, 8.0f);

	World->GetTimerManager().SetTimer(
		BaseLayerBreatheTimerHandle, this,
		&UMOAudioSubsystem::HandleBaseLayerBreatheTimer,
		Delay, /*bLoop=*/false);
}

void UMOAudioSubsystem::HandleBaseLayerBreatheTimer()
{
	if (AmbientLayerComponents.Num() == 0)
	{
		return;
	}

	// Pick one random base layer and drift it to a new volume over 6-12s.
	// Each fire affects ONE layer, so layers drift independently — the mix
	// is constantly evolving without any single layer doing anything sudden.
	const int32 PickIdx = FMath::RandRange(0, AmbientLayerComponents.Num() - 1);
	UAudioComponent* Layer = AmbientLayerComponents[PickIdx];
	if (!Layer || !Layer->IsValidLowLevel())
	{
		ScheduleNextBaseLayerBreathe();
		return;
	}

	// Target volume: 65-100% of nominal base layer volume. Wide enough that
	// the drift is audible without making layers disappear.
	const float NominalVol = CurrentLayerConfig.BaseLayerVolume;
	const float TargetVol = FMath::FRandRange(0.65f, 1.0f) * NominalVol;
	const float DriftDuration = FMath::FRandRange(6.0f, 12.0f);

	Layer->AdjustVolume(DriftDuration, TargetVol);

	UE_LOG(LogMOFramework, Verbose,
		TEXT("[MOAudio]   breathe: layer %d -> vol=%.2f over %.1fs"),
		PickIdx, TargetVol, DriftDuration);

	ScheduleNextBaseLayerBreathe();
}

void UMOAudioSubsystem::HandleBaseLayerStatusLog()
{
	UE_LOG(LogMOFramework, Warning,
		TEXT("[MOAudio] === Base layers (every 10s) — %d active ==="),
		AmbientLayerComponents.Num());

	for (int32 i = 0; i < AmbientLayerComponents.Num(); ++i)
	{
		UAudioComponent* Comp = AmbientLayerComponents[i];
		if (!Comp)
		{
			UE_LOG(LogMOFramework, Warning,
				TEXT("[MOAudio]   [%d] <null component>"), i);
			continue;
		}

		const FString SoundName = Comp->Sound ? Comp->Sound->GetName() : TEXT("(no sound)");
		const TCHAR* StateStr = Comp->IsPlaying() ? TEXT("PLAYING")
		                      : (Comp->IsValidLowLevel() ? TEXT("idle") : TEXT("invalid"));

		UE_LOG(LogMOFramework, Warning,
			TEXT("[MOAudio]   [%d] %s | %s | volMul=%.2f"),
			i, StateStr, *SoundName, Comp->VolumeMultiplier);
	}
}

FName UMOAudioSubsystem::MakeMusicAudioId(EMOMusicState State)
{
	// Mapping must match DataTable row names exactly.
	switch (State)
	{
		case EMOMusicState::MainMenu:          return TEXT("Music.MainMenu");
		case EMOMusicState::Exploration_Day:   return TEXT("Music.Exploration.Day");
		case EMOMusicState::Exploration_Night: return TEXT("Music.Exploration.Night");
		case EMOMusicState::Combat:            return TEXT("Music.Combat");
		case EMOMusicState::Stealth:           return TEXT("Music.Stealth");
		case EMOMusicState::DangerNear:        return TEXT("Music.DangerNear");
		case EMOMusicState::Death:             return TEXT("Music.Death");
		case EMOMusicState::Discovery:         return TEXT("Music.Discovery");
		default:                               return NAME_None;
	}
}

FName UMOAudioSubsystem::MakeAmbientAudioId(EMOAmbientState State)
{
	switch (State)
	{
		case EMOAmbientState::Outdoor_Day:   return TEXT("Ambient.Outdoor.Day");
		case EMOAmbientState::Outdoor_Dusk:  return TEXT("Ambient.Outdoor.Dusk");
		case EMOAmbientState::Outdoor_Night: return TEXT("Ambient.Outdoor.Night");
		case EMOAmbientState::Cave:          return TEXT("Ambient.Cave");
		case EMOAmbientState::Indoor:        return TEXT("Ambient.Indoor");
		case EMOAmbientState::Water:         return TEXT("Ambient.Water");
		default:                             return NAME_None;
	}
}

FVector UMOAudioSubsystem::GetListenerLocation() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return FVector::ZeroVector;

	// Preferred: the active camera (handles cinematic POV, vehicles, etc.).
	if (APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(World, 0))
	{
		return PCM->GetCameraLocation();
	}

	// Fallback: player pawn directly.
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		return Pawn->GetActorLocation();
	}

	return FVector::ZeroVector;
}

FVector UMOAudioSubsystem::PickRandomEventLocation() const
{
	const UMOAudioSettings* Settings = GetDefault<UMOAudioSettings>();
	if (!Settings)
	{
		return GetListenerLocation();
	}

	const FVector Listener = GetListenerLocation();

	// Random horizontal angle around the listener (full 360°).
	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);

	// Random distance within the configured range. Bias slightly toward farther
	// distances so the soundscape feels open — uses sqrt to weight toward outer.
	const float MinDist = Settings->AmbientEventDistanceMin;
	const float MaxDist = Settings->AmbientEventDistanceMax;
	const float DistT = FMath::Sqrt(FMath::FRand());
	const float Distance = FMath::Lerp(MinDist, MaxDist, DistT);

	// Height offset.
	const float Height = FMath::FRandRange(
		Settings->AmbientEventHeightMin,
		Settings->AmbientEventHeightMax);

	return Listener + FVector(
		FMath::Cos(Angle) * Distance,
		FMath::Sin(Angle) * Distance,
		Height);
}

USoundAttenuation* UMOAudioSubsystem::GetAmbientEventAttenuation() const
{
	const UMOAudioSettings* Settings = GetDefault<UMOAudioSettings>();
	if (Settings && !Settings->AmbientEventAttenuation.IsNull())
	{
		if (USoundAttenuation* Override = Cast<USoundAttenuation>(
			Settings->AmbientEventAttenuation.TryLoad()))
		{
			return Override;
		}
	}
	return DefaultEventAttenuation;
}

USoundClass* UMOAudioSubsystem::GetSoundClassForCategory(EMOAudioCategory Category) const
{
	const UMOAudioSettings* Settings = GetDefault<UMOAudioSettings>();
	if (!Settings) return nullptr;

	const FSoftObjectPath* Path = nullptr;
	switch (Category)
	{
		case EMOAudioCategory::Music:   Path = &Settings->MusicSoundClass; break;
		case EMOAudioCategory::Ambient: Path = &Settings->AmbientSoundClass; break;
		case EMOAudioCategory::SFX:     Path = &Settings->SFXSoundClass; break;
		case EMOAudioCategory::UI:      Path = &Settings->UISoundClass; break;
		default:                        return nullptr;
	}

	if (!Path || Path->IsNull()) return nullptr;
	return Cast<USoundClass>(Path->TryLoad());
}

void UMOAudioSubsystem::ApplyVolumeToSoundClass(USoundClass* SoundClass, float Volume)
{
	if (!SoundClass) return;
	SoundClass->Properties.Volume = FMath::Clamp(Volume, 0.0f, 1.0f);
}
