#include "MOPCGConformToGroundSettings.h"
#include "MOWorldGen.h"
#include "MOWorldSeedSubsystem.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

FPCGElementPtr UMOPCGConformToGroundSettings::CreateElement() const
{
	return MakeShared<FMOPCGConformToGroundElement>();
}

float UMOPCGConformToGroundSettings::BaseWeight(float LocalHeightCm, float FollowBelowCm, float FadeCm)
{
	if (LocalHeightCm <= FollowBelowCm)
	{
		return 1.0f;
	}
	if (FadeCm <= 0.0f || LocalHeightCm >= FollowBelowCm + FadeCm)
	{
		return 0.0f;
	}
	const float T = (LocalHeightCm - FollowBelowCm) / FadeCm;
	return 1.0f - T * T * (3.0f - 2.0f * T);
}

double UMOPCGConformToGroundSettings::LowestZ(const FBox& LocalBox, const FTransform& Transform)
{
	double Lowest = TNumericLimits<double>::Max();
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Local((Corner & 1) ? LocalBox.Max.X : LocalBox.Min.X, (Corner & 2) ? LocalBox.Max.Y : LocalBox.Min.Y, (Corner & 4) ? LocalBox.Max.Z : LocalBox.Min.Z);
		Lowest = FMath::Min(Lowest, Transform.TransformPosition(Local).Z);
	}
	return Lowest;
}

double UMOPCGConformToGroundSettings::LowestGroundUnder(const FBox& LocalBox, const FTransform& Transform, const TFunctionRef<double(double X, double Y)>& GroundZ)
{
	const FVector Pivot = Transform.GetLocation();
	double Lowest = GroundZ(Pivot.X, Pivot.Y);
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		const FVector World = Transform.TransformPosition(FVector((Corner & 1) ? LocalBox.Max.X : LocalBox.Min.X, (Corner & 2) ? LocalBox.Max.Y : LocalBox.Min.Y, LocalBox.Min.Z));
		Lowest = FMath::Min(Lowest, GroundZ(World.X, World.Y));
	}
	return Lowest;
}

bool FMOPCGConformToGroundElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGConformToGroundElement::Execute);
	check(Context);
	const UMOPCGConformToGroundSettings* Settings = Context->GetInputSettings<UMOPCGConformToGroundSettings>();
	check(Settings);

	// The composition's root: the actor its points were copied onto.
	const AActor* Target = Context->GetTargetActor(nullptr);
	const FTransform Root = Target ? Target->GetActorTransform() : FTransform::Identity;
	const double RootScale = FMath::Max(FMath::Abs(Root.GetScale3D().Z), 1.0e-3);
	const FVector RootLocation = Root.GetLocation();

	FMOWorldGenParamsRef Params = FMOWorldGenParamsProvider::MakeDefault();   // a TSharedRef cannot be default-constructed (fatal)
	int32 TerrainSeed = 0;
	bool bGround = Settings->bFollowGround && FMOWorldGenParamsProvider::HasPublished();
	if (bGround)
	{
		const UMOWorldSeedSubsystem* Seeds = Target ? UMOWorldSeedSubsystem::Get(Target) : nullptr;
		bGround = Seeds && Seeds->HasActiveSeed();
		if (bGround)
		{
			Params = FMOWorldGenParamsProvider::Get();
			TerrainSeed = Seeds->GetActiveTerrainSeed();
		}
	}

	TMap<FSoftObjectPath, FBox> MeshBounds;   // per mesh, local space
	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
		const UPCGBasePointData* In = Cast<UPCGBasePointData>(Input.Data);
		if (!In || In->GetNumPoints() == 0)
		{
			continue;   // passes through untouched
		}
		const int32 N = In->GetNumPoints();
		UPCGBasePointData* Out = FPCGContext::NewPointData_AnyThread(Context);
		Out->InitializeFromData(In);
		Out->SetNumPoints(N);
		In->CopyPointsTo(Out, 0, 0, N);
		Output.Data = Out;

		// Each piece's mesh (its bounds give the lowest point; without one the pivot stands in).
		TArray<FSoftObjectPath> Meshes;
		{
			FPCGAttributePropertyInputSelector Selector;
			Selector.SetAttributeName(Settings->MeshAttribute);
			const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Out, Selector, /*bQuiet=*/true);
			const TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(Out, Selector);
			Meshes.SetNum(N);
			if (!Accessor || !Keys || !Accessor->GetRange<FSoftObjectPath>(Meshes, 0, *Keys, EPCGAttributeAccessorFlags::AllowBroadcastAndConstructible))
			{
				Meshes.Reset();
			}
		}
		const auto LocalBounds = [&MeshBounds, &Meshes](int32 Index, FBox& OutBox)
		{
			if (!Meshes.IsValidIndex(Index) || Meshes[Index].IsNull())
			{
				return false;
			}
			if (const FBox* Cached = MeshBounds.Find(Meshes[Index]))
			{
				OutBox = *Cached;
				return OutBox.IsValid != 0;
			}
			const UStaticMesh* Mesh = Cast<UStaticMesh>(Meshes[Index].TryLoad());
			OutBox = Mesh ? Mesh->GetBoundingBox() : FBox(ForceInit);
			MeshBounds.Add(Meshes[Index], OutBox);
			return OutBox.IsValid != 0;
		};

		TPCGValueRange<FTransform> Transforms = Out->GetTransformValueRange();
		TArray<float> Weights;
		TArray<double> Corrections;
		TArray<double> BaseCorrections;
		Weights.SetNumZeroed(N);
		Corrections.SetNumZeroed(N);
		const double Embed = Settings->EmbedCm * RootScale;

		// Pass 1: spread the base, measure how far each base piece's lowest point is from where it should rest.
		for (int32 I = 0; I < N; ++I)
		{
			FTransform& T = Transforms[I];
			FVector L = T.GetLocation();
			const float W = UMOPCGConformToGroundSettings::BaseWeight(static_cast<float>((L.Z - RootLocation.Z) / RootScale), Settings->FollowBelowCm, Settings->FadeCm);
			Weights[I] = W;
			if (W <= 0.0f)
			{
				continue;
			}
			const FVector2D Moved = FVector2D(L.X - RootLocation.X, L.Y - RootLocation.Y) * (1.0 + (Settings->BaseSpread - 1.0) * W);
			L.X = RootLocation.X + Moved.X;
			L.Y = RootLocation.Y + Moved.Y;
			T.SetLocation(L);
			if (bGround)
			{
				FBox Box;
				const auto GroundZ = [&Params, TerrainSeed](double X, double Y) { return (double)FMOWorldGen::SampleHeightCm(*Params, X, Y, TerrainSeed, Params->Tuning.VoxelSizeCm); };
				const bool bBox = LocalBounds(I, Box);
				const double Lowest = bBox ? UMOPCGConformToGroundSettings::LowestZ(Box, T) : L.Z;
				// The lowest ground under the piece's footprint, not under its pivot: on a slope its downhill side would show its bottom.
				const double Ground = bBox ? UMOPCGConformToGroundSettings::LowestGroundUnder(Box, T, GroundZ) : GroundZ(L.X, L.Y);
				Corrections[I] = (Ground - Embed) - Lowest;
				if (W >= 0.999f)
				{
					BaseCorrections.Add(Corrections[I]);
				}
			}
		}
		if (!bGround || BaseCorrections.Num() == 0)
		{
			continue;
		}

		// Pass 2: the whole composition moves by the base's median correction (the column stays on its foot); each base piece settles the rest of its own way.
		BaseCorrections.Sort();
		const double Shift = BaseCorrections[BaseCorrections.Num() / 2];
		for (int32 I = 0; I < N; ++I)
		{
			FTransform& T = Transforms[I];
			FVector L = T.GetLocation();
			L.Z += Shift + Weights[I] * (Corrections[I] - Shift);
			T.SetLocation(L);
		}
	}
	return true;
}
