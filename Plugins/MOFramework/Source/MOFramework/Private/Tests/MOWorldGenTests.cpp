// Tests for the C++ world generator (FMOWorldGen), the MO Terrain voxel node and the shared biome resolution. Headless-safe: no world, no PIE.
// MOFramework.WorldGen.DumpMaps also writes PNG maps of a generated world to Saved/WorldGen/ so terrain can be LOOKED at, not just asserted.

#include "Misc/AutomationTest.h"
#include "MOWorldGen.h"
#include "MOWorldGenSettings.h"
#include "MOWorldSeedSubsystem.h"
#include "MOTerrainFunctionLibrary.h"
#include "MOBiomeDatabaseSettings.h"
#include "VoxelFunctionLibrary.h"
#include "VoxelNode.h"
#include "Nodes/VoxelNode_UFunction.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** The three DT_Biomes rows' bands, hard-coded so the pure tests do not depend on project data. */
	FMOWorldGenParamsRef MakeTestParams()
	{
		TSharedRef<FMOWorldGenParams, ESPMode::ThreadSafe> P = MakeShared<FMOWorldGenParams, ESPMode::ThreadSafe>();
		auto Add = [&P](const TCHAR* Id, int32 Priority, float SlopeMin, float SlopeMax, float MoistureMin)
		{
			FMOWorldGenBiome& B = P->Biomes.AddDefaulted_GetRef();
			B.Id = FName(Id);
			B.Priority = Priority;
			B.SlopeMinDeg = SlopeMin;
			B.SlopeMaxDeg = SlopeMax;
			B.MoistureMin = MoistureMin;
		};
		Add(TEXT("Meadow"), 1, 0.0f, 90.0f, 0.0f);
		Add(TEXT("TemperateForest"), 10, 0.0f, 45.0f, 0.4f);
		Add(TEXT("RockyHighland"), 20, 28.0f, 90.0f, 0.0f);
		P->Finalize();
		return FMOWorldGenParamsRef(P);
	}

	constexpr int32 TestSeed = 424242;

	uint32 FnvF(uint32 H, float V)
	{
		const uint8* B = reinterpret_cast<const uint8*>(&V);
		for (int32 I = 0; I < 4; ++I) { H = (H ^ B[I]) * 16777619u; }
		return H;
	}

	FColor BiomeColor(const FMOWorldGenParams& P, int32 Index)
	{
		if (Index == INDEX_NONE) { return FColor(200, 180, 120); }
		const FString Name = P.Biomes[Index].Id.ToString();
		if (Name.Contains(TEXT("Rock"))) { return FColor(150, 148, 142); }
		if (Name.Contains(TEXT("Forest"))) { return FColor(46, 108, 52); }
		if (Name.Contains(TEXT("Meadow"))) { return FColor(148, 184, 88); }
		const uint32 H = GetTypeHash(P.Biomes[Index].Id);
		return FColor(80 + (H & 0x7F), 80 + ((H >> 8) & 0x7F), 80 + ((H >> 16) & 0x7F));
	}

	bool WritePng(const FString& Path, const TArray<FColor>& Pixels, int32 Width, int32 Height)
	{
		IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(EImageFormat::PNG);
		if (!Wrapper.IsValid() || !Wrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
		{
			return false;
		}
		const TArray64<uint8>& Png = Wrapper->GetCompressed();
		return FFileHelper::SaveArrayToFile(Png, *Path);
	}

	/** Renders a hillshaded, biome-tinted relief map + a climate map of a square window. Returns the relief PNG path. */
	void RenderMaps(const FMOWorldGenParams& P, int32 Seed, double CenterX, double CenterY, double SpanCm, int32 Res, const FString& Prefix,
		float VerticalExaggeration, float StepCm, FString& OutReliefPath, FString& OutClimatePath)
	{
		const double Cell = SpanCm / Res;
		const int32 W = Res + 2;
		TArray<float> Heights;
		Heights.SetNumUninitialized(W * W);
		TArray<FMOTerrainSample> Samples;
		Samples.SetNumUninitialized(Res * Res);
		const double X0 = CenterX - SpanCm * 0.5 - Cell;
		const double Y0 = CenterY - SpanCm * 0.5 - Cell;
		for (int32 Y = 0; Y < W; ++Y)
		{
			for (int32 X = 0; X < W; ++X)
			{
				const FMOTerrainSample S = FMOWorldGen::SampleColumn(P, X0 + X * Cell, Y0 + Y * Cell, Seed, StepCm);
				Heights[Y * W + X] = S.HeightCm;
				if (X >= 1 && X <= Res && Y >= 1 && Y <= Res)
				{
					Samples[(Y - 1) * Res + (X - 1)] = S;
				}
			}
		}

		TArray<FColor> Relief, Climate;
		Relief.SetNumUninitialized(Res * Res);
		Climate.SetNumUninitialized(Res * Res);
		const FVector Light = FVector(-0.6, 0.6, 0.7).GetSafeNormal();
		for (int32 Y = 0; Y < Res; ++Y)
		{
			for (int32 X = 0; X < Res; ++X)
			{
				const int32 Cx = X + 1, Cy = Y + 1;
				const float Dx = (Heights[Cy * W + Cx + 1] - Heights[Cy * W + Cx - 1]) / (2.0f * (float)Cell);
				const float Dy = (Heights[(Cy + 1) * W + Cx] - Heights[(Cy - 1) * W + Cx]) / (2.0f * (float)Cell);
				const FVector N = FVector(-Dx * VerticalExaggeration, -Dy * VerticalExaggeration, 1.0).GetSafeNormal();
				const float Shade = FMath::Clamp((float)FVector::DotProduct(N, Light), 0.0f, 1.0f);
				const FMOTerrainSample& S = Samples[Y * Res + X];

				FColor Base = BiomeColor(P, S.BiomeIndex);
				if (S.HeightCm < P.Tuning.SeaLevelCm) { Base = FColor(70, 110, 170); }   // below sea level: where water will be
				const float Lit = 0.35f + 0.75f * Shade;
				// image Y runs top-down; world +Y is north, so flip
				Relief[(Res - 1 - Y) * Res + X] = FColor((uint8)FMath::Clamp(Base.R * Lit, 0.0f, 255.0f), (uint8)FMath::Clamp(Base.G * Lit, 0.0f, 255.0f), (uint8)FMath::Clamp(Base.B * Lit, 0.0f, 255.0f), 255);
				Climate[(Res - 1 - Y) * Res + X] = FColor((uint8)(S.Temperature * 255.0f), (uint8)(S.Moisture * 255.0f), (uint8)((S.Continentalness * 0.5f + 0.5f) * 255.0f), 255);
			}
		}

		const FString Dir = FPaths::ProjectSavedDir() / TEXT("WorldGen");
		IFileManager::Get().MakeDirectory(*Dir, true);
		OutReliefPath = Dir / (Prefix + TEXT("_relief.png"));
		OutClimatePath = Dir / (Prefix + TEXT("_climate.png"));
		WritePng(OutReliefPath, Relief, Res, Res);
		WritePng(OutClimatePath, Climate, Res, Res);
	}
}

