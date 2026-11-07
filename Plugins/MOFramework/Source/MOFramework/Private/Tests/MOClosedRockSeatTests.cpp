// Closed rocks go into the ground where their open scans did (Wes, Nov 6: "the newly closed geometry assets need to still be pushed into the terrain"). A closed copy has a back and walls below its scan's bottom, there to be buried:
// seated by its whole bounds it stood on them like a pie (seen at a cave bowl: the cave spawner lifts a mesh by its lowest point). So its socket MOSeat holds the scan's bottom and FMOSpeciesPlacement::SeatBounds stops there.

#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "MOBiomeDatabaseSettings.h"
#include "MOPCGCaveSpawnerSettings.h"
#include "MOSpeciesPlacement.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOPCG_ClosedRockSeat,
	"MOFramework.PCG.ClosedRockSeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOPCG_ClosedRockSeat::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/MOFramework/Data/DT_CaveScatter.DT_CaveScatter"));
	if (!TestNotNull(TEXT("DT_CaveScatter"), Table))
	{
		return false;
	}
	TSet<UStaticMesh*> Meshes;
	Table->ForeachRow<FMOCaveScatterRow>(TEXT("ClosedRockSeat"), [&Meshes](const FName&, const FMOCaveScatterRow& Row)
	{
		if (UStaticMesh* Mesh = Row.Mesh.LoadSynchronous())
		{
			Meshes.Add(Mesh);
		}
	});
	int32 Closed = 0, Others = 0;
	for (const UStaticMesh* Mesh : Meshes)
	{
		const FBoxSphereBounds Whole = Mesh->GetBounds();
		const FBoxSphereBounds Seat = FMOSpeciesPlacement::SeatBounds(Mesh);
		const bool bClosedCopy = Mesh->GetPathName().StartsWith(TEXT("/Game/ClosedRocks/")) || Mesh->FindSocket(TEXT("MOSeat"));   // a composition's mesh is closed in place
		if (!bClosedCopy)
		{
			++Others;
			TestEqual(FString::Printf(TEXT("%s: a mesh with no seat is seated by its bounds"), *Mesh->GetName()), Seat.BoxExtent.Z, Whole.BoxExtent.Z);
			continue;
		}
		++Closed;
		TestNotNull(FString::Printf(TEXT("%s: a closed copy says where its scan ended"), *Mesh->GetName()), Mesh->FindSocket(TEXT("MOSeat")));
		const double WholeBottom = Whole.Origin.Z - Whole.BoxExtent.Z, SeatBottom = Seat.Origin.Z - Seat.BoxExtent.Z;
		const double WholeTop = Whole.Origin.Z + Whole.BoxExtent.Z, SeatTop = Seat.Origin.Z + Seat.BoxExtent.Z;
		AddInfo(FString::Printf(TEXT("%s: seated at %.1f, %.1f cm of it buried below that, its top %.1f"), *Mesh->GetName(), SeatBottom, SeatBottom - WholeBottom, WholeTop));
		TestTrue(FString::Printf(TEXT("%s: its back lies below the seat"), *Mesh->GetName()), SeatBottom > WholeBottom + 5.0);
		TestTrue(FString::Printf(TEXT("%s: the seat leaves its top where it was"), *Mesh->GetName()), FMath::IsNearlyEqual(SeatTop, WholeTop, 0.5));
	}
	AddInfo(FString::Printf(TEXT("%d closed copies and %d other meshes in the cave dressing"), Closed, Others));
	TestTrue(TEXT("the cave dressing uses closed copies"), Closed > 0);
	TestEqual(TEXT("no mesh: empty bounds"), FMOSpeciesPlacement::SeatBounds(nullptr).BoxExtent.Z, 0.0);
	return true;
}

#endif
