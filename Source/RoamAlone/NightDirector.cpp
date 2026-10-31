#include "NightDirector.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "FirewoodTask.h"
#include "GameFramework/PlayerController.h"
#include "GreyboxVan.h"
#include "Kismet/GameplayStatics.h"
#include "RoamAlone.h"
#include "RoamAloneCharacter.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "VlogTripod.h"

ANightDirector::ANightDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	CueAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("CueAudio"));
	CueAudio->SetupAttachment(SceneRoot);
	CueAudio->bAutoActivate = false;
	CueAudio->bOverrideAttenuation = true;
	CueAudio->AttenuationOverrides.bSpatialize = true;
	CueAudio->AttenuationOverrides.bAttenuate = true;
	CueAudio->AttenuationOverrides.AttenuationShapeExtents = FVector(150);
	CueAudio->AttenuationOverrides.FalloffDistance = 1800;
}

void ANightDirector::BeginPlay()
{
	Super::BeginPlay();
	// The GameMode may create the default pawn after placed actors receive BeginPlay.
	GetWorldTimerManager().SetTimerForNextTick(this, &ANightDirector::InitializeEncounter);
}

void ANightDirector::InitializeEncounter()
{
	if (bReady) return;
	Player = Cast<ARoamAloneCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	ResolvedHornControl = Cast<UPrimitiveComponent>(HornControlReference.GetComponent(this));
	bReady = IsValid(FirewoodTask) && IsValid(Van) && Player.IsValid() && ResolvedHornControl
		&& BranchSnap && Footsteps && Knock && DoorRattle && Horn;
	if (!bReady)
	{
		UE_LOG(LogRoamAlone, Error, TEXT("%s: encounter not ready (task=%s, van=%s, player=%s, horn control=%s). Assign these and all five sounds."),
			*GetName(), *GetNameSafe(FirewoodTask), *GetNameSafe(Van), *GetNameSafe(Player.Get()), *GetNameSafe(ResolvedHornControl));
		return;
	}
	Player->SetNightDirector(this);
	FirewoodTask->OnTaskStateChanged.AddDynamic(this, &ANightDirector::HandleTaskChanged);
	if (IsValid(FilmingTripod)) FilmingTripod->OnClipChanged.AddDynamic(this, &ANightDirector::HandleClipChanged);
	RequestTimeOfDay(1700);
	HandleTaskChanged();
}

void ANightDirector::HandleClipChanged()
{
	OnPresentationChanged.Broadcast();
}

void ANightDirector::HandleTaskChanged()
{
	if (!bReady || !IsValid(FirewoodTask)) return;
	if (Phase == ENightPhase::Gathering && FirewoodTask->GetTaskState() == EFirewoodTaskState::Placed)
	{
		PlayCue(BranchSnap, OutdoorCueOffset);
		SetPhase(ENightPhase::Retreat, NSLOCTEXT("Night", "Snap", "[A branch snaps nearby.]"));
		// Poll only for the spatial prerequisite; no actor searches or per-frame UI work.
		GetWorldTimerManager().SetTimer(ShelterTimer, this, &ANightDirector::CheckShelter, 0.25f, true);
	}
	OnPresentationChanged.Broadcast();
}

bool ANightDirector::IsPlayerInside() const
{
	return Player.IsValid() && IsValid(Van) && Van->ContainsInteriorPoint(Player->GetActorLocation());
}

void ANightDirector::CheckShelter()
{
	if (Phase != ENightPhase::Retreat || !IsPlayerInside()) return;
	GetWorldTimerManager().ClearTimer(ShelterTimer);
	RequestTimeOfDay(2300);
	SetPhase(ENightPhase::Shelter, FText::GetEmpty());
	ScheduleAdvance(PreparationSeconds);
}

void ANightDirector::SetPhase(ENightPhase NewPhase, const FText& NewCaption)
{
	Phase = NewPhase;
	Caption = NewCaption;
	OnPresentationChanged.Broadcast();
}

float ANightDirector::PlayCue(USoundBase* Sound, const FVector& Offset)
{
	CueAudio->Stop();
	if (!IsValid(Sound) || !IsValid(Van)) return 0;
	CueAudio->SetWorldLocation(Van->GetActorTransform().TransformPosition(Offset));
	CueAudio->SetSound(Sound);
	CueAudio->Play();
	return FMath::Clamp(Sound->GetDuration(), 0.1f, 30.0f);
}

