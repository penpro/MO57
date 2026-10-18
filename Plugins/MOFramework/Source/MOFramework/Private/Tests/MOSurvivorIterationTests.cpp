#include "Misc/AutomationTest.h"
#include "MOSurvivorJobQueueComponent.h"
#include "MOSurvivorIterationProbe.h"
#include "GameFramework/Actor.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace MOSurvivorIterationTests
{
	UMOSurvivorJobQueueComponent* MakeQueue()
	{
		return NewObject<UMOSurvivorJobQueueComponent>(NewObject<AActor>());
	}
	FMOSurvivorJobQueueSaveData RoundTrip(FMOSurvivorJobQueueSaveData Data)
	{
		TArray<uint8> Bytes;
		{
			FMemoryWriter Memory(Bytes); FObjectAndNameAsStringProxyArchive Archive(Memory, false);
			FMOSurvivorJobQueueSaveData::StaticStruct()->SerializeItem(Archive, &Data, nullptr);
		}
		FMOSurvivorJobQueueSaveData Loaded;
		FMemoryReader Memory(Bytes); FObjectAndNameAsStringProxyArchive Archive(Memory, true);
		FMOSurvivorJobQueueSaveData::StaticStruct()->SerializeItem(Archive, &Loaded, nullptr);
		return Loaded;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSurvivorIterationPersistence, "MOFramework.Survivor.Iteration.Persistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOSurvivorIterationPersistence::RunTest(const FString&)
{
	using namespace MOSurvivorIterationTests;
	auto* Source = MakeQueue();
	const auto Id = Source->EnqueueJob(EMOSurvivorJobType::ForageNearby, 3);
	auto* Property = FindFProperty<FStructProperty>(Source->GetClass(), TEXT("JobQueue"));
	if (!TestNotNull(TEXT("Live replicated job list property"), Property)) return false;
	auto* List = Property->ContainerPtrToValuePtr<FMOSurvivorJobList>(Source);
	const int32 InitialKey = List->Jobs[0].ReplicationKey;
	Source->SetJobState(Id, EMOSurvivorJobState::Active);
	TestTrue(TEXT("Changing a surviving job state dirties that item"), List->Jobs[0].ReplicationKey != InitialKey);
	const int32 StateKey = List->Jobs[0].ReplicationKey;
	Source->UpdateJobProgress(Id, 0.5f);
	TestTrue(TEXT("Changing a surviving job progress dirties that item"), List->Jobs[0].ReplicationKey != StateKey);
	const int32 KeyBefore = List->Jobs[0].ReplicationKey;
	const auto First = Source->RecordCompletedIteration(Id);
	TestTrue(TEXT("The owning component acknowledges one completed interval"), First.IsRecorded());
	TestEqual(TEXT("The owning entry advances rather than only the controller copy"), Source->GetCurrentJob().CompletedCount, 1);
	TestTrue(TEXT("The surviving job is individually dirtied for client replication"), List->Jobs[0].ReplicationKey != KeyBefore);
	FMOSurvivorJobQueueSaveData Save; Source->BuildSaveData(Save);
	auto* Loaded = MakeQueue();
	TestTrue(TEXT("The real save path restores an assignment between repetitions"), Loaded->ApplySaveDataAuthority(RoundTrip(Save)));
	TestEqual(TEXT("The job keeps its stable identity"), Loaded->GetCurrentJob().JobId, Id);
	TestEqual(TEXT("The completed repetition survives reflected serialization"), Loaded->GetCurrentJob().CompletedCount, 1);
	TestEqual(TEXT("The next actual iteration advances from the saved count"), Loaded->RecordCompletedIteration(Id).CompletedCount, 2);
	TestEqual(TEXT("The last actual iteration reaches the exact requested count"), Loaded->RecordCompletedIteration(Id).CompletedCount, 3);
	TestEqual(TEXT("Extra acknowledgements cannot exceed the requested count"), Loaded->RecordCompletedIteration(Id).Failure, EMOSurvivorIterationFailure::InvalidState);
	TestEqual(TEXT("Recording leaves removal to the executor's cleanup"), Loaded->GetJobCount(), 1);
	Loaded->CompleteCurrentJob(true);
	TestEqual(TEXT("Executor cleanup removes the completed assignment once"), Loaded->GetJobCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSurvivorIterationIdentity, "MOFramework.Survivor.Iteration.AuthorityAndIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOSurvivorIterationIdentity::RunTest(const FString&)
{
	using namespace MOSurvivorIterationTests;
	auto* Queue = MakeQueue();
	const auto First = Queue->EnqueueJob(EMOSurvivorJobType::ForageNearby, 2);
	const auto Next = Queue->EnqueueJob(EMOSurvivorJobType::DigForSupplies, 2);
	TestEqual(TEXT("A queued successor cannot receive the current work acknowledgement"), Queue->RecordCompletedIteration(Next).Failure, EMOSurvivorIterationFailure::WrongJob);
	Queue->GetOwner()->SetRole(ROLE_AutonomousProxy);
	TestEqual(TEXT("Clients cannot advance persisted job counts"), Queue->RecordCompletedIteration(First).Failure, EMOSurvivorIterationFailure::NotAuthority);
	Queue->GetOwner()->SetRole(ROLE_Authority);
	TestEqual(TEXT("Refusals preserve the live count"), Queue->GetCurrentJob().CompletedCount, 0);
	Queue->CompleteCurrentIteration();
	TestEqual(TEXT("The compatibility completion route uses the same owning counter"), Queue->GetCurrentJob().CompletedCount, 1);
	Queue->CompleteCurrentIteration();
	TestEqual(TEXT("Compatibility completion removes only its finished assignment"), Queue->GetCurrentJob().JobId, Next);
	TestEqual(TEXT("The successor's count remains untouched"), Queue->GetCurrentJob().CompletedCount, 0);
	FMOSurvivorJobQueueSaveData Save; Queue->BuildSaveData(Save);
	Save.Jobs[0].RepeatCount = MAX_int32; Save.Jobs[0].CompletedCount = MAX_int32 - 1;
	Queue->ApplySaveDataAuthority(Save);
	TestEqual(TEXT("The largest representable requested count completes without overflow"), Queue->RecordCompletedIteration(Next).CompletedCount, MAX_int32);
	TestEqual(TEXT("An overflow acknowledgement is refused"), Queue->RecordCompletedIteration(Next).Failure, EMOSurvivorIterationFailure::InvalidState);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMOSurvivorIterationReentrancy, "MOFramework.Survivor.Iteration.ReentrantCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMOSurvivorIterationReentrancy::RunTest(const FString&)
{
	using namespace MOSurvivorIterationTests;
	auto* Queue = MakeQueue();
	const auto First = Queue->EnqueueJob(EMOSurvivorJobType::ForageNearby, 2);
	const auto Next = Queue->EnqueueJob(EMOSurvivorJobType::DigForSupplies, 1);
	auto* Probe = NewObject<UMOSurvivorIterationProbe>();
	Probe->Queue = Queue; Probe->JobId = First;
	Queue->OnQueueChanged.AddDynamic(Probe, &UMOSurvivorIterationProbe::QueueChanged);
	Queue->CompleteCurrentIteration();
	TestTrue(TEXT("An observer runs after the iteration is installed"), Probe->bFired);
	TestEqual(TEXT("Recursive acknowledgement cannot count an extra interval"), Probe->NestedFailure, EMOSurvivorIterationFailure::InvalidState);
	TestEqual(TEXT("Cancelling during notification cannot complete the next job"), Queue->GetCurrentJob().JobId, Next);
	TestEqual(TEXT("The successor did not receive an acknowledgement"), Queue->GetCurrentJob().CompletedCount, 0);
	return true;
}
#endif
