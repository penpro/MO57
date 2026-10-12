// Editor-only helper for authoring scripts: make a content folder that is NOT part of the project loadable by its package path, without enabling the plugin it belongs to.
//
// Why: the engine's Mover Examples plugin ships a Manny crouch animation set (Engine/Plugins/Experimental/MoverExamples/Content/...), but enabling the experimental Mover / ChaosMover plugins for the
// sake of a few animation assets would rebuild the editor and change the project. Package loading only needs the mount point: with "/MoverExamples/" registered the animations, their skeleton and mesh load
// like any other asset, and the retargeted copies written into the project (Content/Characters/Crouch) do not depend on the plugin at all.
//
//   unreal.MOEditorContentUtils.mount_content_path("/MoverExamples/", r"D:\UnrealEngine\UE_5.8\Engine\Plugins\Experimental\MoverExamples\Content")

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MOEditorContentUtils.generated.h"

UCLASS()
class MOFRAMEWORKEDITOR_API UMOEditorContentUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Register RootPath (e.g. "/MoverExamples/") -> DiskContentDir and have the asset registry scan it. Returns false if the directory does not exist. Safe to call twice. */
	UFUNCTION(BlueprintCallable, Category="MO|Editor")
	static bool MountContentPath(const FString& RootPath, const FString& DiskContentDir);
};
