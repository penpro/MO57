#include "MOBodySex.h"
#include "MOCharacter.h"
#include "MOIdentityComponent.h"

#define LOCTEXT_NAMESPACE "MOPronouns"

FText FMOPronouns::Subject(EMOBodySex Sex)
{
	switch (Sex)
	{
	case EMOBodySex::Female: return LOCTEXT("SubjectFemale", "she");
	case EMOBodySex::Male: return LOCTEXT("SubjectMale", "he");
	default: return LOCTEXT("SubjectUnspecified", "they");
	}
}

FText FMOPronouns::Object(EMOBodySex Sex)
{
	switch (Sex)
	{
	case EMOBodySex::Female: return LOCTEXT("ObjectFemale", "her");
	case EMOBodySex::Male: return LOCTEXT("ObjectMale", "him");
	default: return LOCTEXT("ObjectUnspecified", "them");
	}
}

FText FMOPronouns::Possessive(EMOBodySex Sex)
{
	switch (Sex)
	{
	case EMOBodySex::Female: return LOCTEXT("PossessiveFemale", "her");
	case EMOBodySex::Male: return LOCTEXT("PossessiveMale", "his");
	default: return LOCTEXT("PossessiveUnspecified", "their");
	}
}

FText FMOPronouns::SexName(EMOBodySex Sex)
{
	switch (Sex)
	{
	case EMOBodySex::Female: return LOCTEXT("SexFemale", "Female");
	case EMOBodySex::Male: return LOCTEXT("SexMale", "Male");
	default: return FText::GetEmpty();
	}
}

EMOBodySex FMOPronouns::SexOf(const APawn* Pawn)
{
	const AMOCharacter* Survivor = Cast<AMOCharacter>(Pawn);
	return Survivor ? Survivor->GetBodySex() : EMOBodySex::Unspecified;
}

FString FMOPronouns::GenderLabel(const APawn* Pawn)
{
	const UMOIdentityComponent* Identity = Pawn ? Pawn->FindComponentByClass<UMOIdentityComponent>() : nullptr;
	if (Identity && !Identity->Gender.IsEmpty())
	{
		return Identity->Gender;
	}
	return SexName(SexOf(Pawn)).ToString();
}

#undef LOCTEXT_NAMESPACE
