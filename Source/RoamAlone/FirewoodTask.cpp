#include "FirewoodTask.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "RoamAlone.h"

AFirewoodTask::AFirewoodTask()
{
	PrimaryActorTick.bCanEverTick = false;
	TaskRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TaskRoot"));
	TaskRoot->SetMobility(EComponentMobility::Static);
	SetRootComponent(TaskRoot);
}

void AFirewoodTask::BeginPlay()
{
	Super::BeginPlay();
	PickupRoot = Cast<USceneComponent>(PickupRootReference.GetComponent(this));
	FirePitRoot = Cast<USceneComponent>(FirePitRootReference.GetComponent(this));
	PlacedWoodRoot = Cast<USceneComponent>(PlacedWoodRootReference.GetComponent(this));
	TArray<UPrimitiveComponent*> Primitives;
	GetComponents(Primitives);
	for (UPrimitiveComponent* Component : Primitives)
	{
		if (IsInGroup(Component, PickupRoot))
		{
			PickupPrimitives.Add(Component);
		}
	}
	bTaskReady = PickupRoot && FirePitRoot && PlacedWoodRoot && PickupRoot != FirePitRoot
		&& PlacedWoodRoot->IsAttachedTo(FirePitRoot) && !FirePitRoot->IsAttachedTo(PickupRoot)
		&& !PickupPrimitives.IsEmpty();
	if (!bTaskReady)
	{
		UE_LOG(LogRoamAlone, Error, TEXT("%s: assign distinct pickup and fire-pit assemblies, with placed wood under the pit and a usable pickup mesh."), *GetName());
		return;
	}
	UpdatePresentation();
	OnTaskStateChanged.Broadcast();
}

bool AFirewoodTask::CanUseTask(const APawn* Interactor) const
{
	return bTaskReady && IsValid(Interactor) && Interactor->GetWorld() == GetWorld() && Interactor->IsPlayerControlled();
}

bool AFirewoodTask::IsInGroup(const UPrimitiveComponent* Component, const USceneComponent* Group) const
{
	return IsValid(Component) && IsValid(Group) && (Component == Group || Component->IsAttachedTo(Group));
}

FText AFirewoodTask::GetObjectiveText() const
{
	if (!bTaskReady)
	{
		return FText::GetEmpty();
	}
	switch (TaskState)
	{
	case EFirewoodTaskState::Available:
		return NSLOCTEXT("FirewoodTask", "CollectObjective", "Collect firewood.");
	case EFirewoodTaskState::Carrying:
		return NSLOCTEXT("FirewoodTask", "CarryObjective", "Carrying firewood. Bring it to the fire pit.");
	case EFirewoodTaskState::Placed:
		return NSLOCTEXT("FirewoodTask", "CompleteObjective", "Firewood placed. Task complete.");
	default:
		return FText::GetEmpty();
	}
}

bool AFirewoodTask::TryCollectFirewood(APawn* Interactor)
{
	if (!CanUseTask(Interactor) || TaskState != EFirewoodTaskState::Available)
	{
		return false;
	}
	Carrier = Interactor;
	Interactor->OnDestroyed.AddDynamic(this, &AFirewoodTask::HandleCarrierDestroyed);
	CommitState(EFirewoodTaskState::Carrying);
	return true;
}

bool AFirewoodTask::TryPlaceFirewood(APawn* Interactor)
{
	if (!CanUseTask(Interactor) || TaskState != EFirewoodTaskState::Carrying || Carrier.Get() != Interactor)
	{
		return false;
	}
	ClearCarrier();
	CommitState(EFirewoodTaskState::Placed);
	return true;
}

void AFirewoodTask::ClearCarrier()
{
	if (APawn* Pawn = Carrier.Get())
	{
		Pawn->OnDestroyed.RemoveDynamic(this, &AFirewoodTask::HandleCarrierDestroyed);
	}
	Carrier.Reset();
}

void AFirewoodTask::ResetTask()
{
	ClearCarrier();
	CommitState(EFirewoodTaskState::Available);
}

void AFirewoodTask::HandleCarrierDestroyed(AActor* DestroyedActor)
{
	// The weak pointer may already be invalid during destruction. There is only one bound carrier.
	ResetTask();
}

void AFirewoodTask::CommitState(EFirewoodTaskState NewState)
{
	if (TaskState == NewState)
	{
		return;
	}
	TaskState = NewState;
	UpdatePresentation();
	OnTaskStateChanged.Broadcast();
}

void AFirewoodTask::UpdatePresentation()
{
	if (!bTaskReady)
	{
		return;
	}
	const bool bAvailable = TaskState == EFirewoodTaskState::Available;
	PickupRoot->SetHiddenInGame(!bAvailable, true);
	for (UPrimitiveComponent* Component : PickupPrimitives)
	{
		if (IsValid(Component))
		{
			Component->SetCollisionEnabled(bAvailable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
	}
	PlacedWoodRoot->SetHiddenInGame(TaskState != EFirewoodTaskState::Placed, true);
}

FText AFirewoodTask::GetInteractionPrompt_Implementation(APawn* Interactor, const FHitResult& Hit) const
{
	if (!CanUseTask(Interactor) || Hit.GetActor() != this)
	{
		return FText::GetEmpty();
	}
	if (IsInGroup(Hit.GetComponent(), PickupRoot) && TaskState == EFirewoodTaskState::Available)
	{
		return NSLOCTEXT("FirewoodTask", "CollectPrompt", "Collect firewood bundle");
	}
	if (IsInGroup(Hit.GetComponent(), FirePitRoot))
	{
		return TaskState == EFirewoodTaskState::Placed
			? NSLOCTEXT("FirewoodTask", "CheckPrompt", "Check fire pit")
			: NSLOCTEXT("FirewoodTask", "PlacePrompt", "Place firewood at the fire pit");
	}
	return FText::GetEmpty();
}

bool AFirewoodTask::TryInteract_Implementation(APawn* Interactor, const FHitResult& Hit, FText& Feedback)
{
	Feedback = FText::GetEmpty();
	if (GetInteractionPrompt_Implementation(Interactor, Hit).IsEmpty())
	{
		return false;
	}
	if (IsInGroup(Hit.GetComponent(), PickupRoot))
	{
		return TryCollectFirewood(Interactor);
	}
	if (TryPlaceFirewood(Interactor))
	{
		Feedback = NSLOCTEXT("FirewoodTask", "PlacedFeedback", "Firewood placed. The fire pit is ready.");
		return true;
	}
	Feedback = TaskState == EFirewoodTaskState::Available
		? NSLOCTEXT("FirewoodTask", "MissingWoodFeedback", "Collect the firewood bundle first.")
		: TaskState == EFirewoodTaskState::Placed
			? NSLOCTEXT("FirewoodTask", "AlreadyPlacedFeedback", "The firewood is already at the fire pit.")
			: NSLOCTEXT("FirewoodTask", "OtherCarrierFeedback", "Only the carrier can place this bundle.");
	return false;
}

void AFirewoodTask::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearCarrier();
	Super::EndPlay(EndPlayReason);
}
