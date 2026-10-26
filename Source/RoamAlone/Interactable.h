#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class APawn;

UINTERFACE(BlueprintType, Blueprintable)
class ROAMALONE_API UInteractable : public UInterface
{
	GENERATED_BODY()
};

/** A reached actor supplies its prompt and owns the rules and effects of its action. */
class ROAMALONE_API IInteractable
{
	GENERATED_BODY()

public:
	/** Empty text means this hit component currently offers no interaction. Keep this query free of side effects. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Interaction")
	FText GetInteractionPrompt(APawn* Interactor, const FHitResult& Hit) const;

	/** Revalidate object-specific state here. Return false on refusal and optionally explain it in Feedback. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Interaction")
	bool TryInteract(APawn* Interactor, const FHitResult& Hit, FText& Feedback);
};
