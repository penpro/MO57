#include "MOSaveSlotEntry.h"
#include "MOFramework.h"
#include "MOCommonButton.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

void UMOSaveSlotEntry::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind rename/delete buttons if the BP author added them. These broadcast
	// to the hosting panel (Save/Load) which decides how to confirm and execute.
	//
	// Also apply default labels here. UMOCommonButton auto-sizes to its label
	// text — if the BP author dropped the button in without setting a label,
	// it shrinks to zero width and looks invisible. Setting labels in C++
	// means the BP author can use these buttons out-of-the-box without
	// per-instance text setup. The BP can still override by calling
	// SetButtonText after construction if a different label is desired.
	if (RenameButton)
	{
		RenameButton->SetButtonText(NSLOCTEXT("MOSaveSlot", "RenameButton", "Rename"));
		RenameButton->OnClicked().RemoveAll(this);
		RenameButton->OnClicked().AddUObject(this, &UMOSaveSlotEntry::HandleRenameButtonClicked);
	}
	if (DeleteButton)
	{
		DeleteButton->SetButtonText(NSLOCTEXT("MOSaveSlot", "DeleteButton", "Delete"));
		DeleteButton->OnClicked().RemoveAll(this);
		DeleteButton->OnClicked().AddUObject(this, &UMOSaveSlotEntry::HandleDeleteButtonClicked);
	}
}

void UMOSaveSlotEntry::NativeOnClicked()
{
	Super::NativeOnClicked();
	OnSlotSelected.Broadcast(Metadata.SlotName);
}

void UMOSaveSlotEntry::HandleRenameButtonClicked()
{
	OnRenameRequested.Broadcast(Metadata.SlotName);
}

void UMOSaveSlotEntry::HandleDeleteButtonClicked()
{
	OnDeleteRequested.Broadcast(Metadata.SlotName);
}

void UMOSaveSlotEntry::InitializeFromMetadata(const FMOSaveMetadata& InMetadata)
{
	Metadata = InMetadata;
	RefreshDisplay();
	OnMetadataUpdated(Metadata);
}

void UMOSaveSlotEntry::RefreshDisplay()
{
	if (SaveNameText)
	{
		SaveNameText->SetText(Metadata.DisplayName);
	}

	if (TimestampText)
	{
		// Compact "MM.dd HH:mm" — fits a single line of the slot entry even
		// at narrow widths, drops year/seconds/AM-PM that aren't useful for
		// "when did I save?" scanning.
		//
		// Sticks to %m/%d/%H/%M which FDateTime::ToString DOES support. The
		// old format used %b/%I/%p (strftime codes UE doesn't implement);
		// they fell through to literal "b"/"I"/"p" producing the
		// "b 25, 2026 l:11 p" output we just fixed.
		TimestampText->SetText(FText::FromString(
			Metadata.Timestamp.ToString(TEXT("%m.%d %H:%M"))));
	}

	if (PlayTimeText)
	{
		// Format: "2h 35m" or "35m" if less than an hour
		const int32 TotalMinutes = FMath::FloorToInt(Metadata.PlayTime.GetTotalMinutes());
		const int32 Hours = TotalMinutes / 60;
		const int32 Minutes = TotalMinutes % 60;

		FString PlayTimeStr;
		if (Hours > 0)
		{
			PlayTimeStr = FString::Printf(TEXT("%dh %dm"), Hours, Minutes);
		}
		else
		{
			PlayTimeStr = FString::Printf(TEXT("%dm"), Minutes);
		}
		PlayTimeText->SetText(FText::FromString(PlayTimeStr));
	}

	if (WorldNameText)
	{
		WorldNameText->SetText(FText::FromString(Metadata.WorldName));
	}

	if (CharacterInfoText)
	{
		CharacterInfoText->SetText(FText::FromString(Metadata.CharacterInfo));
	}

	if (AutosaveIndicator)
	{
		AutosaveIndicator->SetVisibility(Metadata.bIsAutosave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Load screenshot from inline PNG data
	if (ScreenshotImage && Metadata.ScreenshotData.Num() > 0)
	{
		// Decode PNG data
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

		if (ImageWrapper.IsValid() && ImageWrapper->SetCompressed(Metadata.ScreenshotData.GetData(), Metadata.ScreenshotData.Num()))
		{
			TArray64<uint8> RawData;
			if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
			{
				const int32 Width = ImageWrapper->GetWidth();
				const int32 Height = ImageWrapper->GetHeight();

				// Create texture from raw data (store in UPROPERTY to prevent GC)
				CachedScreenshotTexture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
				if (CachedScreenshotTexture)
				{
					// Lock and copy data
					FTexture2DMipMap& Mip = CachedScreenshotTexture->GetPlatformData()->Mips[0];
					void* TextureData = Mip.BulkData.Lock(LOCK_READ_WRITE);
					FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
					Mip.BulkData.Unlock();

					// Update resource
					CachedScreenshotTexture->UpdateResource();

					// Apply to image widget
					ScreenshotImage->SetBrushFromTexture(CachedScreenshotTexture);
					ScreenshotImage->SetVisibility(ESlateVisibility::Visible);
				}
			}
		}
	}
	else if (ScreenshotImage)
	{
		// No screenshot data - hide the image or show placeholder
		CachedScreenshotTexture = nullptr;
		ScreenshotImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}
