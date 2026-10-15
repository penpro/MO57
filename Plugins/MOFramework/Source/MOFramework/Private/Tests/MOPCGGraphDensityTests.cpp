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
#include "ISMPartition/ISMComponentDescriptor.h"

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

// The far tiers (stand-in trees, canopy clumps, far grass) plant their whole cell and keep out of the near field by a per-frame DRAW floor (MinDistanceFromViewerCm -> the descriptor's
// InstanceMinDrawDistance). The engine honours that floor only with an end distance set (GetInstanceDrawDistanceMinMax returns nothing while InstanceEndCullDistance is 0, Nanite and classic
// proxies alike): a far node without a floor, or a floor without an end, draws giant stand-ins and clumps among the real trees around the player (Oct 2026: the floor alone was silently ignored).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPCG_FarTiersDrawFloor,
	"MOFramework.PCG.FarTiersDrawFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPCG_FarTiersDrawFloor::RunTest(const FString& Parameters)
{
	// The descriptor rule itself.
	FISMComponentDescriptor Floored;
	MOPCGBiomeSpawner::ApplyNearDrawFloor(Floored, 150000.0f);
	TestEqual(TEXT("a floor of 1.5 km sets InstanceMinDrawDistance"), Floored.InstanceMinDrawDistance, 150000);
	TestTrue(TEXT("a floor comes with an end distance past it (the engine ignores a minimum without one)"), Floored.InstanceEndCullDistance > Floored.InstanceMinDrawDistance);
	TestEqual(TEXT("a floor leaves the start distance at 0 (a start turns on the classic proxy's per-instance fade)"), Floored.InstanceStartCullDistance, 0);
	FISMComponentDescriptor Unfloored;
	MOPCGBiomeSpawner::ApplyNearDrawFloor(Unfloored, 0.0f);
	TestTrue(TEXT("no floor leaves both distances at 0"), Unfloored.InstanceMinDrawDistance == 0 && Unfloored.InstanceEndCullDistance == 0);

	// Every far node of the shipped graph has a floor, and the end distance clears every far generation radius (the canopy tier reaches 7.5 km).
	static const TCHAR* GraphPath = TEXT("/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1.MOPCG_StampScatter1");
	const UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, GraphPath);
	if (!TestNotNull(TEXT("the scatter graph loads"), Graph))
	{
		return false;
	}
	int32 FarNodes = 0;
	for (const UPCGNode* Node : Graph->GetNodes())
	{
		const UMOPCGBiomeSpawnerSettings* Spawner = Node ? Cast<UMOPCGBiomeSpawnerSettings>(Node->GetSettings()) : nullptr;
		if (!Spawner || !Spawner->bEnabled)
		{
			continue;
		}
		const EMOBiomeSpeciesFilter F = Spawner->SpeciesFilter;
		if (F != EMOBiomeSpeciesFilter::FarLargeOnly && F != EMOBiomeSpeciesFilter::FarCanopyOnly && F != EMOBiomeSpeciesFilter::FarGroundCoverOnly)
		{
			continue;
		}
		++FarNodes;
		TestTrue(*FString::Printf(TEXT("%s (%s) has a near draw floor"), *Node->GetName(), *UEnum::GetValueAsString(F)), Spawner->MinDistanceFromViewerCm > 0.0f);
		TestTrue(*FString::Printf(TEXT("%s: the end distance (%d cm) clears its floor (%.0f cm)"), *Node->GetName(), MOPCGBiomeSpawner::NearFloorEndCullCm, Spawner->MinDistanceFromViewerCm),
			MOPCGBiomeSpawner::NearFloorEndCullCm > Spawner->MinDistanceFromViewerCm);
	}
	TestTrue(TEXT("the scatter graph has far-tier nodes"), FarNodes >= 2);
	TestTrue(TEXT("the end distance clears the 7.5 km canopy reach with the x1.1 cleanup margin"), MOPCGBiomeSpawner::NearFloorEndCullCm >= 825000);
	return true;
}


namespace
{
	// UPCGRandomChoiceSettings is not exported from the PCG module: read it through reflection.
	float RandomChoiceRatio(const UPCGNode* Node, bool& bOutFixed)
	{
		const UPCGSettings* S = Node ? Node->GetSettings() : nullptr;
		bOutFixed = true;
		if (!S || S->GetClass()->GetName() != TEXT("PCGRandomChoiceSettings"))
		{
			return -1.0f;
		}
		const FFloatProperty* RatioProp = FindFProperty<FFloatProperty>(S->GetClass(), TEXT("Ratio"));
		const FBoolProperty* FixedProp = FindFProperty<FBoolProperty>(S->GetClass(), TEXT("bFixedMode"));
		bOutFixed = FixedProp && FixedProp->GetPropertyValue_InContainer(S);
		return RatioProp ? RatioProp->GetPropertyValue_InContainer(S) : -1.0f;
	}

