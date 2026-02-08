#include "MOLakeActor.h"
#include "MOFramework.h"
#include "ProceduralMeshComponent.h"
#include "CollisionQueryParams.h"

AMOLakeActor::AMOLakeActor()
{
	// Smaller defaults for lakes
	LakeSizeX = 2000.0f;
	LakeSizeY = 2000.0f;
	WaterLevelOffset = 0.0f;
	bAutoDetectFromVoxel = false;
	MaxDetectionRadius = 10000.0f;
	VoxelSampleSpacing = 100.0f;
	MinDepthThreshold = 50.0f;
	bEllipticalShape = false;
	EdgeFalloffDistance = 200.0f;

	// Lower resolution for smaller water body
	MeshResolution = 32;

	// Calmer waves for lake
	Waves.Empty();

	FMOGerstnerWave Wave1;
	Wave1.Direction = FVector2D(1.0f, 0.3f);
	Wave1.Amplitude = 15.0f;
	Wave1.Wavelength = 300.0f;
	Wave1.Steepness = 0.3f;
	Wave1.Speed = 0.6f;
	Waves.Add(Wave1);

	FMOGerstnerWave Wave2;
	Wave2.Direction = FVector2D(-0.5f, 0.8f);
	Wave2.Amplitude = 10.0f;
	Wave2.Wavelength = 180.0f;
	Wave2.Steepness = 0.4f;
	Wave2.Speed = 0.8f;
	Wave2.PhaseOffset = 1.5f;
	Waves.Add(Wave2);

	FMOGerstnerWave Wave3;
	Wave3.Direction = FVector2D(0.6f, -0.4f);
	Wave3.Amplitude = 5.0f;
	Wave3.Wavelength = 80.0f;
	Wave3.Steepness = 0.5f;
	Wave3.Speed = 1.0f;
	Wave3.PhaseOffset = 3.0f;
	Waves.Add(Wave3);

	CachedCenter = FVector2D::ZeroVector;
}

void AMOLakeActor::OnConstruction(const FTransform& Transform)
{
	UpdateCachedValues();
	Super::OnConstruction(Transform);
}

void AMOLakeActor::BeginPlay()
{
	UpdateCachedValues();

	if (bAutoDetectFromVoxel)
	{
		DetectBoundsFromVoxel();
	}

	Super::BeginPlay();
}

#if WITH_EDITOR
void AMOLakeActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AMOLakeActor, LakeSizeX) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AMOLakeActor, LakeSizeY) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AMOLakeActor, bEllipticalShape) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AMOLakeActor, MeshResolution))
	{
		UpdateCachedValues();
		GenerateWaterMesh();
	}
}
#endif

void AMOLakeActor::UpdateCachedValues()
{
	FVector Location = GetActorLocation();
	CachedCenter = FVector2D(Location.X, Location.Y);
}

float AMOLakeActor::GetBaseWaterLevel() const
{
	return GetActorLocation().Z + WaterLevelOffset;
}

bool AMOLakeActor::IsInWaterBounds(const FVector2D& WorldXY) const
{
	FVector2D LocalXY = WorldXY - CachedCenter;

	if (bEllipticalShape)
	{
		// Ellipse equation: (x/a)^2 + (y/b)^2 <= 1
		float HalfX = LakeSizeX * 0.5f;
		float HalfY = LakeSizeY * 0.5f;
		float NormX = LocalXY.X / HalfX;
		float NormY = LocalXY.Y / HalfY;
		return (NormX * NormX + NormY * NormY) <= 1.0f;
	}
	else
	{
		// Rectangle
		return FMath::Abs(LocalXY.X) <= LakeSizeX * 0.5f &&
			   FMath::Abs(LocalXY.Y) <= LakeSizeY * 0.5f;
	}
}

FVector2D AMOLakeActor::GetMeshSize() const
{
	return FVector2D(LakeSizeX, LakeSizeY);
}

