// Dev verbs for opening the in-game menus without a keyboard (packaged builds and unattended review runs: injected keys do not reach Enhanced Input).
//
//   MO.UI.Open <Inventory|Crafting|Skills|Quest|Building|Possession|Status|InGame>    open (or toggle, for Status) that menu on the local player's UI manager
//   MO.UI.Close                                                                          close the in-game menu and every other open menu
//   MO.UI.Show <widget class path>                                                       add any Widget Blueprint class to the viewport (e.g. /MOFramework/UI/Kit/WBP_KitGallery.WBP_KitGallery_C); MO.UI.Hide removes it
//   MO.UI.Focus <widget name>                                                            give a live widget keyboard focus (shows the button focus ring without a foreground window) and print its screen rect
//   MO.UI.Notify <Info|Success|Warning|Error|Pickup|Skill> <text...>                     queue a notification on the local player (to look at the notification widget)
//   MO.UI.SkipIntro                                                                      on the menu map: skip the intro video (the any-key handler's path) so the main menu shows
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
#include "MOMainMenuPlayerController.h"
#include "MONotificationComponent.h"
#include "MOPlayerController.h"
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

	FAutoConsoleCommandWithWorldAndArgs GUISkipIntroCmd(
		TEXT("MO.UI.SkipIntro"),
		TEXT("Dev: skip the intro video on the menu map (what any key does)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AMOMainMenuPlayerController* PC = World ? Cast<AMOMainMenuPlayerController>(World->GetFirstPlayerController()) : nullptr)
			{
				PC->SkipIntroVideo();
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI SkipIntro"));
			}
			else
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI SkipIntro FAILED: not on the menu map"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GUIFocusCmd(
		TEXT("MO.UI.Focus"),
		TEXT("Dev: give keyboard / gamepad focus to a live widget by name (shows its focus ring) and print its screen rect. Usage: MO.UI.Focus NewGameButton"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			UWidget* Target = nullptr;
			if (PC && Args.Num() >= 1)
			{
				for (TObjectIterator<UWidget> It; It && !Target; ++It)
				{
					UWidget* W = *It;
					if (IsValid(W) && W->GetWorld() == World && W->GetCachedWidget().IsValid() && W->IsVisible() && W->GetName().Equals(Args[0], ESearchCase::IgnoreCase))
					{
						Target = W;
					}
				}
			}
			if (!Target)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Focus FAILED: no live widget named '%s'"), Args.Num() ? *Args[0] : TEXT(""));
				return;
			}
			Target->SetFocus();
			const FGeometry& Geo = Target->GetCachedGeometry();
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Focus %s: abs pos %s size %s"), *Target->GetName(), *Geo.GetAbsolutePosition().ToString(), *Geo.GetAbsoluteSize().ToString());
		}));

	FAutoConsoleCommandWithWorldAndArgs GUINotifyCmd(
		TEXT("MO.UI.Notify"),
		TEXT("Dev: queue a notification. Usage: MO.UI.Notify <Info|Success|Warning|Error|Pickup|Skill> <text...>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			UMONotificationComponent* Notifier = PC ? PC->FindComponentByClass<UMONotificationComponent>() : nullptr;
			if (!Notifier || Args.Num() < 2)
			{
				UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Notify FAILED: needs a local player, a type and a text"));
				return;
			}
			const FString& Kind = Args[0];
			EMONotificationType Type = EMONotificationType::Info;
			if (Kind.Equals(TEXT("Success"), ESearchCase::IgnoreCase))      { Type = EMONotificationType::Success; }
			else if (Kind.Equals(TEXT("Warning"), ESearchCase::IgnoreCase)) { Type = EMONotificationType::Warning; }
			else if (Kind.Equals(TEXT("Error"), ESearchCase::IgnoreCase))   { Type = EMONotificationType::Error; }
			else if (Kind.Equals(TEXT("Pickup"), ESearchCase::IgnoreCase))  { Type = EMONotificationType::ItemPickup; }
			else if (Kind.Equals(TEXT("Skill"), ESearchCase::IgnoreCase))   { Type = EMONotificationType::SkillGain; }
			FString Text;
			for (int32 i = 1; i < Args.Num(); ++i)
			{
				Text += (i > 1 ? TEXT(" ") : TEXT("")) + Args[i];
			}
			Notifier->ShowNotification(FText::FromString(Text), 6.0f, Type);
			UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] UI Notify %s '%s'"), *Kind, *Text);
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
