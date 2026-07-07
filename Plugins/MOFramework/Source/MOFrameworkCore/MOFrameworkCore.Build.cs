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

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"NetCore",
		});
	}
}