void ANightDirector::ScheduleAdvance(float Delay)
{
	GetWorldTimerManager().SetTimer(PhaseTimer, this, &ANightDirector::AdvanceEncounter, FMath::Max(Delay, 0.01f), false);
}

void ANightDirector::AdvanceEncounter()
{
	if (!bReady || !IsValid(Van) || !Player.IsValid()) return;
	// Leaving never traps the player or silently consumes a critical door beat.
	if (Phase >= ENightPhase::Shelter && Phase <= ENightPhase::Knocking && !IsPlayerInside())
	{
		ScheduleAdvance(0.5f);
		return;
	}
	switch (Phase)
	{
	case ENightPhase::Shelter:
		SetPhase(ENightPhase::Approach, NSLOCTEXT("Night", "Approach", "[Footsteps approach outside.]"));
		ScheduleAdvance(PlayCue(Footsteps, FootstepOffset) + QuietSeconds);
		break;
	case ENightPhase::Approach:
		SetPhase(ENightPhase::Knocking, NSLOCTEXT("Night", "Knock", "[Knocking on the van door.]"));
		ScheduleAdvance(PlayCue(Knock, DoorCueOffset) + QuietSeconds);
		break;
	case ENightPhase::Knocking:
		PlayCue(Van->IsDoorOpen() ? Footsteps : DoorRattle, DoorCueOffset);
		if (Van->IsLocked())
		{
			DoorAttemptResult = EDoorAttemptResult::Locked;
			Caption = NSLOCTEXT("Night", "Locked", "[The handle rattles. The locked door holds.]");
		}
		else if (Van->IsDoorOpen())
		{
			DoorAttemptResult = EDoorAttemptResult::AlreadyOpen;
			Caption = NSLOCTEXT("Night", "Open", "[Footsteps stop at the open doorway.]");
		}
		else if (Van->TrySetDoorOpen(true))
		{
			DoorAttemptResult = EDoorAttemptResult::Opened;
			Caption = NSLOCTEXT("Night", "Opened", "[The handle turns. The unlocked door opens.]");
		}
		else
		{
			DoorAttemptResult = EDoorAttemptResult::Blocked;
			Caption = NSLOCTEXT("Night", "Blocked", "[The handle rattles. The door catches against an obstruction.]");
		}
		SetPhase(ENightPhase::AwaitingHorn, Caption);
		break;
	case ENightPhase::Withdrawing:
		SetPhase(ENightPhase::DawnFade, NSLOCTEXT("Night", "Quiet", "[The footsteps fade away. Later that night... ]"));
		FadeCamera(true);
		ScheduleAdvance(1.5f);
		break;
	case ENightPhase::DawnFade:
		RequestTimeOfDay(800);
		FadeCamera(false);
		SetPhase(ENightPhase::Complete, NSLOCTEXT("Night", "Morning", "Morning. No more footsteps."));
		break;
	default:
		break;
	}
}

bool ANightDirector::TrySoundHorn(APawn* Interactor)
{
	if (!bReady || Interactor != Player.Get() || !IsPlayerInside() || Phase != ENightPhase::AwaitingHorn) return false;
	SetPhase(ENightPhase::Withdrawing, NSLOCTEXT("Night", "Horn", "[The van horn sounds.]"));
	// Play the horn, then the footsteps, with no overlap between these critical cues.
	const float HornSeconds = PlayCue(Horn, HornOffset);
	ScheduleAdvance(HornSeconds + Footsteps->GetDuration() + QuietSeconds);
	GetWorldTimerManager().SetTimer(FootstepTimer, this, &ANightDirector::PlayRetreatFootsteps, HornSeconds + 0.1f, false);
	return true;
}

void ANightDirector::PlayRetreatFootsteps()
{
	if (Phase == ENightPhase::Withdrawing)
	{
		PlayCue(Footsteps, FootstepOffset);
		Caption = NSLOCTEXT("Night", "RetreatSteps", "[Footsteps retreat outside.]");
		OnPresentationChanged.Broadcast();
	}
}

void ANightDirector::FadeCamera(bool bToBlack)
{
	APlayerController* Controller = Player.IsValid() ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (Controller && Controller->PlayerCameraManager)
	{
		Controller->PlayerCameraManager->StartCameraFade(bToBlack ? 0 : 1, bToBlack ? 1 : 0, 1.0f,
			FLinearColor::Black, false, bToBlack);
	}
}