	TArray<const UPCGNode*> DownstreamOf(const UPCGNode* Node, FName PinLabel)
	{
		TArray<const UPCGNode*> Out;
		if (const UPCGPin* Pin = Node ? Node->GetOutputPin(PinLabel) : nullptr)
		{
			for (const UPCGEdge* Edge : Pin->Edges)
			{
				if (Edge && Edge->OutputPin && Edge->OutputPin->Node)
				{
					Out.Add(Edge->OutputPin->Node.Get());
				}
			}
		}
		return Out;
	}

	const UPCGGraph* LoadScatterGraph()
	{
		return LoadObject<UPCGGraph>(nullptr, TEXT("/Game/VoxelExamples/PCGScattering/MOPCG_StampScatter1.MOPCG_StampScatter1"));
	}
}

// The ground ITEM clutter (sticks, bark, stones) is ~100 of VoxelSampler_0's points per hectare: RandomChoice_3 (0.5) -> its own thinning Random Choice (0.1 at 2000/ha, 0.0333 at 6000/ha)
// -> RandomChoice_1 (the item spawners' split).
// (RandomChoice_3's Discarded half once fed CopyPoints_1 -> ... -> StaticMeshSpawner_3, the native black-locust clusters. That spawner is DISABLED -- the biome tier places locusts -- so
// the branch was pure wasted work; cut_pcg_dead_branches.py disconnects it and MOFramework.PCG.NoDeadBranches keeps it disconnected.)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPCG_ItemThinning,
	"MOFramework.PCG.ItemThinning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPCG_ItemThinning::RunTest(const FString& Parameters)
{
	const UPCGGraph* Graph = LoadScatterGraph();
	if (!TestNotNull(TEXT("the scatter graph loads"), Graph))
	{
		return false;
	}
	const UPCGNode* RC3 = nullptr;
	for (const UPCGNode* Node : Graph->GetNodes())
	{
		if (Node && Node->GetName() == TEXT("RandomChoice_3"))
		{
			RC3 = Node;
		}
	}
	if (!TestNotNull(TEXT("RandomChoice_3 (the item share of the elevation band) exists"), RC3))
	{
		return false;
	}
	bool bFixed3 = true;
	const float Ratio3 = RandomChoiceRatio(RC3, bFixed3);
	TestFalse(TEXT("RandomChoice_3 works by ratio"), bFixed3);

	const TArray<const UPCGNode*> AfterChosen = DownstreamOf(RC3, TEXT("Chosen"));
	TestEqual(TEXT("RandomChoice_3.Chosen feeds exactly one node (the item thinning)"), AfterChosen.Num(), 1);
	const UPCGNode* Thin = AfterChosen.Num() == 1 ? AfterChosen[0] : nullptr;
	bool bThinFixed = true;
	const float ThinRatio = RandomChoiceRatio(Thin, bThinFixed);
	TestFalse(TEXT("the item thinning works by ratio"), bThinFixed);
	// The invariant is items per HECTARE, not the share: VoxelSampler_0 also feeds the biome spawner's ground-cover tier, whose rate went 2000 -> 6000/ha (Oct 14 2026, more ground cover),
	// and the thinning ratio went down by the same factor so the sticks and stones stayed where they were (~100 candidate points/ha before the elevation band and the item split).
	const UPCGNode* Sampler = nullptr;
	for (const UPCGNode* Node : Graph->GetNodes())
	{
		if (Node && Node->GetName() == TEXT("VoxelSampler_0"))
		{
			Sampler = Node;
		}
	}
	const UPCGSettings* SamplerSettings = Sampler ? Sampler->GetSettings() : nullptr;
	const FFloatProperty* RateProp = SamplerSettings ? FindFProperty<FFloatProperty>(SamplerSettings->GetClass(), TEXT("PointsPerSquaredMeter")) : nullptr;
	if (!TestNotNull(TEXT("VoxelSampler_0 (the ground tier + item sampler) has a PointsPerSquaredMeter"), RateProp))
	{
		return false;
	}
	const float SamplerPerHectare = RateProp->GetPropertyValue_InContainer(SamplerSettings) * 10000.0f;
	const float ItemPointsPerHectare = SamplerPerHectare * Ratio3 * ThinRatio;
	TestTrue(*FString::Printf(TEXT("the items get 60-160 candidate points/ha (%.0f/ha x %.3f x %.4f = %.0f): about ten times fewer than the authored 1000"), SamplerPerHectare, Ratio3, ThinRatio, ItemPointsPerHectare),
		ItemPointsPerHectare >= 60.0f && ItemPointsPerHectare <= 160.0f);
	const TArray<const UPCGNode*> AfterThin = DownstreamOf(Thin, TEXT("Chosen"));
	TestTrue(TEXT("the item thinning feeds RandomChoice_1 (the item spawners' split)"), AfterThin.Num() == 1 && AfterThin[0]->GetName() == TEXT("RandomChoice_1"));
	TestEqual(TEXT("the item thinning's Discarded output goes nowhere"), DownstreamOf(Thin, TEXT("Discarded")).Num(), 0);
	return true;
}

