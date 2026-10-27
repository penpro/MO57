#include "MOBodyShapeBakeLibrary.h"
#include "Algo/AnyOf.h"
#include "Animation/MorphTarget.h"
#include "DNAReader.h"
#include "DNAToSkelMeshMap.h"
#include "Engine/SkeletalMesh.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterBodyIdentity.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanRigEvaluatedState.h"
#include "Modules/ModuleManager.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshAttributes.h"
#include "SkinnedAssetCompiler.h"
#include "SkelMeshDNAUtils.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, MOBodyShapeEditor)

DEFINE_LOG_CATEGORY_STATIC(LogMOBodyShape, Log, All);

namespace MOBodyShape
{
	/** Character's body state, the character opened for editing for the while (closed again on destruction if this opened it). */
	struct FCharacterBody
	{
		UMetaHumanCharacter* Character = nullptr;
		bool bOpenedHere = false;
		TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> State;
		FString Error;

		explicit FCharacterBody(UMetaHumanCharacter* InCharacter)
			: Character(InCharacter)
		{
			UMetaHumanCharacterEditorSubsystem* Creator = UMetaHumanCharacterEditorSubsystem::Get();
			if (!Creator || !Character)
			{
				Error = !Creator ? TEXT("the MetaHuman Creator's subsystem is not loaded") : TEXT("no character");
				return;
			}
			if (!Creator->IsObjectAddedForEditing(Character))
			{
				if (!Creator->TryAddObjectToEdit(Character))
				{
					Error = TEXT("the Creator would not open the character for editing (its Core Data installed?)");
					return;
				}
				bOpenedHere = true;
			}
			if (Character->bFixedBodyType)
			{
				Error = TEXT("the character's body is a fixed (imported) body, not the Creator's parametric one: nothing to solve");
				return;
			}
			State = Creator->CopyBodyState(Character);
		}

		~FCharacterBody()
		{
			if (bOpenedHere)
			{
				UMetaHumanCharacterEditorSubsystem::Get()->RemoveObjectToEdit(Character);
			}
		}

		bool IsValid() const { return State.IsValid() && Error.IsEmpty(); }
	};

	/** Index of the named measurement, or INDEX_NONE. */
	int32 FindConstraint(const TArray<FMetaHumanCharacterBodyConstraint>& Constraints, const FString& Name)
	{
		return Constraints.IndexOfByPredicate([&Name](const FMetaHumanCharacterBodyConstraint& C) { return C.Name.ToString().Equals(Name, ESearchCase::IgnoreCase); });
	}

	/**
	 * A shape's constraints: Held (every measurement at its value now, the free ones released), Moved (Held with the deltas, each clamped to
	 * its range) and Half (half way between: what a blend at 0.5 stands in for). Returns an error, or empty.
	 */
	FString MakeConstraints(const FMetaHumanCharacterBodyIdentity::FState& State, const FMOBodyShapeBakeSpec& Spec,
		TArray<FMetaHumanCharacterBodyConstraint>& Held, TArray<FMetaHumanCharacterBodyConstraint>& Moved, TArray<FMetaHumanCharacterBodyConstraint>& Half)
	{
		Held = State.GetBodyConstraints();
		for (int32 i = 0; i < Held.Num(); ++i)
		{
			Held[i].bIsActive = true;
			Held[i].TargetMeasurement = State.GetMeasurement(i);
		}
		TArray<FString> Unknown;
		for (const FString& Name : Spec.FreeMeasurements)
		{
			const int32 i = FindConstraint(Held, Name);
			if (i == INDEX_NONE)
			{
				Unknown.Add(Name);
				continue;
			}
			Held[i].bIsActive = false;
		}
		Moved = Held;
		Half = Held;
		for (const TPair<FString, float>& Delta : Spec.MeasurementDeltas)
		{
			const int32 i = FindConstraint(Moved, Delta.Key);
			if (i == INDEX_NONE)
			{
				Unknown.Add(Delta.Key);
				continue;
			}
			Moved[i].bIsActive = Half[i].bIsActive = true;
			// Clamped to the model's range, never against the asked direction: a base can be built past a limit (Lillian's waist, 53.1 cm under
			// a 55.3 minimum, Oct 27), and clamping it back there made her "narrower" waist 0.9 cm wider. Past the limit, it stays put.
			const float Target = FMath::Clamp(Held[i].TargetMeasurement + Delta.Value, Moved[i].MinMeasurement, Moved[i].MaxMeasurement);
			Moved[i].TargetMeasurement = (Target - Held[i].TargetMeasurement) * Delta.Value < 0.0f ? Held[i].TargetMeasurement : Target;
			Half[i].TargetMeasurement = 0.5f * (Held[i].TargetMeasurement + Moved[i].TargetMeasurement);
		}
		return Unknown.Num() > 0 ? TEXT("no such measurement: ") + FString::Join(Unknown, TEXT(", ")) + TEXT(" (GetBodyMeasurements lists them)") : FString();
	}
}

