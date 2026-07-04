#include "MOTravelUtils.h"
#include "MOFramework.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

bool UMOTravelUtils::TravelToGameplayLevel(UObject* WorldContextObject, const FString& LevelPath)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || LevelPath.IsEmpty())
	{
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTravel] TravelToGameplayLevel: no world / empty path"));
		return false;
	}

	switch (World->GetNetMode())
	{
	case NM_Standalone:
		// Single-player: identical to the pre-2026-07 behavior.
		UE_LOG(LogMOFramework, Log, TEXT("[MOTravel] OpenLevel (standalone): %s"), *LevelPath);
		UGameplayStatics::OpenLevel(WorldContextObject, FName(*LevelPath));
		return true;

	case NM_ListenServer:
	case NM_DedicatedServer:
	{
		// Hosting: ServerTravel keeps the server role and pulls connected
		// clients into the new level. "?listen" keeps accepting connections
		// (required in packaged builds; harmless in PIE).
		const FString URL = LevelPath + TEXT("?listen");
		UE_LOG(LogMOFramework, Warning, TEXT("[MOTravel] ServerTravel (%s): %s"),
			World->GetNetMode() == NM_ListenServer ? TEXT("listen server") : TEXT("dedicated"), *URL);
		World->ServerTravel(URL, /*bAbsolute=*/true);
		return true;
	}

	case NM_Client:
	default:
		UE_LOG(LogMOFramework, Error,
			TEXT("[MOTravel] REFUSED client-initiated travel to '%s' — the server drives travel"), *LevelPath);
		return false;
	}
}