bool ANightDirector::ResetEncounter()
{
	if (!bReady || !IsValid(Van) || !IsValid(FirewoodTask)) return false;
	if (Van->IsDoorOpen() && !Van->TrySetDoorOpen(false)) return false;
	if (Van->IsLocked() && !Van->TrySetLocked(false)) return false;
	GetWorldTimerManager().ClearTimer(PhaseTimer);
	GetWorldTimerManager().ClearTimer(ShelterTimer);
	GetWorldTimerManager().ClearTimer(FootstepTimer);
	CueAudio->Stop();
	DoorAttemptResult = EDoorAttemptResult::None;
	SetPhase(ENightPhase::Gathering, FText::GetEmpty());
	FirewoodTask->ResetTask();
	if (IsValid(FilmingTripod)) FilmingTripod->ResetClip();
	RequestTimeOfDay(1700);
	APlayerController* Controller = Player.IsValid() ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (Controller && Controller->PlayerCameraManager) Controller->PlayerCameraManager->StopCameraFade();
	return true;
}

bool ANightDirector::DebugStartShelter()
{
	if (!IsPlayerInside() || !ResetEncounter()) return false;
	SetPhase(ENightPhase::Retreat, FText::GetEmpty());
	CheckShelter();
	return true;
}

bool ANightDirector::CanUseHornControl(const APawn* Interactor, const FHitResult& Hit) const
{
	return bReady && Interactor == Player.Get() && IsPlayerInside() && Hit.GetActor() == this
		&& Hit.GetComponent() == ResolvedHornControl;
}

FText ANightDirector::GetInteractionPrompt_Implementation(APawn* Interactor, const FHitResult& Hit) const
{
	if (!CanUseHornControl(Interactor, Hit)) return FText::GetEmpty();
	return Phase == ENightPhase::Complete ? NSLOCTEXT("Night", "Restart", "Restart prototype")
		: NSLOCTEXT("Night", "HornPrompt", "Sound van horn");
}

bool ANightDirector::TryInteract_Implementation(APawn* Interactor, const FHitResult& Hit, FText& Feedback)
{
	Feedback = FText::GetEmpty();
	if (!CanUseHornControl(Interactor, Hit)) return false;
	if (Phase == ENightPhase::Complete)
	{
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
		return true;
	}
	if (TrySoundHorn(Interactor)) return true;
	Feedback = NSLOCTEXT("Night", "HornWait", "No need to sound the horn right now.");
	return false;
}

FText ANightDirector::GetObjectiveText() const
{
	if (!bReady) return FText::GetEmpty();
	switch (Phase)
	{
	case ENightPhase::Gathering:
		if (IsValid(FilmingTripod))
		{
			const FText FilmingObjective = FilmingTripod->GetObjectiveText();
			if (!FilmingObjective.IsEmpty()) return FilmingObjective;
		}
		return IsValid(FirewoodTask) ? FirewoodTask->GetObjectiveText() : FText::GetEmpty();
	case ENightPhase::Retreat:
		if (IsValid(FilmingTripod) && (FilmingTripod->IsRecording() || FilmingTripod->HasCompletedClip()))
		{
			return FilmingTripod->GetObjectiveText();
		}
		return NSLOCTEXT("Night", "RetreatObjective", "Get back inside the van.");
	case ENightPhase::Shelter: return NSLOCTEXT("Night", "ShelterObjective", "Close the door and use the LOCK tab. Stay inside.");
	case ENightPhase::Approach:
	case ENightPhase::Knocking: return NSLOCTEXT("Night", "ListenObjective", "Someone is outside. Check the door and stay inside.");
	case ENightPhase::AwaitingHorn: return NSLOCTEXT("Night", "HornObjective", "Use the HORN control beside the inside door switch to scare them off.");
	case ENightPhase::Complete: return NSLOCTEXT("Night", "CompleteObjective", "Encounter complete. Use the HORN control to restart.");
	default: return NSLOCTEXT("Night", "WaitObjective", "Listen. Wait for the footsteps to fade.");
	}
}

void ANightDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	CueAudio->Stop();
	if (IsValid(FirewoodTask)) FirewoodTask->OnTaskStateChanged.RemoveDynamic(this, &ANightDirector::HandleTaskChanged);
	if (IsValid(FilmingTripod)) FilmingTripod->OnClipChanged.RemoveDynamic(this, &ANightDirector::HandleClipChanged);
	if (Player.IsValid()) Player->SetNightDirector(nullptr);
	Super::EndPlay(EndPlayReason);
}
