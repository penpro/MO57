#include "MOCarriedLightComponent.h"
#include "MOLightSourceSettings.h"
#include "MOCharacter.h"
#include "MOCharacterMovementComponent.h"
#include "MOGameClockSubsystem.h"
#include "MOItemDatabaseSettings.h"
#include "MOPlayerController.h"
#include "MONotificationWidget.h"
#include "MOFramework.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

#define LOCTEXT_NAMESPACE "MOCarriedLight"

UMOCarriedLightComponent::UMOCarriedLightComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;   // ticks only while something burns
	SetIsReplicatedByDefault(true);
}

void UMOCarriedLightComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMOCarriedLightComponent, LitItemGuid);
	DOREPLIFETIME(UMOCarriedLightComponent, LitSlot);
	DOREPLIFETIME(UMOCarriedLightComponent, LitItemId);
}

void UMOCarriedLightComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UMOEquipmentComponent* Equipment = GetEquipment())
	{
		Equipment->OnEquipmentChanged.RemoveDynamic(this, &UMOCarriedLightComponent::HandleEquipmentChanged);
		Equipment->OnEquipmentChanged.AddDynamic(this, &UMOCarriedLightComponent::HandleEquipmentChanged);
	}
	RefreshFlame();
}

void UMOCarriedLightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMOEquipmentComponent* Equipment = GetEquipment())
	{
		Equipment->OnEquipmentChanged.RemoveDynamic(this, &UMOCarriedLightComponent::HandleEquipmentChanged);
	}
	DestroyFlame();
	Super::EndPlay(EndPlayReason);
}

UMOEquipmentComponent* UMOCarriedLightComponent::GetEquipment() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UMOEquipmentComponent>() : nullptr;
}

UStaticMeshComponent* UMOCarriedLightComponent::GetHeldMesh(EMOEquipmentSlot Slot) const
{
	const AMOCharacter* Character = Cast<AMOCharacter>(GetOwner());
	if (!Character)
	{
		return nullptr;
	}
	return Slot == EMOEquipmentSlot::LeftHand ? Character->LeftHandMesh.Get() : Slot == EMOEquipmentSlot::RightHand ? Character->RightHandMesh.Get() : nullptr;
}

const FMOLightSourceProfile* UMOCarriedLightComponent::GetLitProfile() const
{
	return IsLit() ? UMOLightSourceSettings::Find(LitItemId) : nullptr;
}

FVector UMOCarriedLightComponent::GetLightLocation() const
{
	return FlameLight ? FlameLight->GetComponentLocation() : (GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector);
}

FName UMOCarriedLightComponent::GetLitItemId() const
{
	return IsLit() ? LitItemId : NAME_None;
}

bool UMOCarriedLightComponent::FindUnlitHeld(EMOEquipmentSlot& OutSlot, FMOEquippedItem& OutItem) const
{
	const UMOEquipmentComponent* Equipment = GetEquipment();
	if (!Equipment)
	{
		return false;
	}
	for (const EMOEquipmentSlot Slot : { EMOEquipmentSlot::RightHand, EMOEquipmentSlot::LeftHand })
	{
		const FMOEquippedItem Item = Equipment->GetEquippedItem(Slot);
		if (!Item.IsValid() || Item.CurrentDurability == 0 || !UMOLightSourceSettings::Find(Item.ItemDefinitionId))
		{
			continue;   // empty hand, not a light source, or burnt out (-1 = never burns down)
		}
		if (IsLit() && Item.ItemGuid == LitItemGuid)
		{
			continue;   // already burning
		}
		OutSlot = Slot;
		OutItem = Item;
		return true;
	}
	return false;
}

