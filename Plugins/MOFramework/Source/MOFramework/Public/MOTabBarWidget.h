// A row of kit tabs (48 px, 4 px accent underline on the selected one) that drives a WidgetSwitcher: the ONE tab abstraction for every tabbed panel (options, journal, status, inventory sides).
//
// Use: put WBP_TabBar (a Widget Blueprint of this class with a HorizontalBox named TabRow) next to a UWidgetSwitcher in your panel; on the instance set TabLabels and TargetSwitcherName (the switcher's
// widget name in the same Widget Blueprint). Tab i shows switcher child i. Gamepad: LB / RB cycle while the bar is on screen. OnTabSelected tells code that wants to react (lazy refresh).
//
// The tab buttons are created here (UMOCommonButton with the kit BS_Tab style), so the tab count is data, not layout.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MOTabBarWidget.generated.h"

class UMOCommonButton;
class UPanelWidget;
class UWidgetSwitcher;
class UCommonButtonStyle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMOTabSelectedSignature, int32, TabIndex);

UCLASS()
class MOFRAMEWORK_API UMOTabBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** One label per tab. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Tabs")
	TArray<FText> TabLabels;

	/** Name of a UWidgetSwitcher in the owning Widget Blueprint; tab i shows its child i. Empty = no switcher (handle OnTabSelected yourself). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Tabs")
	FName TargetSwitcherName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Tabs")
	int32 InitialTab = 0;

	/** Button style class for a tab (default: the kit BS_Tab). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Tabs")
	TSoftClassPtr<UCommonButtonStyle> TabStyle = TSoftClassPtr<UCommonButtonStyle>(FSoftObjectPath(TEXT("/MOFramework/UI/Kit/Styles/BS_Tab.BS_Tab_C")));

	/** The Widget Blueprint used for each tab button (default: WBP_MOCommonButton). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MO|UI|Tabs")
	TSoftClassPtr<UMOCommonButton> TabButtonClass = TSoftClassPtr<UMOCommonButton>(FSoftObjectPath(TEXT("/MOFramework/UI/WBP_MOCommonButton.WBP_MOCommonButton_C")));

	UPROPERTY(BlueprintAssignable, Category = "MO|UI|Tabs")
	FMOTabSelectedSignature OnTabSelected;

	UFUNCTION(BlueprintCallable, Category = "MO|UI|Tabs")
	void SelectTab(int32 TabIndex);

	UFUNCTION(BlueprintPure, Category = "MO|UI|Tabs")
	int32 GetSelectedTab() const { return SelectedTab; }

	/** The next / previous index with wrap-around: pure, tested. */
	static int32 CycleIndex(int32 Current, int32 Num, int32 Direction);

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** Required: the box the tab buttons are created in (a HorizontalBox). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UPanelWidget> TabRow;

private:
	void BuildButtons();
	void ApplySelection();
	void HandleTabClicked(int32 TabIndex);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMOCommonButton>> Buttons;

	int32 SelectedTab = -1;
};
