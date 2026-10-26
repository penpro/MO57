#include "MOTestLedge.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

const FName AMOTestLedge::TestLedgeTag(TEXT("MOTestLedge"));

AMOTestLedge::AMOTestLedge()
{
	bReplicates = true;
	Block = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Block"));
	SetRootComponent(Block);
	Block->SetMobility(EComponentMobility::Movable);
	Block->SetCollisionProfileName(TEXT("BlockAll"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Block->SetStaticMesh(Cube.Object);
	}
	Tags.Add(TestLedgeTag);
}

void AMOTestLedge::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMOTestLedge, HeightCm);
	DOREPLIFETIME(AMOTestLedge, DepthCm);
	DOREPLIFETIME(AMOTestLedge, bClimbable);
}

void AMOTestLedge::Configure(float InHeightCm, float InDepthCm, bool bInClimbable)
{
	HeightCm = InHeightCm;
	DepthCm = InDepthCm;
	bClimbable = bInClimbable;
	ApplyShape();
}

void AMOTestLedge::BeginPlay()
{
	Super::BeginPlay();
	ApplyShape();
}

void AMOTestLedge::OnRep_Shape()
{
	ApplyShape();
}

void AMOTestLedge::ApplyShape()
{
	// The engine cube is 100 uu and centred: the block is 2 m wide, DepthCm deep, HeightCm tall (+4 cm sunk into the ground).
	Block->SetRelativeScale3D(FVector(DepthCm / 100.0f, 2.0f, (HeightCm + 4.0f) / 100.0f));
	if (bClimbable)
	{
		Tags.AddUnique(TEXT("MOClimbable"));
	}
	else
	{
		Tags.Remove(TEXT("MOClimbable"));
	}
}
