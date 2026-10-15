#include "MOItemPreviewWidget.h"

#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "MOItemDatabaseSettings.h"
#include "MOItemDefinitionRow.h"
#include "MOItemPreviewStage.h"
#include "MOFramework.h"

namespace
{
	/** Graybox placeholder meshes (items without art yet): a turning gray box tells the player nothing the icon doesn't. */
	bool IsPlaceholderMesh(const FMOItemDefinitionRow& Def)
	{
		return Def.WorldVisual.StaticMesh.IsNull() || Def.WorldVisual.StaticMesh.ToSoftObjectPath().ToString().StartsWith(TEXT("/Game/Penumbra/Graybox/"));
	}

	/** One stage slot per live preview widget so two open previews never share a booth. */
	int32 GNextStageSlot = 0;
}

void UMOItemPreviewWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (CurrentItem.IsNone())
	{
		SetVisibility(ESlateVisibility::Collapsed);   // nothing to show: no empty plate in the panel (SetItem shows it)
	}
}

void UMOItemPreviewWidget::NativeDestruct()
{
	if (Stage)
	{
		Stage->Destroy();
		Stage = nullptr;
	}
	Super::NativeDestruct();
}

AMOItemPreviewStage* UMOItemPreviewWidget::EnsureStage()
{
	if (Stage && IsValid(Stage))
	{
		return Stage;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	Stage = World->SpawnActor<AMOItemPreviewStage>(AMOItemPreviewStage::StageLocation(GNextStageSlot++ % 64), FRotator::ZeroRotator, Params);
	if (Stage)
	{
		Stage->EnsureRenderTarget(Resolution);
	}
	return Stage;
}

bool UMOItemPreviewWidget::SetItem(FName ItemId)
{
	FMOItemDefinitionRow Def;
	const bool bHasDef = !ItemId.IsNone() && UMOItemDatabaseSettings::GetItemDefinition(ItemId, Def);
	if (!bHasDef || (bHidePlaceholderMeshes && IsPlaceholderMesh(Def)))
	{
		ClearItem();
		return false;
	}
	AMOItemPreviewStage* S = EnsureStage();
	if (!S || !S->SetItem(ItemId))
	{
		ClearItem();
		return false;
	}
	if (CurrentItem != ItemId)
	{
		Yaw = 35.0f;
		Pitch = S->GetSuggestedPitch();
	}
	CurrentItem = ItemId;
	S->SetView(Yaw, Pitch);
	S->CaptureNow();
	if (PreviewImage)
	{
		PreviewImage->SetBrushResourceObject(S->GetRenderTarget());
	}
	SetVisibility(ESlateVisibility::Visible);   // Visible, not HitTestInvisible: the drag-to-turn needs the mouse
	return true;
}

void UMOItemPreviewWidget::ClearItem()
{
	CurrentItem = NAME_None;
	if (Stage)
	{
		Stage->ClearItem();
	}
	SetVisibility(ESlateVisibility::Collapsed);   // the whole plate goes: an item without real art shows its icon only
}

void UMOItemPreviewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!Stage || CurrentItem.IsNone() || !Stage->HasItem())
	{
		return;
	}
	if (!bDragging && FPlatformTime::Seconds() >= ResumeSpinAt)
	{
		Yaw = FMath::Fmod(Yaw + SpinDegreesPerSecond * InDeltaTime, 360.0f);
	}
	Stage->SetView(Yaw, Pitch);
	Stage->CaptureNow();   // only while this widget is painted: a hidden panel does not tick
}

FReply UMOItemPreviewWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !CurrentItem.IsNone())
	{
		bDragging = true;
		LastMouse = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UMOItemPreviewWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		ResumeSpinAt = FPlatformTime::Seconds() + 3.0;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UMOItemPreviewWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging)
	{
		const FVector2D Now = InMouseEvent.GetScreenSpacePosition();
		const FVector2D Delta = Now - LastMouse;
		LastMouse = Now;
		Yaw = FMath::Fmod(Yaw - Delta.X * 0.5f, 360.0f);
		Pitch = FMath::Clamp(Pitch - Delta.Y * 0.3f, -60.0f, 30.0f);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UMOItemPreviewWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	// Mouse capture keeps the drag alive outside the widget; nothing to do unless capture was lost.
	if (bDragging && !HasMouseCapture())
	{
		bDragging = false;
		ResumeSpinAt = FPlatformTime::Seconds() + 3.0;
	}
}
