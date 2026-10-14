/**
 * =============================================================================
 * MODevMode.h - Developer mode: the one switch for the console and the dev cheats
 * =============================================================================
 *
 * Developer mode turns on the ~ console popup, the developer command menu (every MO.* command as a button) and the server-side dev cheats behind
 * them (teleport to a biome / POI, set weather, set time). Wes, Oct 14 2026: get the console running in the shipped version, or a "dev mode" in
 * Options with a menu of all the MO. console stuff.
 *
 *   - Development / DebugGame / editor builds: always on (unchanged from before).
 *   - Shipping: Options > Gameplay > Developer mode (UMOGameSettings::bDeveloperMode, saved with the user's settings, off by default),
 *     or the -devmode command line.
 *
 * In co-op the SERVER decides its own cheats: a client's dev RPC runs only if the host's developer mode is on too.
 *
 * Every gate asks IsEnabled() -- never `#if !UE_BUILD_SHIPPING` around a dev surface, or Shipping loses it again.
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"

struct MOFRAMEWORK_API FMODevMode
{
	/** True when the console, the command menu and the dev cheats are available on this machine. */
	static bool IsEnabled();

	/** True when the build itself turns developer mode on (anything but Shipping): the Options toggle cannot turn it off. */
	static bool IsForcedByBuild();
};
