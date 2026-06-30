// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelEditorMinimal.h"
#include "VoxelMetadata.h"
#include "VoxelMetadataOverrides.h"
#include "VoxelPinValueCustomizationHelper.h"

class FVoxelMetadataOverrideCustomization
	: public FVoxelPropertyTypeCustomizationBase
	, public FVoxelTicker
{
public:
	//~ Begin IPropertyTypeCustomization Interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		MetadataHandle = PropertyHandle->GetChildHandleStatic(FVoxelMetadataOverride, Metadata);
		ValueHandle = PropertyHandle->GetChildHandleStatic(FVoxelMetadataOverride, Value);

		UObject* MetadataObject = nullptr;
		MetadataHandle->GetValue(MetadataObject);
		CachedMetadataObject = MetadataObject;

		RefreshDelegate = FVoxelEditorUtilities::MakeRefreshDelegate(this, CustomizationUtils);
	}

	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override
	{
		if (CachedMetadataObject.IsExplicitlyNull())
		{
			ChildBuilder.AddProperty(MetadataHandle.ToSharedRef())
			.CustomWidget(true)
			.NameContent()
			[
				MetadataHandle->CreatePropertyValueWidget()
			]
			.ValueContent()
			[
				SNew(SVoxelDetailText)
				.Text(INVTEXT("No metadata selected"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];
			return;
		}

		Wrapper = FVoxelPinValueCustomizationHelper::CreatePinValueCustomization(
			ValueHandle.ToSharedRef(),
			ChildBuilder,
			FVoxelEditorUtilities::MakeRefreshDelegate(this, CustomizationUtils),
			{},
			[&](FDetailWidgetRow& Row, const TSharedRef<SWidget>& ValueWidget)
			{
				const TSharedPtr<IPropertyHandle> TypeHandle = ValueHandle->GetChildHandle("Type");
				const FVoxelPinType Type = FVoxelEditorUtilities::GetStructPropertyValue<FVoxelPinType>(TypeHandle);
				const float Width = FVoxelPinValueCustomizationHelper::GetValueWidgetWidthByType(PropertyHandle, Type);

				Row
				.NameContent()
				[
					MetadataHandle->CreatePropertyValueWidget()
				]
				.ValueContent()
				.MinDesiredWidth(Width)
				.MaxDesiredWidth(Width)
				[
					ValueWidget
				];
			},
			// Used to load/save expansion state
			FAddPropertyParams().UniqueId("FVoxelMetadataOverrideCustomization"));
	}
	//~ End IPropertyTypeCustomization Interface

	//~ Begin FVoxelTicker Interface
	virtual void Tick() override
	{
		if (!MetadataHandle ||
			MetadataHandle->GetNumPerObjectValues() == 0)
		{
			return;
		}

		UObject* MetadataObject = nullptr;
		MetadataHandle->GetValue(MetadataObject);
		if (MetadataObject == CachedMetadataObject)
		{
			return;
		}

		// The Value type follows the selected metadata - fix it up here, otherwise CreatePinValueCustomization
		// would bail out on an invalid type and the value widget would not be displayed
		if (const UVoxelMetadata* Metadata = Cast<UVoxelMetadata>(MetadataObject))
		{
			if (ensure(ValueHandle))
			{
				const FVoxelPinType NewType = Metadata->GetInnerType().GetExposedType();

				ValueHandle->NotifyPreChange();
				FVoxelEditorUtilities::ForeachData<FVoxelPinValue>(ValueHandle, [&](FVoxelPinValue& Value)
				{
					Value.Fixup(NewType);
				});
				ValueHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
				ValueHandle->NotifyFinishedChangingProperties();
			}
		}

		RefreshDelegate.ExecuteIfBound();
		MetadataHandle = nullptr;
	}
	//~ End FVoxelTicker Interface

private:
	TSharedPtr<FVoxelInstancedStructDetailsWrapper> Wrapper;

	TSharedPtr<IPropertyHandle> MetadataHandle;
	TSharedPtr<IPropertyHandle> ValueHandle;
	TVoxelObjectPtr<UObject> CachedMetadataObject;
	FSimpleDelegate RefreshDelegate;
};

DEFINE_VOXEL_STRUCT_LAYOUT(FVoxelMetadataOverride, FVoxelMetadataOverrideCustomization)