float AMOLakeActor::GetEdgeFalloffMultiplier(const FVector2D& LocalXY) const
{
	if (EdgeFalloffDistance <= 0.0f)
	{
		return 1.0f;
	}

	float DistanceToEdge;

	if (bEllipticalShape)
	{
		// For ellipse, calculate normalized distance from center
		float HalfX = LakeSizeX * 0.5f;
		float HalfY = LakeSizeY * 0.5f;
		float NormX = LocalXY.X / HalfX;
		float NormY = LocalXY.Y / HalfY;
		float NormDist = FMath::Sqrt(NormX * NormX + NormY * NormY);

		// Distance to edge (1.0 = at edge)
		float DistToEdgeNorm = 1.0f - NormDist;
		DistanceToEdge = DistToEdgeNorm * FMath::Min(HalfX, HalfY);
	}
	else
	{
		// For rectangle, find distance to nearest edge
		float DistX = (LakeSizeX * 0.5f) - FMath::Abs(LocalXY.X);
		float DistY = (LakeSizeY * 0.5f) - FMath::Abs(LocalXY.Y);
		DistanceToEdge = FMath::Min(DistX, DistY);
	}

	// Apply falloff
	if (DistanceToEdge >= EdgeFalloffDistance)
	{
		return 1.0f;
	}
	else if (DistanceToEdge <= 0.0f)
	{
		return 0.0f;
	}
	else
	{
		return DistanceToEdge / EdgeFalloffDistance;
	}
}

void AMOLakeActor::GenerateWaterMesh()
{
	if (!WaterMeshComponent)
	{
		return;
	}

	const float HalfSizeX = LakeSizeX * 0.5f;
	const float HalfSizeY = LakeSizeY * 0.5f;
	const int32 NumVerts = MeshResolution + 1;
	const float StepX = LakeSizeX / MeshResolution;
	const float StepY = LakeSizeY / MeshResolution;

	BaseVertices.Reset();
	Triangles.Reset();
	UVs.Reset();

	TArray<FVector> Vertices;
	TArray<int32> ValidTriangles;

	// For elliptical shapes, we need to track which vertices are valid
	TArray<int32> VertexIndexMap;
	VertexIndexMap.SetNumZeroed(NumVerts * NumVerts);

	int32 ValidVertexCount = 0;

	// Generate vertices
	for (int32 Y = 0; Y <= MeshResolution; ++Y)
	{
		for (int32 X = 0; X <= MeshResolution; ++X)
		{
			float PosX = -HalfSizeX + X * StepX;
			float PosY = -HalfSizeY + Y * StepY;

			int32 GridIndex = Y * NumVerts + X;

			if (bEllipticalShape)
			{
				// Check if vertex is inside ellipse
				float NormX = PosX / HalfSizeX;
				float NormY = PosY / HalfSizeY;
				if (NormX * NormX + NormY * NormY > 1.0f)
				{
					VertexIndexMap[GridIndex] = -1; // Invalid
					continue;
				}
			}

			VertexIndexMap[GridIndex] = ValidVertexCount++;
			Vertices.Add(FVector(PosX, PosY, 0.0f));
			BaseVertices.Add(FVector(PosX, PosY, 0.0f));

			float U = static_cast<float>(X) / MeshResolution;
			float V = static_cast<float>(Y) / MeshResolution;
			UVs.Add(FVector2D(U, V));
		}
	}

	// Generate triangles
	for (int32 Y = 0; Y < MeshResolution; ++Y)
	{
		for (int32 X = 0; X < MeshResolution; ++X)
		{
			int32 TopLeftGrid = Y * NumVerts + X;
			int32 TopRightGrid = TopLeftGrid + 1;
			int32 BottomLeftGrid = (Y + 1) * NumVerts + X;
			int32 BottomRightGrid = BottomLeftGrid + 1;

			int32 TopLeft = VertexIndexMap[TopLeftGrid];
			int32 TopRight = VertexIndexMap[TopRightGrid];
			int32 BottomLeft = VertexIndexMap[BottomLeftGrid];
			int32 BottomRight = VertexIndexMap[BottomRightGrid];

			// Only create triangles if all vertices are valid
			if (TopLeft >= 0 && TopRight >= 0 && BottomLeft >= 0)
			{
				ValidTriangles.Add(TopLeft);
				ValidTriangles.Add(BottomLeft);
				ValidTriangles.Add(TopRight);
			}

			if (TopRight >= 0 && BottomLeft >= 0 && BottomRight >= 0)
			{
				ValidTriangles.Add(TopRight);
				ValidTriangles.Add(BottomLeft);
				ValidTriangles.Add(BottomRight);
			}
		}
	}

	Triangles = ValidTriangles;

	// Generate normals
	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, Vertices.Num());

	// Generate tangents
	TArray<FProcMeshTangent> Tangents;
	Tangents.Init(FProcMeshTangent(FVector::ForwardVector, false), Vertices.Num());

	// Create the mesh section
	WaterMeshComponent->CreateMeshSection(
		0,
		Vertices,
		Triangles,
		Normals,
		UVs,
		TArray<FColor>(),
		Tangents,
		false
	);

	// Apply material
	if (WaterMaterial)
	{
		WaterMaterialInstance = UMaterialInstanceDynamic::Create(WaterMaterial, this);
		WaterMeshComponent->SetMaterial(0, WaterMaterialInstance);
	}
}

