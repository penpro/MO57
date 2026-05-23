#include "MOInputHintFormatter.h"
#include "MOPlayerController.h"
#include "MOKeyBindingManager.h"
#include "MOFramework.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"
#include "Internationalization/Regex.h"

namespace MOInputHintFormatterInternal
{
	/**
	 * Map a short context name (used in token like {key:Action:Building}) to
	 * the actual property name on AMOPlayerController. Keeps the token syntax
	 * terse while the controller's property names stay descriptive.
	 */
	FName ResolveContextPropertyName(FName ShortName)
	{
		if (ShortName.IsNone())
		{
			return NAME_None; // caller will iterate
		}

		// Direct match for full property names.
		static const TSet<FName> KnownFullNames = {
			TEXT("PawnControlContext"),
			TEXT("BaseBuildingContext"),
			TEXT("MenuContext")
		};
		if (KnownFullNames.Contains(ShortName))
		{
			return ShortName;
		}

		// Friendly aliases for token authors.
		const FString S = ShortName.ToString();
		if (S.Equals(TEXT("Pawn"), ESearchCase::IgnoreCase) ||
			S.Equals(TEXT("PawnControl"), ESearchCase::IgnoreCase) ||
			S.Equals(TEXT("Default"), ESearchCase::IgnoreCase))
		{
			return TEXT("PawnControlContext");
		}
		if (S.Equals(TEXT("Building"), ESearchCase::IgnoreCase) ||
			S.Equals(TEXT("Build"), ESearchCase::IgnoreCase) ||
			S.Equals(TEXT("BaseBuilding"), ESearchCase::IgnoreCase))
		{
			return TEXT("BaseBuildingContext");
		}
		if (S.Equals(TEXT("Menu"), ESearchCase::IgnoreCase))
		{
			return TEXT("MenuContext");
		}
		// Unknown alias — fall back to caller iteration.
		return NAME_None;
	}

	/**
	 * Read a TObjectPtr<T>-typed UPROPERTY by name. Returns nullptr if the
	 * property doesn't exist or isn't an object property pointing at T.
	 */
	template <typename T>
	T* ReadObjectProperty(const UObject* Owner, FName PropertyName)
	{
		if (!IsValid(Owner) || PropertyName.IsNone())
		{
			return nullptr;
		}
		FProperty* Prop = Owner->GetClass()->FindPropertyByName(PropertyName);
		FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop);
		if (!ObjProp)
		{
			return nullptr;
		}
		UObject* Value = ObjProp->GetObjectPropertyValue_InContainer(Owner);
		return Cast<T>(Value);
	}
}

FKey FMOInputHintFormatter::ResolveActionKey(const AMOPlayerController* MOPC,
	FName ActionPropertyName, FName ContextName, int32 SlotIndex)
{
	using namespace MOInputHintFormatterInternal;

	if (!IsValid(MOPC) || ActionPropertyName.IsNone())
	{
		return FKey();
	}

	UInputAction* Action = ReadObjectProperty<UInputAction>(MOPC, ActionPropertyName);
	if (!Action)
	{
		UE_LOG(LogMOFramework, Verbose, TEXT("[MOInputHintFormatter] No InputAction property named '%s'"),
			*ActionPropertyName.ToString());
		return FKey();
	}

	const FName ResolvedContextProp = ResolveContextPropertyName(ContextName);

	// Try the named context first, then fall back to scanning the standard
	// three contexts in priority order (pawn → building → menu). This makes
	// tokens forgiving when authors omit the context.
	const TArray<FName> ContextProps = !ResolvedContextProp.IsNone()
		? TArray<FName>{ ResolvedContextProp }
		: TArray<FName>{ TEXT("PawnControlContext"), TEXT("BaseBuildingContext"), TEXT("MenuContext") };

	for (const FName& Prop : ContextProps)
	{
		UInputMappingContext* Ctx = ReadObjectProperty<UInputMappingContext>(MOPC, Prop);
		if (!Ctx)
		{
			continue;
		}
		const FKey Key = FMOKeyBindingManager::GetCurrentBinding(Action, Ctx, SlotIndex);
		if (Key.IsValid())
		{
			return Key;
		}
	}

	return FKey();
}

FString FMOInputHintFormatter::FormatKeyGlyph(const FKey& Key)
{
	if (!Key.IsValid())
	{
		return TEXT("[unbound]");
	}
	return FString::Printf(TEXT("[%s]"), *Key.GetDisplayName().ToString());
}

FText FMOInputHintFormatter::Format(const FText& Template, const APlayerController* PC)
{
	if (Template.IsEmpty())
	{
		return Template;
	}

	const AMOPlayerController* MOPC = Cast<AMOPlayerController>(PC);
	const FString Source = Template.ToString();

	// Token grammar: {key:ActionName[:ContextOrSlot[:Slot]]}
	// We accept up to two trailing colon segments. If the first non-action
	// segment parses as an integer, treat it as a slot index; otherwise as a
	// context name. This makes both {key:Move:1} and {key:Move:Building} work
	// without the author having to type a placeholder.
	const FRegexPattern Pattern(TEXT("\\{key:([A-Za-z_][A-Za-z0-9_]*)(?::([A-Za-z0-9_]+))?(?::([0-9]+))?\\}"));
	FRegexMatcher Matcher(Pattern, Source);

	FString Result;
	int32 LastEnd = 0;

	while (Matcher.FindNext())
	{
		const int32 MatchBegin = Matcher.GetMatchBeginning();
		const int32 MatchEnd = Matcher.GetMatchEnding();
		Result.Append(Source.Mid(LastEnd, MatchBegin - LastEnd));

		const FString ActionName = Matcher.GetCaptureGroup(1);
		const FString SecondCap = Matcher.GetCaptureGroup(2);
		const FString ThirdCap = Matcher.GetCaptureGroup(3);

		FName ContextName = NAME_None;
		int32 SlotIndex = 0;

		if (!SecondCap.IsEmpty())
		{
			// If second capture is purely digits, treat as slot; else context.
			if (SecondCap.IsNumeric())
			{
				SlotIndex = FCString::Atoi(*SecondCap);
			}
			else
			{
				ContextName = FName(*SecondCap);
			}
		}
		if (!ThirdCap.IsEmpty())
		{
			SlotIndex = FCString::Atoi(*ThirdCap);
		}

		const FKey Key = ResolveActionKey(MOPC, FName(*ActionName), ContextName, SlotIndex);
		Result.Append(FormatKeyGlyph(Key));

		LastEnd = MatchEnd;
	}

	Result.Append(Source.Mid(LastEnd));
	return FText::FromString(Result);
}
