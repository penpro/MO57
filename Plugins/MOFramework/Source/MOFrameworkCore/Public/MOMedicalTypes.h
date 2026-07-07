/**
 * =============================================================================
 * MOMedicalTypes.h - Umbrella Include for Medical System Types
 * =============================================================================
 *
 * CLAUDE: READ THIS HEADER EVERY TIME YOU TOUCH THIS FILE
 * CLAUDE: UPDATE "KNOWN PITFALLS" WHEN ISSUES ARISE
 *
 * PURPOSE:
 * Umbrella include for backwards compatibility. Includes all split type headers.
 * For new code, prefer including only the specific headers you need to reduce
 * compile times.
 *
 * INCLUDED HEADERS:
 * - MOActivityTypes.h     - Activity levels, stamina, exertion
 * - MOBodyPartTypes.h     - Body parts, wound types, condition types
 * - MOWoundTypes.h        - Wound and condition structures
 * - MOVitalsTypes.h       - Vital signs (heart rate, BP, etc.)
 * - MOMetabolismTypes.h   - Body composition, nutrients, digestion
 * - MOMentalTypes.h       - Mental state, consciousness
 *
 * =============================================================================
 * KNOWN PITFALLS - UPDATE THIS WHEN ISSUES OCCUR
 * =============================================================================
 *
 * [2024-02] COMPILE TIME: Including this header pulls in ALL medical types.
 *   For faster compiles, include only what you need directly.
 *
 * =============================================================================
 * RELATED FILES: MOAnatomyComponent.h, MOVitalsComponent.h, MOMentalStateComponent.h
 * LAST UPDATED: 2026-02-25
 * =============================================================================
 */

#pragma once

#include "MOActivityTypes.h"
#include "MOBodyPartTypes.h"
#include "MOWoundTypes.h"
#include "MOVitalsTypes.h"
#include "MOMetabolismTypes.h"
#include "MOMentalTypes.h"