// Every edge that carries data must lead to work that reaches an ENABLED sink (a node with no outgoing edge: a spawner). A disabled PCG node passes its input through, it does not stop
// the upstream work: three native chains (black-locust clusters, Field_05 trees, the old resource spawner) ended in disabled spawners and were still computed for every cell -- the
// packaged reveal took 47.8 s with the locust chain fed 95 % of the item points vs ~40 s at 50 % (Oct 13 2026). Cut a dead chain at its root (Content/Python/cut_pcg_dead_branches.py).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPCG_NoDeadBranches,
	"MOFramework.PCG.NoDeadBranches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPCG_NoDeadBranches::RunTest(const FString& Parameters)
{
	const UPCGGraph* Graph = LoadScatterGraph();
	if (!TestNotNull(TEXT("the scatter graph loads"), Graph))
	{
		return false;
	}
	struct FEdgeRef { const UPCGNode* From; FName FromPin; const UPCGNode* To; };
	TArray<FEdgeRef> Edges;
	TSet<const UPCGNode*> HasOut;
	TMap<const UPCGNode*, TArray<const UPCGNode*>> Up;
	for (const UPCGNode* Node : Graph->GetNodes())
	{
		if (!Node)
		{
			continue;
		}
		for (const UPCGPin* Pin : Node->GetOutputPins())
		{
			if (!Pin)
			{
				continue;
			}
			for (const UPCGEdge* Edge : Pin->Edges)
			{
				const UPCGNode* To = (Edge && Edge->OutputPin) ? Edge->OutputPin->Node.Get() : nullptr;
				if (To)
				{
					Edges.Add({ Node, Pin->Properties.Label, To });
					HasOut.Add(Node);
					Up.FindOrAdd(To).Add(Node);
				}
			}
		}
	}
	auto IsEnabled = [](const UPCGNode* N) { return !N->GetSettings() || N->GetSettings()->bEnabled; };
	TSet<const UPCGNode*> Live;
	TArray<const UPCGNode*> Stack;
	for (const UPCGNode* Node : Graph->GetNodes())
	{
		if (Node && !HasOut.Contains(Node) && IsEnabled(Node))
		{
			Stack.Add(Node);   // an enabled sink: a spawner (or a node that consumes its input)
		}
	}
	while (Stack.Num() > 0)
	{
		const UPCGNode* N = Stack.Pop();
		if (Live.Contains(N))
		{
			continue;
		}
		Live.Add(N);
		if (const TArray<const UPCGNode*>* Ups = Up.Find(N))
		{
			Stack.Append(*Ups);
		}
	}
	int32 Dead = 0;
	for (const FEdgeRef& E : Edges)
	{
		const bool bCarriesData = Live.Contains(E.From) || E.From->GetInputPins().Num() == 0;   // a live node, or a source that makes data from nothing (Get Actor Data, Create Points Grid)
		if (bCarriesData && !Live.Contains(E.To))
		{
			++Dead;
			AddError(FString::Printf(TEXT("%s.%s feeds %s, whose chain reaches no enabled spawner: its work is computed for every cell and thrown away (cut it at this edge)"),
				*E.From->GetName(), *E.FromPin.ToString(), *E.To->GetName()));
		}
	}
	TestEqual(TEXT("no edge feeds a dead chain"), Dead, 0);
	TestTrue(TEXT("the graph has live spawners"), Live.Num() > 10);

	// TRAP (measured, packaged build, Oct 13 2026): a Hierarchical Generation Grid Size node left with NO edge at all stopped ALL runtime generation of the graph -- not one cell, no error
	// in the log. Cut a dead chain at its root only; the grid node keeps its downstream edge.
	for (const UPCGNode* Node : Graph->GetNodes())
	{
		if (Node && Node->GetSettings() && Node->GetSettings()->GetClass()->GetName() == TEXT("PCGHiGenGridSizeSettings"))
		{
			const bool bHasOut = HasOut.Contains(Node);
			const bool bHasIn = Up.Contains(Node);
			TestTrue(*FString::Printf(TEXT("%s (a grid-size node) is not fully disconnected (in=%d out=%d)"), *Node->GetName(), bHasIn ? 1 : 0, bHasOut ? 1 : 0), bHasIn || bHasOut);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
