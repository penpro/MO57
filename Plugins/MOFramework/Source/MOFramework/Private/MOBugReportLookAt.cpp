#include "MOBugReportLookAt.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodySetup.h"

void FMOBugReportLookAt::Capture(const UWorld* World, FMOBugReportFields& Out)
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		Out.Emplace(TEXT("Hit"), TEXT("(no local player)"));
		return;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MOBugReportLookAt), /*bTraceComplex=*/true);   // the surface the player sees, not its hulls
	Params.bReturnFaceIndex = true;
	Params.bReturnPhysicalMaterial = true;
	if (const APawn* Pawn = PC->GetPawn())
	{
		Params.AddIgnoredActor(Pawn);   // a third-person camera looks over the character's shoulder
	}
	FHitResult Hit;
	World->LineTraceSingleByChannel(Hit, ViewLocation, ViewLocation + ViewRotation.Vector() * RangeCm, ECC_Visibility, Params);
	Describe(World, ViewLocation, ViewRotation, Hit, Out);
}

void FMOBugReportLookAt::Describe(const UWorld* World, const FVector& ViewLocation, const FRotator& ViewRotation, const FHitResult& Hit, FMOBugReportFields& Out)
{
	Out.Emplace(TEXT("View"), FString::Printf(TEXT("%.0f %.0f %.0f, pitch %.1f yaw %.1f"), ViewLocation.X, ViewLocation.Y, ViewLocation.Z, ViewRotation.Pitch, ViewRotation.Yaw));
	const FVector Target = Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : ViewLocation + ViewRotation.Vector() * 10000.0;
	Out.Emplace(TEXT("Reproduce"), FString::Printf(TEXT("MO.Cam.LookAt %.0f %.0f %.0f %.0f %.0f %.0f"), ViewLocation.X, ViewLocation.Y, ViewLocation.Z, Target.X, Target.Y, Target.Z));
	if (!Hit.bBlockingHit)
	{
		Out.Emplace(TEXT("Hit"), FString::Printf(TEXT("(nothing within %.0f m)"), RangeCm / 100.0));
		return;
	}

	Out.Emplace(TEXT("Distance"), FString::Printf(TEXT("%.1f m"), Hit.Distance / 100.0));
	Out.Emplace(TEXT("Point"), FString::Printf(TEXT("%.0f %.0f %.0f"), Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z));
	Out.Emplace(TEXT("Slope"), FString::Printf(TEXT("%.0f deg"), FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Hit.ImpactNormal.Z, -1.0, 1.0)))));

	if (const AActor* Actor = Hit.GetActor())
	{
		FString Tags;
		for (const FName& Tag : Actor->Tags)
		{
			Tags += (Tags.IsEmpty() ? FString() : FString(TEXT(", "))) + Tag.ToString();
		}
		Out.Emplace(TEXT("Actor"), FString::Printf(TEXT("%s (%s)%s"), *Actor->GetName(), *Actor->GetClass()->GetName(), Tags.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" tags: %s"), *Tags)));
	}

	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (Component)
	{
		Out.Emplace(TEXT("Component"), FString::Printf(TEXT("%s (%s)"), *Component->GetName(), *Component->GetClass()->GetName()));
		Out.Emplace(TEXT("CollisionProfile"), Component->GetCollisionProfileName().ToString());

		if (const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			if (const UStaticMesh* Mesh = MeshComponent->GetStaticMesh())
			{
				Out.Emplace(TEXT("Mesh"), Mesh->GetPathName());
				if (const UBodySetup* Body = Mesh->GetBodySetup())
				{
					const FKAggregateGeom& Geom = Body->AggGeom;
					Out.Emplace(TEXT("MeshCollision"), FString::Printf(TEXT("simple shapes %d (box %d, sphere %d, capsule %d, convex %d), complex as simple: %s"),
						Geom.GetElementCount(), Geom.BoxElems.Num(), Geom.SphereElems.Num(), Geom.SphylElems.Num(), Geom.ConvexElems.Num(),
						Body->CollisionTraceFlag == CTF_UseComplexAsSimple ? TEXT("yes") : TEXT("no")));
				}
				else
				{
					Out.Emplace(TEXT("MeshCollision"), TEXT("no body setup"));
				}
			}
			const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component);
			FTransform InstanceTransform;
			if (Instances && Hit.Item != INDEX_NONE && Instances->GetInstanceTransform(Hit.Item, InstanceTransform, /*bWorldSpace=*/true))
			{
				const FVector S = InstanceTransform.GetScale3D();
				Out.Emplace(TEXT("Instance"), FString::Printf(TEXT("%d of %d, scale %.2f %.2f %.2f%s"), Hit.Item, Instances->GetInstanceCount(), S.X, S.Y, S.Z,
					S.X * S.Y * S.Z < 0.0 ? TEXT(" (mirrored)") : TEXT("")));
			}
		}

		int32 Section = INDEX_NONE;
		const UMaterialInterface* Material = Hit.FaceIndex != INDEX_NONE ? Component->GetMaterialFromCollisionFaceIndex(Hit.FaceIndex, Section) : nullptr;
		if (Component->GetClass()->GetName().Contains(TEXT("Voxel")))
		{
			Out.Emplace(TEXT("Material"), TEXT("(voxel terrain: its look is the surface Biome blend below)"));
		}
		else if (Material)
		{
			Out.Emplace(TEXT("Material"), FString::Printf(TEXT("%s (section %d)"), *Material->GetPathName(), Section));
		}
		else if (const UMaterialInterface* First = Component->GetMaterial(0))
		{
			Out.Emplace(TEXT("Material"), FString::Printf(TEXT("%s (slot 0; the face did not say)"), *First->GetPathName()));
		}
	}
	Out.Emplace(TEXT("PhysMaterial"), Hit.PhysMaterial.IsValid() ? Hit.PhysMaterial->GetName() : FString(TEXT("(default)")));

	// The generator's surface biome there: what the ground material and the scatter believe this place is.
	const UMOWorldSeedSubsystem* Seeds = World ? UMOWorldSeedSubsystem::Get(World) : nullptr;
	if (Seeds && Seeds->HasActiveSeed() && FMOWorldGenParamsProvider::HasPublished())
	{
		const FMOWorldGenParamsRef P = FMOWorldGenParamsProvider::Get();
		const int32 Seed = Seeds->GetActiveTerrainSeed();
		const FMOTerrainSample S = FMOWorldGen::SampleColumn(*P, Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Seed, P->Tuning.VoxelSizeCm);
		FMOWorldGen::FBiomeWeight Weights[3];
		const int32 N = FMOWorldGen::SampleSurfaceBiomeWeights(*P, Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Seed, S.HeightCm, S.Moisture, S.Temperature, Weights, 3);
		FString Biomes;
		for (int32 K = 0; K < N; ++K)
		{
			Biomes += FString::Printf(TEXT("%s%s %.0f%%"), K ? TEXT(", ") : TEXT(""), *P->GetBiomeId(Weights[K].Biome).ToString(), Weights[K].Weight * 100.0f);
		}
		Out.Emplace(TEXT("Biome"), Biomes.IsEmpty() ? FString(TEXT("(none)")) : Biomes);
		Out.Emplace(TEXT("GroundHeight"), FString::Printf(TEXT("generator %.0f cm (hit %.0f)"), S.HeightCm, Hit.ImpactPoint.Z));
	}
}