bool UMOCarriedLightComponent::LightHeld()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}
	EMOEquipmentSlot Slot = EMOEquipmentSlot::RightHand;
	FMOEquippedItem Item;
	if (!FindUnlitHeld(Slot, Item))
	{
		return false;
	}
	LitItemGuid = Item.ItemGuid;
	LitItemId = Item.ItemDefinitionId;
	LitSlot = Slot;
	BurnAccumulator = 0.0f;
	UE_LOG(LogMOFramework, Log, TEXT("[MOLight] %s lit %s (%s hand, durability %d)"), *GetOwner()->GetName(), *LitItemId.ToString(),
		Slot == EMOEquipmentSlot::LeftHand ? TEXT("left") : TEXT("right"), Item.CurrentDurability);
	RefreshFlame();
	return true;
}

void UMOCarriedLightComponent::Extinguish(const TCHAR* Why, bool bTellOwner)
{
	if (!IsLit() || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const FName WasItem = LitItemId;
	UE_LOG(LogMOFramework, Log, TEXT("[MOLight] %s: %s went out (%s)"), *GetOwner()->GetName(), *WasItem.ToString(), Why);
	LitItemGuid.Invalidate();
	LitItemId = NAME_None;
	BurnAccumulator = 0.0f;
	RefreshFlame();
	if (bTellOwner)
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		if (AMOPlayerController* PC = Pawn ? Cast<AMOPlayerController>(Pawn->GetController()) : nullptr)
		{
			PC->ClientShowNotification(FText::Format(LOCTEXT("WentOut", "The {0} goes out ({1})."), UMOItemDatabaseSettings::GetItemDisplayName(WasItem),
				FText::FromString(Why)), 4.0f, EMONotificationType::Warning);
		}
	}
}

void UMOCarriedLightComponent::OnRep_Lit()
{
	RefreshFlame();
}

void UMOCarriedLightComponent::HandleEquipmentChanged(EMOEquipmentSlot EquipSlot, const FMOEquippedItem& EquippedItem)
{
	if (!IsLit() || EquipSlot != LitSlot)
	{
		return;
	}
	if (EquippedItem.ItemGuid != LitItemGuid)
	{
		// Put away, dropped or swapped: nobody pockets a burning torch -- it is snuffed first.
		Extinguish(TEXT("left the hand"));
		return;
	}
	RefreshFlame();   // same item, the held mesh may have been rebuilt
}

void UMOCarriedLightComponent::RefreshFlame()
{
	const FMOLightSourceProfile* Profile = GetLitProfile();
	const bool bRenders = GetNetMode() != NM_DedicatedServer;
	if (!Profile || !bRenders)
	{
		DestroyFlame();
	}
	else
	{
		PlaceFlame(*Profile);
	}
	SetComponentTickEnabled(IsLit());
}

void UMOCarriedLightComponent::PlaceFlame(const FMOLightSourceProfile& Profile)
{
	UStaticMeshComponent* Held = GetHeldMesh(LitSlot);
	if (!Held)
	{
		DestroyFlame();
		return;
	}
	if (!FlameLight)
	{
		FlameLight = NewObject<UPointLightComponent>(GetOwner(), NAME_None, RF_Transient);
		FlameLight->SetMobility(EComponentMobility::Movable);
		FlameLight->SetIntensityUnits(ELightUnits::Candelas);   // before the intensity: units first, or the value is read in the old units
		FlameLight->SetUseInverseSquaredFalloff(true);
		FlameLight->RegisterComponent();
		if (UMOAmbientEnvironmentRegistry* Registry = UMOAmbientEnvironmentRegistry::Get(this))
		{
			Registry->RegisterLightSource(this);
		}
	}
	FlameLight->SetUseTemperature(true);
	FlameLight->SetTemperature(Profile.ColorTemperatureK);
	FlameLight->SetLightColor(FLinearColor::White);
	FlameLight->SetAttenuationRadius(Profile.AttenuationRadiusCm);
	FlameLight->SetSourceRadius(Profile.SourceRadiusCm);
	FlameLight->SetCastShadows(Profile.bCastShadows);
	if (FlameLight->GetAttachParent() != Held)
	{
		FlameLight->AttachToComponent(Held, FAttachmentTransformRules::KeepRelativeTransform);
	}
	const UStaticMesh* Mesh = Held->GetStaticMesh();
	const FVector FlameAt = Mesh ? FlameLocalPosition(Mesh->GetBoundingBox(), Profile.FlamePositionAlongMesh) : FVector::ZeroVector;
	FlameLight->SetRelativeLocation(FlameAt);
	PlacedOnMesh = Mesh;

	if (!FlameEffect && Profile.FlameEffect.IsValid())
	{
		if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Profile.FlameEffect.TryLoad()))
		{
			FlameEffect = NewObject<UNiagaraComponent>(GetOwner(), NAME_None, RF_Transient);
			FlameEffect->SetAsset(System);
			FlameEffect->SetAutoActivate(false);
			FlameEffect->RegisterComponent();
			FlameEffect->Activate(true);
		}
	}
	if (FlameEffect)
	{
		if (FlameEffect->GetAttachParent() != Held)
		{
			FlameEffect->AttachToComponent(Held, FAttachmentTransformRules::KeepRelativeTransform);
		}
		// Flames rise: keep it upright in the world whatever angle the hand holds the torch at.
		FlameEffect->SetUsingAbsoluteRotation(true);
		FlameEffect->SetWorldRotation(FRotator::ZeroRotator);
		FlameEffect->SetRelativeLocation(FlameAt);
		FlameEffect->SetWorldScale3D(FVector(Profile.FlameEffectScale));
	}
	UpdateFlameIntensity(Profile, 0.0f);
}

