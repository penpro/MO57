#include "MOWaterActorBase.h"
#include "MOFramework.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AMOWaterActorBase::AMOWaterActorBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	WaterMeshComponent = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMesh"));
	RootComponent = WaterMeshComponent;
	WaterMeshComponent->SetCastShadow(false);
	WaterMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	WaterMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	WaterMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void AMOWaterActorBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureDefaultWaves();
	GenerateWaterMesh();
}

void AMOWaterActorBase::BeginPlay()
{
	Super::BeginPlay();
	EnsureDefaultWaves();
	GenerateWaterMesh();
	UpdateMaterialParameters();
}

void AMOWaterActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update wave time for C++ calculations (buoyancy, surface queries)
	WaveTime += DeltaTime * WaveSpeedMultiplier;

	// Update mesh vertices if enabled (expensive - prefer material WPO)
	if (bUpdateMeshVertices)
	{
		UpdateMeshVertices();
	}
	// Note: Material animation uses built-in Time node, no update needed
}

void AMOWaterActorBase::EnsureDefaultWaves()
{
	if (Waves.Num() == 0)
	{
		// Default waves sized for typical mesh density
		// Wavelengths should be ~6-8x mesh cell size for smooth appearance
		FMOGerstnerWave Wave1;
		Wave1.Direction = FVector2D(1.0f, 0.0f);
		Wave1.Amplitude = 50.0f;
		Wave1.Wavelength = 2000.0f;
		Wave1.Steepness = 0.4f;
		Wave1.Speed = 0.5f;
		Waves.Add(Wave1);

		FMOGerstnerWave Wave2;
		Wave2.Direction = FVector2D(0.7f, 0.7f);
		Wave2.Amplitude = 30.0f;
		Wave2.Wavelength = 1200.0f;
		Wave2.Steepness = 0.35f;
		Wave2.Speed = 0.6f;
		Wave2.PhaseOffset = 1.5f;
		Waves.Add(Wave2);

		FMOGerstnerWave Wave3;
		Wave3.Direction = FVector2D(-0.3f, 0.9f);
		Wave3.Amplitude = 20.0f;
		Wave3.Wavelength = 800.0f;
		Wave3.Steepness = 0.3f;
		Wave3.Speed = 0.7f;
		Wave3.PhaseOffset = 3.0f;
		Waves.Add(Wave3);

		FMOGerstnerWave Wave4;
		Wave4.Direction = FVector2D(0.5f, -0.5f);
		Wave4.Amplitude = 15.0f;
		Wave4.Wavelength = 600.0f;
		Wave4.Steepness = 0.25f;
		Wave4.Speed = 0.8f;
		Wave4.PhaseOffset = 0.7f;
		Waves.Add(Wave4);
	}
}

void AMOWaterActorBase::GenerateWaterMesh()
{
	if (!WaterMeshComponent)
	{
		return;
	}

	const FVector2D MeshSize = GetMeshSize();
	const float HalfSizeX = MeshSize.X * 0.5f;
	const float HalfSizeY = MeshSize.Y * 0.5f;
	const int32 NumVerts = MeshResolution + 1;
	const float StepX = MeshSize.X / MeshResolution;
	const float StepY = MeshSize.Y / MeshResolution;

	BaseVertices.Reset();
	Triangles.Reset();
	UVs.Reset();

	// Generate vertices
	BaseVertices.Reserve(NumVerts * NumVerts);
	UVs.Reserve(NumVerts * NumVerts);

	for (int32 Y = 0; Y <= MeshResolution; ++Y)
	{
		for (int32 X = 0; X <= MeshResolution; ++X)
		{
			float PosX = -HalfSizeX + X * StepX;
			float PosY = -HalfSizeY + Y * StepY;
			BaseVertices.Add(FVector(PosX, PosY, 0.0f));

			float U = static_cast<float>(X) / MeshResolution;
			float V = static_cast<float>(Y) / MeshResolution;
			UVs.Add(FVector2D(U, V));
		}
	}

	// Generate triangles
	Triangles.Reserve(MeshResolution * MeshResolution * 6);

	for (int32 Y = 0; Y < MeshResolution; ++Y)
	{
		for (int32 X = 0; X < MeshResolution; ++X)
		{
			int32 TopLeft = Y * NumVerts + X;
			int32 TopRight = TopLeft + 1;
			int32 BottomLeft = (Y + 1) * NumVerts + X;
			int32 BottomRight = BottomLeft + 1;

			// First triangle
			Triangles.Add(TopLeft);
			Triangles.Add(BottomLeft);
			Triangles.Add(TopRight);

			// Second triangle
			Triangles.Add(TopRight);
			Triangles.Add(BottomLeft);
			Triangles.Add(BottomRight);
		}
	}

	// Generate normals (all up for flat water, waves handled by material or UpdateMeshVertices)
	TArray<FVector> Normals;
	Normals.Init(FVector::UpVector, BaseVertices.Num());

	// Generate tangents
	TArray<FProcMeshTangent> Tangents;
	Tangents.Init(FProcMeshTangent(FVector::ForwardVector, false), BaseVertices.Num());

	// Create the mesh section
	WaterMeshComponent->CreateMeshSection(
		0,
		BaseVertices,
		Triangles,
		Normals,
		UVs,
		TArray<FColor>(),
		Tangents,
		false // No collision for now
	);

	// Apply material
	if (WaterMaterial)
	{
		WaterMaterialInstance = UMaterialInstanceDynamic::Create(WaterMaterial, this);
		WaterMeshComponent->SetMaterial(0, WaterMaterialInstance);
	}
}

