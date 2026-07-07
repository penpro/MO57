/**
 * =============================================================================
 * MOClockSaveDomainAdapter.h - clock persistence adapter (C1 phase 3)
 * =============================================================================
 *
 * PURPOSE:
 * UMOGameClockSubsystem lives in MOFrameworkCore and must know nothing about
 * persistence (which lives above it). This thin adapter is the clock's
 * IMOSaveDomain: it registers itself, and capture/apply delegate to the
 * clock's public BuildSaveData/ApplySaveData. The pattern for any Core
 * subsystem that needs saving: adapter upstairs, service downstairs.
 *
 * =============================================================================
 * RELATED FILES: MOSaveDomainInterface.h, MOGameClockSubsystem.h
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MOSaveDomainInterface.h"
#include "MOClockSaveDomainAdapter.generated.h"

UCLASS()
class MOFRAMEWORK_API UMOClockSaveDomainAdapter : public UWorldSubsystem, public IMOSaveDomain
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	//~ Begin IMOSaveDomain
	virtual FName GetSaveDomainName() const override { return TEXT("GameClock"); }
	virtual int32 GetSaveDomainApplyPriority() const override { return 50; }
	virtual void CaptureSaveDomain(UMOWorldSaveGame& Save) override;
	virtual void ApplySaveDomain(const UMOWorldSaveGame& Save) override;
	//~ End IMOSaveDomain
};
