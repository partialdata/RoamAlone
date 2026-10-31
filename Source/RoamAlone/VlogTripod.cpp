#include "VlogTripod.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "FirewoodTask.h"
#include "GameFramework/Pawn.h"
#include "RoamAlone.h"

AVlogTripod::AVlogTripod()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AVlogTripod::BeginPlay()
{
	Super::BeginPlay();
	StoredTransform = GetActorTransform();
	PhoneControl = Cast<UPrimitiveComponent>(PhoneControlReference.GetComponent(this));
	bReady = IsValid(PhoneControl) && IsValid(FirewoodTask) && IsValid(FilmingSpot)
		&& FilmingSpot != this && FirewoodTask->GetWorld() == GetWorld() && FilmingSpot->GetWorld() == GetWorld();
	if (!bReady)
	{
		UE_LOG(LogRoamAlone, Warning, TEXT("%s: assign the phone target, firewood task and a separate filming spot."), *GetName());
		return;
	}
	FirewoodTask->OnTaskStateChanged.AddDynamic(this, &AVlogTripod::HandleTaskChanged);
	ResetClip();
}

void AVlogTripod::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(FirewoodTask))
	{
		FirewoodTask->OnTaskStateChanged.RemoveDynamic(this, &AVlogTripod::HandleTaskChanged);
	}
	Super::EndPlay(EndPlayReason);
}

void AVlogTripod::SetClipState(EVlogClipState NewState)
{
	if (ClipState == NewState) return;
	ClipState = NewState;
	UpdateRecordingPresentation(IsRecording());
	OnClipChanged.Broadcast();
}

void AVlogTripod::ResetClip()
{
	if (!bReady) return;
	SetActorTransform(StoredTransform);
	if (IsValid(FilmingSpot)) FilmingSpot->SetActorHiddenInGame(false);
	ClipState = EVlogClipState::NeedsPlacement;
	UpdateRecordingPresentation(false);
	OnClipChanged.Broadcast();
}

void AVlogTripod::HandleTaskChanged()
{
	if (!IsValid(FirewoodTask)) return;
	if (FirewoodTask->GetTaskState() == EFirewoodTaskState::Available && FirewoodTask->GetCollectedLogCount() == 0)
	{
		ResetClip();
	}
	else if (ClipState == EVlogClipState::RecordingTask && FirewoodTask->GetTaskState() == EFirewoodTaskState::Placed)
	{
		SetClipState(EVlogClipState::RecordingComplete);
	}
	else
	{
		OnClipChanged.Broadcast();
	}
}

bool AVlogTripod::CanUsePhone(const APawn* Interactor, const FHitResult& Hit) const
{
	return bReady && IsValid(Interactor) && Interactor->GetWorld() == GetWorld() && Interactor->IsLocallyControlled()
		&& IsValid(FirewoodTask) && IsValid(FilmingSpot) && IsValid(PhoneControl)
		&& Hit.GetActor() == this && Hit.GetComponent() == PhoneControl;
}

FText AVlogTripod::GetInteractionPrompt_Implementation(APawn* Interactor, const FHitResult& Hit) const
{
	if (!CanUsePhone(Interactor, Hit)) return FText::GetEmpty();
	if (ClipState == EVlogClipState::NeedsPlacement)
	{
		return NSLOCTEXT("Vlog", "Place", "Set up tripod at the marked filming spot");
	}
	if (HasCompletedClip()) return NSLOCTEXT("Vlog", "CheckClip", "Check campsite clip");
	return IsRecording() ? NSLOCTEXT("Vlog", "Stop", "Stop campsite recording")
		: NSLOCTEXT("Vlog", "Start", "Record firewood task");
}

bool AVlogTripod::TryInteract_Implementation(APawn* Interactor, const FHitResult& Hit, FText& Feedback)
{
	Feedback = FText::GetEmpty();
	if (!CanUsePhone(Interactor, Hit)) return false;
	switch (ClipState)
	{
	case EVlogClipState::NeedsPlacement:
		if (!SetActorTransform(FilmingSpot->GetActorTransform())) return false;
		FilmingSpot->SetActorHiddenInGame(true);
		SetClipState(EVlogClipState::Ready);
		Feedback = NSLOCTEXT("Vlog", "Placed", "Tripod set up. Start recording before placing the firewood.");
		return true;
	case EVlogClipState::Ready:
		if (FirewoodTask->GetTaskState() == EFirewoodTaskState::Placed)
		{
			Feedback = NSLOCTEXT("Vlog", "Missed", "Firewood was finished without recording. No clip this time.");
			return false;
		}
		SetClipState(EVlogClipState::RecordingTask);
		Feedback = NSLOCTEXT("Vlog", "Started", "Recording. Collect the wood and place it at the fire pit.");
		return true;
	case EVlogClipState::RecordingTask:
		SetClipState(EVlogClipState::Ready);
		Feedback = NSLOCTEXT("Vlog", "EarlyStop", "Take incomplete. Start recording again before placing the firewood.");
		return true;
	case EVlogClipState::RecordingComplete:
		SetClipState(EVlogClipState::ClipReady);
		Feedback = NSLOCTEXT("Vlog", "ClipReady", "Campsite clip ready - review it inside the van.");
		return true;
	case EVlogClipState::ClipReady:
		Feedback = NSLOCTEXT("Vlog", "ClipReady", "Campsite clip ready - review it inside the van.");
		return true;
	default:
		return false;
	}
}

FText AVlogTripod::GetObjectiveText() const
{
	if (!bReady || !IsValid(FirewoodTask)) return FText::GetEmpty();
	switch (ClipState)
	{
	case EVlogClipState::NeedsPlacement:
		return NSLOCTEXT("Vlog", "PlaceObjective", "Set up the tripod at the marked filming spot.");
	case EVlogClipState::Ready:
		return FirewoodTask->GetTaskState() == EFirewoodTaskState::Placed ? FText::GetEmpty()
			: NSLOCTEXT("Vlog", "StartObjective", "Use the phone to start recording the firewood task.");
	case EVlogClipState::RecordingTask:
		return FText::Format(NSLOCTEXT("Vlog", "RecordObjective", "REC | {0}"), FirewoodTask->GetObjectiveText());
	case EVlogClipState::RecordingComplete:
		return NSLOCTEXT("Vlog", "FinishObjective", "Return to the tripod and stop recording to save the clip. Then get inside the van.");
	case EVlogClipState::ClipReady:
		return NSLOCTEXT("Vlog", "ClipReady", "Campsite clip ready - review it inside the van.");
	default:
		return FText::GetEmpty();
	}
}