void UMOCarriedLightComponent::DestroyFlame()
{
	if (FlameLight)
	{
		FlameLight->DestroyComponent();
		FlameLight = nullptr;
	}
	if (UMOAmbientEnvironmentRegistry* Registry = UMOAmbientEnvironmentRegistry::Get(this))
	{
		Registry->UnregisterLightSource(this);
	}
	if (FlameEffect)
	{
		FlameEffect->DestroyComponent();
		FlameEffect = nullptr;
	}
	PlacedOnMesh.Reset();
	CurrentIntensityCd = 0.0f;
}

void UMOCarriedLightComponent::UpdateFlameIntensity(const FMOLightSourceProfile& Profile, float DeltaTime)
{
	if (!FlameLight)
	{
		return;
	}
	const float Speed = GetOwner() ? GetOwner()->GetVelocity().Size() : 0.0f;
	const float Target = UMOLightSourceSettings::AirflowScale(Profile, Speed);
	// A flame flares up or dies down over about a second, not at once.
	AirflowSmoothed = DeltaTime > 0.0f ? FMath::FInterpTo(AirflowSmoothed, Target, DeltaTime, 2.0f) : Target;
	FlickerTime += DeltaTime;
	// Two incommensurate noises: the slow sway of the flame and the quick dance of its tips.
	const float Noise = 0.7f * FMath::PerlinNoise1D(FlickerTime * Profile.FlickerHz) + 0.3f * FMath::PerlinNoise1D(FlickerTime * Profile.FlickerHz * 3.7f + 17.0f);
	CurrentIntensityCd = FMath::Max(0.0f, Profile.LuminousIntensityCd * AirflowSmoothed * (1.0f + Profile.FlickerAmount * Noise));
	FlameLight->SetIntensity(CurrentIntensityCd);
}

void UMOCarriedLightComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const FMOLightSourceProfile* Profile = GetLitProfile();
	if (!Profile)
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		TickBurnAuthority(*Profile, DeltaTime);
		if (!IsLit())
		{
			return;
		}
	}
	if (FlameLight)
	{
		const UStaticMeshComponent* Held = GetHeldMesh(LitSlot);
		if (Held && Held->GetStaticMesh() != PlacedOnMesh.Get())
		{
			PlaceFlame(*Profile);   // the held mesh was swapped under the flame (the item's visual loaded late)
		}
		UpdateFlameIntensity(*Profile, DeltaTime);
	}
	else if (GetNetMode() != NM_DedicatedServer)
	{
		PlaceFlame(*Profile);   // lit before the held mesh existed (a late join): try again
	}
}

