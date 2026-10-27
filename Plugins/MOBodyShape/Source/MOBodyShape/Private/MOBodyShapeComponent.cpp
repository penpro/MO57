#include "MOBodyShapeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, MOBodyShape)

const FName UMOBodyShapeComponent::BodyFat(TEXT("BodyFat"));
const FName UMOBodyShapeComponent::MuscleMass(TEXT("MuscleMass"));

float FMOBodyShapeMorph::WeightFor(float DriverValue) const
{
	const float Span = DriverAtFull - DriverAtZero;
	if (!FMath::IsFinite(DriverValue) || FMath::IsNearlyZero(Span))
	{
		return 0.0f;
	}
	return FMath::Clamp((DriverValue - DriverAtZero) / Span, 0.0f, 1.0f);
}

UMOBodyShapeComponent::UMOBodyShapeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	auto Shape = [](const TCHAR* Morph, FName Driver, float AtZero, float AtFull)
	{
		FMOBodyShapeMorph Out;
		Out.MorphTarget = Morph;
		Out.Driver = Driver;
		Out.DriverAtZero = AtZero;
		Out.DriverAtFull = AtFull;
		return Out;
	};
	Shapes = {
		Shape(TEXT("MO_Body_Heavy"), BodyFat, 18.0f, 40.0f),
		Shape(TEXT("MO_Body_Thin"), BodyFat, 18.0f, 8.0f),
		Shape(TEXT("MO_Body_Muscular"), MuscleMass, 30.0f, 40.0f),
		Shape(TEXT("MO_Body_Wasted"), MuscleMass, 30.0f, 20.0f),
		Shape(TEXT("MO_Body_BustLarge"), TEXT("Bust"), 0.0f, 1.0f),
		Shape(TEXT("MO_Body_BustSmall"), TEXT("Bust"), 0.0f, -1.0f),
		Shape(TEXT("MO_Body_HipsWide"), TEXT("Hips"), 0.0f, 1.0f),
		Shape(TEXT("MO_Body_HipsNarrow"), TEXT("Hips"), 0.0f, -1.0f),
		Shape(TEXT("MO_Body_WaistWide"), TEXT("Waist"), 0.0f, 1.0f),
		Shape(TEXT("MO_Body_WaistNarrow"), TEXT("Waist"), 0.0f, -1.0f),
		Shape(TEXT("MO_Body_ThighsFull"), TEXT("Thighs"), 0.0f, 1.0f),
		Shape(TEXT("MO_Body_ThighsSlim"), TEXT("Thighs"), 0.0f, -1.0f),
	};
}

void UMOBodyShapeComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplyShapes();
}

void UMOBodyShapeComponent::SetComposition(float InBodyFatPercent, float InMuscleMassKg)
{
	if (FMath::IsFinite(InBodyFatPercent))
	{
		Drivers.Add(BodyFat, FMath::Clamp(InBodyFatPercent, 2.0f, 70.0f));
	}
	if (FMath::IsFinite(InMuscleMassKg))
	{
		Drivers.Add(MuscleMass, FMath::Clamp(InMuscleMassKg, 5.0f, 100.0f));
	}
	ApplyShapes();
}

void UMOBodyShapeComponent::SetNeutralComposition(float BodyFatPercent, float MuscleMassKg)
{
	for (FMOBodyShapeMorph& Shape : Shapes)
	{
		if (Shape.Driver == BodyFat && FMath::IsFinite(BodyFatPercent))
		{
			Shape.DriverAtZero = BodyFatPercent;
		}
		else if (Shape.Driver == MuscleMass && FMath::IsFinite(MuscleMassKg))
		{
			Shape.DriverAtZero = MuscleMassKg;
		}
	}
	ApplyShapes();
}

void UMOBodyShapeComponent::SetDriver(FName Driver, float Value)
{
	if (Driver == BodyFat || Driver == MuscleMass)
	{
		SetComposition(Driver == BodyFat ? Value : GetDriver(BodyFat), Driver == MuscleMass ? Value : GetDriver(MuscleMass));
		return;
	}
	if (FMath::IsFinite(Value))
	{
		Drivers.Add(Driver, Value);
		ApplyShapes();
	}
}

