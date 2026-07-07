using UnrealBuildTool;

// MOFrameworkMedical — the physiological simulation layer (C1 phase 3).
// Anatomy, vitals, metabolism, mental state, and the medical subsystem.
// RULES: depends only on MOFrameworkCore + engine. Environmental inputs come
// through IMOAmbientEnvironmentProvider (Core registry) — never include the
// weather subsystem. Gameplay/UI/persistence live ABOVE this module.
public class MOFrameworkMedical : ModuleRules
{
	public MOFrameworkMedical(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"NetCore",
			"DeveloperSettings",
			"MOFrameworkCore",
		});
	}
}
