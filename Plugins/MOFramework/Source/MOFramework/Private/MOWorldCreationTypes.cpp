#include "MOWorldCreationTypes.h"
#include "MOGameState.h"
#include "MOWorldGen.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "MOWorldCreation"

namespace
{
	// A pace never reaches zero (Principle 11: no instant work), nor a tenth of real time.
	constexpr float PaceFloor = 0.1f;

	FMOWorldKnob Knob(const TCHAR* Id, EMOWorldKnobGroup Group, const FText& Label, const FText& Hint, float Min, float Max, float Step, float* (*Accessor)(FMOWorldCreationSettings&))
	{
		FMOWorldKnob K;
		K.Id = FName(Id);
		K.Group = Group;
		K.Label = Label;
		K.Hint = Hint;
		K.Min = Min;
		K.Max = Max;
		K.Step = Step;
		K.Default = 1.0f;
		K.Accessor = Accessor;
		return K;
	}

	TArray<FMOWorldKnob> BuildKnobs()
	{
		using G = EMOWorldKnobGroup;
		TArray<FMOWorldKnob> K;
		K.Add(Knob(TEXT("BiomeSize"), G::Shape, LOCTEXT("BiomeSize", "Biome size"), LOCTEXT("BiomeSizeHint", "How wide each biome's land runs before the next begins."),
			0.5f, 2.5f, 0.1f, [](FMOWorldCreationSettings& S) { return &S.Shape.BiomeSize; }));
		K.Add(Knob(TEXT("WorldHeight"), G::Shape, LOCTEXT("WorldHeight", "World height"), LOCTEXT("WorldHeightHint", "How high the whole land rises: hills, valleys and mountains alike."),
			0.5f, 1.5f, 0.05f, [](FMOWorldCreationSettings& S) { return &S.Shape.WorldHeight; }));
		K.Add(Knob(TEXT("Mountains"), G::Shape, LOCTEXT("Mountains", "Mountains"), LOCTEXT("MountainsHint", "How tall the mountain ranges stand above the land. 0 = none."),
			0.0f, 2.0f, 0.1f, [](FMOWorldCreationSettings& S) { return &S.Shape.Mountains; }));
		K.Add(Knob(TEXT("Lakes"), G::Shape, LOCTEXT("Lakes", "Lakes"), LOCTEXT("LakesHint", "How often ponds and lakes form. 0 = none."),
			0.0f, 2.0f, 0.1f, [](FMOWorldCreationSettings& S) { return &S.Shape.Lakes; }));
		K.Add(Knob(TEXT("Caves"), G::Shape, LOCTEXT("Caves", "Caves"), LOCTEXT("CavesHint", "How often a cave opens to the surface. 0 = no cave mouths."),
			0.0f, 2.0f, 0.1f, [](FMOWorldCreationSettings& S) { return &S.Shape.Caves; }));
		K.Add(Knob(TEXT("Vegetation"), G::Shape, LOCTEXT("Vegetation", "Vegetation"), LOCTEXT("VegetationHint", "How thickly trees, plants and rocks grow. Above 1 costs frame rate."),
			0.25f, 1.5f, 0.05f, [](FMOWorldCreationSettings& S) { return &S.Shape.Vegetation; }));
		K.Add(Knob(TEXT("Gathering"), G::Pace, LOCTEXT("Gathering", "Gathering time"), LOCTEXT("GatheringHint", "Harvesting, chopping, mining, foraging and butchering. 1 = real time."),
			PaceFloor, 4.0f, 0.05f, [](FMOWorldCreationSettings& S) { return &S.Pace.Gathering; }));
		K.Add(Knob(TEXT("Crafting"), G::Pace, LOCTEXT("Crafting", "Crafting time"), LOCTEXT("CraftingHint", "Every recipe's time. 1 = real time."),
			PaceFloor, 4.0f, 0.05f, [](FMOWorldCreationSettings& S) { return &S.Pace.Crafting; }));
		K.Add(Knob(TEXT("Building"), G::Pace, LOCTEXT("Building", "Building time"), LOCTEXT("BuildingHint", "Putting up a structure. 1 = real time."),
			PaceFloor, 4.0f, 0.05f, [](FMOWorldCreationSettings& S) { return &S.Pace.Building; }));
		K.Add(Knob(TEXT("Terraforming"), G::Pace, LOCTEXT("Terraforming", "Digging time"), LOCTEXT("TerraformingHint", "Digging and filling ground. 1 = real time."),
			PaceFloor, 4.0f, 0.05f, [](FMOWorldCreationSettings& S) { return &S.Pace.Terraforming; }));
		K.Add(Knob(TEXT("Inspecting"), G::Pace, LOCTEXT("Inspecting", "Study time"), LOCTEXT("InspectingHint", "Studying an item to learn what it is. 1 = real time."),
			PaceFloor, 4.0f, 0.05f, [](FMOWorldCreationSettings& S) { return &S.Pace.Inspecting; }));
		K.Add(Knob(TEXT("SkillGain"), G::Pace, LOCTEXT("SkillGain", "Skill learning"), LOCTEXT("SkillGainHint", "How fast skills grow with use. 2 = twice as fast."),
			PaceFloor, 4.0f, 0.05f, [](FMOWorldCreationSettings& S) { return &S.Pace.SkillGain; }));
		FMOWorldKnob Creative = Knob(TEXT("CreativeMode"), G::Rules, LOCTEXT("CreativeMode", "Creative mode"),
			LOCTEXT("CreativeModeHint", "Crafting and building cost no materials. The work still takes its time."),
			0.0f, 1.0f, 1.0f, [](FMOWorldCreationSettings& S) { return &S.CreativeMode; });
		Creative.Default = 0.0f;
		Creative.bSwitch = true;
		K.Add(Creative);
		return K;
	}

