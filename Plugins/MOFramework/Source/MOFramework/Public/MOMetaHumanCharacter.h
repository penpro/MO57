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
 * [2024-02] GROOMS IN BLUEPRINT: the Blueprint's groom components are the
 *   default look. [2026-10-25] The appearance's groom paths swap them at run
 *   time (UMOAppearanceSubsystem::ApplyGroomsToCharacter, with a binding per
 *   groom and face mesh; it adds the beard / mustache components the survivor
 *   Blueprint lacks).
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
	AMOMetaHumanCharacter(const FObjectInitializer& ObjectInitializer);

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

	/** Take only the look of Look -- the colours (skin tone, hair, eyes) and the grooms (hair, beard, mustache styles; an empty path keeps the
	 *  Blueprint's own) -- and put it on the pawn, no body / face mesh reload: the character creator's live controls and every place that restores
	 *  a look over the pawn's own meshes. Before BeginPlay only the record changes (ApplySkinLook puts the grooms on then). */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void SetAppearanceLook(const FMOCharacterAppearance& Look);

	/** Apply the current stored appearance (call after spawning or loading). */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void ApplyCurrentAppearance();

	/** Put Look's grooms on (UMOAppearanceSubsystem::ApplyGroomsToCharacter): a slot whose new groom is still loading keeps its old one, and this
	 *  tries again every GroomRetrySeconds with the pawn's CURRENT look (the latest choice wins) until it can, or GroomReadyTimeoutSeconds have
	 *  passed (then it swaps anyway). A swap re-applies the colours (a new groom brings untinted materials). False if a groom could not load. */
	bool ApplyGrooms(const FMOCharacterAppearance& Look);

	/** A groom that will not get ready (a broken asset) is swapped in after this long anyway. */
	UPROPERTY(EditDefaultsOnly, Category="MO|Appearance")
	float GroomReadyTimeoutSeconds = 30.0f;

	UPROPERTY(EditDefaultsOnly, Category="MO|Appearance")
	float GroomRetrySeconds = 0.2f;

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

	/** Current character appearance data. Replicated: a co-op partner sees the look this pawn's owner chose. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance", SaveGame, ReplicatedUsing=OnRep_CurrentAppearance)
	FMOCharacterAppearance CurrentAppearance;

	UFUNCTION()
	void OnRep_CurrentAppearance();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The skin look pass (AMOCharacter) + this character's grooms and colours (skin tone, hair, eyes): it runs at BeginPlay and again once the
	 *  MetaHuman component has rebuilt its face materials, so the look survives that rebuild. */
	virtual void ApplySkinLook() override;

private:
	FTimerHandle GroomRetryTimer;
	/** When the oldest groom still waiting to swap in was asked for (world seconds); negative = none waiting. */
	double GroomWaitStartSeconds = -1.0;
};
