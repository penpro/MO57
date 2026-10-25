#pragma once

#include "CoreMinimal.h"

class UDataTable;
class ITargetPlatform;

/**
 * Cooking the world features. A DT_WorldFeatures row names its composition in plain text -- ActorProperties values such as
 * Variants = ("/Game/WorldFeatures/DarkRuins/PCG_DR_NordicMountain.PCG_DR_NordicMountain") or an Electric Dreams assembly Blueprint's PCG data
 * asset -- which the cooker cannot see. The graphs, their PCG data assets and every mesh those name were left out of the packaged build, which
 * then placed no cliffs at all (Wes, Oct 25 2026: "neither the cliffs nor the ED cliffs show up in the packaged build").
 *
 * The module binds ModifyCook to FGameDelegates' ModifyCookDelegate: the cook adds every package a row reaches, so a new row cooks with no config
 * edit (MOFramework.WorldFeatures.CompositionsAreCooked guards it).
 */
struct MOFRAMEWORK_API FMOWorldFeatureCook
{
	/** The content packages a feature table's ENABLED rows name: each Blueprint ActorClass (not a native one) and every object path inside the ActorProperties values. Sorted, unique. */
	static void CollectNamedPackages(const UDataTable& Table, TArray<FName>& OutPackages);

#if WITH_EDITOR
	/**
	 * CollectNamedPackages of the project's feature table (UMOWorldFeatureSettings) plus everything those reach at run time: the asset registry's
	 * game dependencies, hard and soft, and every content path written in the files of the PCG packages among them (an Electric Dreams assembly
	 * names its meshes in text, in no registry dependency). Levels and external-actor packages are left out (an assembly's source level is editor
	 * data). Sorted, unique; empty when there is no table.
	 */
	static void CollectCookPackages(TArray<FName>& OutPackages);

	/** FGameDelegates::GetModifyCookDelegate(): adds CollectCookPackages to the cook. */
	static void ModifyCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& InOutPackagesToCook, TArray<FName>& InOutPackagesToNeverCook);
#endif
};
