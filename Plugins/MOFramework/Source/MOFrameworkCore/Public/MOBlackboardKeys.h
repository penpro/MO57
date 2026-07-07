// Copyright Penumbra Group Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * MOBlackboardKeys.h - Canonical blackboard key names (audit M1).
 *
 * Every C++ read/write of a Blackboard key MUST use these constants instead of
 * inline TEXT("...") literals, so a key rename is a one-line change and C++ and
 * the BB_* Blackboard assets cannot silently drift per-call-site.
 * Grouped by which Blackboard asset family uses them. The string values must
 * match the key names in the Blackboard assets (BB_Creature, survivor BB).
 */
namespace MOBlackboardKeys
{
	// --- Shared threat/target keys (creatures + survivors) ---
	inline const FName TargetActor(TEXT("TargetActor"));
	inline const FName TargetLocation(TEXT("TargetLocation"));
	inline const FName HasTarget(TEXT("HasTarget"));
	inline const FName HomeLocation(TEXT("HomeLocation"));

	// --- Generic AI task assignment (AMOAIController) ---
	inline const FName TaskName(TEXT("TaskName"));

	// --- Creature combat / flee (BB_Creature) ---
	inline const FName ShouldFlee(TEXT("ShouldFlee"));
	inline const FName HealthPercent(TEXT("HealthPercent"));
	inline const FName DistanceToTarget(TEXT("DistanceToTarget"));
	inline const FName IsInAttackRange(TEXT("IsInAttackRange"));
	inline const FName IsAttacking(TEXT("IsAttacking"));
	inline const FName ThreatActor(TEXT("ThreatActor"));

	// --- Creature activity state (BB_Creature) ---
	inline const FName ActivityState(TEXT("ActivityState"));
	inline const FName IsDead(TEXT("IsDead"));
	inline const FName IsResting(TEXT("IsResting"));
	inline const FName IsSleeping(TEXT("IsSleeping"));

	// --- Survivor command/orders keys (survivor BB) ---
	inline const FName IsFollowing(TEXT("IsFollowing"));
	inline const FName ShouldStay(TEXT("ShouldStay"));
	inline const FName IsGoingHome(TEXT("IsGoingHome"));
	inline const FName IsProcessingJob(TEXT("IsProcessingJob"));
	inline const FName FollowTarget(TEXT("FollowTarget"));
	inline const FName StayLocation(TEXT("StayLocation"));

	// --- Survivor job execution keys (survivor BB) ---
	inline const FName CurrentJobType(TEXT("CurrentJobType"));
	inline const FName JobTargetLocation(TEXT("JobTargetLocation"));
	inline const FName JobTargetActor(TEXT("JobTargetActor"));
}
