#include "MOCharacterAppearance.h"

FMOCharacterAppearance FMOCharacterAppearance::GetDefaultMale()
{
	FMOCharacterAppearance Appearance;

	// Character info
	Appearance.bIsFemale = false;
	Appearance.CharacterName = TEXT("New Character");

	// These paths should point to your default character assets
	// For Genesis 8: single body mesh with morphs
	// For MetaHuman: modular Face/Body/Torso/Legs/Feet
	Appearance.FaceAssetPath = TEXT("/Game/Characters/Faces/Default_Male_Face");
	Appearance.BodyAssetPath = TEXT("/Game/Characters/Bodies/Default_Male_Body");
	Appearance.HairAssetPath = TEXT("/Game/Characters/Hair/Default_Male_Hair");
	Appearance.EyebrowsAssetPath = TEXT("/Game/Characters/Hair/Default_Eyebrows");
	Appearance.EyelashesAssetPath = TEXT("/Game/Characters/Hair/Default_Eyelashes");

	// Body params - average male
	Appearance.BodyFat = 0.25f;
	Appearance.MuscleMass = 0.4f;
	Appearance.Height = 1.0f;
	Appearance.ShoulderWidth = 1.0f;
	Appearance.HipWidth = 0.95f;

	// Colors - default Caucasian
	Appearance.SkinTone = FMOCharacterAppearance::DefaultSkinTone;
	Appearance.HairMelanin = 0.85f;  // Dark brown
	Appearance.HairRedness = 0.2f;
	Appearance.EyeColor = NAME_None;

	// Default morphs can be added here if needed
	// Appearance.SetMorphValue(TEXT("MuscleDefinition"), 0.5f);

	return Appearance;
}

FMOCharacterAppearance FMOCharacterAppearance::GetDefaultFemale()
{
	FMOCharacterAppearance Appearance;

	// Character info
	Appearance.bIsFemale = true;
	Appearance.CharacterName = TEXT("New Character");

	Appearance.FaceAssetPath = TEXT("/Game/Characters/Faces/Default_Female_Face");
	Appearance.BodyAssetPath = TEXT("/Game/Characters/Bodies/Default_Female_Body");
	Appearance.HairAssetPath = TEXT("/Game/Characters/Hair/Default_Female_Hair");
	Appearance.EyebrowsAssetPath = TEXT("/Game/Characters/Hair/Default_Eyebrows");
	Appearance.EyelashesAssetPath = TEXT("/Game/Characters/Hair/Default_Eyelashes");

	// Body params - average female
	Appearance.BodyFat = 0.3f;
	Appearance.MuscleMass = 0.25f;
	Appearance.Height = 0.95f;
	Appearance.ShoulderWidth = 0.9f;
	Appearance.HipWidth = 1.05f;

	// Colors
	Appearance.SkinTone = FMOCharacterAppearance::DefaultSkinTone;
	Appearance.HairMelanin = 0.8f;   // Brown (the shipped MetaHuman)
	Appearance.HairRedness = 0.25f;
	Appearance.EyeColor = NAME_None;

	return Appearance;
}
