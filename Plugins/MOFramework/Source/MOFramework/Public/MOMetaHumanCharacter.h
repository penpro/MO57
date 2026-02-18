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