float UMOBodyShapeComponent::GetDriver(FName Driver) const
{
	if (const float* Value = Drivers.Find(Driver))
	{
		return *Value;
	}
	const FMOBodyShapeMorph* Shape = Shapes.FindByPredicate([Driver](const FMOBodyShapeMorph& S) { return S.Driver == Driver; });
	return Shape ? Shape->DriverAtZero : 0.0f;
}

TArray<FName> UMOBodyShapeComponent::GetDriverNames() const
{
	TArray<FName> Names;
	for (const FMOBodyShapeMorph& Shape : Shapes)
	{
		Names.AddUnique(Shape.Driver);
	}
	return Names;
}

void UMOBodyShapeComponent::GetDriverRange(FName Driver, float& OutMin, float& OutMax) const
{
	OutMin = TNumericLimits<float>::Max();
	OutMax = TNumericLimits<float>::Lowest();
	for (const FMOBodyShapeMorph& Shape : Shapes)
	{
		if (Shape.Driver == Driver)
		{
			OutMin = FMath::Min3(OutMin, Shape.DriverAtZero, Shape.DriverAtFull);
			OutMax = FMath::Max3(OutMax, Shape.DriverAtZero, Shape.DriverAtFull);
		}
	}
	if (OutMin > OutMax)
	{
		OutMin = OutMax = 0.0f;
	}
}

void UMOBodyShapeComponent::ResetDrivers()
{
	Drivers.Reset();
	ApplyShapes();
}

void UMOBodyShapeComponent::ApplyShapes()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		if (!Asset)
		{
			continue;
		}
		for (const FMOBodyShapeMorph& Shape : Shapes)
		{
			if (Asset->FindMorphTarget(Shape.MorphTarget))
			{
				Mesh->SetMorphTarget(Shape.MorphTarget, Shape.WeightFor(GetDriver(Shape.Driver)), false);
			}
		}
	}
}

// mo.BodyShape <BodyFatPercent> [MuscleMassKg]: every body-shape component in the world takes that composition (a look at the range).
static FAutoConsoleCommandWithWorldAndArgs GBodyShapeCmd(
	TEXT("mo.BodyShape"),
	TEXT("Every MOBodyShapeComponent takes this composition. Usage: mo.BodyShape <BodyFatPercent> [MuscleMassKg]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() < 1)
		{
			return;
		}
		int32 Count = 0;
		for (TObjectIterator<UMOBodyShapeComponent> It; It; ++It)
		{
			if (It->GetWorld() == World)
			{
				It->SetComposition(FCString::Atof(*Args[0]), Args.Num() > 1 ? FCString::Atof(*Args[1]) : It->GetDriver(UMOBodyShapeComponent::MuscleMass));
				++Count;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[MOBodyShape] %d body(ies) set to %s %% body fat%s"), Count, *Args[0],
			Args.Num() > 1 ? *FString::Printf(TEXT(", %s kg of muscle"), *Args[1]) : TEXT(""));
	}));

// mo.BodyShapeSet <Driver> <Value>: one driver (Bust, Hips, Waist, Thighs: -1..1; BodyFat; MuscleMass) on every body-shape component.
static FAutoConsoleCommandWithWorldAndArgs GBodyShapeSetCmd(
	TEXT("mo.BodyShapeSet"),
	TEXT("Every MOBodyShapeComponent sets one driver. Usage: mo.BodyShapeSet <Driver: Bust|Hips|Waist|Thighs|BodyFat|MuscleMass> <Value>"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.Num() < 2)
		{
			return;
		}
		int32 Count = 0;
		for (TObjectIterator<UMOBodyShapeComponent> It; It; ++It)
		{
			if (It->GetWorld() == World)
			{
				It->SetDriver(FName(*Args[0]), FCString::Atof(*Args[1]));
				++Count;
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("[MOBodyShape] %d body(ies): %s = %s"), Count, *Args[0], *Args[1]);
	}));