TArray<FString> UMOBodyShapeBakeLibrary::GetBodyMeasurements(UMetaHumanCharacter* Character)
{
	MOBodyShape::FCharacterBody Body(Character);
	if (!Body.IsValid())
	{
		return { TEXT("FAILED: ") + Body.Error };
	}
	TArray<FString> Out;
	const TArray<FMetaHumanCharacterBodyConstraint> Constraints = Body.State->GetBodyConstraints();
	for (int32 i = 0; i < Constraints.Num(); ++i)
	{
		Out.Add(FString::Printf(TEXT("%s = %.1f (%.1f..%.1f)"), *Constraints[i].Name.ToString(), Body.State->GetMeasurement(i),
			Constraints[i].MinMeasurement, Constraints[i].MaxMeasurement));
	}
	return Out;
}

TArray<FString> UMOBodyShapeBakeLibrary::BakeBodyMorphs(UMetaHumanCharacter* Character, USkeletalMesh* BodyMesh, const TArray<FMOBodyShapeBakeSpec>& Shapes)
{
	MOBodyShape::FCharacterBody Body(Character);
	if (!Body.IsValid() || !BodyMesh)
	{
		return { TEXT("FAILED: ") + (!Body.Error.IsEmpty() ? Body.Error : FString(TEXT("no body mesh"))) };
	}
	const TSharedPtr<IDNAReader> Dna = USkelMeshDNAUtils::GetDNAReader(BodyMesh);
	if (!Dna.IsValid())
	{
		return { TEXT("FAILED: the body mesh carries no DNA (build the character with the Creator first)") };
	}
	// Each mesh vertex's DNA vertex, and the body model's vertex in UE's frame by FState::GetVertex: exactly how the Creator writes a body
	// state into its own body mesh (UpdateLODModelVertexPositions), overlapping (split) vertices included.
	TUniquePtr<FDNAToSkelMeshMap> Map(USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(Dna.Get(), BodyMesh));
	if (!Map.IsValid())
	{
		return { TEXT("FAILED: no map from the mesh's vertices to its DNA") };
	}
	FSkeletalMeshModel* Model = BodyMesh->GetImportedModel();
	const int32 NumLods = FMath::Min(Model->LODModels.Num(), Map->ImportVtxToDNAVtxIndex.Num());
	const FMetaHumanRigEvaluatedState OwnVertices = Body.State->GetVerticesAndVertexNormals();

	TArray<FString> Reports;
	TSet<int32> Written;   // LODs whose mesh description takes a shape
	for (const FMOBodyShapeBakeSpec& Spec : Shapes)
	{
		TArray<FMetaHumanCharacterBodyConstraint> Held, Moved, Half;
		const FString Error = Spec.MorphName.IsNone() ? FString(TEXT("no morph name")) : MOBodyShape::MakeConstraints(*Body.State, Spec, Held, Moved, Half);
		if (!Error.IsEmpty())
		{
			Reports.Add(FString::Printf(TEXT("FAILED: %s: %s"), *Spec.MorphName.ToString(), *Error));
			continue;
		}
		FMetaHumanCharacterBodyIdentity::FState HeldState(*Body.State);
		HeldState.EvaluateBodyConstraints(Held);
		FMetaHumanCharacterBodyIdentity::FState MovedState(*Body.State);
		MovedState.EvaluateBodyConstraints(Moved);
		FMetaHumanCharacterBodyIdentity::FState HalfState(*Body.State);
		HalfState.EvaluateBodyConstraints(Half);
		const FMetaHumanRigEvaluatedState HeldVertices = HeldState.GetVerticesAndVertexNormals();
		const FMetaHumanRigEvaluatedState MovedVertices = MovedState.GetVerticesAndVertexNormals();
		const FMetaHumanRigEvaluatedState HalfVertices = HalfState.GetVerticesAndVertexNormals();

		// How far holding every measurement moves the character's own body: the shape is the difference from the HELD body, so this says
		// how near that is to the character as authored.
		double Drift = 0.0, DriftMax = 0.0;
		const int32 NumOwn = FMath::Min(OwnVertices.Vertices.Num(), HeldVertices.Vertices.Num());
		for (int32 i = 0; i < NumOwn; ++i)
		{
			const double D = FVector3f::Dist(OwnVertices.Vertices[i], HeldVertices.Vertices[i]);
			Drift += D;
			DriftMax = FMath::Max(DriftMax, D);
		}
		FString Report = FString::Printf(TEXT("%s on %s: held re-solve drift %.2f cm mean / %.2f max; measurements"), *Spec.MorphName.ToString(),
			*BodyMesh->GetName(), NumOwn ? Drift / NumOwn : 0.0, DriftMax);
		for (int32 i = 0; i < Held.Num(); ++i)
		{
			const FString Name = Held[i].Name.ToString();
			const bool bAsked = Algo::AnyOf(Spec.MeasurementDeltas,
				[&Name](const TPair<FString, float>& Delta) { return Delta.Key.Equals(Name, ESearchCase::IgnoreCase); });
			const float Before = HeldState.GetMeasurement(i), After = MovedState.GetMeasurement(i);
			if (bAsked || FMath::Abs(After - Before) >= 0.5f)
			{
				Report += FString::Printf(TEXT(" %s %.1f -> %.1f%s,"), *Name, Before, After, bAsked ? TEXT(" (asked)") : TEXT(""));
			}
		}
		Report.RemoveFromEnd(TEXT(","));
		Report += TEXT(";");

		// The shape goes into each LOD's mesh description as a morph attribute and the engine's own build makes the morph target from it. A
		// morph written straight into the LOD model is dropped by the next build from the mesh description (a save, a load -- Oct 26).
		for (int32 Lod = 0; Lod < NumLods; ++Lod)
		{
			const FSkeletalMeshLODModel& LodModel = Model->LODModels[Lod];
			FMeshDescription* Description = BodyMesh->HasMeshDescription(Lod) ? BodyMesh->GetMeshDescription(Lod) : nullptr;
			if (!Description || LodModel.MeshToImportVertexMap.Num() < static_cast<int32>(LodModel.NumVertices))
			{
				Report += FString::Printf(TEXT(" LOD%d has no mesh description of its own (the engine generates it, morphs included);"), Lod);
				continue;
			}
			FSkeletalMeshAttributes Attributes(*Description);
			if (!Attributes.GetMorphTargetNames().Contains(Spec.MorphName))
			{
				Attributes.RegisterMorphTargetAttribute(Spec.MorphName, false);
			}
			TVertexAttributesRef<FVector3f> Deltas = Attributes.GetVertexMorphPositionDelta(Spec.MorphName);
			for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
			{
				Deltas.Set(Vertex, FVector3f::ZeroVector);
			}
			float Largest = 0.0f;
			double HeldError = 0.0, HeldErrorMax = 0.0;   // the mesh against the held body
			int32 Mapped = 0, MovedCount = 0;
			TArray<float> Heights;   // LOD0: the height of each vertex moved over 2 mm (cm, the body's frame): where the shape acts
			double Bend = 0.0, BendMax = 0.0;   // LOD0: the body solved half way against the morph at 0.5
			for (int32 SectionIndex = 0; SectionIndex < LodModel.Sections.Num(); ++SectionIndex)
			{
				const FSkelMeshSection& Section = LodModel.Sections[SectionIndex];
				const int32 DnaMesh = Map->ImportVtxToDNAMeshIndex[Lod][Section.GetVertexBufferIndex()];
				const TArray<TArray<int32>>& Overlapping = Map->OverlappingVertices[Lod][SectionIndex];
				for (int32 v = 0; v < Section.GetNumVertices(); ++v)
				{
					const int32 DnaVertex = Map->ImportVtxToDNAVtxIndex[Lod][Section.GetVertexBufferIndex() + v];
					if (DnaVertex < 0)
					{
						continue;
					}
					const FVector3f HeldPosition = HeldState.GetVertex(HeldVertices.Vertices, DnaMesh, DnaVertex);
					const FVector3f Move = MovedState.GetVertex(MovedVertices.Vertices, DnaMesh, DnaVertex) - HeldPosition;
					const double Gap = FVector3f::Dist(HeldPosition, Section.SoftVertices[v].Position);
					HeldError += Gap;
					HeldErrorMax = FMath::Max(HeldErrorMax, Gap);
					++Mapped;
					if (Lod == 0 && Move.SizeSquared() >= FMath::Square(0.2f))
					{
						Heights.Add(HeldPosition.Z);
						const double D = FVector3f::Dist(HalfState.GetVertex(HalfVertices.Vertices, DnaMesh, DnaVertex), HeldPosition + 0.5f * Move);
						Bend += D;
						BendMax = FMath::Max(BendMax, D);
					}
					if (Move.SizeSquared() < FMath::Square(0.01f))   // under 0.1 mm: not part of the shape
					{
						continue;
					}
					TArray<int32> Targets = { v };
					if (Overlapping.IsValidIndex(v))
					{
						Targets.Append(Overlapping[v]);   // split copies of the vertex (seams) may come from their own mesh-description vertex
					}
					for (const int32 Target : Targets)
					{
						const int32 Imported = LodModel.MeshToImportVertexMap[Section.BaseVertexIndex + Target];
						if (Description->Vertices().IsValid(FVertexID(Imported)))
						{
							Deltas.Set(FVertexID(Imported), Move);
						}
					}
					Largest = FMath::Max(Largest, Move.Size());
					++MovedCount;
				}
			}
			Written.Add(Lod);
			if (Lod == 0 && Heights.Num() > 0)
			{
				Heights.Sort();
				Report += FString::Printf(TEXT(" %d vertices move over 2 mm, 90 %% of them %.0f..%.0f cm up; half way the body is %.2f cm mean / %.2f max from the morph at 0.5;"),
					Heights.Num(), Heights[Heights.Num() * 5 / 100], Heights[Heights.Num() * 95 / 100], Bend / Heights.Num(), BendMax);
			}
			Report += FString::Printf(TEXT(" LOD%d %d vertices (largest %.2f cm), mesh vs held body %.2f cm mean / %.2f max;"),
				Lod, MovedCount, Largest, Mapped ? HeldError / Mapped : 0.0, HeldErrorMax);
		}
		Reports.Add(Report);
	}
	if (Written.Num() == 0)
	{
		return Reports;
	}
	for (const int32 Lod : Written)
	{
		BodyMesh->CommitMeshDescription(Lod);
	}
	BodyMesh->Build();
	FSkinnedAssetCompilingManager::Get().FinishCompilation({ BodyMesh });

	// Read back: each morph as the build made it, per LOD.
	FString Built = TEXT("built:");
	for (const FMOBodyShapeBakeSpec& Spec : Shapes)
	{
		const UMorphTarget* Morph = BodyMesh->FindMorphTarget(Spec.MorphName);
		Built += TEXT(" ") + Spec.MorphName.ToString();
		for (int32 Lod = 0; Morph && Lod < Morph->GetMorphLODModels().Num(); ++Lod)
		{
			Built += FString::Printf(TEXT(" LOD%d %d"), Lod, Morph->GetMorphLODModels()[Lod].Vertices.Num());
		}
		Built += Morph ? TEXT(";") : TEXT(" NONE;");
	}
	Reports.Add(Built);
	BodyMesh->MarkPackageDirty();
	for (const FString& Report : Reports)
	{
		UE_LOG(LogMOBodyShape, Warning, TEXT("[MOBodyShape] baked %s"), *Report);
	}
	return Reports;
}

