#include "MOActivityTypes.h"

// Activity energy model (moved from MOMedicalTypes.cpp in the C1 split):
//   Light Work 2-3x BMR, Medium 4-6x, Heavy 8-10x, Combat 10-15x.
//   Stamina drain is game-balanced, not purely realistic.
FMOActivityConfig FMOActivityConfig::GetDefaultConfig(EMOActivityLevel Level)
{
	FMOActivityConfig Config;

	switch (Level)
	{
	case EMOActivityLevel::Resting:
		Config.CalorieMultiplier = 0.9f;
		Config.ExertionLevel = 0.0f;
		Config.StaminaDrainPerSecond = 0.0f;
		Config.FatiguePerHour = 0.0f;
		Config.bIsCardioTraining = false;
		Config.bIsStrengthTraining = false;
		Config.TrainingIntensity = 0.0f;
		Config.bRequiresStamina = false;
		Config.MinimumStaminaToStart = 0.0f;
		break;

	case EMOActivityLevel::Idle:
		Config.CalorieMultiplier = 1.0f;
		Config.ExertionLevel = 0.0f;
		Config.StaminaDrainPerSecond = 0.0f;
		Config.FatiguePerHour = 0.0f;
		Config.bIsCardioTraining = false;
		Config.bIsStrengthTraining = false;
		Config.TrainingIntensity = 0.0f;
		Config.bRequiresStamina = false;
		Config.MinimumStaminaToStart = 0.0f;
		break;

	case EMOActivityLevel::Walking:
		Config.CalorieMultiplier = 3.0f;
		Config.ExertionLevel = 15.0f;
		Config.StaminaDrainPerSecond = 0.0f;  // Walking doesn't drain stamina
		Config.FatiguePerHour = 2.0f;
		Config.bIsCardioTraining = false;  // Too low intensity
		Config.bIsStrengthTraining = false;
		Config.TrainingIntensity = 0.0f;
		Config.bRequiresStamina = false;
		Config.MinimumStaminaToStart = 0.0f;
		break;

	case EMOActivityLevel::Jogging:
		Config.CalorieMultiplier = 7.5f;
		Config.ExertionLevel = 50.0f;
		Config.StaminaDrainPerSecond = 2.0f;  // Moderate stamina drain
		Config.FatiguePerHour = 8.0f;
		Config.bIsCardioTraining = true;
		Config.bIsStrengthTraining = false;
		Config.TrainingIntensity = 0.5f;
		Config.bRequiresStamina = true;
		Config.MinimumStaminaToStart = 0.1f;
		break;

	case EMOActivityLevel::Sprinting:
		Config.CalorieMultiplier = 14.0f;
		Config.ExertionLevel = 95.0f;
		Config.StaminaDrainPerSecond = 10.0f;  // High stamina drain
		Config.FatiguePerHour = 25.0f;
		Config.bIsCardioTraining = true;
		Config.bIsStrengthTraining = false;
		Config.TrainingIntensity = 0.9f;
		Config.bRequiresStamina = true;
		Config.MinimumStaminaToStart = 0.2f;
		break;

	case EMOActivityLevel::LightWork:
		Config.CalorieMultiplier = 2.5f;
		Config.ExertionLevel = 20.0f;
		Config.StaminaDrainPerSecond = 0.5f;
		Config.FatiguePerHour = 3.0f;
		Config.bIsCardioTraining = false;
		Config.bIsStrengthTraining = false;
		Config.TrainingIntensity = 0.0f;
		Config.bRequiresStamina = false;
		Config.MinimumStaminaToStart = 0.0f;
		break;

	case EMOActivityLevel::MediumWork:
		Config.CalorieMultiplier = 5.0f;
		Config.ExertionLevel = 45.0f;
		Config.StaminaDrainPerSecond = 1.5f;
		Config.FatiguePerHour = 6.0f;
		Config.bIsCardioTraining = false;
		Config.bIsStrengthTraining = true;
		Config.TrainingIntensity = 0.3f;
		Config.bRequiresStamina = true;
		Config.MinimumStaminaToStart = 0.05f;
		break;

	case EMOActivityLevel::HeavyWork:
		Config.CalorieMultiplier = 9.0f;
		Config.ExertionLevel = 75.0f;
		Config.StaminaDrainPerSecond = 4.0f;
		Config.FatiguePerHour = 12.0f;
		Config.bIsCardioTraining = false;
		Config.bIsStrengthTraining = true;
		Config.TrainingIntensity = 0.6f;
		Config.bRequiresStamina = true;
		Config.MinimumStaminaToStart = 0.15f;
		break;

	case EMOActivityLevel::Combat:
		Config.CalorieMultiplier = 12.0f;
		Config.ExertionLevel = 90.0f;
		Config.StaminaDrainPerSecond = 8.0f;
		Config.FatiguePerHour = 20.0f;
		Config.bIsCardioTraining = true;
		Config.bIsStrengthTraining = true;
		Config.TrainingIntensity = 0.8f;
		Config.bRequiresStamina = true;
		Config.MinimumStaminaToStart = 0.1f;
		break;

	default:
		// Default to idle
		Config.CalorieMultiplier = 1.0f;
		Config.ExertionLevel = 0.0f;
		break;
	}

	return Config;
}
