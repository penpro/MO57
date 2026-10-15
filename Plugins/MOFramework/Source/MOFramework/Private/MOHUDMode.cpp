#include "MOHUDMode.h"

#define LOCTEXT_NAMESPACE "MOHUDMode"

bool FMOHUDModePolicy::Shows(EMOHUDMode Mode, EMOHUDElementKind Kind)
{
	switch (Mode)
	{
	case EMOHUDMode::Full:
		return true;
	case EMOHUDMode::Minimal:
		return Kind != EMOHUDElementKind::Info;
	default:
		return false;
	}
}

EMOHUDMode FMOHUDModePolicy::Next(EMOHUDMode Mode)
{
	switch (Mode)
	{
	case EMOHUDMode::Full:
		return EMOHUDMode::Minimal;
	case EMOHUDMode::Minimal:
		return EMOHUDMode::Off;
	default:
		return EMOHUDMode::Full;
	}
}

FText FMOHUDModePolicy::DisplayName(EMOHUDMode Mode)
{
	switch (Mode)
	{
	case EMOHUDMode::Full:
		return LOCTEXT("Full", "Full");
	case EMOHUDMode::Minimal:
		return LOCTEXT("Minimal", "Minimal");
	default:
		return LOCTEXT("Off", "Off");
	}
}

bool FMOHUDModePolicy::Parse(const FString& Text, EMOHUDMode& OutMode)
{
	const FString T = Text.TrimStartAndEnd().ToLower();
	if (T == TEXT("full"))
	{
		OutMode = EMOHUDMode::Full;
		return true;
	}
	if (T == TEXT("minimal") || T == TEXT("min"))
	{
		OutMode = EMOHUDMode::Minimal;
		return true;
	}
	if (T == TEXT("off") || T == TEXT("none"))
	{
		OutMode = EMOHUDMode::Off;
		return true;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
