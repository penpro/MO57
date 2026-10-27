// A body that follows its composition and the build chosen for it (Wes, Oct 26: "a single character go from skinny to fat and vice versa,
// starvation etc in the survival mechanics"; "can we adjust any body parameters independently? breasts, butt, waist, etc?"). The shapes
// are morph targets baked onto the MetaHuman's own body by UMOBodyShapeBakeLibrary (MOBodyShapeEditor) from the MetaHuman Creator's body
// model; this sets their weights from named drivers:
// - BodyFat (percent of body weight) and MuscleMass (kg): what the game simulates (MO57: FMOBodyComposition in the metabolism);
// - Bust, Hips, Waist, Thighs (-1..1): the build chosen at character creation, each between a smaller and a larger shape.
// Weights are continuous in their driver, so the body changes as gradually as the driver does. Visual only and per machine: each machine
// applies the drivers it has.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOBodyShapeComponent.generated.h"

/** One baked shape and the stretch of its driver over which it comes in. */
USTRUCT(BlueprintType)
struct MOBODYSHAPE_API FMOBodyShapeMorph
{
	GENERATED_BODY()

	/** The morph target on the body (and on any mesh that carries one of the same name: the face, clothing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|BodyShape")
	FName MorphTarget;

	/** What it follows: BodyFat (%), MuscleMass (kg), or a build slider (Bust, Hips, Waist, Thighs: -1..1). Any name works: SetDriver sets it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|BodyShape")
	FName Driver = TEXT("BodyFat");

	/** The driver value where the shape is off: the body as authored (MO57's starting composition: 18 % body fat, 30 kg of muscle). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|BodyShape")
	float DriverAtZero = 18.0f;

	/** The driver value where the shape is fully on (below DriverAtZero for a smaller shape). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|BodyShape")
	float DriverAtFull = 40.0f;

	/** 0..1: the shape's weight at DriverValue. */
	float WeightFor(float DriverValue) const;
};

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOBODYSHAPE_API UMOBodyShapeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static const FName BodyFat;      // percent of body weight
	static const FName MuscleMass;   // kg

	UMOBodyShapeComponent();

	/**
	 * The shapes and their ranges. Defaults: heavier from 18 % to 40 % body fat, thinner from 18 % down to 8 %; more muscular from 30 kg
	 * of muscle up to 40, wasted down to 20; each build slider between its smaller shape at -1 and its larger at +1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|BodyShape")
	TArray<FMOBodyShapeMorph> Shapes;

	/** The body's composition now: MO57's FMOBodyComposition as it is (body fat %, muscle mass kg). */
	UFUNCTION(BlueprintCallable, Category="MO|BodyShape")
	void SetComposition(float InBodyFatPercent, float InMuscleMassKg);

	/**
	 * The composition the mesh was made at: the weight and muscle shapes are off there and reach their full shapes at the same composition on
	 * every body (a base built slim starts thinner and has less far to go). Each base knows its own (the bake prints it: Lillian 12.4 % / 28.5
	 * kg); the defaults are 18 % / 30 kg.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|BodyShape")
	void SetNeutralComposition(float BodyFatPercent, float MuscleMassKg);

	/** Set one driver (a build slider, or BodyFat / MuscleMass) and apply. */
	UFUNCTION(BlueprintCallable, Category="MO|BodyShape")
	void SetDriver(FName Driver, float Value);

	/** A driver's value; one never set reads as its shapes' neutral value (their DriverAtZero), 0 if no shape follows it. */
	UFUNCTION(BlueprintPure, Category="MO|BodyShape")
	float GetDriver(FName Driver) const;

	/** Every driver the shapes follow, once each, in the order of Shapes. */
	UFUNCTION(BlueprintPure, Category="MO|BodyShape")
	TArray<FName> GetDriverNames() const;

	/** The span of a driver over which its shapes change: the lowest and highest of their DriverAtZero / DriverAtFull. */
	UFUNCTION(BlueprintPure, Category="MO|BodyShape")
	void GetDriverRange(FName Driver, float& OutMin, float& OutMax) const;

	/** Every driver back to neutral (the body as authored). */
	UFUNCTION(BlueprintCallable, Category="MO|BodyShape")
	void ResetDrivers();

	/** Set the shapes' weights on every skeletal mesh of the owner that carries them. */
	UFUNCTION(BlueprintCallable, Category="MO|BodyShape")
	void ApplyShapes();

protected:
	virtual void BeginPlay() override;

	/** Every driver set so far. */
	UPROPERTY(EditAnywhere, Category="MO|BodyShape")
	TMap<FName, float> Drivers;
};
