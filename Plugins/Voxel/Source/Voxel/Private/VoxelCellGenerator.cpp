// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "VoxelCellGenerator.h"
#include "VoxelQuery.h"
#include "VoxelLayers.h"
#include "VoxelHeightLayer.h"

VOXEL_CONSOLE_VARIABLE(
	VOXEL_API, bool, GVoxelCellGeneratorDebug, false,
	"voxel.CellGenerator.Debug",
	"If true will draw debug points");

VOXEL_CONSOLE_VARIABLE(
	VOXEL_API, float, GVoxelCellGeneratorTolerance, 0.1f,
	"voxel.CellGenerator.Tolerance",
	"Used to make the cell generator be less aggressive in cell-skipping");

VOXEL_CONSOLE_VARIABLE(
	VOXEL_API, bool, GVoxelCellGeneratorPropagate, true,
	"voxel.CellGenerator.Propagate",
	"Add a 1-chunk wide border to cell checks");

VOXEL_CONSOLE_VARIABLE(
	VOXEL_API, bool, GVoxelCellGeneratorCheckSigns, true,
	"voxel.CellGenerator.CheckSigns",
	"Force mesh if chunk changes sign");

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelCellGenerator::FVoxelCellGenerator(
	const int32 LOD,
	const FVoxelLayers& Layers,
	const FVoxelSurfaceTypeTable& SurfaceTypeTable,
	const FVector& Start,
	const FIntVector& Size,
	const float CellSize,
	const FVoxelWeakStackLayer& WeakLayer,
	const TSharedPtr<const FVoxelCellGeneratorHeights>& CachedHeights)
	: LOD(LOD)
	, Layers(Layers)
	, SurfaceTypeTable(SurfaceTypeTable)
	, Start(Start)
	, Size(Size)
	, CellSize(CellSize)
	, WeakLayer(WeakLayer)
	, Bounds(FVoxelIntBox(0, Size).ToVoxelBox().Scale(CellSize).ShiftBy(Start))
	, Query(
		LOD,
		Layers,
		SurfaceTypeTable,
		DependencyCollector,
		nullptr)

	, HeightLayer(INLINE_LAMBDA -> TSharedPtr<const FVoxelHeightLayer>
	{
		const TVoxelOptional<FVoxelWeakStackLayer> WeakHeightLayer = Query.GetFirstHeightLayer(WeakLayer);

		if (!WeakHeightLayer)
		{
			return {};
		}

		const TSharedPtr<const FVoxelHeightLayer> Result = Layers.FindHeightLayer(WeakHeightLayer.GetValue(), DependencyCollector);
		ensure(Result);
		return Result;
	})
	, VolumeLayer(Layers.FindVolumeLayer(WeakLayer, DependencyCollector))
	, Heights(CachedHeights)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelCellGenerator::ForeachCell(
	FVoxelDependencyCollector& OutDependencyCollector,
	const TVoxelFunctionRef<void(const FIntVector&, const TVoxelStaticArray<float, 8>&)> Lambda)
{
	VOXEL_FUNCTION_COUNTER();

	ON_SCOPE_EXIT
	{
		OutDependencyCollector.AddDependencies(DependencyCollector);
	};

	const FVoxelOptionalIntBox OptionalVolumeBounds = INLINE_LAMBDA -> FVoxelOptionalIntBox
	{
		if (!VolumeLayer)
		{
			return {};
		}

		const FVoxelOptionalBox VolumeBounds = VolumeLayer->GetVolumeStampBounds(
			Query,
			Bounds,
			EVoxelStampBehavior::AffectShape);

		if (!VolumeBounds.IsValid())
		{
			return {};
		}

		return FVoxelOptionalIntBox(
			FVoxelIntBox::FromFloatBox_WithPadding(VolumeBounds.GetBox().ShiftBy(-Start) / CellSize)
			.IntersectWith(FVoxelIntBox(0, Size)));
	};

	if (!OptionalVolumeBounds)
	{
		ForeachHeightCell<false>(Lambda, {});
		return;
	}

	const FVoxelIntBox VolumeBounds = OptionalVolumeBounds.GetBox();
	const FIntVector VolumeSize = VolumeBounds.Size();

	ForeachHeightCell<true>(Lambda, VolumeBounds);

	const TVoxelArray<FVoxelIntBox> AllBoundsToCompute = INLINE_LAMBDA -> TVoxelArray<FVoxelIntBox>
	{
		VOXEL_SCOPE_COUNTER("BoundsToCompute");

		const int32 Multiplier = 4;

		const FVoxelIntBox SparseVolumeBounds = VolumeBounds.DivideBigger(Multiplier).Extend(1);

		const FVector SparseStart =
			Start +
			FVector(SparseVolumeBounds.Min) * CellSize * Multiplier +
			CellSize * (Multiplier - 1) / 2.f;

		TVoxelArray<float> SparseDistances;
		{
			VOXEL_SCOPE_COUNTER_NUM("Query sparse distances", SparseVolumeBounds.Count_int32());

			FVoxelUtilities::SetNumFast(SparseDistances, SparseVolumeBounds.Count_int32());
			FVoxelUtilities::SetAll(SparseDistances, FVoxelUtilities::NaNf());

			VolumeLayer->Sample(FVoxelVolumeBulkQuery::Create(
				Query,
				SparseDistances,
				SparseStart,
				SparseVolumeBounds.Size(),
				CellSize * Multiplier));
		}

		const FIntVector SparseSize = SparseVolumeBounds.Size();
		const int32 SparseCount = SparseSize.X * SparseSize.Y * SparseSize.Z;

		FVoxelBitArray ShouldQuery;
		{
			VOXEL_SCOPE_COUNTER("ShouldQuery");

			{
				VOXEL_SCOPE_COUNTER("First pass");

				ShouldQuery.SetNum(SparseCount, false);

				for (int32 Z = 0; Z < SparseSize.Z; Z++)
				{
					for (int32 Y = 0; Y < SparseSize.Y; Y++)
					{
						for (int32 X = 0; X < SparseSize.X; X++)
						{
							const int32 Index = FVoxelUtilities::Get3DIndex<int32>(SparseSize, X, Y, Z);
							const double Distance = SparseDistances[Index];

							if (FVoxelUtilities::IsNaN(Distance))
							{
								continue;
							}

							if (FMath::Abs(Distance) > Multiplier * CellSize * UE_HALF_SQRT_3 * (1.f + GVoxelCellGeneratorTolerance))
							{
								continue;
							}

							ShouldQuery[Index] = true;
						}
					}
				}
			}

			if (GVoxelCellGeneratorCheckSigns)
			{
				VOXEL_SCOPE_COUNTER("Check signs");

				// Check transitions from negative to positive and force them to be meshes
				// This avoids holes when the distance field is incorrect

				for (int32 Z = 0; Z < SparseSize.Z; Z++)
				{
					for (int32 Y = 0; Y < SparseSize.Y; Y++)
					{
						for (int32 X = 0; X < SparseSize.X; X++)
						{
							const int32 Index = FVoxelUtilities::Get3DIndex<int32>(SparseSize, X, Y, Z);
							if (ShouldQuery[Index])
							{
								continue;
							}

							const bool bIsNegative = SparseDistances[Index] < 0;

							const bool bAnyNeighborDifferent = INLINE_LAMBDA
							{
								const FIntVector Min = FVoxelUtilities::ComponentMax(FIntVector(0), FIntVector(X - 1, Y - 1, Z - 1));
								const FIntVector Max = FVoxelUtilities::ComponentMin(SparseSize - 1, FIntVector(X + 1, Y + 1, Z + 1));

								for (int32 NeighborZ = Min.Z; NeighborZ <= Max.Z; NeighborZ++)
								{
									for (int32 NeighborY = Min.Y; NeighborY <= Max.Y; NeighborY++)
									{
										for (int32 NeighborX = Min.X; NeighborX <= Max.X; NeighborX++)
										{
											const bool bNeighborIsNegative = SparseDistances[FVoxelUtilities::Get3DIndex<int32>(SparseSize, NeighborX, NeighborY, NeighborZ)] < 0;
											if (bNeighborIsNegative != bIsNegative)
											{
												return true;
											}
										}
									}
								}

								return false;
							};

							if (bAnyNeighborDifferent)
							{
								ShouldQuery[Index] = true;
							}
						}
					}
				}
			}

			if (GVoxelCellGeneratorPropagate)
			{
				VOXEL_SCOPE_COUNTER("Propagate");

				// Don't recursively propagate
				const FVoxelBitArray ShouldQueryCopy = ShouldQuery;

				for (int32 Z = 0; Z < SparseSize.Z; Z++)
				{
					for (int32 Y = 0; Y < SparseSize.Y; Y++)
					{
						for (int32 X = 0; X < SparseSize.X; X++)
						{
							if (!ShouldQueryCopy[FVoxelUtilities::Get3DIndex<int32>(SparseSize, X, Y, Z)])
							{
								continue;
							}

							const FIntVector Min = FVoxelUtilities::ComponentMax(FIntVector(0), FIntVector(X - 1, Y - 1, Z - 1));
							const FIntVector Max = FVoxelUtilities::ComponentMin(SparseSize - 1, FIntVector(X + 1, Y + 1, Z + 1));

							for (int32 NeighborZ = Min.Z; NeighborZ <= Max.Z; NeighborZ++)
							{
								for (int32 NeighborY = Min.Y; NeighborY <= Max.Y; NeighborY++)
								{
									for (int32 NeighborX = Min.X; NeighborX <= Max.X; NeighborX++)
									{
										ShouldQuery[FVoxelUtilities::Get3DIndex<int32>(SparseSize, NeighborX, NeighborY, NeighborZ)] = true;
									}
								}
							}
						}
					}
				}
			}
		}

		if (ShouldQuery.AllEqual(false))
		{
			return {};
		}

		if (GVoxelCellGeneratorDebug)
		{
			VOXEL_SCOPE_COUNTER("Debug");

			FVoxelDebugDrawer Drawer(Layers.World);

			for (int32 Z = 0; Z < SparseSize.Z; Z++)
			{
				for (int32 Y = 0; Y < SparseSize.Y; Y++)
				{
					for (int32 X = 0; X < SparseSize.X; X++)
					{
						Drawer.Color(INLINE_LAMBDA
						{
							if (ShouldQuery[FVoxelUtilities::Get3DIndex<int32>(SparseSize, X, Y, Z)])
							{
								switch (LOD)
								{
								case 0: return FLinearColor(1.0f, 0.0f, 0.0f);
								case 1: return FLinearColor(1.0f, 0.3f, 0.0f);
								case 2: return FLinearColor(1.0f, 0.5f, 0.0f);
								case 3: return FLinearColor(1.0f, 0.7f, 0.0f);
								case 4: return FLinearColor(1.0f, 1.0f, 0.0f);
								case 5: return FLinearColor(0.9f, 0.9f, 0.3f);
								case 6: return FLinearColor(0.8f, 0.8f, 0.5f);
								case 7: return FLinearColor(0.7f, 0.6f, 0.4f);
								case 8: return FLinearColor(0.6f, 0.4f, 0.3f);
								case 9: return FLinearColor(0.5f, 0.3f, 0.2f);
								default: return FLinearColor(0.4f, 0.2f, 0.1f);
								}
							}
							else
							{
								switch (LOD)
								{
								case 0: return FLinearColor(0.0f, 1.0f, 0.0f);
								case 1: return FLinearColor(0.0f, 0.9f, 0.3f);
								case 2: return FLinearColor(0.0f, 0.8f, 0.5f);
								case 3: return FLinearColor(0.0f, 0.7f, 0.7f);
								case 4: return FLinearColor(0.0f, 0.6f, 0.9f);
								case 5: return FLinearColor(0.2f, 0.5f, 1.0f);
								case 6: return FLinearColor(0.3f, 0.4f, 0.9f);
								case 7: return FLinearColor(0.4f, 0.3f, 0.8f);
								case 8: return FLinearColor(0.5f, 0.2f, 0.7f);
								case 9: return FLinearColor(0.4f, 0.1f, 0.6f);
								default: return FLinearColor(0.3f, 0.0f, 0.5f);
								}
							}
						});

						const FVector Center = SparseStart + FVector(X, Y, Z) * CellSize * Multiplier;

						Drawer.DrawBox(FVoxelBox(Center).Extend(CellSize * Multiplier / 2), FTransform::Identity);
						Drawer.DrawPoint(Center);
					}
				}
			}
		}

		const TVoxelArray<FVoxelIntBox> AllSparseBounds = ShouldQuery.View().GreedyMeshing3D(SparseSize);

		TVoxelArray<FVoxelIntBox> Result;
		Result.Reserve(AllSparseBounds.Num());

		for (const FVoxelIntBox& SparseBounds : AllSparseBounds)
		{
			const FVoxelIntBox BoundsToCompute = SparseBounds.ShiftBy(SparseVolumeBounds.Min).Scale(Multiplier).IntersectWith(VolumeBounds);
			if (!BoundsToCompute.IsValid())
			{
				// Padding only
				continue;
			}

			for (const FVoxelIntBox& Other : Result)
			{
				ensureVoxelSlow(!Other.Intersects(BoundsToCompute));
			}

			Result.Add_EnsureNoGrow(BoundsToCompute);
		}

		return Result;
	};

	TVoxelArray<float> Distances;
	{
		VOXEL_SCOPE_COUNTER_NUM("Allocate distances", VolumeBounds.Count_int32());

		FVoxelUtilities::SetNumFast(Distances, VolumeBounds.Count_int32());
		FVoxelUtilities::SetAll(Distances, FVoxelUtilities::NaNf());
	}

	for (const FVoxelIntBox& BoundsToCompute : AllBoundsToCompute)
	{
		VOXEL_SCOPE_COUNTER("Process bounds");
		checkVoxelSlow(VolumeBounds.Contains(BoundsToCompute));

		if (GVoxelCellGeneratorDebug)
		{
			VOXEL_SCOPE_COUNTER("Debug");

			FVoxelDebugDrawer Drawer(Layers.World);
			Drawer.Color(FLinearColor::MakeRandomColor());

			for (int32 Z = BoundsToCompute.Min.Z; Z < BoundsToCompute.Max.Z; Z++)
			{
				for (int32 Y = BoundsToCompute.Min.Y; Y < BoundsToCompute.Max.Y; Y++)
				{
					for (int32 X = BoundsToCompute.Min.X; X < BoundsToCompute.Max.X; X++)
					{
						Drawer.DrawPoint(Start + FVector(X, Y, Z) * CellSize);
					}
				}
			}
		}

		const auto QueryHeights = [&](const TVoxelArrayView<float> OutHeights, const FVoxelIntBox2D& QueryIndices2D)
		{
			VOXEL_SCOPE_COUNTER("Copy heights");
			checkVoxelSlow(OutHeights.Num() == QueryIndices2D.Count_int32());

			if (!Heights)
			{
				ensureVoxelSlowNoSideEffects(VolumeLayer->HasIntersectStamps());
				return false;
			}

			const FVoxelIntBox2D Indices2D = QueryIndices2D.ShiftBy(FIntPoint(VolumeBounds.Min.X, VolumeBounds.Min.Y));
			checkVoxelSlow(FVoxelIntBox2D(0, FIntPoint(Size.X, Size.Y)).Contains(Indices2D));

			if (!Heights->Indices.Contains(Indices2D))
			{
				// Height stamps don't encompass the entire chunk, NaN the padding
				FVoxelUtilities::SetAll(OutHeights, FVoxelUtilities::NaNf());
			}

			const FVoxelIntBox2D Indices2DToCopy = Indices2D.IntersectWith(Heights->Indices);
			const int32 CopySizeX = Indices2DToCopy.Size().X;
			const int32 QuerySizeX = QueryIndices2D.Size().X;
			const int32 HeightSizeX = Heights->Indices.Size().X;

			for (int32 Y = Indices2DToCopy.Min.Y; Y < Indices2DToCopy.Max.Y; Y++)
			{
				const int32 X = Indices2DToCopy.Min.X;
				checkVoxelSlow(Heights->Indices.GetX().Contains(X));
				checkVoxelSlow(Heights->Indices.GetY().Contains(Y));

				const int32 QueryX = X - VolumeBounds.Min.X;
				const int32 QueryY = Y - VolumeBounds.Min.Y;
				checkVoxelSlow(QueryIndices2D.GetX().Contains(QueryX));
				checkVoxelSlow(QueryIndices2D.GetY().Contains(QueryY));

				FVoxelUtilities::Memcpy(
					OutHeights.Slice(QuerySizeX * (QueryY - QueryIndices2D.Min.Y) + QueryX - QueryIndices2D.Min.X, CopySizeX),
					Heights->Heights.View().Slice(HeightSizeX * (Y - Heights->Indices.Min.Y) + X - Heights->Indices.Min.X, CopySizeX));
			}

			return true;
		};

		FVoxelVolumeBulkQuery BulkQuery = FVoxelVolumeBulkQuery::Create(
			Query,
			Distances,
			Start + FVector(VolumeBounds.Min) * CellSize,
			VolumeSize,
			BoundsToCompute.ShiftBy(-VolumeBounds.Min),
			CellSize);

		BulkQuery.QueryHeights = QueryHeights;

		VolumeLayer->Sample(BulkQuery);
	}

	VOXEL_SCOPE_COUNTER_NUM("Iterate distances", VolumeBounds.Count_int32());

	for (int32 LocalX = 0; LocalX < VolumeSize.X - 1; LocalX++)
	{
		for (int32 LocalY = 0; LocalY < VolumeSize.Y - 1; LocalY++)
		{
			for (int32 LocalZ = 0; LocalZ < VolumeSize.Z - 1; LocalZ++)
			{
				TVoxelStaticArray<float, 8> CellDistances{ NoInit };

				CellDistances[0] = Distances[FVoxelUtilities::Get3DIndex<int32>(VolumeSize, LocalX + 0, LocalY + 0, LocalZ + 0)];
				if (FVoxelUtilities::IsNaN(CellDistances[0])) { continue; }
				CellDistances[1] = Distances[FVoxelUtilities::Get3DIndex<int32>(VolumeSize, LocalX + 1, LocalY + 0, LocalZ + 0)];
				if (FVoxelUtilities::IsNaN(CellDistances[1])) { continue; }
				CellDistances[2] = Distances[FVoxelUtilities::Get3DIndex<int32>(VolumeSize, LocalX + 0, LocalY + 1, LocalZ + 0)];
				if (FVoxelUtilities::IsNaN(CellDistances[2])) { continue; }
				CellDistances[3] = Distances[FVoxelUtilities::Get3DIndex<int32>(VolumeSize, LocalX + 1, LocalY + 1, LocalZ + 0)];
				if (FVoxelUtilities::IsNaN(CellDistances[3])) { continue; }
				CellDistances[4] = Distances[FVoxelUtilities::Get3DIndex<int32>(VolumeSize, LocalX + 0, LocalY + 0, LocalZ + 1)];
				if (FVoxelUtilities::IsNaN(CellDistances[4])) { continue; }
				CellDistances[5] = Distances[FVoxelUtilities::Get3DIndex<int32>(VolumeSize, LocalX + 1, LocalY + 0, LocalZ + 1)];
				if (FVoxelUtilities::IsNaN(CellDistances[5])) { continue; }
				CellDistances[6] = Distances[FVoxelUtilities::Get3DIndex<int32>(VolumeSize, LocalX + 0, LocalY + 1, LocalZ + 1)];
				if (FVoxelUtilities::IsNaN(CellDistances[6])) { continue; }
				CellDistances[7] = Distances[FVoxelUtilities::Get3DIndex<int32>(VolumeSize, LocalX + 1, LocalY + 1, LocalZ + 1)];
				if (FVoxelUtilities::IsNaN(CellDistances[7])) { continue; }

				if (CellDistances[0] >= 0)
				{
					if (CellDistances[1] >= 0 &&
						CellDistances[2] >= 0 &&
						CellDistances[3] >= 0 &&
						CellDistances[4] >= 0 &&
						CellDistances[5] >= 0 &&
						CellDistances[6] >= 0 &&
						CellDistances[7] >= 0)
					{
						continue;
					}
				}
				else
				{
					if (CellDistances[1] < 0 &&
						CellDistances[2] < 0 &&
						CellDistances[3] < 0 &&
						CellDistances[4] < 0 &&
						CellDistances[5] < 0 &&
						CellDistances[6] < 0 &&
						CellDistances[7] < 0)
					{
						continue;
					}
				}

				const FIntVector Position = VolumeBounds.Min + FIntVector(LocalX, LocalY, LocalZ);

				ensureVoxelSlowNoSideEffects(IsValidCellDistances(CellDistances));
				ensureVoxelSlowNoSideEffects(VolumeBounds.Contains(Position));

				Lambda(
					Position,
					CellDistances);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<bool bCheckVolumeBounds>
void FVoxelCellGenerator::ForeachHeightCell(
	const TVoxelFunctionRef<void(const FIntVector&, const TVoxelStaticArray<float, 8>&)> Lambda,
	const FVoxelIntBox& VolumeBounds)
{
	VOXEL_FUNCTION_COUNTER();

	if (VolumeLayer &&
		VolumeLayer->HasIntersectStamps())
	{
		// Can't generate height cells
		return;
	}

	if (!HeightLayer ||
		!HeightLayer->HasStamps(
			Query,
			Bounds,
			EVoxelStampBehavior::AffectShape,
			true))
	{
		return;
	}

	if (!Heights)
	{
		VOXEL_SCOPE_COUNTER("Compute heights");

		const FVoxelOptionalBox2D HeightBounds = HeightLayer->GetStampBounds(
			Query,
			FVoxelBox2D(Bounds),
			EVoxelStampBehavior::AffectShape);

		if (!ensure(HeightBounds.IsValid()))
		{
			return;
		}

		const FVoxelIntBox2D Indices =
			FVoxelIntBox2D::FromFloatBox_WithPadding(HeightBounds.GetBox().ShiftBy(-FVector2D(Start)) / CellSize)
			.IntersectWith(FVoxelIntBox2D(0, FIntPoint(Size.X, Size.Y)));

		const TSharedRef<FVoxelCellGeneratorHeights> NewHeights = MakeShared<FVoxelCellGeneratorHeights>();
		NewHeights->Indices = Indices;
		FVoxelUtilities::SetNumFast(NewHeights->Heights, Indices.Count_int32());
		FVoxelUtilities::SetAll(NewHeights->Heights, FVoxelUtilities::NaNf());

		const FVoxelQuery HeightQuery(
			LOD,
			Layers,
			SurfaceTypeTable,
			NewHeights->DependencyCollector,
			nullptr);

		VOXEL_SCOPE_COUNTER_NUM("Query heights", NewHeights->Heights.Num());

		HeightLayer->Sample(FVoxelHeightBulkQuery::Create(
			HeightQuery,
			NewHeights->Heights,
			FVector2D(Start) + FVector2D(Indices.Min) * CellSize,
			Indices.Size(),
			CellSize));

		Heights = NewHeights;
	}

	DependencyCollector.AddDependencies(Heights->DependencyCollector);

	if (bCheckVolumeBounds &&
		VolumeBounds == FVoxelIntBox(0, Size))
	{
		// Still cache heights so we can reuse it, but no need to iterate
		return;
	}

	const FVoxelIntBox2D Indices = Heights->Indices;
	const FIntPoint Size2D = Indices.Size();
	const TConstVoxelArrayView<float> HeightView = Heights->Heights.View();

	for (int32 LocalY = 0; LocalY < Size2D.Y - 1; LocalY++)
	{
		for (int32 LocalX = 0; LocalX < Size2D.X - 1; LocalX++)
		{
			const float Height00 = HeightView[FVoxelUtilities::Get2DIndex<int32>(Size2D, LocalX + 0, LocalY + 0)];
			const float Height01 = HeightView[FVoxelUtilities::Get2DIndex<int32>(Size2D, LocalX + 1, LocalY + 0)];
			const float Height10 = HeightView[FVoxelUtilities::Get2DIndex<int32>(Size2D, LocalX + 0, LocalY + 1)];
			const float Height11 = HeightView[FVoxelUtilities::Get2DIndex<int32>(Size2D, LocalX + 1, LocalY + 1)];

			if (FVoxelUtilities::IsNaN(Height00) ||
				FVoxelUtilities::IsNaN(Height01) ||
				FVoxelUtilities::IsNaN(Height10) ||
				FVoxelUtilities::IsNaN(Height11))
			{
				continue;
			}

			const float MinHeight = FMath::Min3(Height00, Height01, FMath::Min(Height10, Height11));
			const float MaxHeight = FMath::Max3(Height00, Height01, FMath::Max(Height10, Height11));

			const int32 MinZ = FMath::Max(FMath::FloorToInt32((MinHeight - Start.Z) / CellSize) - 1, 0);
			const int32 MaxZ = FMath::Min(FMath::FloorToInt32((MaxHeight - Start.Z) / CellSize) + 1, Size.Z - 1);

			for (int32 Z = MinZ; Z < MaxZ; Z++)
			{
				const double WorldMinZ = Start.Z + (Z + 0) * CellSize;
				const double WorldMaxZ = Start.Z + (Z + 1) * CellSize;

				const TVoxelStaticArray<float, 8> CellDistances
				{
					WorldMinZ - Height00,
					WorldMinZ - Height01,
					WorldMinZ - Height10,
					WorldMinZ - Height11,
					WorldMaxZ - Height00,
					WorldMaxZ - Height01,
					WorldMaxZ - Height10,
					WorldMaxZ - Height11
				};

				if (CellDistances[0] >= 0)
				{
					if (CellDistances[1] >= 0 &&
						CellDistances[2] >= 0 &&
						CellDistances[3] >= 0 &&
						CellDistances[4] >= 0 &&
						CellDistances[5] >= 0 &&
						CellDistances[6] >= 0 &&
						CellDistances[7] >= 0)
					{
						continue;
					}
				}
				else
				{
					if (CellDistances[1] < 0 &&
						CellDistances[2] < 0 &&
						CellDistances[3] < 0 &&
						CellDistances[4] < 0 &&
						CellDistances[5] < 0 &&
						CellDistances[6] < 0 &&
						CellDistances[7] < 0)
					{
						continue;
					}
				}

				const FIntVector Position = FIntVector(
					Indices.Min.X + LocalX,
					Indices.Min.Y + LocalY,
					Z);

				ensureVoxelSlowNoSideEffects(IsValidCellDistances(CellDistances));
				ensureVoxelSlowNoSideEffects(FVoxelIntBox(0, Size - 1).Contains(Position));

				if (bCheckVolumeBounds &&
					FVoxelIntBox(VolumeBounds.Min, VolumeBounds.Max - 1).Contains(Position))
				{
					// Will be handled by the volume pass
					continue;
				}

				Lambda(
					Position,
					CellDistances);
			}
		}
	}
}

bool FVoxelCellGenerator::IsValidCellDistances(const TVoxelStaticArray<float, 8>& CellDistances)
{
	for (const float Distance : CellDistances)
	{
		if (!ensure(!FVoxelUtilities::IsNaN(Distance)))
		{
			return false;
		}
	}

	for (const float Distance : CellDistances)
	{
		if ((Distance >= 0) != CellDistances[0] >= 0)
		{
			return true;
		}
	}

	ensure(false);
	return false;
}