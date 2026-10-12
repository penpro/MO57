// Dev verbs for opening the in-game menus without a keyboard (packaged builds and unattended review runs: injected keys do not reach Enhanced Input).
//
//   MO.UI.Open <Inventory|Crafting|Skills|Quest|Building|Possession|Status|InGame>    open (or toggle, for Status) that menu on the local player's UI manager
//   MO.UI.Close                                                                          close the in-game menu and every other open menu
//   MO.UI.Show <widget class path>                                                       add any Widget Blueprint class to the viewport (e.g. /MOFramework/UI/Kit/WBP_KitGallery.WBP_KitGallery_C); MO.UI.Hide removes it
//   MO.UI.Token <name>                                                                   print a design token as the runtime resolves it (colour / number / string)
//   MO.UI.TokensReload                                                                   re-read DT_UITokens
//   MO.UI.Accessibility <colorblind 0|1> <highcontrast 0|1> <reducedmotion 0|1> <textscale 1|1.25|1.5>

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "MOFramework.h"
#include "MOUIManagerComponent.h"
#include "MOUITokens.h"

namespace
{
	UMOUIManagerComponent* LocalUIManager(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? PC->FindComponentByClass<UMOUIManagerComponent>() : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GUIOpenCmd(
		TEXT("MO.UI.Open"),
		TEXT("Dev: open an in-game menu. Usage: MO.UI.Open <Inventory|Crafting|Skills|Quest|Building|Possession|Status|InGame>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UMOUIManagerComponent* UI = LocalUIManager(World);
			if (!UI || Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Open FAILED: needs a local player and a menu name"));
				return;
			}
			const FString& Name = Args[0];
			auto Is = [&Name](const TCHAR* N) { return Name.Equals(N, ESearchCase::IgnoreCase); };
			if (Is(TEXT("Inventory")))       { UI->OpenInventoryMenu(); }
			else if (Is(TEXT("Crafting")))   { UI->OpenCraftingMenu(); }
			else if (Is(TEXT("Skills")))     { UI->OpenSkillsPanel(); }
			else if (Is(TEXT("Quest")))      { UI->OpenQuestLog(); }
			else if (Is(TEXT("Building")))   { UI->OpenBuildingMenu(); }
			else if (Is(TEXT("Possession"))) { UI->OpenPossessionMenu(); }
			else if (Is(TEXT("Status")))     { UI->TogglePlayerStatus(); }
			else if (Is(TEXT("InGame")))     { UI->OpenInGameMenu(); }
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Open FAILED: unknown menu '%s'"), *Name);
				return;
			}
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Open %s"), *Name);
		}));

	FAutoConsoleCommandWithWorldAndArgs GUICloseCmd(
		TEXT("MO.UI.Close"),
		TEXT("Dev: close the in-game menu and every other open menu."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UMOUIManagerComponent* UI = LocalUIManager(World))
			{
				UI->CloseAllMenus();
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Close"));
			}
		}));

	TWeakObjectPtr<UUserWidget> GUIShownWidget;

	FAutoConsoleCommandWithWorldAndArgs GUIShowCmd(
		TEXT("MO.UI.Show"),
		TEXT("Dev: add a widget class to the viewport. Usage: MO.UI.Show /MOFramework/UI/Kit/WBP_KitGallery.WBP_KitGallery_C"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			if (!PC || Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Show FAILED: needs a local player and a widget class path"));
				return;
			}
			UClass* Class = LoadClass<UUserWidget>(nullptr, *Args[0]);
			if (!Class)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Show FAILED: no widget class '%s'"), *Args[0]);
				return;
			}
			if (UUserWidget* Old = GUIShownWidget.Get())
			{
				Old->RemoveFromParent();
			}
			UUserWidget* Widget = CreateWidget<UUserWidget>(PC, Class);
			Widget->AddToViewport(1000);
			GUIShownWidget = Widget;
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Show %s"), *Args[0]);
		}));

	FAutoConsoleCommand GUIHideCmd(
		TEXT("MO.UI.Hide"),
		TEXT("Dev: remove the widget added by MO.UI.Show."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UUserWidget* W = GUIShownWidget.Get())
			{
				W->RemoveFromParent();
			}
			GUIShownWidget.Reset();
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Hide"));
		}));

	FAutoConsoleCommand GUITokenCmd(
		TEXT("MO.UI.Token"),
		TEXT("Dev: print a UI design token as resolved at runtime (with the accessibility swaps). Usage: MO.UI.Token color.accent"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Token FAILED: needs a token name"));
				return;
			}
			const FName Token(*Args[0]);
			if (!UMOUITokens::IsLoaded())
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Token FAILED: DT_UITokens is not loaded"));
				return;
			}
			const FString Raw = UMOUITokens::String(Token);
			const FLinearColor C = UMOUITokens::Color(Token);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Token %s raw='%s' number=%.3f color(sRGB)=%s"), *Args[0], *Raw, UMOUITokens::Number(Token, -1.0f), *C.ToFColor(true).ToHex());
		}));

	FAutoConsoleCommand GUITokensReloadCmd(
		TEXT("MO.UI.TokensReload"),
		TEXT("Dev: re-read the UI token table (after re-importing DT_UITokens)."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			UMOUITokens::Reload();
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI TokensReload loaded=%d"), UMOUITokens::IsLoaded() ? 1 : 0);
		}));

	FAutoConsoleCommand GUIAccessibilityCmd(
		TEXT("MO.UI.Accessibility"),
		TEXT("Dev: set the UI accessibility switches. Usage: MO.UI.Accessibility <colorblind 0|1> <highcontrast 0|1> <reducedmotion 0|1> <textscale 1|1.25|1.5>"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 4)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Accessibility FAILED: needs 4 values"));
				return;
			}
			UMOUITokens::SetAccessibility(FCString::Atoi(*Args[0]) != 0, FCString::Atoi(*Args[1]) != 0, FCString::Atoi(*Args[2]) != 0, FCString::Atof(*Args[3]));
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Accessibility cb=%s hc=%s rm=%s scale=%.2f"), *Args[0], *Args[1], *Args[2], UMOUITokens::GetAccessibility().TextScale);
		}));
}

#endif // !UE_BUILD_SHIPPING
