#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MOCharacterHistoryComponent.h"
#include "MOIdentifiableInterface.h"
#include "MOIdentityComponent.h"
#include "MOInventoryComponent.h"
#include "MOInventoryHolderInterface.h"
#include "MOKnowledgeComponent.h"
#include "MOPersonalityComponent.h"
#include "MOPersistenceTestPawn.generated.h"

/** The least a pawn needs to go through the save's own capture and apply (MOPersistencePawnStateTests.cpp); the survivor itself brings voxel invokers and a MetaHuman with it. */
UCLASS(NotBlueprintable, NotPlaceable)
class AMOPersistenceTestPawn : public APawn, public IMOIdentifiableInterface, public IMOInventoryHolderInterface
{
	GENERATED_BODY()
public:
	AMOPersistenceTestPawn()
	{
		Identity = CreateDefaultSubobject<UMOIdentityComponent>(TEXT("Identity"));
		Inventory = CreateDefaultSubobject<UMOInventoryComponent>(TEXT("Inventory"));
		Knowledge = CreateDefaultSubobject<UMOKnowledgeComponent>(TEXT("Knowledge"));
		Personality = CreateDefaultSubobject<UMOPersonalityComponent>(TEXT("Personality"));
		History = CreateDefaultSubobject<UMOCharacterHistoryComponent>(TEXT("History"));
	}

	UPROPERTY()
	TObjectPtr<UMOIdentityComponent> Identity;

	UPROPERTY()
	TObjectPtr<UMOInventoryComponent> Inventory;

	UPROPERTY()
	TObjectPtr<UMOKnowledgeComponent> Knowledge;

	UPROPERTY()
	TObjectPtr<UMOPersonalityComponent> Personality;

	UPROPERTY()
	TObjectPtr<UMOCharacterHistoryComponent> History;

	virtual UMOIdentityComponent* GetIdentityComponent_Implementation() const override { return Identity; }
	virtual UMOInventoryComponent* GetInventory_Implementation() const override { return Inventory; }
};
