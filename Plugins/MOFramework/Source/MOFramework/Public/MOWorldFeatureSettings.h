#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MOWorldFeatureSettings.generated.h"

class UDataTable;

/** Project Settings > MOFramework > World Features: the catalog of self-contained features (FMOWorldFeatureDefinitionRow rows). */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="World Features"))
class MOFRAMEWORK_API UMOWorldFeatureSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("MOFramework"); }
	virtual FName GetSectionName() const override { return TEXT("World Features"); }

	/** DT_WorldFeatures: one FMOWorldFeatureDefinitionRow per feature type. */
	UPROPERTY(EditAnywhere, Config, Category="Database")
	TSoftObjectPtr<UDataTable> FeatureTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/MOFramework/Data/DT_WorldFeatures.DT_WorldFeatures")));
};
