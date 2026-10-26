/**
 * =============================================================================
 * MOAppearanceSubsystem.h - Character Appearance Asset Manager
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * GameInstance subsystem for discovering and managing character appearance
 * assets (faces, hair, bodies). Scans content folders including mod paths
 * and builds a catalog for character customization UI.
 *
 * FEATURES:
 * - DiscoverAppearanceAssets(): Scans configured paths
 * - GetAssetsForCategory(): Returns discovered assets by type
 * - ApplyAppearance(): Applies FMOCharacterAppearance to character
 * - GenerateRandomAppearance(): Creates random valid appearance
 *
 * MOD SUPPORT:
 * - Scans AdditionalModPaths for modded assets
 * - Assets tagged bIsModded=true in catalog
 * - Supports runtime asset discovery
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] DISCOVERY TIMING: Call DiscoverAppearanceAssets() early (game
 *   start or mod load). Character creation UI depends on populated catalog.
 *
 * [2024-02] ASYNC LOADING: LoadSkeletalMesh/LoadGroomAsset are synchronous.
 *   Consider async loading for large asset sets.
 *
 * [2024-02] TAG INFERENCE: InferTagsFromAssetName() uses naming conventions.
 *   Assets should follow "Gender_Type_Name" pattern for proper tagging.
 *
 * [2026-10-25] GROOMS: the hair / beard / mustache styles are the MetaHuman groom library (70 grooms, Tools/import_metahuman_grooms.py) under
 *   HairContentPath. A groom goes on a pawn with the GroomBindingAsset that binds it to the pawn's FACE mesh (Content/Python/make_groom_bindings.py
 *   makes one per groom and face; FindGroomBinding looks it up by the binding's Groom registry tag). No binding: the engine projects the groom
 *   at run time (a GPU cost once), logged.
 *
 * =============================================================================
 * RELATED FILES: MOCharacterAppearance.h, MOMetaHumanCharacter.h, MOCustomizableCharacter.h
 * LAST UPDATED: 2026-10-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MOCharacterAppearance.h"
#include "MOAppearanceSubsystem.generated.h"

class AMOMetaHumanCharacter;
class USkeletalMesh;
class UGroomAsset;
class UGroomBindingAsset;
class UGroomComponent;
UCLASS()
class MOFRAMEWORK_API UMOAppearanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ============================================================================
	// ASSET DISCOVERY
	// ============================================================================

	/**
	 * Scan content folders for available appearance assets.
	 * Call this at game start or when mods are loaded.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	void DiscoverAppearanceAssets();

	/** DiscoverAppearanceAssets once (the character creator asks for the catalog when it opens). */
	void EnsureDiscovered() { if (!bDiscoveryComplete) { DiscoverAppearanceAssets(); } }

	/**
	 * Get all discovered assets for a category.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	TArray<FMOAppearanceAssetEntry> GetAssetsForCategory(EMOAppearanceCategory Category) const;

	/**
	 * Get assets filtered by tags (e.g., "Male", "Female").
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	TArray<FMOAppearanceAssetEntry> GetAssetsForCategoryWithTags(EMOAppearanceCategory Category, const TArray<FName>& RequiredTags) const;

	/**
	 * Check if a specific asset path exists in the catalog.
	 */
	UFUNCTION(BlueprintPure, Category="MO|Appearance")
	bool IsAssetPathValid(const FString& AssetPath) const;

	// ============================================================================
	// APPEARANCE APPLICATION
	// ============================================================================

	/**
	 * Apply a full appearance to a MetaHuman character.
	 * Loads and sets all meshes, grooms, and parameters.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	bool ApplyAppearance(AMOMetaHumanCharacter* Character, const FMOCharacterAppearance& Appearance);

	/**
	 * Apply only the face mesh.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	bool ApplyFace(AMOMetaHumanCharacter* Character, const FString& FaceAssetPath);

	/**
	 * Apply only the scalp hair (a groom path; FMOCharacterAppearance::NoGroom = bald): ApplyGroomsToCharacter with the other slots left alone.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	bool ApplyHair(AMOMetaHumanCharacter* Character, const FString& HairAssetPath);

	/**
	 * Put an appearance's grooms on a MetaHuman pawn: each slot (hair, eyebrows, eyelashes, beard, mustache) is the pawn's groom component named
	 * after it (Hair, Eyebrows1, ...; a missing one is created on the face at the MetaHuman's socket), given the groom and its binding to the
	 * pawn's face (FindGroomBinding). An empty path leaves the slot as the Blueprint made it, NoGroom empties it, an unknown path is logged and left.
	 * Cheap when nothing changes (a slot already wearing its groom is skipped). Only once the pawn has begun play (its Blueprint components exist);
	 * no-op on a dedicated server. Returns false if a groom could not be loaded. Static like ApplyColorsToActor: the creator's pawn and gameplay pawns.
	 * bWaitUntilReady: a slot whose new groom or binding cannot draw yet keeps its old groom and sets *bOutPending (AMOMetaHumanCharacter::ApplyGrooms
	 * retries); *bOutChanged: some slot swapped (its colours need applying again).
	 */
	static bool ApplyGroomsToCharacter(AMOMetaHumanCharacter* Character, const FMOCharacterAppearance& Appearance, bool bWaitUntilReady = true,
		bool* bOutPending = nullptr, bool* bOutChanged = nullptr);

	/** Every binding of a groom (any face), from the registry alone: the creator preloads a style's bindings with it. */
	static TArray<FSoftObjectPath> FindGroomBindingPaths(const FSoftObjectPath& Groom);

	/** The appearance's groom path for a groom slot (Hair, Eyebrows, Eyelashes, Beard, Mustache); null for Face / Body. */
	static FString* GetGroomPath(FMOCharacterAppearance& Appearance, EMOAppearanceCategory Slot);

	/** The actor's groom component for a slot (named after it: Hair, Eyebrows1, Beard ...), or null. */
	static UGroomComponent* FindGroomComponent(const AActor* Actor, EMOAppearanceCategory Slot);

	/** The binding that puts Groom on Face: a GroomBindingAsset whose Groom registry tag is this groom and whose target mesh is Face; null if none. */
	static UGroomBindingAsset* FindGroomBinding(const UGroomAsset* Groom, const USkeletalMesh* Face);

	/** A MetaHuman groom's name for the creator: "Hair_M_SideSweptFringe" -> "Side swept fringe (medium)", "Goatee_S_ChinStrap" -> "Chin strap goatee
	 *  (short)", "Mustache_L_Handlebar" -> "Handlebar mustache (long)". Any other name: its words. Pure: unit-tested. */
	static FText MakeGroomDisplayName(const FString& AssetName);

	/**
	 * Whether a groom at Location simulates its physics (MO.Hair.Simulation): 0 never, 1 always, 2 (the default) only near the world origin, where
	 * the engine's hair simulation is steady -- every coordinate within RadiusCm (MO.Hair.SimulationRadiusKm), back on only HairSimulationReturnCm
	 * inside it (bCurrentlyOn: no flapping on the line). Fork F39 option D (Wes, Oct 26). Pure: unit-tested.
	 */
	static bool WantsHairSimulation(int32 Mode, const FVector& Location, bool bCurrentlyOn, double RadiusCm);

	/** How far inside the radius a groom that stopped simulating starts again. */
	static constexpr double HairSimulationReturnCm = 50000.0;

	/** Apply MO.Hair.Simulation to every groom of Actor where it stands now (a groom whose state changes is rebuilt; the rest cost nothing). */
	static void ApplyHairSimulation(AActor* Actor);

	/**
	 * Apply body parameters (for Mutable integration).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	bool ApplyBodyParameters(AMOMetaHumanCharacter* Character, float BodyFat, float MuscleMass, float Height);

	/**
	 * Apply color parameters (skin, hair, eyes): ApplyColorsToActor.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	bool ApplyColors(AMOMetaHumanCharacter* Character, const FMOCharacterAppearance& Appearance);

	/**
	 * Push an appearance's colours into an actor's MetaHuman materials: every skin material (M_skin_unified_baked: body, face LODs -- the
	 * post-bake basecolor value / saturation / multiply), the hair and eyebrow grooms (M_hair_v4: hairMelanin / hairRedness; eyelashes keep
	 * their own) and the eyes (M_eye_eyeball_unified: the iris preset). Creates the dynamic instances it needs; a material without the
	 * parameter is left alone. Static: the character creator's preview pawn and the gameplay pawns share it. No-op on a dedicated server.
	 */
	static void ApplyColorsToActor(AActor* Actor, const FMOCharacterAppearance& Appearance);

	/** Skin tone -> the skin material's post-bake basecolor value (brightness multiplier), saturation offset and multiply tint. Pure: unit-tested. */
	static void ComputeSkinParameters(float SkinTone, float& OutValue, float& OutSaturation, FLinearColor& OutMultiply);

	/** The iris colours the creator offers, in display order. */
	static const TArray<FMOEyeColorPreset>& GetEyeColorPresets();

	/** A preset by id, or null (None and unknown ids leave the iris as shipped). */
	static const FMOEyeColorPreset* FindEyeColorPreset(FName Id);

	/** Random colours: any skin tone, natural hair (melanin, a little red), any eye preset. */
	static void RandomizeColors(FMOCharacterAppearance& Appearance);

	// ============================================================================
	// RANDOM GENERATION
	// ============================================================================

	/**
	 * Generate a random appearance from available assets.
	 * @param bMale If true, filter to male-tagged assets; if false, female.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Appearance")
	FMOCharacterAppearance GenerateRandomAppearance(bool bMale = true);

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Base content path for faces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Paths")
	FString FacesContentPath = TEXT("/Game/Characters/Faces");

	/** Base content path for grooms: its Hair, Eyebrows, Eyelashes, Beards and Mustaches folders (the MetaHuman library's own layout). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Paths")
	FString HairContentPath = TEXT("/Game/MetaHumans/Common/Optional/Grooms/GroomAssets");

	/** Base content path for body meshes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Paths")
	FString BodiesContentPath = TEXT("/Game/Characters/Bodies");

	/** Additional mod paths to scan (e.g., "/Game/Mods/Characters"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Appearance|Paths")
	TArray<FString> AdditionalModPaths;

protected:
	/** Scan a directory for skeletal mesh assets. */
	void ScanDirectoryForSkeletalMeshes(const FString& Path, EMOAppearanceCategory Category, bool bIsModded);

	/** Scan a directory for groom assets. */
	void ScanDirectoryForGrooms(const FString& Path, EMOAppearanceCategory Category, bool bIsModded);

	/** Extract display name from asset path. */
	FText GetDisplayNameFromPath(const FString& AssetPath) const;

	/** Infer tags from asset name (e.g., "Male_Face_01" -> ["Male"]). */
	TArray<FName> InferTagsFromAssetName(const FString& AssetName) const;

	/** Load a skeletal mesh from path (async-friendly). */
	USkeletalMesh* LoadSkeletalMesh(const FString& AssetPath) const;

	/** Load a groom asset from path. */
	UGroomAsset* LoadGroomAsset(const FString& AssetPath) const;

private:
	/** Discovered assets by category (runtime cache, not serialized). */
	TMap<EMOAppearanceCategory, TArray<FMOAppearanceAssetEntry>> AssetCatalog;

	/** Set of all known asset paths for quick validation. */
	TSet<FString> KnownAssetPaths;

	/** Whether discovery has been run. */
	bool bDiscoveryComplete = false;
};
