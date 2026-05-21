/**
 * =============================================================================
 * MOPersistenceSettings.h - Persistence System Project Settings
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Project Settings for the persistence/save system. Configures fallback
 * behavior when loading saves with missing pawn classes.
 * Accessible via Project Settings -> Plugins -> MO Persistence.
 *
 * KEY SETTINGS:
 * - DefaultPersistedPawnClass: Fallback when original pawn class not found
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] FALLBACK CLASS: DefaultPersistedPawnClass MUST have
 *   UMOIdentityComponent and UMOInventoryComponent or data will be lost.
 *
 * [2024-02] SOFT CLASS PTR: Uses TSoftClassPtr for deferred loading. Call
 *   GetDefaultPersistedPawnClass() to load synchronously.
 *
 * [2024-02] VALIDATION: Call ValidateConfiguration() at startup to log
 *   warnings if not properly configured.
 *
 * =============================================================================
 * RELATED FILES: MOPersistenceSubsystem.h, MOWorldSaveGame.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "MOPersistenceSettings.generated.h"

class APawn;

/**
 * Project Settings entry for MOFramework persistence configuration.
 * Configure fallback classes and behavior for save/load operations.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Persistence"))
class MOFRAMEWORK_API UMOPersistenceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides — unified under "MOFramework" section in Project Settings.
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("MOFramework"); }
	virtual FName GetSectionName() const override { return TEXT("Persistence"); }

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
