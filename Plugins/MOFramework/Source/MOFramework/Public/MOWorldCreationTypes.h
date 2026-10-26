/**
 * =============================================================================
 * MOWorldCreationTypes.h - A world's creation settings: its shape, its pace, where it starts
 * =============================================================================
 *
 * Wes, Oct 22 2026: "implement a suite of 'global multipliers' for terraforming, crafting, gatherings, inspecting, etc so we can configure those
 * on a per world basis and add those to the world generation menu, along with some knobs to adjust stuff like biome size, overall world height,
 * etc, whatever makes sense, and then a 'starting biome' selector".
 *
 * Three parts, chosen once in the New Game panel and kept with the world for good (UMOWorldSaveGame::WorldCreation):
 *   - FMOWorldShape: how the land is generated (biome size, height, mountains, lakes, caves, vegetation). It travels with the world seed
 *     (FMOWorldSeedInfo) because every machine generates its own terrain: a joiner must have it before its first chunk.
 *     UMOWorldGenSettings::BuildParams folds it into the generator tuning (FMOWorldCreation::ApplyShape); all 1 = the tuning as authored, bit for bit.
 *   - FMOWorldPace: how long the work takes (gathering, crafting, building, terraforming, inspecting) and how fast skills grow. Replicated on
 *     AMOGameState. Every duration site asks FMOWorldRules::ScaleSeconds with its activity; the next activity is one enum entry and one call.
 *     Principle 11 still holds: a multiplier never reaches zero (a 0.1 floor), 1 is real time, and it is a property of the WORLD chosen before
 *     the first step, not a mid-game skip.
 *   - StartingBiome: where a new game's first survivor stands (AMOGameMode::FindNewGameSpawn); None = the coast, as before.
 *
 * The knob table (FMOWorldCreation::GetKnobs) is the one list the UI, the console (MO.World.Creation) and the clamps read: a new knob is a field
 * plus one table entry.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOWorldCreationTypes.generated.h"

struct FMOWorldGenTuning;

/** The kinds of work a world's pace scales (FMOWorldRules). */
UENUM(BlueprintType)
enum class EMOWorldPace : uint8
{
	Gathering,      // harvesting, chopping, mining, foraging, butchering, gathering cover
	Crafting,       // the crafting queue
	Building,       // construction progress
	Terraforming,   // digging and filling ground
	Inspecting,     // studying an item
	SkillGain,      // skill experience (a rate: more is faster, unlike the times above)
	Growing         // a crop's season, sowing to ripe (Docs/Farming_And_Husbandry.md; DECISIONS F42)
};

/** How the land is generated. Every field 1 = the generator as authored. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWorldShape
{
	GENERATED_BODY()

	/** Biome patches: the climate fields' wavelengths (moisture, temperature). 2 = biomes twice as wide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float BiomeSize = 1.0f;

	/** The whole land's height (the generator's VerticalScale): hills, mountains and valleys alike. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float WorldHeight = 1.0f;

	/** Mountain ranges on top of the land: massifs, peaks and the old single mountain field. 0 = no ranges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float Mountains = 1.0f;

	/** How often ponds, lakes and upland lakes form (their cells' chances; a chance never passes 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float Lakes = 1.0f;

	/** How often a cave opens to the surface (the entrance cells' chance). 0 = no cave mouths. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float Caves = 1.0f;

	/** Every biome species' density (trees, plants, rocks). Above 1 costs frame time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float Vegetation = 1.0f;

	bool operator==(const FMOWorldShape& O) const
	{
		return BiomeSize == O.BiomeSize && WorldHeight == O.WorldHeight && Mountains == O.Mountains && Lakes == O.Lakes && Caves == O.Caves && Vegetation == O.Vegetation;
	}
	bool operator!=(const FMOWorldShape& O) const { return !(*this == O); }
	bool IsDefault() const { return *this == FMOWorldShape(); }
};

/** How long the work takes. Times: 1 = real time, 0.5 = twice as fast, 2 = twice as slow. SkillGain is a rate: 2 = skills grow twice as fast. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWorldPace
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float Gathering = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float Crafting = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float Building = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float Terraforming = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float Inspecting = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float SkillGain = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float Growing = 1.0f;

	/** The multiplier of one activity, clamped to the knob's range (never zero). */
	float For(EMOWorldPace Activity) const;

	bool operator==(const FMOWorldPace& O) const
	{
		return Gathering == O.Gathering && Crafting == O.Crafting && Building == O.Building && Terraforming == O.Terraforming && Inspecting == O.Inspecting
			&& SkillGain == O.SkillGain && Growing == O.Growing;
	}
	bool operator!=(const FMOWorldPace& O) const { return !(*this == O); }
};

