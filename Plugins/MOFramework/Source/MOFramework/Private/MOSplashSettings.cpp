#include "MOSplashSettings.h"
#include "MOSplashWidget.h"
#include "Engine/Texture2D.h"

UMOSplashSettings::UMOSplashSettings()
{
	// The studio card: the Penumbra corona-eclipse mark (D:\LocalLLM\Branding\penumbra-brand\logos\corona-eclipse.svg, rasterised) over the studio name.
	// A DefaultGame.ini [/Script/MOFramework.MOSplashSettings] section replaces these.
	FMOSplashCard Studio;
	Studio.Logo = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/MOFramework/UI/Splash/T_Splash_PenumbraMark.T_Splash_PenumbraMark")));
	Studio.Caption = NSLOCTEXT("MOSplash", "Studio", "PENUMBRA");
	Studio.HoldSeconds = 2.6f;
	Studio.LogoSize = 300.0f;
	Cards.Add(Studio);

	WidgetClass = TSoftClassPtr<UMOSplashWidget>(FSoftObjectPath(TEXT("/MOFramework/UI/MainMenu_UI/WBP_SplashSequence.WBP_SplashSequence_C")));
}
