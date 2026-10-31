#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Interactable.h"
#include "NightDirector.generated.h"

class AFirewoodTask;
class AGreyboxVan;
class ARoamAloneCharacter;
class UAudioComponent;
class USoundBase;
class AVlogTripod;

UENUM(BlueprintType)
enum class ENightPhase : uint8
{
	Gathering, Retreat, Shelter, Approach, Knocking, AwaitingHorn, Withdrawing, DawnFade, Complete
};

UENUM(BlueprintType)
enum class EDoorAttemptResult : uint8
{
	None, Locked, Opened, AlreadyOpen, Blocked
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNightPresentationChanged);

/** One authored night. References, sound choices, cue locations and timing are tuned in its Blueprint. */
UCLASS(Blueprintable)
class ROAMALONE_API ANightDirector : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ANightDirector();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Night")
	TObjectPtr<AFirewoodTask> FirewoodTask;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Night")
	TObjectPtr<AGreyboxVan> Van;

	/** Optional filming objective; the tripod owns clip rules. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Night")
	TObjectPtr<AVlogTripod> FilmingTripod;

	UPROPERTY(BlueprintAssignable, Category="Night")
	FNightPresentationChanged OnPresentationChanged;

	UFUNCTION(BlueprintPure, Category="Night")
	ENightPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category="Night")
	EDoorAttemptResult GetDoorAttemptResult() const { return DoorAttemptResult; }

	UFUNCTION(BlueprintPure, Category="Night")
	FText GetObjectiveText() const;

	UFUNCTION(BlueprintPure, Category="Night")
	FText GetCaptionText() const { return Caption; }

	/** Safe development reset; refuses to close the door through an occupied destination. */
	UFUNCTION(BlueprintCallable, Category="Night|Debug")
	bool ResetEncounter();

	/** Skip the outdoor task for testing. Requires the player to be inside; door rules still apply. */
	UFUNCTION(BlueprintCallable, Category="Night|Debug")
	bool DebugStartShelter();

	bool TrySoundHorn(APawn* Interactor);
	virtual FText GetInteractionPrompt_Implementation(APawn* Interactor, const FHitResult& Hit) const override;
	virtual bool TryInteract_Implementation(APawn* Interactor, const FHitResult& Hit, FText& Feedback) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Implemented in the project Blueprint; no native dependency on vendor classes. */
	UFUNCTION(BlueprintImplementableEvent, Category="Night|Environment")
	void RequestTimeOfDay(float TimeOfDay);

	UPROPERTY(EditDefaultsOnly, Category="Night|Interaction", meta=(UseComponentPicker, AllowedClasses="/Script/Engine.PrimitiveComponent"))
	FComponentReference HornControlReference;

	UPROPERTY(EditDefaultsOnly, Category="Night|Audio")
	TObjectPtr<USoundBase> BranchSnap;
	UPROPERTY(EditDefaultsOnly, Category="Night|Audio")
	TObjectPtr<USoundBase> Footsteps;
	UPROPERTY(EditDefaultsOnly, Category="Night|Audio")
	TObjectPtr<USoundBase> Knock;
	UPROPERTY(EditDefaultsOnly, Category="Night|Audio")
	TObjectPtr<USoundBase> DoorRattle;
	UPROPERTY(EditDefaultsOnly, Category="Night|Audio")
	TObjectPtr<USoundBase> Horn;

	UPROPERTY(EditAnywhere, Category="Night|Timing", meta=(ClampMin="1", Units="s"))
	float PreparationSeconds = 8;
	UPROPERTY(EditAnywhere, Category="Night|Timing", meta=(ClampMin="0", Units="s"))
	float QuietSeconds = 2;

	// Relative to the van, so its layout and these anchors can be moved together.
	UPROPERTY(EditAnywhere, Category="Night|Audio", meta=(Units="cm"))
	FVector OutdoorCueOffset = FVector(-600, -600, 90);
	UPROPERTY(EditAnywhere, Category="Night|Audio", meta=(Units="cm"))
	FVector FootstepOffset = FVector(-200, -300, 10);
	UPROPERTY(EditAnywhere, Category="Night|Audio", meta=(Units="cm"))
	FVector DoorCueOffset = FVector(-15, -120, 160);
	UPROPERTY(EditAnywhere, Category="Night|Audio", meta=(Units="cm"))
	FVector HornOffset = FVector(200, 0, 160);

private:
	friend struct FNightEncounterTestWorld;
	UFUNCTION()
	void HandleTaskChanged();
	UFUNCTION()
	void HandleClipChanged();
	void InitializeEncounter();
	void CheckShelter();
	void AdvanceEncounter();
	void SetPhase(ENightPhase NewPhase, const FText& NewCaption);
	void ScheduleAdvance(float Delay);
	float PlayCue(USoundBase* Sound, const FVector& Offset);
	bool IsPlayerInside() const;
	bool CanUseHornControl(const APawn* Interactor, const FHitResult& Hit) const;
	void FadeCamera(bool bToBlack);
	void PlayRetreatFootsteps();

	UPROPERTY(VisibleAnywhere, Category="Night")
	TObjectPtr<USceneComponent> SceneRoot;
	UPROPERTY(VisibleAnywhere, Category="Night|Audio")
	TObjectPtr<UAudioComponent> CueAudio;
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ResolvedHornControl;
	UPROPERTY(Transient)
	TWeakObjectPtr<ARoamAloneCharacter> Player;
	UPROPERTY(VisibleInstanceOnly, Category="Night")
	ENightPhase Phase = ENightPhase::Gathering;
	UPROPERTY(VisibleInstanceOnly, Category="Night")
	EDoorAttemptResult DoorAttemptResult = EDoorAttemptResult::None;
	FText Caption;
	FTimerHandle PhaseTimer;
	FTimerHandle ShelterTimer;
	FTimerHandle FootstepTimer;
	bool bReady = false;
};