/** Everything a world was created with. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWorldCreationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	FMOWorldShape Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	FMOWorldPace Pace;

	/** DT_Biomes row the first survivor starts in; None = the coast (FMOWorldGen::FindCoastalSpawn). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	FName StartingBiome;

	/**
	 * CREATIVE MODE (Wes, Oct 24 2026: "make a 'creative mode' where crafting and building costs nothing"): 1 = recipes and buildings need and take
	 * no materials (FMOWorldRules::IsCreative); 0 = off. Time still runs: a craft or a build takes its full (paced) time -- Principle 11 holds, only
	 * the cost goes. A 0 / 1 knob (the panel shows Off / On); `MO.World.Creative 0|1` switches a running world on the host.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="MO|World")
	float CreativeMode = 0.0f;

	/**
	 * Where this world's (0,0) lies in the generator's field (cm, whole metres): the start the new game chose (AMOGameMode::ChooseNewWorldOrigin),
	 * so the first survivor and the colony stand at the origin, where floats are finest (Wes, Oct 26 2026). Not a knob: set once by the game, saved
	 * with the world (a world saved before has none: (0,0), its land unmoved) and published with the seed (FMOWorldSeedInfo::Origin).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category="MO|World")
	FVector2D WorldOrigin = FVector2D::ZeroVector;
};

/** Which part of the settings a knob belongs to (the panel shows Shape in one box, Pace and Rules in the other). */
UENUM(BlueprintType)
enum class EMOWorldKnobGroup : uint8
{
	Shape,
	/** Time multipliers: never zero (Principle 11), floored at PaceFloor. */
	Pace,
	/** On / off world rules (bSwitch knobs, e.g. CreativeMode): not multipliers, so the pace floor does not apply. */
	Rules
};

/** One adjustable number of the creation settings: its range, its step and how the panel names it. */
USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOWorldKnob
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="MO|World")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category="MO|World")
	EMOWorldKnobGroup Group = EMOWorldKnobGroup::Shape;

	UPROPERTY(BlueprintReadOnly, Category="MO|World")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category="MO|World")
	FText Hint;

	UPROPERTY(BlueprintReadOnly, Category="MO|World")
	float Min = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|World")
	float Max = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category="MO|World")
	float Step = 0.05f;

	UPROPERTY(BlueprintReadOnly, Category="MO|World")
	float Default = 1.0f;

	/** An on / off switch rather than a multiplier (Min 0, Max 1, Step 1): the panel shows Off / On instead of "x1". */
	UPROPERTY(BlueprintReadOnly, Category="MO|World")
	bool bSwitch = false;

	/** Get / set this knob's field (the table's accessors). */
	float* Field(FMOWorldCreationSettings& Settings) const { return Accessor ? Accessor(Settings) : nullptr; }

	float* (*Accessor)(FMOWorldCreationSettings&) = nullptr;
};

struct MOFRAMEWORK_API FMOWorldCreation
{
	/** Every knob, in panel order (shape first, then pace). */
	static const TArray<FMOWorldKnob>& GetKnobs();

	static const FMOWorldKnob* FindKnob(FName Id);

	/** The knob's value in Settings (its default if Id is unknown). */
	static float GetKnob(const FMOWorldCreationSettings& Settings, FName Id);

	/** Sets the knob, clamped to its range and snapped to its step. False if Id is unknown. */
	static bool SetKnob(FMOWorldCreationSettings& Settings, FName Id, float Value);

	/** Every knob clamped into its range (a save or a console line may hold anything). */
	static void Sanitize(FMOWorldCreationSettings& Settings);

	/** The shape folded into the generator tuning. All 1 leaves the tuning untouched (x 1.0 is exact). */
	static void ApplyShape(const FMOWorldShape& Shape, FMOWorldGenTuning& Tuning);

	/** One line for logs and bug reports: "shape biome=1 height=1 ... pace gather=1 ... start=Rainforest". */
	static FString Describe(const FMOWorldCreationSettings& Settings);

	/** A biome's band floor must reach within this of sea level for a game to start in it. */
	static constexpr float StartingBiomeMaxFloorCm = 5000.0f;

	/** Can a new game start in a biome with this band (Wes, Oct 23: "gate the higher elevation biomes, so no alpine heath, or alpine rock or
	 *  highlands")? Only lowland ground: the band reaches down to within StartingBiomeMaxFloorCm of the sea (not AlpineHeath, AlpineRock) and
	 *  asks no minimum slope (not RockyHighland). The New Game panel's cards and AMOGameMode::FindNewGameSpawn both ask this. */
	static bool IsStartingBand(float HeightMinCm, float SlopeMinDeg) { return HeightMinCm < StartingBiomeMaxFloorCm && SlopeMinDeg <= 0.0f; }
};

/** The world's pace at runtime (AMOGameState::GetWorldPace on every machine). */
struct MOFRAMEWORK_API FMOWorldRules
{
	/** The multiplier for this activity in WorldContext's world (1 with no MO game state, e.g. the editor or a test world). */
	static float GetPace(const UObject* WorldContext, EMOWorldPace Activity);

	/** Seconds of a timed activity in this world: Seconds x the activity's pace (never below a tenth); 0 for an instant one (IsInstant). */
	static float ScaleSeconds(const UObject* WorldContext, EMOWorldPace Activity, float Seconds);

	/** Creative mode (FMOWorldCreationSettings::CreativeMode): crafting and building cost no materials and take no time. False with no MO game state. */
	static bool IsCreative(const UObject* WorldContext);

	/**
	 * Whether this activity takes no time in this world: building and crafting in a creative world (Wes, Oct 26 2026: "in creative mode make it so
	 * building happens instantly, no build time"; crafting the same). The one exception to Principle 11 -- a world rule chosen when the world is
	 * created, never a skip inside a survival world. Pure on (bCreative, Activity) through IsInstantActivity: unit-tested.
	 */
	static bool IsInstant(const UObject* WorldContext, EMOWorldPace Activity);
	static bool IsInstantActivity(bool bCreative, EMOWorldPace Activity);
};
