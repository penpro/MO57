/**
 * =============================================================================
 * MOGatheringSettingsActor.h - World-Placed Gathering/Foraging Configuration
 * =============================================================================
 *
 * PURPOSE:
 * A placeable actor that configures all gathering, foraging, and resource
 * collection settings for the level. Place one in your persistent level
 * and tweak settings in the Details panel.
 *
 * USAGE:
 * 1. Place BP_GatheringSettings (or this class) in your level
 * 2. Configure settings in the Details panel
 * 3. Subsystems and BT tasks will automatically find and use these settings
 *
 * If no actor is placed, systems use their hardcoded defaults.
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOGatheringSettingsActor.generated.h"

/**
 * Settings for survivor foraging behavior (wandering to find items).
 */
USTRUCT(BlueprintType)
struct FMOForageBehaviorSettings
{
	GENERATED_BODY()

	/** How far survivors will wander to find forage points (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behavior", meta=(ClampMin="500", Units="cm"))
	float WanderRadius = 2000.0f;

	/** Minimum distance survivor must move before foraging (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behavior", meta=(ClampMin="100", Units="cm"))
	float MinMoveDistance = 200.0f;

	/** Time to complete one forage action (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Behavior", meta=(ClampMin="0.5", Units="s"))
	float ForageDuration = 4.0f;
};

/**
 * Settings for item detection when foraging.
 */
USTRUCT(BlueprintType)
struct FMOForageDetectionSettings
{
	GENERATED_BODY()

	/** Base radius to detect forageable items at skill level 0 (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Detection", meta=(ClampMin="100", Units="cm"))
	float BaseSearchRadius = 800.0f;

	/** Additional search radius per foraging skill level (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Detection", meta=(ClampMin="0", Units="cm"))
	float RadiusPerSkillLevel = 25.0f;

	/** Maximum search radius cap (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Detection", meta=(ClampMin="100", Units="cm"))
	float MaxSearchRadius = 1500.0f;

	/** Max items to reveal per search (performance limit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Detection", meta=(ClampMin="1"))
	int32 MaxItemsPerSearch = 20;
};

/**
 * Settings for dig-for-supplies action.
 */
USTRUCT(BlueprintType)
struct FMODigSettings
{
	GENERATED_BODY()

	/** Radius around dig point where items spawn (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dig", meta=(ClampMin="10", Units="cm"))
	float SpawnRadius = 50.0f;
};

/**
 * XP rewards for foraging actions.
 */
USTRUCT(BlueprintType)
struct FMOForageXPSettings
{
	GENERATED_BODY()

	/** XP awarded per item revealed through searching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="XP", meta=(ClampMin="0"))
	float XPPerRevealedItem = 2.0f;

	/** XP awarded per item found through digging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="XP", meta=(ClampMin="0"))
	float XPPerDugItem = 5.0f;
};

/**
 * World actor for configuring all gathering and foraging settings.
 * Place one in your level and configure in the Details panel.
 */
UCLASS(Blueprintable, meta=(DisplayName="Gathering Settings"))
class MOFRAMEWORK_API AMOGatheringSettingsActor : public AActor
{
	GENERATED_BODY()

public:
	AMOGatheringSettingsActor();

	// ============================================================================
	// SETTINGS
	// ============================================================================

	/** Survivor foraging behavior (wandering, timing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foraging")
	FMOForageBehaviorSettings ForageBehavior;

	/** Item detection settings (search radius, skill scaling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foraging")
	FMOForageDetectionSettings ForageDetection;

	/** Dig-for-supplies settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foraging")
	FMODigSettings DigSettings;

	/** XP rewards for foraging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foraging")
	FMOForageXPSettings ForageXP;

	// ============================================================================
	// STATIC ACCESS
	// ============================================================================

	/**
	 * Find the gathering settings actor in the world.
	 * Returns nullptr if none exists (use defaults).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Gathering", meta=(WorldContext="WorldContextObject"))
	static AMOGatheringSettingsActor* GetGatheringSettings(const UObject* WorldContextObject);

	/**
	 * Get forage behavior settings (from actor if exists, otherwise defaults).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Gathering", meta=(WorldContext="WorldContextObject"))
	static FMOForageBehaviorSettings GetForageBehaviorSettings(const UObject* WorldContextObject);

	/**
	 * Get forage detection settings (from actor if exists, otherwise defaults).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Gathering", meta=(WorldContext="WorldContextObject"))
	static FMOForageDetectionSettings GetForageDetectionSettings(const UObject* WorldContextObject);

	/**
	 * Get dig settings (from actor if exists, otherwise defaults).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Gathering", meta=(WorldContext="WorldContextObject"))
	static FMODigSettings GetDigSettings(const UObject* WorldContextObject);

	/**
	 * Get forage XP settings (from actor if exists, otherwise defaults).
	 */
	UFUNCTION(BlueprintPure, Category="MO|Gathering", meta=(WorldContext="WorldContextObject"))
	static FMOForageXPSettings GetForageXPSettings(const UObject* WorldContextObject);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Cached instance for fast lookup (includes world validation). */
	static TWeakObjectPtr<AMOGatheringSettingsActor> CachedInstance;
	static TWeakObjectPtr<UWorld> CachedWorld;

	/** Register this actor as the settings provider. */
	void RegisterAsSettingsProvider();

	/** Unregister when destroyed. */
	void UnregisterAsSettingsProvider();
};
