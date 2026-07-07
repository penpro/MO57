using UnrealBuildTool;

// MOFrameworkCore — the types-and-contracts layer (C1 module split, phase 1).
// DataTable row schemas, *Types.h families, cross-system interfaces, and the
// shared delegate library live here. RULES: no gameplay logic, no subsystems,
// no widgets; nothing in this module may depend on MOFramework. If a header
// you want to move here includes one that stays behind, the seam is wrong —
// fix the dependency, don't force the move.
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
