#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "MOPersistenceSettings.generated.h"

class APawn;

/**
 * Project Settings entry for MOFramework persistence configuration.
 * Configure fallback classes and behavior for save/load operations.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="MO Persistence"))
class MOFRAMEWORK_API UMOPersistenceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("MO Persistence"); }

	/**
	 * Fallback pawn class to use when loading a save if the original pawn class cannot be found.
	 * This prevents data loss when Blueprint pawns are renamed/moved/deleted.
	 * Should be a class with UMOIdentityComponent and UMOInventoryComponent.
	 * If not set and a pawn class fails to load, that pawn's data will be lost.
	 */
	UPROPERTY(EditAnywhere, Config, Category="Fallback Classes")
	TSoftClassPtr<APawn> DefaultPersistedPawnClass;

	/** Get the configured fallback pawn class. May return nullptr if not configured. */
	UFUNCTION(BlueprintCallable, Category="MO|Persistence")
	static TSubclassOf<APawn> GetDefaultPersistedPawnClass();

	/** Check if persistence is properly configured. Logs a warning if not. */
	UFUNCTION(BlueprintCallable, Category="MO|Persistence")
	static bool IsConfigured();

	/** Validate configuration and log warnings for any issues. Call at startup. */
	static void ValidateConfiguration();
};
