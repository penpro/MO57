#include "MOSplashSettings.h"
#include "MOSplashWidget.h"
#include "Engine/Texture2D.h"

UMOSplashSettings::UMOSplashSettings()
{
	// The studio card: the Penumbra Tech logomark (Art/Branding/penumbra-tech-mark.svg, rasterised and imported by Content/Python/splash_import.py) over the studio name.
	// A DefaultGame.ini [/Script/MOFramework.MOSplashSettings] section replaces these.
	FMOSplashCard Studio;
	Studio.Logo = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/MOFramework/UI/Splash/T_Splash_PenumbraMark.T_Splash_PenumbraMark")));
	Studio.Caption = NSLOCTEXT("MOSplash", "Studio", "PENUMBRA TECH");
	Studio.HoldSeconds = 2.6f;
	Studio.LogoSize = 300.0f;
	Cards.Add(Studio);

	WidgetClass = TSoftClassPtr<UMOSplashWidget>(FSoftObjectPath(TEXT("/MOFramework/UI/MainMenu_UI/WBP_SplashSequence.WBP_SplashSequence_C")));
}
