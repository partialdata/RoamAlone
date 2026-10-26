#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "InteractionComponent.generated.h"

class APawn;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInteractionPresentationChanged,
	const FText&, Prompt, const FText&, Feedback);

/** Local pawn interaction: bounded aiming trace, physical reach, and interface dispatch. No object-specific rules or UI. */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class ROAMALONE_API UInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInteractionComponent();

	/** Forward an Enhanced Input Started event here. Always targets and validates again. */
	UFUNCTION(BlueprintCallable, Category="Interaction")
	bool TryInteract();

	/** Void input delegate adapter; Blueprint callers can use TryInteract's result. */
	void Interact();

	/** Supply the pawn's active camera. Without one, use its controller's view. */
	UFUNCTION(BlueprintCallable, Category="Interaction")
	void SetViewSource(USceneComponent* ViewSource);

	UFUNCTION(BlueprintCallable, Category="Interaction")
	void RefreshTarget();

	UFUNCTION(BlueprintPure, Category="Interaction")
	AActor* GetFocusedActor() const { return FocusedActor.Get(); }

	UFUNCTION(BlueprintPure, Category="Interaction")
	FText GetPrompt() const { return Prompt; }

	UFUNCTION(BlueprintPure, Category="Interaction")
	FText GetFeedback() const { return Feedback; }

	/** Fires only when displayed text or the focused actor changes. Bind UI once; no UMG polling required. */
	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FInteractionPresentationChanged OnPresentationChanged;

	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Independent of camera distance, measured from the pawn's eye position to the reached surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Targeting", meta=(ClampMin="1", Units="cm"))
	float ReachDistance = 220.0f;

	/** Includes the third-person camera's spring-arm offset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Targeting", meta=(ClampMin="1", Units="cm"))
	float AimTraceDistance = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interaction|Targeting")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category="Interaction|Targeting", meta=(ClampMin="0.02", Units="s"))
	float TargetRefreshInterval = 0.1f;

private:
	bool FindTarget(FHitResult& OutHit, FText& OutPrompt) const;
	void StartRefreshTimer();
	void ClearPresentation();
	void PublishPresentation(AActor* Target, const FText& NewPrompt, const FText& NewFeedback);

	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> ActiveViewSource;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> FocusedActor;

	FText Prompt;
	FText Feedback;
	FTimerHandle RefreshTimer;
	double FeedbackUntil = 0;
	bool bExecutingInteraction = false;
};
