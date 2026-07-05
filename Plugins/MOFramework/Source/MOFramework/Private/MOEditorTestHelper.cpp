#include "MOEditorTestHelper.h"
#include "MOFramework.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#endif

namespace
{
	const TCHAR* NetModeToName(ENetMode Mode)
	{
		switch (Mode)
		{
		case NM_Standalone:      return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer:    return TEXT("ListenServer");
		case NM_Client:          return TEXT("Client");
		default:                 return TEXT("Unknown");
		}
	}
}

FString UMOEditorTestHelper::GetPIEWorldsSummary()
{
	FString Out;
	if (!GEngine)
	{
		return TEXT("no GEngine");
	}
	int32 Count = 0;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		UWorld* World = Ctx.World();
		if (!World || Ctx.WorldType != EWorldType::PIE)
		{
			continue;
		}
		int32 NumPCs = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			++NumPCs;
		}
		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		const FString Line = FString::Printf(TEXT("PIE[%d] world=%s netMode=%s PCs=%d pawn=%s"),
			Count, *World->GetMapName(), NetModeToName(World->GetNetMode()), NumPCs,
			Pawn ? *Pawn->GetName() : TEXT("none"));
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] %s"), *Line);
		Out += Line + TEXT("\n");
		++Count;
	}
	if (Count == 0)
	{
		Out = TEXT("no PIE worlds");
		UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] PIE: %s"), *Out);
	}
	return Out;
}

UWorld* UMOEditorTestHelper::FindPIEWorldByNetMode(const FString& NetModeName, int32 ClientIndex)
{
	if (!GEngine)
	{
		return nullptr;
	}
	int32 MatchIndex = 0;
	for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
	{
		UWorld* World = Ctx.World();
		if (!World || Ctx.WorldType != EWorldType::PIE)
		{
			continue;
		}
		if (!NetModeName.Equals(NetModeToName(World->GetNetMode()), ESearchCase::IgnoreCase))
		{
			continue;
		}
		if (MatchIndex == ClientIndex)
		{
			return World;
		}
		++MatchIndex;
	}
	return nullptr;
}

bool UMOEditorTestHelper::ConfigurePIE(int32 NumPlayers, bool bListenServer)
{
#if WITH_EDITOR
	ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
	if (!Settings)
	{
		return false;
	}
	Settings->SetPlayNumberOfClients(FMath::Max(1, NumPlayers));
	Settings->SetPlayNetMode(bListenServer ? EPlayNetMode::PIE_ListenServer : EPlayNetMode::PIE_Standalone);
	Settings->SetRunUnderOneProcess(true);
	Settings->PostEditChange();
	Settings->SaveConfig();
	UE_LOG(LogMOFramework, Warning, TEXT("[MOQUERY] PIE configured: %d player(s), %s, one process"),
		FMath::Max(1, NumPlayers), bListenServer ? TEXT("listen-server") : TEXT("standalone"));
	return true;
#else
	(void)NumPlayers;
	(void)bListenServer;
	return false;
#endif
}

UWorldSubsystem* UMOEditorTestHelper::GetWorldSubsystem(UWorld* World, TSubclassOf<UWorldSubsystem> SubsystemClass)
{
	return World ? World->GetSubsystemBase(SubsystemClass) : nullptr;
}

UGameInstanceSubsystem* UMOEditorTestHelper::GetGameInstanceSubsystem(UWorld* World, TSubclassOf<UGameInstanceSubsystem> SubsystemClass)
{
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystemBase(SubsystemClass) : nullptr;
}
