// Dev verbs for opening the in-game menus without a keyboard (packaged builds and unattended review runs: injected keys do not reach Enhanced Input).
//
//   MO.UI.Open <Inventory|Crafting|Skills|Quest|Building|Possession|Status|InGame>    open (or toggle, for Status) that menu on the local player's UI manager
//   MO.UI.Close                                                                          close the in-game menu and every other open menu

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "MOFramework.h"
#include "MOUIManagerComponent.h"

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
}

#endif // !UE_BUILD_SHIPPING