void UMOCarriedLightComponent::TickBurnAuthority(const FMOLightSourceProfile& Profile, float DeltaTime)
{
	// Under water: a flame does not survive a dunking. Swimming takes the hands under; wading with it held up is fine.
	if (const AMOCharacter* Character = Cast<AMOCharacter>(GetOwner()))
	{
		bool bDoused = Character->IsSwimming();
		const UMOCharacterMovementComponent* Move = Cast<UMOCharacterMovementComponent>(Character->GetCharacterMovement());
		float SurfaceZ = 0.0f;
		if (!bDoused && Move && Move->GetImmersion() > 0.0f && Move->GetWaterSurfaceZ(SurfaceZ))
		{
			const FVector Flame = FlameLight ? FlameLight->GetComponentLocation()
				: (GetHeldMesh(LitSlot) ? GetHeldMesh(LitSlot)->GetComponentLocation() : Character->GetActorLocation());
			bDoused = Flame.Z < SurfaceZ;
		}
		if (bDoused)
		{
			Extinguish(TEXT("doused"), true);
			return;
		}
	}

	const UMOGameClockSubsystem* Clock = UMOGameClockSubsystem::Get(this);
	const int32 Points = ConsumeBurn(BurnAccumulator, Clock ? Clock->GetScaledDeltaTime(DeltaTime) : DeltaTime, Profile.BurnSecondsPerDurability);
	if (Points <= 0)
	{
		return;
	}
	UMOEquipmentComponent* Equipment = GetEquipment();
	const FMOEquippedItem Item = Equipment ? Equipment->GetEquippedItem(LitSlot) : FMOEquippedItem();
	if (!Item.IsValid() || Item.ItemGuid != LitItemGuid)
	{
		Extinguish(TEXT("left the hand"));
		return;
	}
	if (Item.CurrentDurability < 0)
	{
		return;   // a light that never burns down (no durability on its row)
	}
	// The burn wears the item itself, so a torch put out half-burnt and relit carries on from where it was; at 0 it is used up (the slot clears). Burning
	// out clears the slot inside the call, which would read as "left the hand": the flame is put out as burnt out first.
	const EMOEquipmentSlot Slot = LitSlot;
	if (Points >= Item.CurrentDurability)
	{
		Extinguish(TEXT("burnt out"), true);
	}
	bool bBurntOut = false;
	if (!Equipment->ReduceEquippedDurability(Slot, Points, bBurntOut) && IsLit())
	{
		BurnAccumulator += Points * FMath::Max(Profile.BurnSecondsPerDurability, 0.001f);   // refused (a hand swap in progress): owe it, try next tick
	}
}

FVector UMOCarriedLightComponent::FlameLocalPosition(const FBox& LocalBounds, float Position)
{
	if (!LocalBounds.IsValid)
	{
		return FVector::ZeroVector;
	}
	const FVector Size = LocalBounds.GetSize();
	const int32 Axis = Size.X >= Size.Y && Size.X >= Size.Z ? 0 : (Size.Y >= Size.Z ? 1 : 2);
	// The grip is the component's origin (the hand socket): the flame is at the end of the long axis farther from it.
	const double Lo = LocalBounds.Min[Axis];
	const double Hi = LocalBounds.Max[Axis];
	const double FarEnd = FMath::Abs(Hi) >= FMath::Abs(Lo) ? Hi : Lo;
	const double NearEnd = FarEnd == Hi ? Lo : Hi;
	FVector Out = LocalBounds.GetCenter();
	Out[Axis] = FMath::Lerp(NearEnd, FarEnd, (double)FMath::Clamp(Position, 0.0f, 1.0f));
	return Out;
}

int32 UMOCarriedLightComponent::ConsumeBurn(float& Accumulator, float GameSeconds, float SecondsPerPoint)
{
	Accumulator += FMath::Max(0.0f, GameSeconds);
	const float Per = FMath::Max(SecondsPerPoint, 0.001f);
	const int32 Points = FMath::FloorToInt(Accumulator / Per);
	Accumulator -= Points * Per;
	return Points;
}

#undef LOCTEXT_NAMESPACE
