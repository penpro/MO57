// Dev verbs for carried light and exposure. Output lines start with [MOQUERY] LIGHT.
//
//   MO.Light.Carried                     the local pawn's carried flame: what burns, in which hand, its intensity now (cd), the item's durability
//   MO.Light.Carried give                a Torch01 into the pack and into the right hand (the equip swap takes a moment)
//   MO.Light.Carried light               light what is in the hand without a fire (tests)
//   MO.Light.Carried out                 put it out
//   MO.Light.Exposure                    every exposure setting that can apply to the view: post process volumes and components (enabled, unbound, priority,
//                                        weight, and their auto exposure overrides), the camera's own, the sky actor's exposure / interior properties (and its
//                                        player-occlusion numbers now, the sky light's intensity now), and the renderer defaults -- what decides whether a torch-lit
//                                        cave reads or is black, and whether a forest is taken for a cave

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Camera/CameraComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "MOCarriedLightComponent.h"
#include "MOEyeAdaptationComponent.h"
#include "MOCharacter.h"
#include "MOEquipmentComponent.h"
#include "MOFramework.h"
#include "MOInventoryComponent.h"
#include "MOLightSourceSettings.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace MOLightCheats
{
	AMOCharacter* LocalPawn(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<AMOCharacter>(PC->GetPawn()) : nullptr;
	}

	FString ExposureOf(const FPostProcessSettings& S)
	{
		TArray<FString> Parts;
		if (S.bOverride_AutoExposureMethod)
		{
			Parts.Add(FString::Printf(TEXT("method=%s"), *UEnum::GetValueAsString(S.AutoExposureMethod.GetValue())));
		}
		if (S.bOverride_AutoExposureBias)
		{
			Parts.Add(FString::Printf(TEXT("bias=%.2f"), S.AutoExposureBias));
		}
		if (S.bOverride_AutoExposureMinBrightness)
		{
			Parts.Add(FString::Printf(TEXT("minEV=%.2f"), S.AutoExposureMinBrightness));
		}
		if (S.bOverride_AutoExposureMaxBrightness)
		{
			Parts.Add(FString::Printf(TEXT("maxEV=%.2f"), S.AutoExposureMaxBrightness));
		}
		if (S.bOverride_AutoExposureSpeedUp)
		{
			Parts.Add(FString::Printf(TEXT("speedUp=%.2f"), S.AutoExposureSpeedUp));
		}
		if (S.bOverride_AutoExposureSpeedDown)
		{
			Parts.Add(FString::Printf(TEXT("speedDown=%.2f"), S.AutoExposureSpeedDown));
		}
		if (S.bOverride_AutoExposureApplyPhysicalCameraExposure)
		{
			Parts.Add(FString::Printf(TEXT("physicalCamera=%d"), S.AutoExposureApplyPhysicalCameraExposure ? 1 : 0));
		}
		if (S.bOverride_AutoExposureBiasCurve)
		{
			Parts.Add(FString::Printf(TEXT("biasCurve=%s"), *GetNameSafe(S.AutoExposureBiasCurve)));
		}
		if (S.bOverride_LocalExposureShadowContrastScale)
		{
			Parts.Add(FString::Printf(TEXT("localShadowContrast=%.2f"), S.LocalExposureShadowContrastScale));
		}
		return Parts.Num() ? FString::Join(Parts, TEXT(" ")) : TEXT("(no exposure overrides)");
	}

	/** Every property of an object whose name mentions exposure, interior or occlusion, as text (the sky actor's settings are Blueprint variables). */
	void LogMatchingProperties(UObject* Object)
	{
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			const FString Name = It->GetName();
			if (!(Name.Contains(TEXT("Exposure")) || Name.Contains(TEXT("Interior")) || Name.Contains(TEXT("Occlusion"))))
			{
				continue;
			}
			if (It->IsA<FDelegateProperty>() || It->IsA<FMulticastDelegateProperty>())
			{
				continue;
			}
			FString Value;
			It->ExportTextItem_Direct(Value, It->ContainerPtrToValuePtr<void>(Object), nullptr, Object, PPF_None);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT   %s.%s = %s"), *Object->GetClass()->GetName(), *Name, *Value.Left(200));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs GCarriedCmd(
		TEXT("MO.Light.Carried"),
		TEXT("Dev: the local pawn's carried flame. Usage: MO.Light.Carried [give|light|out]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AMOCharacter* Pawn = LocalPawn(World);
			UMOCarriedLightComponent* Carried = Pawn ? Pawn->FindComponentByClass<UMOCarriedLightComponent>() : nullptr;
			UMOEquipmentComponent* Equipment = Pawn ? Pawn->FindComponentByClass<UMOEquipmentComponent>() : nullptr;
			UMOInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UMOInventoryComponent>() : nullptr;
			if (!Carried || !Equipment || !Inventory)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT Carried FAILED: no local MO pawn"));
				return;
			}
			const FString Verb = Args.Num() > 0 ? Args[0].ToLower() : FString();
			if (!Verb.IsEmpty() && !Pawn->HasAuthority())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT Carried %s FAILED: server only (a client lights its torch at a fire: MO.Fire.Do LightCarried)"), *Verb);
				return;
			}
			if (Verb == TEXT("give"))
			{
				const FName TorchId(TEXT("Torch01"));
				const FGuid Guid = FGuid::NewGuid();
				const bool bAdded = Inventory->AddItemByGuidWithDurability(Guid, TorchId, 1, -1);
				const bool bEquipping = bAdded && Equipment->EquipFromInventory(Inventory, Guid, EMOEquipmentSlot::RightHand);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT Carried give: added %d, equipping %d"), bAdded ? 1 : 0, bEquipping ? 1 : 0);
				return;
			}
			if (Verb == TEXT("light"))
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT Carried light: %d"), Carried->LightHeld() ? 1 : 0);
				return;
			}
			if (Verb == TEXT("out"))
			{
				Carried->Extinguish(TEXT("dev"));
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT Carried out"));
				return;
			}
			for (const EMOEquipmentSlot Slot : { EMOEquipmentSlot::RightHand, EMOEquipmentSlot::LeftHand })
			{
				const FMOEquippedItem Item = Equipment->GetEquippedItem(Slot);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT Carried hand %s: %s durability %d light source %d"), Slot == EMOEquipmentSlot::RightHand ? TEXT("right") : TEXT("left"),
					*Item.ItemDefinitionId.ToString(), Item.CurrentDurability, UMOLightSourceSettings::Find(Item.ItemDefinitionId) ? 1 : 0);
			}
			const UMOEyeAdaptationComponent* Eye = Pawn->FindComponentByClass<UMOEyeAdaptationComponent>();
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT Carried lit=%d item=%s intensity=%.2f cd; eye: %.3f lux at the camera, exposure floor %.2f EV100"),
				Carried->IsLit() ? 1 : 0, *Carried->GetLitItemId().ToString(), Carried->GetCurrentIntensityCd(), Eye ? Eye->GetLastIlluminanceLux() : -1.0f,
				Eye ? Eye->GetCurrentFloorEV100() : -100.0f);
		}));

	FAutoConsoleCommandWithWorldAndArgs GExposureCmd(
		TEXT("MO.Light.Exposure"),
		TEXT("Dev: every exposure setting that can apply to the view (volumes, post process components, the camera, the sky actor, renderer defaults)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World)
			{
				return;
			}
			for (const TCHAR* Var : { TEXT("r.DefaultFeature.AutoExposure"), TEXT("r.DefaultFeature.AutoExposure.Method"), TEXT("r.DefaultFeature.AutoExposure.Bias"),
				TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"), TEXT("r.EyeAdaptationQuality"), TEXT("r.EyeAdaptation.MethodOverride") })
			{
				const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Var);
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT cvar %s = %s"), Var, CVar ? *CVar->GetString() : TEXT("(none)"));
			}
			for (TActorIterator<APostProcessVolume> It(World); It; ++It)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT volume %s enabled=%d unbound=%d priority=%.1f weight=%.2f: %s"), *It->GetName(), It->bEnabled ? 1 : 0,
					It->bUnbound ? 1 : 0, It->Priority, It->BlendWeight, *ExposureOf(It->Settings));
			}
			for (TObjectIterator<UPostProcessComponent> It; It; ++It)
			{
				if (It->GetWorld() != World)
				{
					continue;
				}
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT component %s.%s enabled=%d unbound=%d priority=%.1f weight=%.2f: %s"), *GetNameSafe(It->GetOwner()), *It->GetName(),
					It->bEnabled ? 1 : 0, It->bUnbound ? 1 : 0, It->Priority, It->BlendWeight, *ExposureOf(It->Settings));
			}
			if (const AMOCharacter* Pawn = LocalPawn(World))
			{
				TArray<UCameraComponent*> Cameras;
				Pawn->GetComponents(Cameras);
				for (const UCameraComponent* Camera : Cameras)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT camera %s weight=%.2f: %s"), *Camera->GetName(), Camera->PostProcessBlendWeight, *ExposureOf(Camera->PostProcessSettings));
				}
			}
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->GetClass()->GetName().Contains(TEXT("Ultra_Dynamic_Sky")))
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT sky %s"), *It->GetName());
					LogMatchingProperties(*It);
					// What the sky does NOW: its player-occlusion component's live numbers (how enclosed the view reads: a cave, or a forest taken for one,
					// Wes's "BLACK" rainforest, Oct 23) and the sky light's current intensity, which the interior adjustment scales.
					TArray<UActorComponent*> Components;
					It->GetComponents(Components);
					for (UActorComponent* Component : Components)
					{
						if (Component->GetClass()->GetName().Contains(TEXT("Occlusion")))
						{
							for (TFieldIterator<FProperty> Prop(Component->GetClass()); Prop; ++Prop)
							{
								if (Prop->IsA<FFloatProperty>() || Prop->IsA<FDoubleProperty>() || Prop->IsA<FBoolProperty>() || Prop->IsA<FIntProperty>())
								{
									FString Value;
									Prop->ExportTextItem_Direct(Value, Prop->ContainerPtrToValuePtr<void>(Component), nullptr, Component, PPF_None);
									UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT   %s.%s = %s"), *Component->GetClass()->GetName(), *Prop->GetName(), *Value.Left(80));
								}
							}
						}
					}
				}
			}
			for (TObjectIterator<USkyLightComponent> Sky; Sky; ++Sky)
			{
				if (Sky->GetWorld() == World)
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] LIGHT sky light %s.%s intensity %.3f"), *GetNameSafe(Sky->GetOwner()), *Sky->GetName(), Sky->Intensity);
				}
			}
		}));
}

#endif // !UE_BUILD_SHIPPING
