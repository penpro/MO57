#include "MOPCGElevationBandsSettings.h"
#include "MOFramework.h"

#include "PCGContext.h"
#include "PCGPoint.h"
#include "PCGComponent.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Helpers/PCGHelpers.h"

#define LOCTEXT_NAMESPACE "MOPCGElevationBands"

// Output pin names
namespace MOPCGElevationPins
{
	const FName Band0 = TEXT("Band0");
	const FName Band1 = TEXT("Band1");
	const FName Band2 = TEXT("Band2");
	const FName Band3 = TEXT("Band3");
	const FName Band4 = TEXT("Band4");
	const FName Band5 = TEXT("Band5");
	const FName Band6 = TEXT("Band6");
	const FName Band7 = TEXT("Band7");

	const FName AllBands[] = { Band0, Band1, Band2, Band3, Band4, Band5, Band6, Band7 };
}

// ============================================================================
// SETTINGS
// ============================================================================

UMOPCGElevationBandsSettings::UMOPCGElevationBandsSettings()
{
#if WITH_EDITOR
	Category = LOCTEXT("Category", "MO");
#endif

	// Add some default bands as examples
	Bands.Add(FMOPCGElevationBand{ TEXT("Lowlands"), -10000.0f, 500.0f, 50.0f, 100.0f });
	Bands.Add(FMOPCGElevationBand{ TEXT("Midlands"), 400.0f, 1500.0f, 100.0f, 100.0f });
	Bands.Add(FMOPCGElevationBand{ TEXT("Highlands"), 1400.0f, 3000.0f, 100.0f, 200.0f });
}

FPCGElementPtr UMOPCGElevationBandsSettings::CreateElement() const
{
	return MakeShared<FMOPCGElevationBandsElement>();
}

#if WITH_EDITOR
FText UMOPCGElevationBandsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Splits input points into elevation bands based on Z height.\n\n"
		"Configure up to 8 bands with min/max heights and falloff distances.\n"
		"Points in falloff zones are probabilistically selected based on density.\n"
		"Overlapping bands allow points to appear in multiple outputs.");
}
#endif

TArray<FPCGPinProperties> UMOPCGElevationBandsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point, true, true);
	return Properties;
}

TArray<FPCGPinProperties> UMOPCGElevationBandsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;

	// Create output pins based on configured bands (up to 8)
	const int32 NumBands = FMath::Min(Bands.Num(), 8);
	for (int32 i = 0; i < NumBands; ++i)
	{
		FName PinName = MOPCGElevationPins::AllBands[i];

		// Use band name if specified, otherwise use default pin name
		FText PinLabel;
		if (!Bands[i].BandName.IsNone())
		{
			PinLabel = FText::FromName(Bands[i].BandName);
		}
		else
		{
			PinLabel = FText::FromName(PinName);
		}

		FPCGPinProperties& Pin = Properties.Emplace_GetRef(PinName, EPCGDataType::Point, false, false);
#if WITH_EDITOR
		Pin.Tooltip = FText::Format(
			LOCTEXT("BandPinTooltip", "Points in elevation range {0} to {1}"),
			Bands[i].MinHeight, Bands[i].MaxHeight);
#endif
	}

	return Properties;
}

// ============================================================================
// ELEMENT
// ============================================================================

bool FMOPCGElevationBandsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMOPCGElevationBandsElement::Execute);

	const UMOPCGElevationBandsSettings* Settings = Context->GetInputSettings<UMOPCGElevationBandsSettings>();
	check(Settings);

	// Validate settings
	const int32 NumBands = FMath::Min(Settings->Bands.Num(), 8);
	if (NumBands == 0)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOPCGElevationBands] No bands configured"));
		return true;
	}

	// Cache sea level offset
	const float SeaLevelZ = Settings->SeaLevelZ;

	// Get input points
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	// Create random stream for density filtering
	FRandomStream RandomStream(Context->GetSeed() + Settings->SeedOffset);

	// NOTE: We use the point's position directly - PCG points should already be in the correct coordinate space
	// The execution source transform is the PCG component's location, NOT a transform to apply to points

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(Input.Data);
		if (!InputPointData)
		{
			continue;
		}

		const TArray<FPCGPoint>& InputPoints = InputPointData->GetPoints();
		if (InputPoints.Num() == 0)
		{
			continue;
		}

		// Create output point data for each band
		TArray<UPCGPointData*> BandOutputs;
		TArray<TArray<FPCGPoint>*> BandPoints;
		BandOutputs.SetNum(NumBands);
		BandPoints.SetNum(NumBands);

		for (int32 BandIdx = 0; BandIdx < NumBands; ++BandIdx)
		{
			UPCGPointData* OutputPointData = NewObject<UPCGPointData>();
			OutputPointData->InitializeFromData(InputPointData);
			BandOutputs[BandIdx] = OutputPointData;
			BandPoints[BandIdx] = &OutputPointData->GetMutablePoints();
			BandPoints[BandIdx]->Reserve(InputPoints.Num() / NumBands); // Rough estimate
		}

		// Process each input point
		for (const FPCGPoint& InputPoint : InputPoints)
		{
			// Calculate height relative to sea level
			const float Height = InputPoint.Transform.GetLocation().Z - SeaLevelZ;

			// Check each band
			for (int32 BandIdx = 0; BandIdx < NumBands; ++BandIdx)
			{
				const FMOPCGElevationBand& Band = Settings->Bands[BandIdx];
				const float Density = Band.GetDensityForHeight(Height);

				if (Density <= 0.0f)
				{
					continue;
				}

				// Density-based filtering
				if (Settings->bUseDensityFiltering && Density < 1.0f)
				{
					if (RandomStream.FRand() >= Density)
					{
						continue;
					}
				}

				// Add point to this band
				BandPoints[BandIdx]->Add(InputPoint);
			}
		}

		// Add outputs to context
		for (int32 BandIdx = 0; BandIdx < NumBands; ++BandIdx)
		{
			FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
			Output.Data = BandOutputs[BandIdx];
			Output.Pin = MOPCGElevationPins::AllBands[BandIdx];
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
