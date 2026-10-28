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
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "MOEditorContentUtils.generated.h"

UCLASS()
class MOFRAMEWORKEDITOR_API UMOEditorContentUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Register RootPath (e.g. "/MoverExamples/") -> DiskContentDir and have the asset registry scan it. Returns false if the directory does not exist. Safe to call twice. */
	UFUNCTION(BlueprintCallable, Category="MO|Editor")
	static bool MountContentPath(const FString& RootPath, const FString& DiskContentDir);

	/**
	 * Create or update a COMPOSITE Font asset at AssetPath (e.g. "/MOFramework/UI/Kit/Fonts/F_IBMPlexSans") whose typefaces are TypefaceNames[i] -> Faces[i] (the first is the default typeface).
	 * Why here: Slate resolves an FSlateFontInfo through IFontProviderInterface, which only UFont implements (a bare UFontFace as the font object falls back to a glyph-less path: tofu), and Python
	 * cannot author the composite structure. Returns the font (not saved: save it like any asset).
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Editor")
	static UFont* MakeCompositeFont(const FString& AssetPath, const TArray<FName>& TypefaceNames, const TArray<UFontFace*>& Faces);

	/**
	 * Make Blueprint implement InterfaceClass (a Blueprint Interface's generated class or a native UInterface), adding a graph for each of its functions
	 * that returns a value; true if it does now (also when it already did). Python cannot do this (the Class Settings "Implemented Interfaces" list);
	 * GASP phase 1 needs it for the survivor to answer BPI_SandboxCharacter_Pawn (Docs/Animation_GASP.md). Not compiled or saved: do both after.
	 */
	UFUNCTION(BlueprintCallable, Category="MO|Editor")
	static bool ImplementInterface(UBlueprint* Blueprint, UClass* InterfaceClass);
};
