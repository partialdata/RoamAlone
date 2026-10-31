#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "VlogTripod.generated.h"

class UPrimitiveComponent;
class AFirewoodTask;

UENUM(BlueprintType)
enum class EVlogClipState : uint8
{
	NeedsPlacement, Ready, RecordingTask, RecordingComplete, ClipReady
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVlogClipChanged);

/** Optional greybox recording interaction. Simulates a take; does not capture media or take camera control. */
UCLASS(Blueprintable)
class ROAMALONE_API AVlogTripod : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AVlogTripod();

	UFUNCTION(BlueprintPure, Category="Vlog")
	bool IsRecording() const { return ClipState == EVlogClipState::RecordingTask || ClipState == EVlogClipState::RecordingComplete; }

	UFUNCTION(BlueprintPure, Category="Vlog")
	EVlogClipState GetClipState() const { return ClipState; }

	UFUNCTION(BlueprintPure, Category="Vlog")
	bool HasCompletedClip() const { return ClipState == EVlogClipState::ClipReady; }

	UFUNCTION(BlueprintPure, Category="Vlog")
	FText GetObjectiveText() const;

	UFUNCTION(BlueprintCallable, Category="Vlog")
	void ResetClip();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Vlog")
	TObjectPtr<AFirewoodTask> FirewoodTask;

	/** One authored placement, not free placement or a camera coverage simulation. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Vlog")
	TObjectPtr<AActor> FilmingSpot;

	UPROPERTY(BlueprintAssignable, Category="Vlog")
	FVlogClipChanged OnClipChanged;

	virtual FText GetInteractionPrompt_Implementation(APawn* Interactor, const FHitResult& Hit) const override;
	virtual bool TryInteract_Implementation(APawn* Interactor, const FHitResult& Hit, FText& Feedback) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Blueprint assembles the phone/tripod and presents its recording indicator. */
	UFUNCTION(BlueprintImplementableEvent, Category="Vlog")
	void UpdateRecordingPresentation(bool bIsRecording);

	UPROPERTY(EditDefaultsOnly, Category="Vlog", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.PrimitiveComponent"))
	FComponentReference PhoneControlReference;

private:
	bool CanUsePhone(const APawn* Interactor, const FHitResult& Hit) const;
	void SetClipState(EVlogClipState NewState);
	UFUNCTION()
	void HandleTaskChanged();

	UPROPERTY(VisibleAnywhere, Category="Vlog")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> PhoneControl;

	UPROPERTY(VisibleInstanceOnly, Category="Vlog")
	EVlogClipState ClipState = EVlogClipState::NeedsPlacement;

	FTransform StoredTransform;
	bool bReady = false;
};
