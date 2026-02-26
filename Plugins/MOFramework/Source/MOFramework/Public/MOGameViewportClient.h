/**
 * =============================================================================
 * MOGameViewportClient.h - Custom Viewport Client for CommonUI
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Custom game viewport client extending UCommonGameViewportClient. Required
 * for proper CommonUI input routing and focus management.
 *
 * SETUP:
 * Set in Project Settings > Engine > General Settings > Game Viewport Client Class.
 * Also requires UMOLocalPlayer for full CommonUI support.
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] PROJECT SETTINGS: Must set Game Viewport Client Class to this class.
 *   Without this, CommonUI input routing may not work correctly.
 *
 * [2024-02] MINIMAL CLASS: Currently just inherits base functionality. Override
 *   methods here for custom viewport behavior (cursor, focus, etc.).
 *
 * =============================================================================
 * RELATED FILES: MOLocalPlayer.h, MOGameInstance.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "CoreMinimal.h"
#include "CommonGameViewportClient.h"
#include "MOGameViewportClient.generated.h"

/**
 * Custom game viewport client for MO57.
 * See file header for setup and pitfalls.
 */
UCLASS()
class MOFRAMEWORK_API UMOGameViewportClient : public UCommonGameViewportClient
{
	GENERATED_BODY()

public:
	UMOGameViewportClient();
};