	float ClampToKnob(const FMOWorldKnob& K, float Value)
	{
		if (!FMath::IsFinite(Value))
		{
			return K.Default;
		}
		const float Snapped = K.Step > 0.0f ? FMath::GridSnap(Value, K.Step) : Value;
		return FMath::Clamp(Snapped, K.Min, K.Max);
	}
}

float FMOWorldPace::For(EMOWorldPace Activity) const
{
	float V = 1.0f;
	switch (Activity)
	{
	case EMOWorldPace::Gathering:    V = Gathering; break;
	case EMOWorldPace::Crafting:     V = Crafting; break;
	case EMOWorldPace::Building:     V = Building; break;
	case EMOWorldPace::Terraforming: V = Terraforming; break;
	case EMOWorldPace::Inspecting:   V = Inspecting; break;
	case EMOWorldPace::SkillGain:    V = SkillGain; break;
	}
	return FMath::IsFinite(V) ? FMath::Clamp(V, PaceFloor, 4.0f) : 1.0f;
}

const TArray<FMOWorldKnob>& FMOWorldCreation::GetKnobs()
{
	static const TArray<FMOWorldKnob> Knobs = BuildKnobs();
	return Knobs;
}

const FMOWorldKnob* FMOWorldCreation::FindKnob(FName Id)
{
	return GetKnobs().FindByPredicate([Id](const FMOWorldKnob& K) { return K.Id == Id; });
}

float FMOWorldCreation::GetKnob(const FMOWorldCreationSettings& Settings, FName Id)
{
	const FMOWorldKnob* K = FindKnob(Id);
	if (!K)
	{
		return 1.0f;
	}
	const float* Field = K->Field(const_cast<FMOWorldCreationSettings&>(Settings));
	return Field ? *Field : K->Default;
}

bool FMOWorldCreation::SetKnob(FMOWorldCreationSettings& Settings, FName Id, float Value)
{
	const FMOWorldKnob* K = FindKnob(Id);
	float* Field = K ? K->Field(Settings) : nullptr;
	if (!Field)
	{
		return false;
	}
	*Field = ClampToKnob(*K, Value);
	return true;
}

void FMOWorldCreation::Sanitize(FMOWorldCreationSettings& Settings)
{
	for (const FMOWorldKnob& K : GetKnobs())
	{
		if (float* Field = K.Field(Settings))
		{
			*Field = FMath::IsFinite(*Field) ? FMath::Clamp(*Field, K.Min, K.Max) : K.Default;   // no snapping: a saved value stays exactly as saved
		}
	}
}

void FMOWorldCreation::ApplyShape(const FMOWorldShape& Shape, FMOWorldGenTuning& T)
{
	// x 1.0 is exact, so a default shape leaves every field (and the params hash, and the golden checksums) as authored.
	T.MoistureWavelengthCm *= Shape.BiomeSize;
	T.TemperatureWavelengthCm *= Shape.BiomeSize;
	T.VerticalScale *= Shape.WorldHeight;
	T.MountainAmplitudeM *= Shape.Mountains;
	T.MassifAmplitudeM *= Shape.Mountains;
	T.PeakAmplitudeM *= Shape.Mountains;
	T.CoastalRangeAmplitudeM *= Shape.Mountains;
	if (Shape.Lakes != 1.0f)
	{
		T.PondChance = FMath::Clamp(T.PondChance * Shape.Lakes, 0.0f, 1.0f);
		T.LakeChance = FMath::Clamp(T.LakeChance * Shape.Lakes, 0.0f, 1.0f);
		T.UplandLakeChance = FMath::Clamp(T.UplandLakeChance * Shape.Lakes, 0.0f, 1.0f);
	}
	if (Shape.Caves != 1.0f)
	{
		T.CaveEntranceChance = FMath::Clamp(T.CaveEntranceChance * Shape.Caves, 0.0f, 1.0f);
	}
	T.VegetationDensityScale *= Shape.Vegetation;
}

FString FMOWorldCreation::Describe(const FMOWorldCreationSettings& S)
{
	FString Out = TEXT("shape");
	for (const FMOWorldKnob& K : GetKnobs())
	{
		if (K.Group == EMOWorldKnobGroup::Pace && Out.Find(TEXT(" pace")) == INDEX_NONE)
		{
			Out += TEXT(" pace");
		}
		if (K.Group == EMOWorldKnobGroup::Rules && Out.Find(TEXT(" rules")) == INDEX_NONE)
		{
			Out += TEXT(" rules");
		}
		Out += FString::Printf(TEXT(" %s=%g"), *K.Id.ToString(), GetKnob(S, K.Id));
	}
	Out += FString::Printf(TEXT(" start=%s"), S.StartingBiome.IsNone() ? TEXT("coast") : *S.StartingBiome.ToString());
	if (!S.WorldOrigin.IsZero())
	{
		Out += FString::Printf(TEXT(" origin=(%.1f, %.1f) km"), S.WorldOrigin.X / 100000.0, S.WorldOrigin.Y / 100000.0);
	}
	return Out;
}

float FMOWorldRules::GetPace(const UObject* WorldContext, EMOWorldPace Activity)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AMOGameState* State = World ? World->GetGameState<AMOGameState>() : nullptr;
	return State ? State->GetWorldPace().For(Activity) : 1.0f;
}

float FMOWorldRules::ScaleSeconds(const UObject* WorldContext, EMOWorldPace Activity, float Seconds)
{
	return Seconds * GetPace(WorldContext, Activity);
}

bool FMOWorldRules::IsCreative(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const AMOGameState* State = World ? World->GetGameState<AMOGameState>() : nullptr;
	return State && State->GetWorldCreation().CreativeMode >= 0.5f;
}

#undef LOCTEXT_NAMESPACE
