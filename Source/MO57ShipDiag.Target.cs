// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

// DIAGNOSTIC ONLY -- never ship it. The MO57 game target, built Shipping, but MOFramework's UE_LOG lines reach the log file and -ExecCmds is
// honoured (MO_SHIPPING_DIAG: MOFramework.Build.cs, MOFramework.h, UMOGameInstance::Init), to reproduce bugs only Shipping shows.
// Shared build environment (the engine is an installed build): only the project's modules compile. Run it on a Shipping cook.
public class MO57ShipDiagTarget : TargetRules
{
	public MO57ShipDiagTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("MO57");
	}
}
