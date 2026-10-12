#include "MOTabBarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "MOCommonButton.h"
#include "MOFramework.h"

int32 UMOTabBarWidget::CycleIndex(int32 Current, int32 Num, int32 Direction)
{
	if (Num <= 0)
	{
		return INDEX_NONE;
	}
	const int32 Step = Direction >= 0 ? 1 : -1;
	return ((Current + Step) % Num + Num) % Num;
}

void UMOTabBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildButtons();
	SelectedTab = -1;
	SelectTab(FMath::Clamp(InitialTab, 0, FMath::Max(0, TabLabels.Num() - 1)));
}

void UMOTabBarWidget::BuildButtons()
{
	Buttons.Reset();
	if (!TabRow)
	{
		return;
	}
	TabRow->ClearChildren();
	TSubclassOf<UMOCommonButton> ButtonClass = TabButtonClass.LoadSynchronous();
	TSubclassOf<UCommonButtonStyle> StyleClass = TabStyle.LoadSynchronous();
	if (!ButtonClass)
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTabBar] no tab button class (%s)"), *TabButtonClass.ToString());
		return;
	}
	for (int32 i = 0; i < TabLabels.Num(); ++i)
	{
		UMOCommonButton* Button = CreateWidget<UMOCommonButton>(this, ButtonClass);
		if (!Button)
		{
			continue;
		}
		if (StyleClass)
		{
			Button->SetStyle(StyleClass);
		}
		Button->SetIsSelectable(true);
		Button->SetIsToggleable(false);   // a selected tab stays selected when clicked again
		Button->SetButtonText(TabLabels[i]);
		Button->OnClicked().AddWeakLambda(this, [this, i]() { HandleTabClicked(i); });
		if (UPanelSlot* PanelSlot = TabRow->AddChild(Button))
		{
			if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
			{
				BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));   // kit: 4 px between tabs
			}
		}
		Buttons.Add(Button);
	}
}

void UMOTabBarWidget::HandleTabClicked(int32 TabIndex)
{
	SelectTab(TabIndex);
}

void UMOTabBarWidget::SelectTab(int32 TabIndex)
{
	if (TabIndex < 0 || TabIndex >= Buttons.Num() || TabIndex == SelectedTab)
	{
		ApplySelection();
		return;
	}
	SelectedTab = TabIndex;
	ApplySelection();

	if (!TargetSwitcherName.IsNone())
	{
		if (UUserWidget* Owner = GetTypedOuter<UUserWidget>())
		{
			if (Owner->WidgetTree)
			{
				if (UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(Owner->WidgetTree->FindWidget(TargetSwitcherName)))
				{
					Switcher->SetActiveWidgetIndex(FMath::Min(TabIndex, Switcher->GetNumWidgets() - 1));
				}
				else
				{
					UE_LOG(LogMOFramework, Warning, TEXT("[MOTabBar] no WidgetSwitcher named '%s' in %s"), *TargetSwitcherName.ToString(), *Owner->GetName());
				}
			}
		}
	}
	OnTabSelected.Broadcast(TabIndex);
}

void UMOTabBarWidget::ApplySelection()
{
	for (int32 i = 0; i < Buttons.Num(); ++i)
	{
		if (UMOCommonButton* Button = Buttons[i])
		{
			Button->SetSelectedState(i == SelectedTab);   // not SetIsSelected(false): a no-op on non-toggleable tabs
		}
	}
}

FReply UMOTabBarWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// gamepad bumpers cycle the tabs (kit: LB / RB)
	const FKey Key = InKeyEvent.GetKey();
	if (Buttons.Num() > 1 && (Key == EKeys::Gamepad_LeftShoulder || Key == EKeys::Gamepad_RightShoulder))
	{
		SelectTab(CycleIndex(SelectedTab, Buttons.Num(), Key == EKeys::Gamepad_RightShoulder ? 1 : -1));
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}
