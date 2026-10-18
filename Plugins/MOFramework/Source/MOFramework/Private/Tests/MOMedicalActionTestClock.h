#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "MOMedicalActionTestClock.generated.h"

/** Controlled server clock for timed-action regression tests; no production timer bypass. */
UCLASS()
class AMOMedicalActionTestClock : public AGameStateBase
{
	GENERATED_BODY()
public:
	double Now = 0;
	virtual double GetServerWorldTimeSeconds() const override { return Now; }
};
