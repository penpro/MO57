// Copyright Voxel Plugin SAS, 2026. All Rights Reserved.

#include "Scatter/VoxelScatterUtilities.h"
#include "TransvoxelData.h"
#include "VoxelPointId.h"
#include "VoxelPointSet.h"
#include "VoxelCellGenerator.h"
#include "Buffer/VoxelFloatBuffers.h"
#include "Utilities/VoxelBufferMathUtilities.h"
#include "Surface/VoxelSmartSurfaceTypeResolver.h"

FVoxelPointSet FVoxelScatterUtilities::ScatterPoints3D(
	const FVoxelQuery& Query,
	const FVector& Start,
	const FIntVector& Size,
	const float DistanceBetweenPoints,
	const uint64 Seed,
	const float Looseness,
	const FVoxelWeakStackLayer& WeakLayer,
	const bool bQuerySurfaceTypes,
	const bool bResolveSurfaceTypes,
	const TConstVoxelArrayView<FVoxelMetadataRef> MetadatasToQuery)
{
	VOXEL_FUNCTION_COUNTER();
	VOXEL_SCOPE_COUNTER_FORMAT("Size = %dx%dx%d", Size.X, Size.Y, Size.Z);

	TVoxelChunkedArray<FVoxelPointId> ChunkedIds;
	TVoxelChunkedArray<double> ChunkedPositionsX;
	TVoxelChunkedArray<double> ChunkedPositionsY;
	TVoxelChunkedArray<double> ChunkedPositionsZ;
	TVoxelChunkedArray<float> ChunkedNormalsX;
	TVoxelChunkedArray<float> ChunkedNormalsY;
	TVoxelChunkedArray<float> ChunkedNormalsZ;
	{
		VOXEL_SCOPE_COUNTER("CellGenerator");

		FVoxelCellGenerator CellGenerator(
			Query.LOD,
			Query.Layers,
			Query.SurfaceTypeTable,
			Start,
			Size,
			DistanceBetweenPoints,
			WeakLayer,
			nullptr);

		CellGenerator.ForeachCell(
			Query.DependencyCollector,
			[&](
			const FIntVector& CellPositionInBounds,
			const TVoxelStaticArray<float, 8>& CellDistances)
			{
				int32 CellCode =
					((CellDistances[0] >= 0) << 0) |
					((CellDistances[1] >= 0) << 1) |
					((CellDistances[2] >= 0) << 2) |
					((CellDistances[3] >= 0) << 3) |
					((CellDistances[4] >= 0) << 4) |
					((CellDistances[5] >= 0) << 5) |
					((CellDistances[6] >= 0) << 6) |
					((CellDistances[7] >= 0) << 7);

				CellCode = ~CellCode & 0xFF;
				checkVoxelSlow(CellCode != 0 && CellCode != 255);

				const FVector CellPosition = Start + FVector(CellPositionInBounds) * DistanceBetweenPoints;
				const uint64 PointSeed = FVoxelUtilities::MurmurHashMulti(
					Seed,
					CellPosition.X,
					CellPosition.Y,
					CellPosition.Z);

				const FRandomStream RandomStream(PointSeed);

				const int32 CellClass = Voxel::Transvoxel::GetCellClass(CellCode);
				const Voxel::Transvoxel::FCellIndices CellIndices = Voxel::Transvoxel::CellClassToCellIndices[CellClass];
				const Voxel::Transvoxel::FCellVertices CellVertices = Voxel::Transvoxel::CellCodeToCellVertices[CellCode];

				TVoxelFixedArray<FVector3f, 12> Vertices;
				for (int32 CellVertexIndex = 0; CellVertexIndex < CellVertices.NumVertices(); CellVertexIndex++)
				{
					const Voxel::Transvoxel::FVertexData VertexData = CellVertices.GetVertexData(CellVertexIndex);

					// A: low point / B: high point
					const int32 IndexA = VertexData.IndexA;
					const int32 IndexB = VertexData.IndexB;
					checkVoxelSlow(0 <= IndexA && IndexA < 8);
					checkVoxelSlow(0 <= IndexB && IndexB < 8);

					const float DistanceA = CellDistances[IndexA];
					const float DistanceB = CellDistances[IndexB];
					ensureVoxelSlow((DistanceA >= 0) != (DistanceB >= 0));

					const int32 EdgeIndex = VertexData.EdgeIndex;
					checkVoxelSlow(0 <= EdgeIndex && EdgeIndex < 3);

					FVector3f Position = FVector3f(
						bool(IndexA & 1),
						bool(IndexA & 2),
						bool(IndexA & 4));
					Position[EdgeIndex] += DistanceA / (DistanceA - DistanceB);

					Vertices.Add(Position);
				}

				FVector3f BestPosition = FVector3f(ForceInit);
				float BestDistance = MAX_flt;

				const FVector3f SamplePosition
				{
					RandomStream.FRand() * Looseness,
					RandomStream.FRand() * Looseness,
					RandomStream.FRand() * Looseness
				};

				for (int32 Index = 0; Index < CellIndices.NumTriangles(); Index++)
				{
					const int32 IndexA = CellIndices.GetIndex(3 * Index + 0);
					const int32 IndexB = CellIndices.GetIndex(3 * Index + 1);
					const int32 IndexC = CellIndices.GetIndex(3 * Index + 2);

					const FVector3f VertexA = Vertices[IndexA];
					const FVector3f VertexB = Vertices[IndexB];
					const FVector3f VertexC = Vertices[IndexC];

					if (!FVoxelUtilities::IsTriangleValid(
						VertexA,
						VertexB,
						VertexC,
						0.001f))
					{
						continue;
					}

					FVector3f Barycentrics;
					const float Distance = FVoxelUtilities::PointTriangleDistanceSquared(
						SamplePosition,
						VertexA,
						VertexB,
						VertexC,
						Barycentrics);

					if (Distance > BestDistance)
					{
						continue;
					}

					BestPosition =
						Barycentrics.X * VertexA +
						Barycentrics.Y * VertexB +
						Barycentrics.Z * VertexC;

					BestDistance = Distance;
				}

				const FVector Position = CellPosition + FVector(BestPosition) * DistanceBetweenPoints;

				const FVector Alpha = (Position - CellPosition) / DistanceBetweenPoints;
				ensureVoxelSlow(-KINDA_SMALL_NUMBER < Alpha.X && Alpha.X < 1.f + KINDA_SMALL_NUMBER);
				ensureVoxelSlow(-KINDA_SMALL_NUMBER < Alpha.Y && Alpha.Y < 1.f + KINDA_SMALL_NUMBER);
				ensureVoxelSlow(-KINDA_SMALL_NUMBER < Alpha.Z && Alpha.Z < 1.f + KINDA_SMALL_NUMBER);

				const float MinX = FVoxelUtilities::BilinearInterpolation(
					CellDistances[0b000],
					CellDistances[0b010],
					CellDistances[0b100],
					CellDistances[0b110],
					Alpha.Y,
					Alpha.Z);

				const float MaxX = FVoxelUtilities::BilinearInterpolation(
					CellDistances[0b001],
					CellDistances[0b011],
					CellDistances[0b101],
					CellDistances[0b111],
					Alpha.Y,
					Alpha.Z);

				const float MinY = FVoxelUtilities::BilinearInterpolation(
					CellDistances[0b000],
					CellDistances[0b001],
					CellDistances[0b100],
					CellDistances[0b101],
					Alpha.X,
					Alpha.Z);

				const float MaxY = FVoxelUtilities::BilinearInterpolation(
					CellDistances[0b010],
					CellDistances[0b011],
					CellDistances[0b110],
					CellDistances[0b111],
					Alpha.X,
					Alpha.Z);

				const float MinZ = FVoxelUtilities::BilinearInterpolation(
					CellDistances[0b000],
					CellDistances[0b001],
					CellDistances[0b010],
					CellDistances[0b011],
					Alpha.X,
					Alpha.Y);

				const float MaxZ = FVoxelUtilities::BilinearInterpolation(
					CellDistances[0b100],
					CellDistances[0b101],
					CellDistances[0b110],
					CellDistances[0b111],
					Alpha.X,
					Alpha.Y);

				const FVector3f Normal = FVector3f(
					MaxX - MinX,
					MaxY - MinY,
					MaxZ - MinZ).GetSafeNormal();

				ChunkedIds.Add(PointSeed);

				ChunkedPositionsX.Add(Position.X);
				ChunkedPositionsY.Add(Position.Y);
				ChunkedPositionsZ.Add(Position.Z);

				ChunkedNormalsX.Add(Normal.X);
				ChunkedNormalsY.Add(Normal.Y);
				ChunkedNormalsZ.Add(Normal.Z);
			});
	}

	const int32 Num = ChunkedPositionsX.Num();
	checkVoxelSlow(Num == ChunkedPositionsY.Num());
	checkVoxelSlow(Num == ChunkedPositionsZ.Num());
	checkVoxelSlow(Num == ChunkedNormalsX.Num());
	checkVoxelSlow(Num == ChunkedNormalsY.Num());
	checkVoxelSlow(Num == ChunkedNormalsZ.Num());
	checkVoxelSlow(Num == ChunkedIds.Num());

	if (Num == 0)
	{
		return {};
	}

	FVoxelPointIdBuffer Ids;
	FVoxelDoubleVectorBuffer Positions;
	FVoxelVectorBuffer Normals;
	{
		VOXEL_SCOPE_COUNTER("Copy");

		Ids.Allocate(Num);
		Positions.Allocate(Num);
		Normals.Allocate(Num);

		ChunkedIds.CopyTo(Ids.View());

		ChunkedPositionsX.CopyTo(Positions.X.View());
		ChunkedPositionsY.CopyTo(Positions.Y.View());
		ChunkedPositionsZ.CopyTo(Positions.Z.View());

		ChunkedNormalsX.CopyTo(Normals.X.View());
		ChunkedNormalsY.CopyTo(Normals.Y.View());
		ChunkedNormalsZ.CopyTo(Normals.Z.View());
	}

	FVoxelSurfaceTypeBlendBuffer SurfaceTypes;
	TVoxelMap<FVoxelMetadataRef, TSharedRef<FVoxelBuffer>> MetadataToBuffer;

	if (bQuerySurfaceTypes ||
		MetadatasToQuery.Num() > 0)
	{
		VOXEL_SCOPE_COUNTER("SurfaceTypes & Metadata");

		if (bQuerySurfaceTypes)
		{
			SurfaceTypes.AllocateZeroed(Num);
		}

		{
			MetadataToBuffer.Reserve(MetadatasToQuery.Num());

			for (const FVoxelMetadataRef& MetadataToQuery : MetadatasToQuery)
			{
				if (!MetadataToQuery.IsValid())
				{
					continue;
				}

				MetadataToBuffer.Add_EnsureNew(
					MetadataToQuery,
					MetadataToQuery.MakeDefaultBuffer(Num));
			}
		}

		(void)Query.SampleVolumeLayer(
			WeakLayer,
			Positions,
			SurfaceTypes.View(),
			MetadataToBuffer);

		if (bQuerySurfaceTypes &&
			bResolveSurfaceTypes)
		{
			VOXEL_SCOPE_COUNTER("Smart surface types");

			FVoxelSmartSurfaceTypeResolver Resolver(
				0,
				WeakLayer,
				Query.Layers,
				Query.SurfaceTypeTable,
				Query.DependencyCollector,
				Positions,
				Normals,
				SurfaceTypes.View());

			Resolver.Resolve();
		}
	}

	FVoxelPointSet Points;
	Points.SetNum(Num);
	Points.Add(FVoxelPointAttributes::Id, MakeSharedCopy(MoveTemp(Ids)));
	Points.Add(FVoxelPointAttributes::Position, MakeSharedCopy(MoveTemp(Positions)));
	Points.Add(FVoxelPointAttributes::Rotation, MakeSharedCopy(FVoxelBufferMathUtilities::MakeQuaternionFromZ(Normals)));

	if (bQuerySurfaceTypes)
	{
		Points.Add(FVoxelPointAttributes::SurfaceTypes, MakeSharedCopy(MoveTemp(SurfaceTypes)));
	}

	for (const auto& It : MetadataToBuffer)
	{
		Points.Add(It.Key.GetFName(), It.Value);
	}

	return Points;
}