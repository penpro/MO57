// A carcass survives a save, body and all, and the same data draws it on a joiner (AMOCarcassActor::Body, UMOCarcassSaveDomain). Until Nov 2026 a kill left lying was gone at a load, and a
// joiner's copy of a carcass had no mesh at all. Expected: one carcass back, its parts, decay and pose as they were; failing before: none. Run: python Tools/ue.py auto --filter "MOFramework.Carcass."

#include "Misc/AutomationTest.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MOCarcassActor.h"
#include "MOCarcassSaveDomain.h"
#include "MOworldSaveGame.h"
#include "Misc/ScopeExit.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOCarcass_SavedAndRestored,
	"MOFramework.Carcass.SavedAndRestored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FMOCarcass_SavedAndRestored::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MOCarcassSaveWorld"));
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/MOFramework/Data/DT_Carcasses.DT_Carcasses"));
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Engine/EditorMeshes/SkeletalMesh/DefaultSkeletalMesh.DefaultSkeletalMesh"));
	UMOCarcassSaveDomain* Domain = World->GetSubsystem<UMOCarcassSaveDomain>();
	if (!TestNotNull(TEXT("the carcass table loads"), Table) || !TestNotNull(TEXT("a skeletal mesh loads"), Mesh) || !TestNotNull(TEXT("the save domain exists in a game world"), Domain))
	{
		return false;
	}

	// A body lying in a pose of its own, with a part cut and the decay under way.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Where(FRotator(0.0, 30.0, 0.0), FVector(1234.0, -567.0, 89.0));
	AMOCarcassActor* Source = World->SpawnActor<AMOCarcassActor>(AMOCarcassActor::StaticClass(), Where, Params);
	Source->CarcassDefinition.DataTable = Table;
	Source->CarcassDefinition.RowName = TEXT("Wolf");
	Source->HarvestedParts.Add(TEXT("Hide"));
	Source->TimeSinceDeath = 321.0f;
	Source->CarcassMesh->SetSkinnedAssetAndUpdate(Mesh, false);
	const int32 NumBones = Source->CarcassMesh->GetNumBones();
	if (!TestTrue(TEXT("the mesh has a bone to pose"), NumBones >= 1))
	{
		return false;
	}
	const FName Moved = Source->CarcassMesh->GetBoneName(NumBones - 1);
	FTransform Pose = Source->CarcassMesh->GetBoneTransformByName(Moved, EBoneSpaces::ComponentSpace);
	Pose.AddToTranslation(FVector(11.0, 22.0, 33.0));
	Source->CarcassMesh->SetBoneTransformByName(Moved, Pose, EBoneSpaces::ComponentSpace);
	Source->CaptureBody();
	TestEqual(TEXT("the body has every bone"), Source->Body.BoneTransforms.Num(), NumBones);
	TestTrue(TEXT("the body names its mesh"), Source->Body.Mesh.ToSoftObjectPath().IsValid());

	// Through the save, and through the bytes as a save file does it.
	UMOWorldSaveGame* Save = NewObject<UMOWorldSaveGame>();
	Domain->CaptureSaveDomain(*Save);
	if (!TestEqual(TEXT("the save holds the carcass"), Save->CarcassData.Carcasses.Num(), 1))
	{
		return false;
	}
	TArray<uint8> Bytes;
	{
		FMemoryWriter Memory(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Memory, false);
		FMOCarcassSaveData::StaticStruct()->SerializeItem(Archive, &Save->CarcassData, nullptr);
	}
	UMOWorldSaveGame* Loaded = NewObject<UMOWorldSaveGame>();
	{
		FMemoryReader Memory(Bytes);
		FObjectAndNameAsStringProxyArchive Archive(Memory, true);
		FMOCarcassSaveData::StaticStruct()->SerializeItem(Archive, &Loaded->CarcassData, nullptr);
	}

	// The load replaces what lies in the world.
	Domain->ApplySaveDomain(*Loaded);
	TArray<AMOCarcassActor*> Found;
	for (TActorIterator<AMOCarcassActor> It(World); It; ++It)
	{
		Found.Add(*It);
	}
	if (!TestEqual(TEXT("one carcass lies there after the load"), Found.Num(), 1) || !TestTrue(TEXT("it is a new actor: the old one went"), Found[0] != Source))
	{
		return false;
	}
	AMOCarcassActor* Back = Found[0];
	TestTrue(TEXT("in the same place"), Back->GetActorLocation().Equals(Where.GetLocation(), 0.01));
	TestEqual(TEXT("the same row"), Back->CarcassDefinition.RowName, FName(TEXT("Wolf")));
	TestNotNull(TEXT("its row resolves, for the butcher's menu"), Back->GetCarcassDefinitionRow());
	TestTrue(TEXT("the part cut stays cut"), Back->IsPartHarvested(TEXT("Hide")));
	TestEqual(TEXT("the decay goes on where it stood"), Back->TimeSinceDeath, 321.0f);
	TestTrue(TEXT("the body is drawn again"), Back->CarcassMesh->GetSkinnedAsset() == Mesh);
	TestTrue(TEXT("in the pose it lay in"), Back->CarcassMesh->GetBoneTransformByName(Moved, EBoneSpaces::ComponentSpace).GetLocation().Equals(Pose.GetLocation(), 0.1));

	// What a joiner gets: the same Body, replicated, with a notify that runs ApplyBody.
	const FProperty* BodyProperty = FindFProperty<FProperty>(AMOCarcassActor::StaticClass(), TEXT("Body"));
	if (TestNotNull(TEXT("the actor has its Body property"), BodyProperty))
	{
		TestTrue(TEXT("Body replicates"), BodyProperty->HasAnyPropertyFlags(CPF_Net));
		TestTrue(TEXT("Body has its notify"), BodyProperty->RepNotifyFunc == FName(TEXT("OnRep_Body")));
	}
	const FProperty* StageProperty = FindFProperty<FProperty>(AMOCarcassActor::StaticClass(), TEXT("DecayStage"));
	TestTrue(TEXT("a client follows the decay stage's look"), StageProperty && StageProperty->RepNotifyFunc == FName(TEXT("OnRep_DecayStage")));

	// An older save has no carcasses: nothing in the world is touched but what the load replaces.
	Domain->ApplySaveDomain(*NewObject<UMOWorldSaveGame>());
	int32 Left = 0;
	for (TActorIterator<AMOCarcassActor> It(World); It; ++It)
	{
		++Left;
	}
	TestEqual(TEXT("a save with none leaves none"), Left, 0);
	return true;
}

#endif
