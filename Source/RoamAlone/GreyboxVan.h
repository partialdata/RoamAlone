#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "GreyboxVan.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** Gameplay owner for a van assembled in a child Blueprint. No mesh generation. */
UCLASS(Abstract, Blueprintable)
class ROAMALONE_API AGreyboxVan : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	AGreyboxVan();

	UFUNCTION(BlueprintPure, Category="Van")
	bool ContainsInteriorPoint(FVector WorldPoint, bool bAlreadyInside = false) const;

	/** Refuses repeated requests and a destination occupied by a pawn. */
	UFUNCTION(BlueprintCallable, Category="Van|Door")
	bool TrySetDoorOpen(bool bOpen);

	UFUNCTION(BlueprintPure, Category="Van|Door")
	bool IsDoorOpen() const { return bDoorOpen; }

	/** Locking requires a ready, closed door and an assigned inside unlock control. */
	UFUNCTION(BlueprintCallable, Category="Van|Door")
	bool TrySetLocked(bool bLocked);

	UFUNCTION(BlueprintPure, Category="Van|Door")
	bool IsLocked() const { return bDoorLocked; }

	bool IsDoorComponent(const UPrimitiveComponent* Component) const;

	virtual FText GetInteractionPrompt_Implementation(APawn* Interactor, const FHitResult& Hit) const override;
	virtual bool TryInteract_Implementation(APawn* Interactor, const FHitResult& Hit, FText& Feedback) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** All visual children of this Blueprint assembly move with the door. */
	UPROPERTY(EditDefaultsOnly, Category="Van|Door", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.SceneComponent"))
	FComponentReference DoorRootReference;

	UPROPERTY(EditDefaultsOnly, Category="Van|Door", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.StaticMeshComponent"))
	FComponentReference DoorPanelReference;

	/** Separate wall-mounted switch for closing the parked door from inside. */
	UPROPERTY(EditDefaultsOnly, Category="Van|Door", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.PrimitiveComponent"))
	FComponentReference InsideSwitchReference;

	/** Fixed, labeled interior control, separate from the open/close switch. */
	UPROPERTY(EditAnywhere, Category="Van|Door", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.PrimitiveComponent"))
	FComponentReference LockControlReference;

	UPROPERTY(EditDefaultsOnly, Category="Van|Door", meta=(Units="cm"))
	FVector DoorOpenOffset = FVector::ZeroVector;

private:
	UFUNCTION()
	void OnInteriorBegin(UPrimitiveComponent* Overlapped, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit);

	UFUNCTION()
	void OnInteriorEnd(UPrimitiveComponent* Overlapped, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, int32 BodyIndex);

	UPROPERTY(VisibleAnywhere, Category="Van")
	TObjectPtr<USceneComponent> VanRoot;

	/** Gameplay trigger; size and placement are tuned in the child Blueprint. */
	UPROPERTY(VisibleAnywhere, Category="Van")
	TObjectPtr<UBoxComponent> Interior;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ResolvedDoorRoot;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ResolvedDoorPanel;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ResolvedInsideSwitch;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ResolvedLockControl;

	UPROPERTY(VisibleInstanceOnly, Category="Van|Door")
	bool bDoorOpen = false;

	UPROPERTY(VisibleInstanceOnly, Category="Van|Door")
	bool bDoorLocked = false;

	FTransform ClosedDoorTransform;
	bool bDoorReady = false;
};