void AMOWaterActorBase::UpdateMeshVertices()
{
	if (!WaterMeshComponent || BaseVertices.Num() == 0)
	{
		return;
	}

	TArray<FVector> DisplacedVertices;
	TArray<FVector> Normals;
	DisplacedVertices.Reserve(BaseVertices.Num());
	Normals.Reserve(BaseVertices.Num());

	const FVector ActorLocation = GetActorLocation();

	for (int32 i = 0; i < BaseVertices.Num(); ++i)
	{
		const FVector& BaseVert = BaseVertices[i];
		FVector2D WorldPos2D(ActorLocation.X + BaseVert.X, ActorLocation.Y + BaseVert.Y);

		FVector Displacement = CalculateGerstnerDisplacement(WorldPos2D, WaveTime);
		FVector Normal = CalculateGerstnerNormal(WorldPos2D, WaveTime);

		DisplacedVertices.Add(FVector(BaseVert.X + Displacement.X, BaseVert.Y + Displacement.Y, Displacement.Z));
		Normals.Add(Normal);
	}

	TArray<FProcMeshTangent> Tangents;
	Tangents.Init(FProcMeshTangent(FVector::ForwardVector, false), DisplacedVertices.Num());

	WaterMeshComponent->UpdateMeshSection(
		0,
		DisplacedVertices,
		Normals,
		UVs,
		TArray<FColor>(),
		Tangents
	);
}

void AMOWaterActorBase::UpdateMaterialParameters()
{
	if (!WaterMaterialInstance)
	{
		return;
	}

	// Material uses built-in Time node for smooth GPU animation
	// Set first wave parameters to control the primary wave characteristics
	// Secondary waves in material HLSL scale from these values
	if (Waves.Num() > 0)
	{
		const FMOGerstnerWave& Wave = Waves[0];
		FVector2D NormDir = Wave.Direction.GetSafeNormal();

		WaterMaterialInstance->SetScalarParameterValue(TEXT("WaveDirectionX"), NormDir.X);
		WaterMaterialInstance->SetScalarParameterValue(TEXT("WaveDirectionY"), NormDir.Y);
		WaterMaterialInstance->SetScalarParameterValue(TEXT("WaveAmplitude"), Wave.Amplitude * WaveAmplitudeMultiplier);
		WaterMaterialInstance->SetScalarParameterValue(TEXT("Wavelength"), Wave.Wavelength);
		WaterMaterialInstance->SetScalarParameterValue(TEXT("WaveSteepness"), Wave.Steepness);
		WaterMaterialInstance->SetScalarParameterValue(TEXT("WaveSpeed"), Wave.Speed * WaveSpeedMultiplier);
	}
}

// ============================================================================
// GERSTNER WAVE MATH
// ============================================================================

