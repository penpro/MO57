#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MOInteractorComponent.generated.h"

USTRUCT(BlueprintType)
struct MOFRAMEWORK_API FMOInteractionTraceConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float TraceDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float TraceRadius = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	bool bTraceComplex = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MO|Interaction")
	float ViewStartForwardOffset = 15.0f;
};

UCLASS(ClassGroup=(MO), meta=(BlueprintSpawnableComponent))
class MOFRAMEWORK_API UMOInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMOInteractorComponent();

	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	bool FindInteractTarget(AActor*& OutTargetActor, FHitResult& OutHitResult) const;

	UFUNCTION(BlueprintCallable, Category="MO|Interaction")
	bool TryInteract();

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerRequestInteract(AActor* TargetActor);

private:
	bool ResolveViewpoint(FVector& OutViewLocation, FRotator& OutViewRotation) const;
	void BuildTrace(const FVector& ViewLocation, const FRotator& ViewRotation, FVector& OutTraceStart, FVector& OutTraceEnd) const;
	bool TraceForHit(const FVector& TraceStart, const FVector& TraceEnd, FHitResult& OutHitResult) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MO|Interaction", meta=(AllowPrivateAccess="true"))
	FMOInteractionTraceConfig TraceConfig;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastTracedActor;
};
