/**
 * MOSurvivorBase.h - the survivor bases the character creator offers
 *
 * Wes, Oct 27 2026: "i made a few bases with the new metahumans stuff, see if you can get them all customizable and out into the game". A base is
 * a MetaHuman built in the MetaHuman Creator (made and baked in MO_Tools: Tools/body_shape) -- its body and face meshes, which carry the body
 * shapes (MOBodyShape: heavier / thinner with the body fat the metabolism simulates, more muscular / wasted with its muscle, and the creator's
 * build sliders), its own grooms, and the body fat and muscle it was made with: the shapes' neutral point and the survivor's starting body.
 * The creator's Base picker steps through Bases in order; a new base is one more entry in Config/DefaultGame.ini (no code). An appearance names
 * its base by Id (FMOCharacterAppearance::BaseId; None = the pawn Blueprint's own meshes).
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOSurvivorBase.generated.h"

struct FMOCharacterAppearance;

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOSurvivorBase
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	FName Id;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	FText DisplayName;

	/** The body skeletal mesh (the MetaHuman body skeleton; it carries the MO_Body_* shapes). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="MO|Appearance", meta=(AllowedClasses="/Script/Engine.SkeletalMesh"))
	FSoftObjectPath Body;

	/** The face skeletal mesh (its neck carries the body shapes too: the seam moves with the body). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="MO|Appearance", meta=(AllowedClasses="/Script/Engine.SkeletalMesh"))
	FSoftObjectPath Face;

	/** The base's own grooms (GroomAsset paths, each with a binding to Face), worn when the base is chosen; FMOCharacterAppearance::NoGroom = none. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	FString Hair;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	FString Eyebrows;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	FString Eyelashes;

	/** Facial hair (the men Wes made, Oct 27: Harvey, Trey). Empty = clean-shaven: taking a base without one takes the last base's beard off. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	FString Beard;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	FString Mustache;

	/** The body fat (%) and muscle (kg) the base was made with (MO_Tools bake.py prints them: "NEUTRAL"). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	float BodyFatPercent = 18.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="MO|Appearance")
	float MuscleMassKg = 30.0f;

	/** Put this base on a look: its id and its grooms (the hair the player then picks replaces its hair). */
	void ApplyToLook(FMOCharacterAppearance& Look) const;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="MO Framework - Survivor Bases"))
class MOFRAMEWORK_API UMOSurvivorBaseSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** The bases, in the creator's order. */
	UPROPERTY(Config, EditAnywhere, Category="MO|Appearance")
	TArray<FMOSurvivorBase> Bases;

	static const TArray<FMOSurvivorBase>& GetBases();

	/** The base of this id, or null (None, or an id no longer listed: the pawn keeps its Blueprint's meshes). */
	static const FMOSurvivorBase* Find(FName Id);
};