FVector AMOWaterActorBase::CalculateSingleWaveDisplacement(const FMOGerstnerWave& Wave, const FVector2D& Position2D, float Time) const
{
	FVector2D Direction = Wave.Direction.GetSafeNormal();
	float Amplitude = Wave.Amplitude * WaveAmplitudeMultiplier;
	float Frequency = 2.0f * PI / Wave.Wavelength;
	float Speed = Wave.Speed * WaveSpeedMultiplier;
	float Steepness = Wave.Steepness;

	// Phase: dot(Direction, Position) * Frequency + Time * Speed * Frequency + PhaseOffset
	float Phase = FVector2D::DotProduct(Direction, Position2D) * Frequency + Time * Speed * Frequency + Wave.PhaseOffset;

	float CosPhase = FMath::Cos(Phase);
	float SinPhase = FMath::Sin(Phase);

	// Gerstner displacement
	// Horizontal: Direction * Steepness * Amplitude * cos(Phase)
	// Vertical: Amplitude * sin(Phase)
	FVector Displacement;
	Displacement.X = Direction.X * Steepness * Amplitude * CosPhase;
	Displacement.Y = Direction.Y * Steepness * Amplitude * CosPhase;
	Displacement.Z = Amplitude * SinPhase;

	return Displacement;
}

FVector AMOWaterActorBase::CalculateGerstnerDisplacement(const FVector2D& Position2D, float Time) const
{
	FVector TotalDisplacement = FVector::ZeroVector;

	for (const FMOGerstnerWave& Wave : Waves)
	{
		TotalDisplacement += CalculateSingleWaveDisplacement(Wave, Position2D, Time);
	}

	return TotalDisplacement;
}

FVector AMOWaterActorBase::CalculateGerstnerNormal(const FVector2D& Position2D, float Time) const
{
	// Calculate normal from wave derivatives
	FVector Normal = FVector::UpVector;

	for (const FMOGerstnerWave& Wave : Waves)
	{
		FVector2D Direction = Wave.Direction.GetSafeNormal();
		float Amplitude = Wave.Amplitude * WaveAmplitudeMultiplier;
		float Frequency = 2.0f * PI / Wave.Wavelength;
		float Speed = Wave.Speed * WaveSpeedMultiplier;
		float Steepness = Wave.Steepness;

		float Phase = FVector2D::DotProduct(Direction, Position2D) * Frequency + Time * Speed * Frequency + Wave.PhaseOffset;

		float CosPhase = FMath::Cos(Phase);
		float SinPhase = FMath::Sin(Phase);

		// Partial derivatives for normal calculation
		// dZ/dX = Direction.X * Frequency * Amplitude * cos(Phase)
		// dZ/dY = Direction.Y * Frequency * Amplitude * cos(Phase)
		float WA = Frequency * Amplitude;
		Normal.X -= Direction.X * WA * CosPhase;
		Normal.Y -= Direction.Y * WA * CosPhase;
		// Z stays at 1.0 (up), adjusted by steepness effects
	}

	return Normal.GetSafeNormal();
}

// ============================================================================
// SURFACE QUERIES
// ============================================================================

FMOWaterSurfaceInfo AMOWaterActorBase::GetWaterSurfaceInfo(const FVector& WorldLocation) const
{
	FMOWaterSurfaceInfo Info;

	FVector2D WorldXY(WorldLocation.X, WorldLocation.Y);
	Info.bIsInWaterBounds = IsInWaterBounds(WorldXY);

	if (Info.bIsInWaterBounds)
	{
		FVector Displacement = CalculateGerstnerDisplacement(WorldXY, WaveTime);
		Info.SurfaceHeight = GetBaseWaterLevel() + Displacement.Z;
		Info.SurfaceNormal = CalculateGerstnerNormal(WorldXY, WaveTime);
		Info.HorizontalDisplacement = FVector2D(Displacement.X, Displacement.Y);
	}
	else
	{
		Info.SurfaceHeight = GetBaseWaterLevel();
		Info.SurfaceNormal = FVector::UpVector;
	}

	return Info;
}

float AMOWaterActorBase::GetWaterHeightAtLocation(const FVector& WorldLocation) const
{
	FVector2D WorldXY(WorldLocation.X, WorldLocation.Y);

	if (!IsInWaterBounds(WorldXY))
	{
		return GetBaseWaterLevel();
	}

	FVector Displacement = CalculateGerstnerDisplacement(WorldXY, WaveTime);
	return GetBaseWaterLevel() + Displacement.Z;
}

bool AMOWaterActorBase::IsUnderwater(const FVector& WorldLocation) const
{
	return WorldLocation.Z < GetWaterHeightAtLocation(WorldLocation);
}

float AMOWaterActorBase::GetDepthAtLocation(const FVector& WorldLocation) const
{
	float SurfaceHeight = GetWaterHeightAtLocation(WorldLocation);
	return SurfaceHeight - WorldLocation.Z;
}
