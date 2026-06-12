/**
 * =============================================================================
 * MOMenuWidgetBase.h - Compatibility Layer for UMOMenuWidget
 * =============================================================================
 *
 * This class exists for backward compatibility with existing widgets.
 * New widgets should inherit from UMOMenuWidget directly.
 *
 * All functionality (input handling, focus, back actions) is provided by
 * the UMOMenuWidget base class via CommonUI.
 *
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "MOMenuWidget.h"
#include "MOMenuWidgetBase.generated.h"

/**
 * Compatibility layer for existing menu widgets.
 * New widgets should inherit from UMOMenuWidget directly.
 */
UCLASS(Abstract)
class MOFRAMEWORK_API UMOMenuWidgetBase : public UMOMenuWidget
{
	GENERATED_BODY()

public:
	UMOMenuWidgetBase(const FObjectInitializer& ObjectInitializer);
};
