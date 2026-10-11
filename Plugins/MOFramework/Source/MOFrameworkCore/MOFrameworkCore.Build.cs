using UnrealBuildTool;

// MOFrameworkCore — the types-contracts-and-services layer (C1 split).
// DataTable row schemas, *Types.h families, cross-system interfaces, the
// shared delegate library, and CORE RUNTIME SERVICES (game clock, ambient
// environment registry, stateless BP utility libraries). RULES: services
// here must be policy-free plumbing every layer may use — no gameplay
// decisions, no widgets, no persistence knowledge (a Core service that
// needs saving gets an adapter upstairs, see MOClockSaveDomainAdapter).
// Nothing in this module may depend on MOFramework or MOFrameworkMedical.
// If a header you want to move here includes one that stays behind, the
// seam is wrong — fix the dependency, don't force the move.
public class MOFrameworkCore : ModuleRules
{
	public MOFrameworkCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// The world generator (MOWorldGen*.cpp, MOWorldGenNoise.h) must produce BIT-IDENTICAL results in every build: co-op clients generate their own terrain from the seed.
		// Editor targets default to /fp:precise but Game / Client / Server default to /fp:fast, which fuses and reassociates the same source differently (MEASURED 2026-10-10:
		// the packaged game printed a different MO.WorldGen.Checksum than the editor on the same CPU, same params hash). Pin this module to precise in every target.
		FPSemantics = FPSemanticsMode.Precise;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"NetCore",
		});
	}
}
