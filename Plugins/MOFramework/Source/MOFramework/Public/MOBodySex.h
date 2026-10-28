// The survivor's sex, as the body has it (Wes, Oct 28 2026: "add it" -- the game tracked no sex, so nothing could say "her" or pick a voice).
// It belongs to the body: a base names it (FMOSurvivorBase::Sex, Config/DefaultGame.ini), a pawn without a base takes its class default
// (AMOCharacter::DefaultBodySex), and AMOCharacter::GetBodySex is the one place to ask -- derived, so it needs no replication or save of its
// own (the base already replicates and saves with the look). Pronouns follow it; the identity's free-text Gender (the player's words) is
// shown in its place when set. A voice set keys on it when the survivors get voices (there are none yet).

#pragma once

#include "CoreMinimal.h"
#include "MOBodySex.generated.h"

class APawn;

UENUM(BlueprintType)
enum class EMOBodySex : uint8
{
	Unspecified,
	Female,
	Male
};

/** Words for a person by their body's sex: she / he, and they when it is not known. */
struct MOFRAMEWORK_API FMOPronouns
{
	/** she / he / they */
	static FText Subject(EMOBodySex Sex);
	/** her / him / them */
	static FText Object(EMOBodySex Sex);
	/** her / his / their (before a noun: "her torch") */
	static FText Possessive(EMOBodySex Sex);
	/** Female / Male (empty when not known) */
	static FText SexName(EMOBodySex Sex);

	/** The pawn's body sex (AMOCharacter::GetBodySex), Unspecified for any other pawn or none. */
	static EMOBodySex SexOf(const APawn* Pawn);

	/** What to show as the pawn's gender: the identity's Gender when the player gave one, else its body's sex, else empty. */
	static FString GenderLabel(const APawn* Pawn);
};
