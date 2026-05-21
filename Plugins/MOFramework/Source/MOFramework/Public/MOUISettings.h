/**
 * =============================================================================
 * MOUISettings.h - UI System Developer Settings
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Project Settings entry for UI system configuration. Allows Blueprint
 * configuration of the primary game layout class without C++ changes.
 *
 * LOCATION IN PROJECT SETTINGS:
 * Project Settings -> Plugins -> MO UI Settings
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2026-03] CLASS REFERENCE: PrimaryGameLayoutClass must be set to a Widget
 *   Blueprint that inherits from UMOPrimaryGameLayout with all 5 layer stacks.
 *
 * =============================================================================
 * RELATED FILES: MOGameUIManagerSubsystem.h, MOPrimaryGameLayout.h
 * LAST UPDATED: 2026-03-28
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOUISettings.generated.h"

class UMOPrimaryGameLayout;

/**
 * UI System configuration in Project Settings.
 * Accessible at: Project Settings -> Plugins -> MO UI Settings
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="UI"))
class MOFRAMEWORK_API UMOUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// UDeveloperSettings overrides — unified under "MOFramework" section in Project Settings.
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("MOFramework"); }
	virtual FName GetSectionName() const override { return TEXT("UI"); }

	/**
	 * The Widget Blueprint class to use as the primary game layout.
	 * Must inherit from UMOPrimaryGameLayout and contain 5 layer stacks:
	 * - HUDLayer (UCommonActivatableWidgetStack)
	 * - GameLayer (UCommonActivatableWidgetStack)
	 * - GameOverlayLayer (UCommonActivatableWidgetStack)
	 * - MenuLayer (UCommonActivatableWidgetStack)
	 * - ModalLayer (UCommonActivatableWidgetStack)
	 */
	UPROPERTY(EditAnywhere, Config, Category="Layout")
	TSoftClassPtr<UMOPrimaryGameLayout> PrimaryGameLayoutClass;

	/**
	 * Get the primary game layout class, loading it if necessary.
	 * @return The layout class, or nullptr if not configured
	 */
	UFUNCTION(BlueprintPure, Category="MO|UI")
	static TSubclassOf<UMOPrimaryGameLayout> GetPrimaryGameLayoutClass();

	/**
	 * Check if the UI system is properly configured.
	 * @return True if PrimaryGameLayoutClass is set
	 */
	UFUNCTION(BlueprintPure, Category="MO|UI")
	static bool IsConfigured();
};