void AMOLakeActor::SetLakeSize(float NewSizeX, float NewSizeY)
{
	LakeSizeX = FMath::Max(100.0f, NewSizeX);
	LakeSizeY = FMath::Max(100.0f, NewSizeY);
	GenerateWaterMesh();
}

// ============================================================================
// VOXEL DETECTION
// ============================================================================

bool AMOLakeActor::IsVoxelEmptyAt(const FVector& WorldPosition) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	// Use line trace to detect if there's solid terrain at this point
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	// Trace down from above the position
	FVector TraceStart = FVector(WorldPosition.X, WorldPosition.Y, WorldPosition.Z + 100.0f);
	FVector TraceEnd = FVector(WorldPosition.X, WorldPosition.Y, WorldPosition.Z - 100.0f);

	bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		ECC_WorldStatic,
		QueryParams
	);

	// If we hit something at or above water level, it's not empty
	if (bHit && HitResult.Location.Z >= WorldPosition.Z - MinDepthThreshold)
	{
		return false;
	}

	return true;
}

float AMOLakeActor::FindEmptyExtent(const FVector& Center, const FVector2D& Direction) const
{
	FVector2D NormDir = Direction.GetSafeNormal();
	float Extent = 0.0f;
	float Step = VoxelSampleSpacing;

	while (Extent < MaxDetectionRadius)
	{
		Extent += Step;
		FVector TestPos = FVector(
			Center.X + NormDir.X * Extent,
			Center.Y + NormDir.Y * Extent,
			Center.Z
		);

		if (!IsVoxelEmptyAt(TestPos))
		{
			// Found solid terrain, return the extent before this point
			return Extent - Step;
		}
	}

	return MaxDetectionRadius;
}

void AMOLakeActor::DetectBoundsFromVoxel()
{
	FVector Center = GetActorLocation();

	// Sample in multiple directions to find bounds
	TArray<FVector2D> SampleDirections;
	const int32 NumSamples = 16;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		float Angle = (2.0f * PI * i) / NumSamples;
		SampleDirections.Add(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)));
	}

	// Find extent in each direction
	TArray<float> Extents;
	Extents.Reserve(NumSamples);

	for (const FVector2D& Dir : SampleDirections)
	{
		float Extent = FindEmptyExtent(Center, Dir);
		Extents.Add(Extent);
	}

	// Calculate bounding box from extents
	float MinX = FLT_MAX, MaxX = -FLT_MAX;
	float MinY = FLT_MAX, MaxY = -FLT_MAX;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		FVector2D EdgePoint = SampleDirections[i] * Extents[i];
		MinX = FMath::Min(MinX, EdgePoint.X);
		MaxX = FMath::Max(MaxX, EdgePoint.X);
		MinY = FMath::Min(MinY, EdgePoint.Y);
		MaxY = FMath::Max(MaxY, EdgePoint.Y);
	}

	// Set lake size based on detected bounds
	LakeSizeX = MaxX - MinX;
	LakeSizeY = MaxY - MinY;

	// Clamp to reasonable values
	LakeSizeX = FMath::Max(100.0f, LakeSizeX);
	LakeSizeY = FMath::Max(100.0f, LakeSizeY);

	UE_LOG(LogMOFramework, Log, TEXT("[MOLake] Detected bounds: %.0f x %.0f cm"), LakeSizeX, LakeSizeY);

	// Regenerate mesh with new size
	UpdateCachedValues();
	GenerateWaterMesh();
}
