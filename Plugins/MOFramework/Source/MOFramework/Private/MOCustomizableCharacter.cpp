#include "MOCustomizableCharacter.h"
#include "MOFramework.h"
#include "GroomComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AMOCustomizableCharacter::AMOCustomizableCharacter()
{
	// Create groom components attached to the main mesh
	HairGroom = CreateDefaultSubobject<UGroomComponent>(TEXT("HairGroom"));
	HairGroom->SetupAttachment(GetMesh());

	EyebrowsGroom = CreateDefaultSubobject<UGroomComponent>(TEXT("EyebrowsGroom"));
	EyebrowsGroom->SetupAttachment(GetMesh());

	EyelashesGroom = CreateDefaultSubobject<UGroomComponent>(TEXT("EyelashesGroom"));
	EyelashesGroom->SetupAttachment(GetMesh());

	BeardGroom = CreateDefaultSubobject<UGroomComponent>(TEXT("BeardGroom"));
	BeardGroom->SetupAttachment(GetMesh());

	// Update hand socket names for Genesis 8 / custom skeleton
	// Override in Blueprint if using different socket names
	LeftHandSocketName = TEXT("lHand");
	RightHandSocketName = TEXT("rHand");
}

void AMOCustomizableCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Create dynamic materials for runtime parameter changes
	CreateDynamicMaterials();

	if (bAutoApplyAppearance && CurrentAppearance.IsValid())
	{
		ApplyCurrentAppearance();
	}
}

void AMOCustomizableCharacter::CreateDynamicMaterials()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	const int32 NumMaterials = MeshComp->GetNumMaterials();
	DynamicMaterials.Empty(NumMaterials);

	for (int32 i = 0; i < NumMaterials; ++i)
	{
		UMaterialInterface* BaseMaterial = MeshComp->GetMaterial(i);
		if (BaseMaterial)
		{
			UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			DynamicMaterials.Add(DynMat);
			MeshComp->SetMaterial(i, DynMat);
		}
		else
		{
			DynamicMaterials.Add(nullptr);
		}
	}

	UE_LOG(LogMOFramework, Log, TEXT("AMOCustomizableCharacter: Created %d dynamic material instances"), NumMaterials);
}

void AMOCustomizableCharacter::SetAppearance(const FMOCharacterAppearance& NewAppearance)
{
	CurrentAppearance = NewAppearance;
	ApplyCurrentAppearance();
}

void AMOCustomizableCharacter::ApplyCurrentAppearance()
{
	// Apply all morph targets
	ApplyAllMorphTargets();

	// Apply color parameters to materials
	ApplyColorParameters();

	UE_LOG(LogMOFramework, Log, TEXT("AMOCustomizableCharacter: Applied appearance with %d morph targets"),
		CurrentAppearance.MorphTargets.Num());
}

void AMOCustomizableCharacter::SetMorphTargetValue(FName MorphName, float Value)
{
	// Update stored value
	CurrentAppearance.SetMorphValue(MorphName, Value);

	// Apply to mesh immediately
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetMorphTarget(MorphName, Value);
	}
}

float AMOCustomizableCharacter::GetMorphTargetValue(FName MorphName) const
{
	return CurrentAppearance.GetMorphValue(MorphName);
}

void AMOCustomizableCharacter::ApplyAllMorphTargets()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return;
	}

	// Clear existing morph targets first
	MeshComp->ClearMorphTargets();

	// Apply all stored morphs
	for (const FMOMorphTargetValue& Morph : CurrentAppearance.MorphTargets)
	{
		MeshComp->SetMorphTarget(Morph.MorphName, Morph.Value);
	}
}

TArray<FName> AMOCustomizableCharacter::GetAvailableMorphTargets() const
{
	TArray<FName> MorphNames;

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp || !MeshComp->GetSkeletalMeshAsset())
	{
		return MorphNames;
	}

	// Get morph target names from the skeletal mesh
	const TMap<FName, int32>& MorphTargetMap = MeshComp->GetSkeletalMeshAsset()->GetMorphTargetIndexMap();
	MorphTargetMap.GetKeys(MorphNames);

	return MorphNames;
}

void AMOCustomizableCharacter::SetSkinTone(const FLinearColor& NewSkinTone)
{
	CurrentAppearance.SkinTone = NewSkinTone;
	ApplyColorParameters();
}

void AMOCustomizableCharacter::SetHairColor(const FLinearColor& NewHairColor)
{
	CurrentAppearance.HairColor = NewHairColor;
	ApplyColorParameters();
}

void AMOCustomizableCharacter::SetEyeColor(const FLinearColor& NewEyeColor)
{
	CurrentAppearance.EyeColor = NewEyeColor;
	ApplyColorParameters();
}

void AMOCustomizableCharacter::ApplyColorParameters()
{
	// Apply skin tone to specified material slots (or all if none specified)
	if (SkinMaterialSlots.Num() > 0)
	{
		for (int32 SlotIndex : SkinMaterialSlots)
		{
			if (DynamicMaterials.IsValidIndex(SlotIndex) && DynamicMaterials[SlotIndex])
			{
				DynamicMaterials[SlotIndex]->SetVectorParameterValue(SkinToneParameterName, CurrentAppearance.SkinTone);
			}
		}
	}
	else
	{
		// Apply to all materials if no specific slots defined
		for (UMaterialInstanceDynamic* DynMat : DynamicMaterials)
		{
			if (DynMat)
			{
				DynMat->SetVectorParameterValue(SkinToneParameterName, CurrentAppearance.SkinTone);
			}
		}
	}

	// Apply eye color to specified slots
	for (int32 SlotIndex : EyeMaterialSlots)
	{
		if (DynamicMaterials.IsValidIndex(SlotIndex) && DynamicMaterials[SlotIndex])
		{
			DynamicMaterials[SlotIndex]->SetVectorParameterValue(EyeColorParameterName, CurrentAppearance.EyeColor);
		}
	}

	// Apply hair color to groom materials
	if (HairGroom)
	{
		for (int32 i = 0; i < HairGroom->GetNumMaterials(); ++i)
		{
			UMaterialInterface* HairMat = HairGroom->GetMaterial(i);
			if (HairMat)
			{
				UMaterialInstanceDynamic* DynHairMat = Cast<UMaterialInstanceDynamic>(HairMat);
				if (!DynHairMat)
				{
					DynHairMat = UMaterialInstanceDynamic::Create(HairMat, this);
					HairGroom->SetMaterial(i, DynHairMat);
				}
				DynHairMat->SetVectorParameterValue(HairColorParameterName, CurrentAppearance.HairColor);
			}
		}
	}

	// Apply to beard if present
	if (BeardGroom)
	{
		for (int32 i = 0; i < BeardGroom->GetNumMaterials(); ++i)
		{
			UMaterialInterface* BeardMat = BeardGroom->GetMaterial(i);
			if (BeardMat)
			{
				UMaterialInstanceDynamic* DynBeardMat = Cast<UMaterialInstanceDynamic>(BeardMat);
				if (!DynBeardMat)
				{
					DynBeardMat = UMaterialInstanceDynamic::Create(BeardMat, this);
					BeardGroom->SetMaterial(i, DynBeardMat);
				}
				DynBeardMat->SetVectorParameterValue(HairColorParameterName, CurrentAppearance.HairColor);
			}
		}
	}
}
