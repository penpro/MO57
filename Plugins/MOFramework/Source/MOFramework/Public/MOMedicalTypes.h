#pragma once

/**
 * MOMedicalTypes.h - Umbrella include for all medical system types
 *
 * This header provides backwards compatibility by including all split type headers.
 * For new code, prefer including only the specific headers you need to reduce
 * compile times:
 *
 *   #include "MOActivityTypes.h"     - Activity levels, stamina, exertion
 *   #include "MOBodyPartTypes.h"     - Body parts, wound types, condition types
 *   #include "MOWoundTypes.h"        - Wound and condition structures
 *   #include "MOVitalsTypes.h"       - Vital signs (heart rate, BP, etc.)
 *   #include "MOMetabolismTypes.h"   - Body composition, nutrients, digestion
 *   #include "MOMentalTypes.h"       - Mental state, consciousness
 */

#include "MOActivityTypes.h"
#include "MOBodyPartTypes.h"
#include "MOWoundTypes.h"
#include "MOVitalsTypes.h"
#include "MOMetabolismTypes.h"
#include "MOMentalTypes.h"
