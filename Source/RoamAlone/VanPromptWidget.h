#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VanPromptWidget.generated.h"

class UTextBlock;

/** Event-updated prototype instructions. No per-frame UMG property bindings. */
UCLASS()
class ROAMALONE_API UVanPromptWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetPrompt(const FText& Text);
	void SetReticleVisible(bool bVisible);
protected:
	virtual void NativeOnInitialized() override;
private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Prompt;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Reticle;
};
