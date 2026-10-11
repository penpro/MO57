// The Voxel Sampler places AT MOST ONE point per CellSize lattice cell, so a sampler's real emission rate is min(PointsPerSquaredMeter, 1e4 / CellSize^2) points / m^2 -- the
// PointsPerSquaredMeter field only thins below that cap. The biome spawner turns each point into species by DensityPerHectare / InputPointsPerHectare, so if the lattice emits less than
// the node's InputPointsPerHectare every authored density in DT_Biomes is silently scaled down by the ratio (the Oct 2026 bug: 10 m and 25 m lattices = 100 and 16 points / ha while the
// nodes assumed 2000 and 1200, so forests came out as parkland). This test walks the shipped scatter graph and checks every biome spawner against the sampler that feeds it.

#include "Misc/AutomationTest.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGEdge.h"
#include "MOPCGBiomeSpawnerSettings.h"
#include "PCGVoxelSampler.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Nearest voxel sampler upstream of a node (breadth-first over the input pins), or null. */
	const UPCGVoxelSamplerSettings* FindUpstreamSampler(const UPCGNode* Start)
	{
		TArray<const UPCGNode*> Queue;
		TSet<const UPCGNode*> Seen;
		Queue.Add(Start);
		Seen.Add(Start);
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			const UPCGNode* N = Queue[Head];
			if (N != Start)
			{
				if (const UPCGVoxelSamplerSettings* S = Cast<UPCGVoxelSamplerSettings>(N->GetSettings()))
				{
					return S;
				}
			}
			for (const UPCGPin* Pin : N->GetInputPins())
			{
				if (!Pin)
				{
					continue;
				}
				for (const UPCGEdge* Edge : Pin->Edges)
				{
					const UPCGNode* Up = (Edge && Edge->InputPin) ? Edge->InputPin->Node.Get() : nullptr;
					if (Up && !Seen.Contains(Up))
					{
						Seen.Add(Up);
						Queue.Add(Up);
					}
				}
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPCG_BiomeSamplerLattice,
	"MOFramework.PCG.BiomeSamplerLattice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPCG_BiomeSamplerLattice::RunTest(const FString& Parameters)
{
	static const TCHAR* GraphPath = TEXT("/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1.MOPCG_StampScatter1");
	const UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, GraphPath);
	if (!TestNotNull(TEXT("the scatter graph loads"), Graph))
	{
		return false;
	}

	int32 Spawners = 0;
	for (const UPCGNode* Node : Graph->GetNodes())
	{
		const UMOPCGBiomeSpawnerSettings* Spawner = Node ? Cast<UMOPCGBiomeSpawnerSettings>(Node->GetSettings()) : nullptr;
		if (!Spawner)
		{
			continue;
		}
		++Spawners;
		const UPCGVoxelSamplerSettings* Sampler = FindUpstreamSampler(Node);
		if (!TestNotNull(*FString::Printf(TEXT("%s has a voxel sampler upstream"), *Node->GetName()), Sampler))
		{
			continue;
		}
		const double CellM = Sampler->CellSize / 100.0;
		const double LatticeCap = 1.0 / (CellM * CellM);   // points / m^2 when EVERY lattice cell holds a point
		const double Emitted = FMath::Min<double>(Sampler->PointsPerSquaredMeter, LatticeCap);
		const double EmittedPerHa = Emitted * 10000.0;
		TestTrue(*FString::Printf(TEXT("%s: the sampler emits %.0f points / ha, the node assumes InputPointsPerHectare = %.0f (lattice %.0f cm caps at %.0f / ha, ppsm asks %.0f / ha) -- within 15%%"),
			*Node->GetName(), EmittedPerHa, Spawner->InputPointsPerHectare, Sampler->CellSize, LatticeCap * 10000.0, Sampler->PointsPerSquaredMeter * 10000.0),
			FMath::Abs(EmittedPerHa - Spawner->InputPointsPerHectare) <= 0.15 * Spawner->InputPointsPerHectare);
		// Also keep the lattice from running saturated (ratio 1: no randomness left, the points would line up in rows of CellSize).
		TestTrue(*FString::Printf(TEXT("%s: the lattice is not saturated (ppsm %.3f vs cap %.3f per m^2) -- thinning, not a grid"), *Node->GetName(), Sampler->PointsPerSquaredMeter, LatticeCap),
			Sampler->PointsPerSquaredMeter <= 0.9 * LatticeCap);
	}
	TestTrue(TEXT("the scatter graph has at least one biome spawner"), Spawners >= 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
