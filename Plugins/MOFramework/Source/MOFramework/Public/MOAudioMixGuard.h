// See MOAudioMixGuard.cpp: finds sound mixes whose adjusters reference sound classes the audio device does not know, and registers them (stops the engine's per-update log spam).

#pragma once

#include "CoreMinimal.h"

namespace MOAudioMix
{
	/** Inspect the main audio device's active sound mixes (on the audio thread). bRegisterOrphans: register any adjuster class the device does not know. bLogAll: log every mix and adjuster. */
	MOFRAMEWORK_API void Inspect(bool bRegisterOrphans, bool bLogAll);
}
