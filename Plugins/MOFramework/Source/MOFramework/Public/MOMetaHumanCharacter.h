/**
 * =============================================================================
 * MOMetaHumanCharacter.h - MetaHuman-Compatible Character Class
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Extends AMOCharacter with MetaHuman mesh support. Uses 2 skeletal meshes:
 * - Body (inherited Mesh component) - Main skeleton, acts as LEADER
 * - Face (FaceMesh component) - Head with facial rig, follows Body
 *
 * BLUEPRINT SETUP:
 * 1. Create Blueprint child of this class
 * 2. Set Body mesh on inherited Mesh component
 * 3. Set Face mesh on FaceMesh component
 * 4. Add Groom components for hair/eyebrows as needed
 * 5. Configure Animation Blueprint on Body mesh
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] LEADER POSE: Face mesh uses SetLeaderPoseComponent to follow Body.
 *   Set bAutoSetupLeaderPose=true (default) or call SetupLeaderPoseComponents().
 *
 * [2024-02] APPEARANCE APPLY ORDER: Appearance must be applied AFTER meshes
 *   are set up. bAutoApplyAppearance handles this in BeginPlay.
 *
 * [2024-02] GROOMS IN BLUEPRINT: Groom components for hair must be added in
 *   Blueprint, not C++. They need specific binding groups per MetaHuman.
 *
 * =============================================================================
 * RELATED FILES: MOCharacter.h, MOCharacterAppearance.h, MOMorphDefinitionRow.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOCharacter.h"
#include "MOCharacterAppearance.h"
#include "MOMetaHumanCharacter.generated.h"

class USkeletalMeshComponent;
class UMOAppearanceSubsystem;

/**
 * MetaHuman-compatible character class.
 * Extends AMOCharacter with a Face mesh component that follows the Body.
 *
 * Uses 2 skeletal meshes:
 * - Body (inherited Mesh) - Main skeleton, acts as LEADER
 * - Face - Head with facial rig, follows Body via SetLeaderPoseComponent
 *
 * Add Groom components in Blueprint as needed for hair/eyebrows/etc.
 *
 * Usage:
 * 1. Create a Blueprint child of this class
 * 2. Set the Body skeletal mesh on the inherited Mesh component
 * 3. Set Face mesh on the FaceMesh component
 * 4. Add Groom components in Blueprint for hair as needed
 * 5. Configure the Animation Blueprint on the Body mesh
 */
UCLASS()
class MOFRAMEWORK_API AMOMetaHumanCharacter : public AMOCharacter
{
	GENERATED_BODY()

public:
	AMOMetaHumanCharacter();

	// ============================================================================
	// METAHUMAN COMPONENT ACCESSORS
	// ============================================================================

	/** Get the Face skeletal mesh component. */
	UFUNCTION(BlueprintPure, Category="MO|MetaHuman")
	USkeletalMeshComponent* GetFaceMesh() const { return FaceMesh; }


	// ============================================================================
	// METAHUMAN SETUP
	// ============================================================================

	/**
	 * Set up the MetaHuman mesh references at runtime.
	 * This configures the leader pose relationship between Body and Face.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|MetaHuman")
	void SetupMetaHumanMeshes(USkeletalMesh* BodyMesh, USkeletalMesh* FaceSkeletalMesh);

	// ============================================================================
	// APPEARANCE
	// ============================================================================

	/** Get the current character appearance. */
	UFUNCTION(BlueprintPure, Category="MO|Appearance")
	const FMOCharacterAppearance& GetAppearance() const { return CurrentAppearance; }

	/** Set and apply a new appearance. */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void SetAppearance(const FMOCharacterAppearance& NewAppearance);

	/** Apply the current stored appearance (call after spawning or loading). */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void ApplyCurrentAppearance();

protected:
	virtual void BeginPlay() override;

	/** Initialize the leader pose component relationship. */
	void SetupLeaderPoseComponents();

	// ============================================================================
	// METAHUMAN MESH COMPONENTS
	// ============================================================================

	/**
	 * Face skeletal mesh component (head with facial rig).
	 * Follows the Body mesh via SetLeaderPoseComponent.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MO|MetaHuman")
	TObjectPtr<USkeletalMeshComponent> FaceMesh;


	// ============================================================================
	// METAHUMAN CONFIGURATION
	// ============================================================================

	/** Socket name on the body mesh where the face attaches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|MetaHuman")
	FName FaceAttachSocket = NAME_None;

	/** Whether to automatically set up leader pose in BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|MetaHuman")
	bool bAutoSetupLeaderPose = true;

	/** Whether to automatically apply appearance in BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|MetaHuman")
	bool bAutoApplyAppearance = true;

	/** Current character appearance data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance", SaveGame)
	FMOCharacterAppearance CurrentAppearance;
};