// ----------------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_Determinism,
	"MOFramework.WorldGen.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_Determinism::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeTestParams();
	int32 Differ = 0, Total = 0;
	bool bIdentical = true;
	FRandomStream R(7);
	for (int32 I = 0; I < 3000; ++I)
	{
		const double X = R.FRandRange(-2.5e6f, 2.5e6f), Y = R.FRandRange(-2.5e6f, 2.5e6f);
		const FMOTerrainSample A = FMOWorldGen::SampleColumn(*P, X, Y, TestSeed);
		const FMOTerrainSample B = FMOWorldGen::SampleColumn(*P, X, Y, TestSeed);
		const FMOTerrainSample C = FMOWorldGen::SampleColumn(*P, X, Y, TestSeed + 1);
		bIdentical &= (FMemory::Memcmp(&A, &B, sizeof(A)) == 0);
		Differ += (A.HeightCm != C.HeightCm) ? 1 : 0;
		++Total;
	}
	TestTrue(TEXT("the same (params, seed, position) is bit-identical every call"), bIdentical);
	TestTrue(FString::Printf(TEXT("CONTROL: a different seed changes the terrain (%d/%d samples differ)"), Differ, Total), Differ > Total * 95 / 100);

	// Large coordinates must not alias or degrade (lattice indices are 64-bit, positions double).
	const FMOTerrainSample Far1 = FMOWorldGen::SampleColumn(*P, 4.0e8, -3.0e8, TestSeed);
	const FMOTerrainSample Far2 = FMOWorldGen::SampleColumn(*P, 4.0e8 + 5000.0, -3.0e8, TestSeed);
	TestTrue(TEXT("sampling 4000 km from the origin still varies smoothly (no NaN, finite, in range)"),
		FMath::IsFinite(Far1.HeightCm) && FMath::IsFinite(Far2.HeightCm) && Far1.HeightCm >= P->MinHeightCm && Far1.HeightCm <= P->MaxHeightCm);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_ContinuityAndRange,
	"MOFramework.WorldGen.ContinuityAndRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_ContinuityAndRange::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeTestParams();
	float MaxStep = 0.0f;
	bool bInRange = true, bFinite = true;
	FRandomStream R(11);
	for (int32 Line = 0; Line < 60; ++Line)
	{
		const double X0 = R.FRandRange(-2.0e6f, 2.0e6f), Y0 = R.FRandRange(-2.0e6f, 2.0e6f);
		const double Dx = (Line % 2 == 0) ? 10.0 : 0.0, Dy = (Line % 2 == 0) ? 0.0 : 10.0;   // 10 cm steps
		float Prev = FMOWorldGen::SampleHeightCm(*P, X0, Y0, TestSeed);
		for (int32 I = 1; I <= 400; ++I)
		{
			const float H = FMOWorldGen::SampleHeightCm(*P, X0 + Dx * I, Y0 + Dy * I, TestSeed);
			bFinite &= FMath::IsFinite(H);
			bInRange &= (H >= P->MinHeightCm && H <= P->MaxHeightCm);
			MaxStep = FMath::Max(MaxStep, FMath::Abs(H - Prev));
			Prev = H;
		}
	}
	TestTrue(TEXT("every height is finite"), bFinite);
	TestTrue(FString::Printf(TEXT("every height is inside [MinHeightCm %.0f, MaxHeightCm %.0f]"), P->MinHeightCm, P->MaxHeightCm), bInRange);
	// 10 cm horizontal step: a jump above ~60 cm would be a 80-degree wall, i.e. a seam or a discontinuity.
	TestTrue(FString::Printf(TEXT("no discontinuities: max height change over a 10 cm step is %.1f cm (limit 60)"), MaxStep), MaxStep < 60.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_LODStability,
	"MOFramework.WorldGen.LODStability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_LODStability::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeTestParams();
	float MaxDelta = 0.0f;
	FRandomStream R(23);
	for (int32 I = 0; I < 2000; ++I)
	{
		const double X = R.FRandRange(-2.5e6f, 2.5e6f), Y = R.FRandRange(-2.5e6f, 2.5e6f);
		const float Fine = FMOWorldGen::SampleHeightCm(*P, X, Y, TestSeed, 100.0f);
		const float Coarse = FMOWorldGen::SampleHeightCm(*P, X, Y, TestSeed, 1600.0f);   // LOD 4: the detail octaves are skipped
		MaxDelta = FMath::Max(MaxDelta, FMath::Abs(Fine - Coarse));
	}
	// Skipped octaves may only remove their own energy: detail (2.5 m) plus the finest culled mountain/hill octaves.
	TestTrue(FString::Printf(TEXT("coarse LOD stays within %.0f cm of fine LOD (limit 900)"), MaxDelta), MaxDelta < 900.0f);

	// CONTROL: the gate really does something (otherwise the bound above proves nothing).
	float Differ = 0.0f;
	for (int32 I = 0; I < 200; ++I)
	{
		const double X = R.FRandRange(-2.5e6f, 2.5e6f), Y = R.FRandRange(-2.5e6f, 2.5e6f);
		Differ += FMath::Abs(FMOWorldGen::SampleHeightCm(*P, X, Y, TestSeed, 100.0f) - FMOWorldGen::SampleHeightCm(*P, X, Y, TestSeed, 6400.0f));
	}
	TestTrue(TEXT("CONTROL: a very coarse LOD does change the result (detail/octaves are actually skipped)"), Differ > 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_Coverage,
	"MOFramework.WorldGen.Coverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_Coverage::RunTest(const FString& Parameters)
{
	const FMOWorldGenParamsRef P = MakeTestParams();
	TArray<float> H, Cn, Er, M, T;
	TMap<int32, int32> BiomeCounts;
	const int32 N = 48;
	for (int32 Y = 0; Y < N; ++Y)
	{
		for (int32 X = 0; X < N; ++X)
		{
			const double Wx = -2.5e6 + 5.0e6 * (X + 0.5) / N, Wy = -2.5e6 + 5.0e6 * (Y + 0.5) / N;
			const FMOTerrainSample S = FMOWorldGen::SampleColumn(*P, Wx, Wy, TestSeed, 400.0f);
			H.Add(S.HeightCm); Cn.Add(S.Continentalness); Er.Add(S.Erosion); M.Add(S.Moisture); T.Add(S.Temperature);
			BiomeCounts.FindOrAdd(S.BiomeIndex)++;
		}
	}
	auto Pct = [](TArray<float> V, float Q) { V.Sort(); return V[FMath::Clamp((int32)(Q * (V.Num() - 1)), 0, V.Num() - 1)]; };
	AddInfo(FString::Printf(TEXT("height cm p5/p50/p95 = %.0f / %.0f / %.0f   continentalness p5/p95 = %.2f / %.2f   erosion p5/p95 = %.2f / %.2f   moisture p5/p95 = %.2f / %.2f   temp p5/p95 = %.2f / %.2f"),
		Pct(H, 0.05f), Pct(H, 0.5f), Pct(H, 0.95f), Pct(Cn, 0.05f), Pct(Cn, 0.95f), Pct(Er, 0.05f), Pct(Er, 0.95f), Pct(M, 0.05f), Pct(M, 0.95f), Pct(T, 0.05f), Pct(T, 0.95f)));
	for (const TPair<int32, int32>& B : BiomeCounts)
	{
		AddInfo(FString::Printf(TEXT("biome %s: %.1f%%"), *P->GetBiomeId(B.Key).ToString(), 100.0f * B.Value / (N * N)));
	}

	// The fields must use their range (otherwise the splines are decoration) and terrain must actually vary.
	TestTrue(TEXT("continentalness spans most of its range (p5 < -0.35, p95 > 0.35)"), Pct(Cn, 0.05f) < -0.35f && Pct(Cn, 0.95f) > 0.35f);
	TestTrue(TEXT("erosion spans most of its range (p5 < -0.35, p95 > 0.35)"), Pct(Er, 0.05f) < -0.35f && Pct(Er, 0.95f) > 0.35f);
	TestTrue(TEXT("moisture varies enough to produce more than one biome (p5 < 0.35, p95 > 0.6)"), Pct(M, 0.05f) < 0.35f && Pct(M, 0.95f) > 0.6f);
	TestTrue(TEXT("at least 100 m of relief between the 5th and 95th percentile of ground height"), Pct(H, 0.95f) - Pct(H, 0.05f) > 10000.0f);
	TestTrue(TEXT("more than one biome occurs"), BiomeCounts.Num() >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_GoldenChecksum,
	"MOFramework.WorldGen.GoldenChecksum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_GoldenChecksum::RunTest(const FString& Parameters)
{
	// If this fails after you changed the generator ON PURPOSE: bump FMOWorldGenParams::CurrentGeneratorVersion and paste the new value.
	// If it fails on ANOTHER MACHINE with the same binary: the generator is not deterministic across CPUs -- that desyncs co-op worlds.
	constexpr uint32 Expected = 0x9C4AE2B3u;   // generator v1; on an intentional change: bump CurrentGeneratorVersion, rerun, paste the printed value

	const FMOWorldGenParamsRef P = MakeTestParams();
	uint32 H = 2166136261u;
	for (int32 Y = 0; Y < 64; ++Y)
	{
		for (int32 X = 0; X < 64; ++X)
		{
			const FMOTerrainSample S = FMOWorldGen::SampleColumn(*P, -3.0e6 + 97003.0 * X, -3.0e6 + 97001.0 * Y, TestSeed, 100.0f);
			H = FnvF(H, S.HeightCm);
			H = FnvF(H, S.Moisture);
			H = FnvF(H, S.Temperature);
			H = (H ^ (uint32)(S.BiomeIndex + 1)) * 16777619u;
		}
	}
	AddInfo(FString::Printf(TEXT("GOLDEN checksum = 0x%08X, params hash = 0x%08X"), H, P->ComputeHash()));
	TestTrue(FString::Printf(TEXT("golden checksum of 4096 samples is 0x%08X (got 0x%08X)"), Expected, H), H == Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_BiomesFromDataTable,
	"MOFramework.WorldGen.BiomesFromDataTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_BiomesFromDataTable::RunTest(const FString& Parameters)
{
	// Module startup published the snapshot from DT_Biomes; rebuilding it must agree.
	const FMOWorldGenParamsRef Built = UMOWorldGenSettings::BuildParams();
	TestTrue(TEXT("DT_Biomes produced biomes"), Built->Biomes.Num() >= 3);
	TestTrue(TEXT("highest priority first: RockyHighland leads"), Built->GetBiomeId(0) == FName("RockyHighland"));
	TestTrue(TEXT("then TemperateForest"), Built->GetBiomeId(1) == FName("TemperateForest"));
	TestTrue(TEXT("then Meadow"), Built->GetBiomeId(2) == FName("Meadow"));
	TestTrue(TEXT("the published snapshot exists and matches a fresh build"), FMOWorldGenParamsProvider::HasPublished() && FMOWorldGenParamsProvider::Get()->ComputeHash() == Built->ComputeHash());

	// The shared resolver and the generator agree (this is the PCG spawner's biome question).
	FRandomStream R(5);
	int32 Mismatch = 0;
	for (int32 I = 0; I < 500; ++I)
	{
		const FVector L(R.FRandRange(-2.0e6f, 2.0e6f), R.FRandRange(-2.0e6f, 2.0e6f), R.FRandRange(-500.0f, 6000.0f));
		const float Slope = R.FRandRange(0.0f, 60.0f);
		const FMOClimateSample C = FMOWorldGen::SampleClimate(*Built, L.X, L.Y, TestSeed);
		const FName Expected = Built->GetBiomeId(FMOWorldGen::ResolveBiome(*Built, L.Z, Slope, C.Moisture, FMOWorldGen::TemperatureAtHeight(*Built, C.TemperatureSeaLevel, L.Z)));
		Mismatch += (UMOBiomeDatabaseSettings::ResolveBiomeAtWorld(L, Slope, TestSeed) == Expected) ? 0 : 1;
	}
	TestEqual(TEXT("UMOBiomeDatabaseSettings::ResolveBiomeAtWorld == FMOWorldGen (500 random points)"), Mismatch, 0);

	// Seeds: the terrain seed a graph sees is a pure function of the world seed.
	TestEqual(TEXT("TerrainSeedFromWorldSeed is stable"), UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(12345), UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(12345));
	TestNotEqual(TEXT("CONTROL: different world seeds give different terrain seeds"), UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(12345), UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(12346));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_TerrainNode,
	"MOFramework.WorldGen.TerrainNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_TerrainNode::RunTest(const FString& Parameters)
{
	// Registration: without VOXEL_REGISTER_FUNCTION the node would not evaluate in a graph.
	const UFunction* Function = UMOTerrainFunctionLibrary::StaticClass()->FindFunctionByName(TEXT("MOTerrain"));
	if (TestNotNull(TEXT("the MOTerrain UFUNCTION is reflected"), Function))
	{
		TestTrue(TEXT("the plugin has a native entry point registered for it (VOXEL_REGISTER_FUNCTION)"), FVoxelFunctionLibraryRegistry::FindFunction(*Function) != nullptr);

		// The graph editor builds exactly this node from the UFUNCTION: every parameter must translate to a valid pin of the right direction.
		const TSharedRef<FVoxelNode_UFunction> Node = FVoxelNode_UFunction::Make(const_cast<UFunction*>(Function));
		TSet<FName> Inputs, Outputs;
		bool bAllValid = true;
		for (const FVoxelPin& Pin : Node->GetPins())
		{
			bAllValid &= Pin.GetType().IsValid();
			(Pin.bIsInput ? Inputs : Outputs).Add(Pin.Name);
		}
		TestTrue(TEXT("every pin has a valid Voxel pin type"), bAllValid);
		for (const TCHAR* Name : { TEXT("WorldPosition"), TEXT("Seed") })
		{
			TestTrue(*FString::Printf(TEXT("input pin '%s'"), Name), Inputs.Contains(FName(Name)));
		}
		for (const TCHAR* Name : { TEXT("Height"), TEXT("BiomeIndex"), TEXT("Moisture"), TEXT("Temperature"), TEXT("SurfaceType") })
		{
			TestTrue(*FString::Printf(TEXT("output pin '%s'"), Name), Outputs.Contains(FName(Name)));
		}
		TestEqual(TEXT("exactly 2 input pins"), Inputs.Num(), 2);
		TestEqual(TEXT("exactly 5 output pins (Bounds/HeightRange are NOT here: see the header PITFALLS)"), Outputs.Num(), 5);
	}

	// The bounds node must have NO inputs: the stamp evaluates it where no position exists.
	const UFunction* BoundsFunction = UMOTerrainFunctionLibrary::StaticClass()->FindFunctionByName(TEXT("MOTerrainBounds"));
	if (TestNotNull(TEXT("the MOTerrainBounds UFUNCTION is reflected"), BoundsFunction))
	{
		TestTrue(TEXT("MOTerrainBounds is registered with the plugin"), FVoxelFunctionLibraryRegistry::FindFunction(*BoundsFunction) != nullptr);
		const TSharedRef<FVoxelNode_UFunction> BoundsNode = FVoxelNode_UFunction::Make(const_cast<UFunction*>(BoundsFunction));
		int32 NumIn = 0, NumOut = 0;
		for (const FVoxelPin& Pin : BoundsNode->GetPins()) { (Pin.bIsInput ? NumIn : NumOut)++; }
		TestEqual(TEXT("MOTerrainBounds has no input pins"), NumIn, 0);
		TestEqual(TEXT("MOTerrainBounds has Bounds + HeightRange outputs"), NumOut, 2);
	}

	// Array input: every output matches FMOWorldGen exactly.
	const FMOWorldGenParamsRef P = FMOWorldGenParamsProvider::Get();
	constexpr int32 Num = 257;
	FVoxelDoubleVector2DBuffer Position;
	Position.X.Allocate(Num);
	Position.Y.Allocate(Num);
	FRandomStream R(99);
	for (int32 I = 0; I < Num; ++I)
	{
		Position.X.Set(I, R.FRandRange(-2.5e6f, 2.5e6f));
		Position.Y.Set(I, R.FRandRange(-2.5e6f, 2.5e6f));
	}
	FVoxelFloatBuffer Height, Biome, Moisture, Temperature;
	FVoxelSurfaceTypeBlendBuffer Surface;
	UMOTerrainFunctionLibrary::Compute(Position, TestSeed, 0, Height, Biome, Moisture, Temperature, Surface);
	FVoxelBox2D Bounds;
	FVoxelFloatRange Range;
	UMOTerrainFunctionLibrary::ComputeBounds(Bounds, Range);

	int32 Wrong = 0;
	for (int32 I = 0; I < Num; ++I)
	{
		const FMOTerrainSample S = FMOWorldGen::SampleColumn(*P, Position.X[I], Position.Y[I], TestSeed, P->Tuning.VoxelSizeCm);
		Wrong += (Height[I] == S.HeightCm && Biome[I] == (float)S.BiomeIndex && Moisture[I] == S.Moisture && Temperature[I] == S.Temperature) ? 0 : 1;
	}
	TestEqual(TEXT("array input: Height/BiomeIndex/Moisture/Temperature equal FMOWorldGen::SampleColumn at all 257 positions"), Wrong, 0);
	TestEqual(TEXT("array input: outputs have one value per position"), Height.Num(), Num);
	TestEqual(TEXT("Bounds is the world footprint (min X)"), Bounds.Min.X, -(double)P->Tuning.WorldHalfSizeCm);
	TestEqual(TEXT("Bounds is the world footprint (max Y)"), Bounds.Max.Y, (double)P->Tuning.WorldHalfSizeCm);
	TestEqual(TEXT("HeightRange.Min == snapshot MinHeightCm"), Range.Min, P->MinHeightCm);
	TestEqual(TEXT("HeightRange.Max == snapshot MaxHeightCm"), Range.Max, P->MaxHeightCm);

	// Constant input (a graph that feeds one position): one output value, same answer.
	FVoxelDoubleVector2DBuffer One;
	One.X.SetConstant(123456.0);
	One.Y.SetConstant(-654321.0);
	FVoxelFloatBuffer H1, B1, M1, T1;
	FVoxelSurfaceTypeBlendBuffer S1;
	UMOTerrainFunctionLibrary::Compute(One, TestSeed, 0, H1, B1, M1, T1, S1);
	const FMOTerrainSample Expected = FMOWorldGen::SampleColumn(*P, 123456.0, -654321.0, TestSeed, P->Tuning.VoxelSizeCm);
	TestEqual(TEXT("constant input: one value"), H1.Num(), 1);
	TestEqual(TEXT("constant input: same height as FMOWorldGen"), H1[0], Expected.HeightCm);

	// LOD: coarser LOD asks the generator for coarser spacing (a different, smoother answer is allowed, never a wild one).
	FVoxelFloatBuffer H4, B4, M4, T4;
	FVoxelSurfaceTypeBlendBuffer S4;
	UMOTerrainFunctionLibrary::Compute(Position, TestSeed, 4, H4, B4, M4, T4, S4);
	float MaxDelta = 0.0f;
	for (int32 I = 0; I < Num; ++I) { MaxDelta = FMath::Max(MaxDelta, FMath::Abs(H4[I] - Height[I])); }
	TestTrue(FString::Printf(TEXT("LOD 4 output stays within 900 cm of LOD 0 (max %.0f)"), MaxDelta), MaxDelta < 900.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_Performance,
	"MOFramework.WorldGen.Performance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_Performance::RunTest(const FString& Parameters)
{
	// A voxel chunk asks for ~1.1k heights; the volume path ~36k. Measure the scalar C++ cost (Development editor build: a pessimistic number).
	const FMOWorldGenParamsRef P = MakeTestParams();
	auto TimeUs = [&P](float StepCm, int32 Count)
	{
		double Sink = 0.0;
		const double T0 = FPlatformTime::Seconds();
		for (int32 I = 0; I < Count; ++I)
		{
			Sink += FMOWorldGen::SampleColumn(*P, (double)I * 137.0 - 1.0e6, (double)(I % 977) * 211.0, TestSeed, StepCm).HeightCm;
		}
		const double T1 = FPlatformTime::Seconds();
		if (Sink == 12345.678) { UE_LOG(LogTemp, Log, TEXT("unreachable")); }   // keep the loop alive
		return (T1 - T0) * 1.0e6 / Count;
	};
	const double Lod0 = TimeUs(100.0f, 20000);
	const double Lod4 = TimeUs(1600.0f, 20000);
	AddInfo(FString::Printf(TEXT("SampleColumn: %.2f us/sample at LOD0, %.2f us/sample at LOD4  =>  ~%.1f ms per 1.1k-sample chunk at LOD0 (single thread)"), Lod0, Lod4, Lod0 * 1100.0 / 1000.0));
	TestTrue(FString::Printf(TEXT("LOD0 costs under 30 us per sample (got %.2f)"), Lod0), Lod0 < 30.0);
	// (Not asserted: a coarser LOD is NOT measurably cheaper -- the cost is the climate fields, warp and mountain octaves, none of which fall below the cut. The LOD gate
	// exists to stop aliasing at coarse spacing, not to save time; LODStability covers its correctness.)
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOWorldGen_DumpMaps,
	"MOFramework.WorldGen.DumpMaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOWorldGen_DumpMaps::RunTest(const FString& Parameters)
{
	// Visual QA artifact: Saved/WorldGen/*.png. Uses the REAL snapshot (tuning from settings, biomes from DT_Biomes).
	const FMOWorldGenParamsRef P = UMOWorldGenSettings::BuildParams();
	const int32 Seed = UMOWorldSeedSubsystem::TerrainSeedFromWorldSeed(12345);
	FString Relief, Climate;

	RenderMaps(*P, Seed, 0.0, 0.0, 2.0 * P->Tuning.WorldHalfSizeCm, 1024, TEXT("world_seed12345"), 6.0f, 6000.0f, Relief, Climate);
	TestTrue(TEXT("world relief map written"), IFileManager::Get().FileExists(*Relief));
	TestTrue(TEXT("world climate map written"), IFileManager::Get().FileExists(*Climate));
	AddInfo(FString::Printf(TEXT("world map: %s"), *Relief));

	// The highest point of a coarse scan, seen at 10 km: this is where the mountains are, which the origin window may not show.
	double PeakX = 0.0, PeakY = 0.0;
	float PeakH = -1.0e9f;
	for (int32 Y = 0; Y < 64; ++Y)
	{
		for (int32 X = 0; X < 64; ++X)
		{
			const double Wx = -P->Tuning.WorldHalfSizeCm + 2.0 * P->Tuning.WorldHalfSizeCm * (X + 0.5) / 64.0;
			const double Wy = -P->Tuning.WorldHalfSizeCm + 2.0 * P->Tuning.WorldHalfSizeCm * (Y + 0.5) / 64.0;
			const float H = FMOWorldGen::SampleHeightCm(*P, Wx, Wy, Seed, 6000.0f);
			if (H > PeakH) { PeakH = H; PeakX = Wx; PeakY = Wy; }
		}
	}
	RenderMaps(*P, Seed, PeakX, PeakY, 1000000.0, 1024, TEXT("peak10km_seed12345"), 2.0f, 1000.0f, Relief, Climate);
	TestTrue(TEXT("peak-region relief map written"), IFileManager::Get().FileExists(*Relief));
	AddInfo(FString::Printf(TEXT("peak map (highest coarse sample %.0f cm at %.0f, %.0f): %s"), PeakH, PeakX, PeakY, *Relief));

	RenderMaps(*P, Seed, 0.0, 0.0, 600000.0, 1024, TEXT("region6km_seed12345"), 3.0f, 600.0f, Relief, Climate);
	TestTrue(TEXT("regional relief map written"), IFileManager::Get().FileExists(*Relief));
	AddInfo(FString::Printf(TEXT("region map: %s"), *Relief));

	RenderMaps(*P, Seed, 0.0, 0.0, 60000.0, 1024, TEXT("close600m_seed12345"), 1.5f, 100.0f, Relief, Climate);
	TestTrue(TEXT("close-up relief map written"), IFileManager::Get().FileExists(*Relief));
	AddInfo(FString::Printf(TEXT("close-up map: %s"), *Relief));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
