/**
 * What E does on a scattered instance: UMOInteractorComponent::IsHarvestableInstanceComponent (a named harvest / pickup) vs.
 * IsWorldCoverComponent (forage). Run: python Tools/ue.py auto --filter "MOFramework.Interaction.TargetKind"
 *
 * Found Oct 15 2026: E on Common Hazel -- the commonest bush around a woodland spawn -- did nothing (decorative cover, no resource row),
 * while Wes wants every thing in the world to have an action: named harvests from DT_Resources, everything else forages.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "MOInteractorComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOInteraction_TargetKind,
	"MOFramework.Interaction.TargetKind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOInteraction_TargetKind::RunTest(const FString& Parameters)
{
	auto Make = [](std::initializer_list<const TCHAR*> Tags)
	{
		UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>();
		for (const TCHAR* Tag : Tags)
		{
			ISM->ComponentTags.Add(FName(Tag));
		}
		return ISM;
	};

	// The palette's tags as the spawner writes them (MO.PCG census near a Locust Woodland spawn).
	const UInstancedStaticMeshComponent* Hazel = Make({TEXT("MOBiomeCover_Shrub"), TEXT("MOBiome_LocustWoodland"), TEXT("grass")});
	const UInstancedStaticMeshComponent* Raspberry = Make({TEXT("Name Wild Raspberry Bush"), TEXT("MOResource_Bush"), TEXT("Action_GatherBerries"), TEXT("Gives_Berries01"), TEXT("grass")});
	const UInstancedStaticMeshComponent* Grass = Make({TEXT("grass")});
	const UInstancedStaticMeshComponent* Untagged = Make({});

	TestFalse(TEXT("hazel cover has no harvest of its own"), UMOInteractorComponent::IsHarvestableInstanceComponent(Hazel, nullptr));
	TestTrue(TEXT("... so it is world cover: E forages there"), UMOInteractorComponent::IsWorldCoverComponent(Hazel));
	TestTrue(TEXT("a raspberry bush is a harvest source"), UMOInteractorComponent::IsHarvestableInstanceComponent(Raspberry, nullptr));
	TestTrue(TEXT("native grass is world cover"), UMOInteractorComponent::IsWorldCoverComponent(Grass));
	TestFalse(TEXT("an untagged instance (a building part, a prop) is neither"), UMOInteractorComponent::IsHarvestableInstanceComponent(Untagged, nullptr));
	TestFalse(TEXT("... and does not forage"), UMOInteractorComponent::IsWorldCoverComponent(Untagged));
	TestFalse(TEXT("no component is neither"), UMOInteractorComponent::IsWorldCoverComponent(nullptr) || UMOInteractorComponent::IsHarvestableInstanceComponent(nullptr, nullptr));
	return true;
}

#endif
