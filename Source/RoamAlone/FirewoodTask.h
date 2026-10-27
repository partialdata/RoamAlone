#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "FirewoodTask.generated.h"

UENUM(BlueprintType)
enum class EFirewoodTaskState : uint8
{
	Available,
	Carrying,
	Placed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFirewoodTaskChanged);

/** One campsite task. Pickup and fire-pit visuals are assembled in a child Blueprint. */
UCLASS(Abstract, Blueprintable)
class ROAMALONE_API AFirewoodTask : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AFirewoodTask();

	UFUNCTION(BlueprintPure, Category="Firewood")
	EFirewoodTaskState GetTaskState() const { return TaskState; }

	UFUNCTION(BlueprintPure, Category="Firewood")
	int32 GetCollectedLogCount() const { return CollectedLogIndices.Num(); }

	UFUNCTION(BlueprintPure, Category="Firewood")
	int32 GetRequiredLogCount() const { return PickupReferences.Num(); }

	UFUNCTION(BlueprintPure, Category="Firewood")
	FText GetObjectiveText() const;

	UFUNCTION(BlueprintCallable, Category="Firewood")
	bool TryCollectFirewood(APawn* Interactor, UPrimitiveComponent* Log);

	UFUNCTION(BlueprintCallable, Category="Firewood")
	bool TryPlaceFirewood(APawn* Interactor);

	/** Restores every log, the empty pit and objective; safe to repeat. */
	UFUNCTION(BlueprintCallable, Category="Firewood")
	void ResetTask();

	/** The HUD and later encounter progression can observe committed task changes. */
	UPROPERTY(BlueprintAssignable, Category="Firewood")
	FFirewoodTaskChanged OnTaskStateChanged;

	virtual FText GetInteractionPrompt_Implementation(APawn* Interactor, const FHitResult& Hit) const override;
	virtual bool TryInteract_Implementation(APawn* Interactor, const FHitResult& Hit, FText& Feedback) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Each referenced mesh is one independently collectible log. All are required. */
	UPROPERTY(EditDefaultsOnly, Category="Firewood|Assembly", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.PrimitiveComponent"))
	TArray<FComponentReference> PickupReferences;

	UPROPERTY(EditDefaultsOnly, Category="Firewood|Assembly", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.SceneComponent"))
	FComponentReference FirePitRootReference;

	UPROPERTY(EditDefaultsOnly, Category="Firewood|Assembly", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.SceneComponent"))
	FComponentReference PlacedWoodRootReference;

private:
	bool CanUseTask(const APawn* Interactor) const;
	bool IsInGroup(const UPrimitiveComponent* Component, const USceneComponent* Group) const;
	void UpdatePresentation();
	void ClearCarrier();
	void PublishTaskChange();

	UFUNCTION()
	void HandleCarrierDestroyed(AActor* DestroyedActor);

	UPROPERTY(VisibleAnywhere, Category="Firewood")
	TObjectPtr<USceneComponent> TaskRoot;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> FirePitRoot;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PlacedWoodRoot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPrimitiveComponent>> PickupPrimitives;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> Carrier;

	UPROPERTY(VisibleInstanceOnly, Category="Firewood")
	EFirewoodTaskState TaskState = EFirewoodTaskState::Available;

	TSet<int32> CollectedLogIndices;
	bool bTaskReady = false;
};
