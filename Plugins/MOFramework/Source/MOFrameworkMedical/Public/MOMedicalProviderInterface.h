/**
 * =============================================================================
 * MOMedicalProviderInterface.h - Medical Component Access Contract
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Standardized access to medical/health components without FindComponentByClass.
 * Decouples medical systems from specific character implementations.
 *
 * WHO IMPLEMENTS THIS:
 * - AMOCharacter - Has all medical components
 * - AMOCreature - Has subset of medical components (Vitals, Anatomy)
 *
 * WHO USES THIS:
 * - UMOMedicalSubsystem - Medical calculations
 * - Combat system - Apply damage to body parts
 * - UI - Display health status
 *
 * MEDICAL COMPONENT CASCADE:
 * The medical components interact in a cascade:
 *
 *   Wounds (Anatomy) → Blood loss → Vitals (HR, BP, SpO2)
 *                                        ↓
 *   Metabolism (glucose, hydration) → Vitals → Mental (consciousness)
 *                                        ↓
 *   Adrenaline (combat stress) → Vitals → Performance modifiers
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-01] CREATURE SUBSET: AMOCreature only has Vitals and Anatomy.
 *   GetMentalState(), GetAdrenaline(), GetMetabolism() return nullptr for
 *   creatures. Always null-check results.
 *
 * [2024-02] COMPONENT DEPENDENCIES: Medical components depend on each other
 *   via tick updates. Don't call medical methods during construction.
 *
 * [2024-02] REPLICATION: Medical state is server-authoritative. Clients
 *   receive replicated values but shouldn't modify directly.
 *
 * =============================================================================
 * RELATED FILES
 * =============================================================================
 * - MOVitalsComponent.h - Blood, O2, temp, heart rate
 * - MOAnatomyComponent.h - Body parts, wounds
 * - MOMentalStateComponent.h - Consciousness, morale
 * - MOMetabolismComponent.h - Hunger, thirst, digestion
 *
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MOMedicalProviderInterface.generated.h"

class UMOVitalsComponent;
class UMOAnatomyComponent;
class UMOMentalStateComponent;
class UMOAdrenalineComponent;
class UMOMetabolismComponent;

/**
 * Interface for medical component access.
 * See file header for usage patterns and pitfalls.
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UMOMedicalProviderInterface : public UInterface
{
	GENERATED_BODY()
};

class MOFRAMEWORKMEDICAL_API IMOMedicalProviderInterface
{
	GENERATED_BODY()

public:
	/**
	 * Get the vitals component (health, stamina, etc.).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Medical")
	UMOVitalsComponent* GetVitals() const;

	/**
	 * Get the anatomy component (body parts, injuries).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Medical")
	UMOAnatomyComponent* GetAnatomy() const;

	/**
	 * Get the mental state component (stress, morale).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Medical")
	UMOMentalStateComponent* GetMentalState() const;

	/**
	 * Get the adrenaline component.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Medical")
	UMOAdrenalineComponent* GetAdrenaline() const;

	/**
	 * Get the metabolism component (hunger, thirst).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="MO|Medical")
	UMOMetabolismComponent* GetMetabolism() const;
};
