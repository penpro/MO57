#include "MOSpawnPoint.h"
#include "MOFramework.h"

#if WITH_EDITOR
#include "Components/BillboardComponent.h"
#include "Components/SphereComponent.h"
#endif

AMOSpawnPoint::AMOSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// Set up root component
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

#if WITH_EDITORONLY_DATA
	// Editor visualization
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (SpriteComponent)
	{
		SpriteComponent->SetupAttachment(RootComponent);
		SpriteComponent->bIsScreenSizeScaled = true;
	}

	RadiusVisualization = CreateEditorOnlyDefaultSubobject<USphereComponent>(TEXT("RadiusVis"));
	if (RadiusVisualization)
	{
		RadiusVisualization->SetupAttachment(RootComponent);
		RadiusVisualization->SetSphereRadius(SpawnRadius);
		RadiusVisualization->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RadiusVisualization->SetVisibility(true);
		RadiusVisualization->bHiddenInGame = true;
	}
#endif

	// Initialize runtime state
	LastUsedTime = FDateTime::MinValue();
}

bool AMOSpawnPoint::IsAvailable() const
{
	if (!bEnabled)
	{
		return false;
	}

	// Check cooldown
	if (LastUsedTime != FDateTime::MinValue())
	{
		const FTimespan TimeSinceUse = FDateTime::Now() - LastUsedTime;
		if (TimeSinceUse.GetTotalSeconds() < PointCooldownSeconds)
		{
			return false;
		}
	}

	return true;
}

bool AMOSpawnPoint::CanSpawnCategory(EMOSpawnCategory Category) const
{
	// Exact match or this point accepts any category
	return SpawnCategory == Category || SpawnCategory == EMOSpawnCategory::Custom;
}

FVector AMOSpawnPoint::GetRandomSpawnLocation() const
{
	if (SpawnRadius <= 0.0f)
	{
		return GetActorLocation();
	}

	// Random point within spawn radius
	const float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float RandomDistance = FMath::FRandRange(0.0f, SpawnRadius);

	FVector Offset;
	Offset.X = FMath::Cos(RandomAngle) * RandomDistance;
	Offset.Y = FMath::Sin(RandomAngle) * RandomDistance;
	Offset.Z = 0.0f;

	return GetActorLocation() + Offset;
}

void AMOSpawnPoint::MarkUsed()
{
	LastUsedTime = FDateTime::Now();
	UE_LOG(LogMOFramework, Verbose, TEXT("[SpawnPoint] %s marked used, cooldown: %.1fs"),
		*GetName(), PointCooldownSeconds);
}

float AMOSpawnPoint::GetRemainingCooldown() const
{
	if (LastUsedTime == FDateTime::MinValue())
	{
		return 0.0f;
	}

	const FTimespan TimeSinceUse = FDateTime::Now() - LastUsedTime;
	const float RemainingSeconds = PointCooldownSeconds - TimeSinceUse.GetTotalSeconds();
	return FMath::Max(0.0f, RemainingSeconds);
}
