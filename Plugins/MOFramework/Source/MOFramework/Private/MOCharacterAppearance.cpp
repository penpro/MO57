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
	Appearance.SkinTone = FLinearColor(0.8f, 0.65f, 0.55f, 1.0f);
	Appearance.HairColor = FLinearColor(0.15f, 0.1f, 0.05f, 1.0f);  // Dark brown
	Appearance.EyeColor = FLinearColor(0.2f, 0.35f, 0.25f, 1.0f);   // Hazel

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
	Appearance.SkinTone = FLinearColor(0.85f, 0.7f, 0.6f, 1.0f);
	Appearance.HairColor = FLinearColor(0.25f, 0.15f, 0.1f, 1.0f);  // Brown
	Appearance.EyeColor = FLinearColor(0.15f, 0.3f, 0.45f, 1.0f);   // Blue-green

	return Appearance;
}