TArray<FString> UMOBodyShapeBakeLibrary::TransferBodyMorphs(USkeletalMesh* BodyMesh, USkeletalMesh* Target, const TArray<FName>& MorphNames,
	float NearCm, float FarCm)
{
	if (!BodyMesh || !Target || !BodyMesh->HasMeshDescription(0))
	{
		return { TEXT("FAILED: ") + FString(!BodyMesh || !Target ? TEXT("no body or target mesh") : TEXT("the body mesh has no LOD 0 mesh description")) };
	}
	NearCm = FMath::Max(0.0f, NearCm);
	FarCm = FMath::Max(NearCm + 0.01f, FarCm);

	// The body's LOD 0 vertices on a grid of FarCm cells: the nearest one within FarCm is in the 27 cells round a point.
	const FMeshDescription* BodyDescription = BodyMesh->GetMeshDescription(0);
	FSkeletalMeshConstAttributes BodyAttributes(*BodyDescription);
	const TVertexAttributesConstRef<FVector3f> BodyPositions = BodyAttributes.GetVertexPositions();
	TArray<FVertexID> BodyVertices;
	TMultiMap<FIntVector, int32> Grid;
	auto CellOf = [FarCm](const FVector3f& P) { return FIntVector(FMath::FloorToInt(P.X / FarCm), FMath::FloorToInt(P.Y / FarCm), FMath::FloorToInt(P.Z / FarCm)); };
	for (const FVertexID Vertex : BodyDescription->Vertices().GetElementIDs())
	{
		Grid.Add(CellOf(BodyPositions[Vertex]), BodyVertices.Add(Vertex));
	}
	auto Nearest = [&](const FVector3f& P, float& OutDistance)
	{
		const FIntVector Cell = CellOf(P);
		int32 Best = INDEX_NONE;
		float BestSquared = FMath::Square(FarCm);
		for (int32 X = -1; X <= 1; ++X)
		for (int32 Y = -1; Y <= 1; ++Y)
		for (int32 Z = -1; Z <= 1; ++Z)
		{
			const FIntVector Neighbour = Cell + FIntVector(X, Y, Z);
			for (auto It = Grid.CreateConstKeyIterator(Neighbour); It; ++It)
			{
				const float Squared = FVector3f::DistSquared(P, BodyPositions[BodyVertices[It.Value()]]);
				if (Squared < BestSquared)
				{
					BestSquared = Squared;
					Best = It.Value();
				}
			}
		}
		OutDistance = FMath::Sqrt(BestSquared);
		return Best;
	};

	TArray<FName> Missing;
	TArray<FName> Morphs;
	for (const FName Morph : MorphNames)
	{
		(BodyAttributes.GetMorphTargetNames().Contains(Morph) ? Morphs : Missing).Add(Morph);
	}
	TArray<FString> Reports;
	if (Missing.Num() > 0)
	{
		Reports.Add(TEXT("FAILED: the body has no shape ") + FString::JoinBy(Missing, TEXT(", "), [](FName N) { return N.ToString(); }));
	}
	TMap<FName, FString> Lines;
	bool bWrote = false;
	for (int32 Lod = 0; Lod < Target->GetLODNum(); ++Lod)
	{
		FMeshDescription* Description = Target->HasMeshDescription(Lod) ? Target->GetMeshDescription(Lod) : nullptr;
		if (!Description)
		{
			continue;
		}
		// Each target vertex once: its nearest body vertex and how much of its move it takes (smoothstep from NearCm to FarCm).
		FSkeletalMeshAttributes Attributes(*Description);
		const TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
		TArray<TTuple<FVertexID, FVertexID, float>> Followers;
		int32 Touching = 0;
		for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
		{
			float Distance = 0.0f;
			const int32 Body = Nearest(Positions[Vertex], Distance);
			if (Body == INDEX_NONE)
			{
				continue;
			}
			const float T = FMath::Clamp((Distance - NearCm) / (FarCm - NearCm), 0.0f, 1.0f);
			Followers.Add(MakeTuple(Vertex, BodyVertices[Body], 1.0f - T * T * (3.0f - 2.0f * T)));
			Touching += Distance <= NearCm ? 1 : 0;
		}
		for (const FName Morph : Morphs)
		{
			if (!Attributes.GetMorphTargetNames().Contains(Morph))
			{
				Attributes.RegisterMorphTargetAttribute(Morph, false);
			}
			TVertexAttributesRef<FVector3f> Deltas = Attributes.GetVertexMorphPositionDelta(Morph);
			const TVertexAttributesConstRef<FVector3f> BodyDeltas = BodyAttributes.GetVertexMorphPositionDelta(Morph);
			for (const FVertexID Vertex : Description->Vertices().GetElementIDs())
			{
				Deltas.Set(Vertex, FVector3f::ZeroVector);
			}
			int32 Moved = 0;
			float Largest = 0.0f;
			for (const TTuple<FVertexID, FVertexID, float>& Follower : Followers)
			{
				const FVector3f Move = BodyDeltas[Follower.Get<1>()] * Follower.Get<2>();
				if (Move.SizeSquared() >= FMath::Square(0.01f))
				{
					Deltas.Set(Follower.Get<0>(), Move);
					Largest = FMath::Max(Largest, Move.Size());
					++Moved;
				}
			}
			Lines.FindOrAdd(Morph) += FString::Printf(TEXT(" LOD%d %d of %d vertices follow (%d within %.1f cm of the body), largest %.2f cm;"), Lod,
				Moved, Description->Vertices().Num(), Touching, NearCm, Largest);
		}
		Target->CommitMeshDescription(Lod);
		bWrote = true;
	}
	if (!bWrote)
	{
		Reports.Add(TEXT("FAILED: the target has no mesh description of its own on any LOD"));
		return Reports;
	}
	Target->Build();
	FSkinnedAssetCompilingManager::Get().FinishCompilation({ Target });
	FString Built = TEXT("built:");
	for (const FName Morph : Morphs)
	{
		Reports.Add(FString::Printf(TEXT("%s on %s from %s:%s"), *Morph.ToString(), *Target->GetName(), *BodyMesh->GetName(), *Lines.FindRef(Morph)));
		const UMorphTarget* Made = Target->FindMorphTarget(Morph);
		Built += TEXT(" ") + Morph.ToString();
		for (int32 Lod = 0; Made && Lod < Made->GetMorphLODModels().Num(); ++Lod)
		{
			Built += FString::Printf(TEXT(" LOD%d %d"), Lod, Made->GetMorphLODModels()[Lod].Vertices.Num());
		}
		Built += Made ? TEXT(";") : TEXT(" NONE;");
	}
	Reports.Add(Built);
	Target->MarkPackageDirty();
	for (const FString& Report : Reports)
	{
		UE_LOG(LogMOBodyShape, Warning, TEXT("[MOBodyShape] transferred %s"), *Report);
	}
	return Reports;
}
