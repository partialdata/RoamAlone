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
	FirePitRoot = Cast<USceneComponent>(FirePitRootReference.GetComponent(this));
	PlacedWoodRoot = Cast<USceneComponent>(PlacedWoodRootReference.GetComponent(this));
	bTaskReady = FirePitRoot && PlacedWoodRoot && PlacedWoodRoot->IsAttachedTo(FirePitRoot)
		&& !PickupReferences.IsEmpty();
	for (const FComponentReference& Reference : PickupReferences)
	{
		UPrimitiveComponent* Log = Cast<UPrimitiveComponent>(Reference.GetComponent(this));
		bTaskReady &= IsValid(Log) && Log->GetOwner() == this && !IsInGroup(Log, FirePitRoot)
			&& !PickupPrimitives.Contains(Log);
		PickupPrimitives.Add(Log);
	}
	if (!bTaskReady)
	{
		UE_LOG(LogRoamAlone, Error, TEXT("%s: assign unique log meshes outside the fire-pit assembly, and placed wood under the pit."), *GetName());
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
	if (TaskState == EFirewoodTaskState::Placed)
	{
		return NSLOCTEXT("FirewoodTask", "CompleteObjective", "Firewood placed. Task complete.");
	}
	return FText::Format(GetCollectedLogCount() < GetRequiredLogCount()
		? NSLOCTEXT("FirewoodTask", "CollectLogsObjective", "Collect firewood: {0}/{1}.")
		: NSLOCTEXT("FirewoodTask", "AllLogsObjective", "Firewood: {0}/{1}. Bring it to the fire pit."),
		FText::AsNumber(GetCollectedLogCount()), FText::AsNumber(GetRequiredLogCount()));
}

bool AFirewoodTask::TryCollectFirewood(APawn* Interactor, UPrimitiveComponent* Log)
{
	const int32 LogIndex = PickupPrimitives.IndexOfByKey(Log);
	if (!CanUseTask(Interactor) || TaskState == EFirewoodTaskState::Placed
		|| LogIndex == INDEX_NONE || CollectedLogIndices.Contains(LogIndex)
		|| (!CollectedLogIndices.IsEmpty() && Carrier.Get() != Interactor))
	{
		return false;
	}
	if (CollectedLogIndices.IsEmpty())
	{
		Carrier = Interactor;
		Interactor->OnDestroyed.AddDynamic(this, &AFirewoodTask::HandleCarrierDestroyed);
	}
	CollectedLogIndices.Add(LogIndex);
	TaskState = EFirewoodTaskState::Carrying;
	PublishTaskChange();
	return true;
}

bool AFirewoodTask::TryPlaceFirewood(APawn* Interactor)
{
	if (!CanUseTask(Interactor) || TaskState != EFirewoodTaskState::Carrying || Carrier.Get() != Interactor
		|| GetCollectedLogCount() != GetRequiredLogCount())
	{
		return false;
	}
	ClearCarrier();
	TaskState = EFirewoodTaskState::Placed;
	PublishTaskChange();
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
	if (TaskState != EFirewoodTaskState::Available || !CollectedLogIndices.IsEmpty())
	{
		CollectedLogIndices.Reset();
		TaskState = EFirewoodTaskState::Available;
		PublishTaskChange();
	}
}

void AFirewoodTask::HandleCarrierDestroyed(AActor* DestroyedActor)
{
	// The weak pointer may already be invalid during destruction. There is only one bound carrier.
	ResetTask();
}

void AFirewoodTask::PublishTaskChange()
{
	UpdatePresentation();
	OnTaskStateChanged.Broadcast();
}

void AFirewoodTask::UpdatePresentation()
{
	if (!bTaskReady)
	{
		return;
	}
	for (int32 Index = 0; Index < PickupPrimitives.Num(); ++Index)
	{
		UPrimitiveComponent* Component = PickupPrimitives[Index];
		if (IsValid(Component))
		{
			const bool bCollected = CollectedLogIndices.Contains(Index);
			Component->SetHiddenInGame(bCollected, true);
			Component->SetCollisionEnabled(bCollected ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
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
	const int32 LogIndex = PickupPrimitives.IndexOfByKey(Hit.GetComponent());
	if (LogIndex != INDEX_NONE && !CollectedLogIndices.Contains(LogIndex) && TaskState != EFirewoodTaskState::Placed)
	{
		return NSLOCTEXT("FirewoodTask", "CollectLogPrompt", "Collect firewood log");
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
	if (PickupPrimitives.Contains(Hit.GetComponent()))
	{
		return TryCollectFirewood(Interactor, Hit.GetComponent());
	}
	if (TryPlaceFirewood(Interactor))
	{
		Feedback = NSLOCTEXT("FirewoodTask", "PlacedFeedback", "Firewood placed. The fire pit is ready.");
		return true;
	}
	Feedback = TaskState == EFirewoodTaskState::Placed
		? NSLOCTEXT("FirewoodTask", "AlreadyPlacedFeedback", "The firewood is already at the fire pit.")
		: GetCollectedLogCount() < GetRequiredLogCount()
			? FText::Format(NSLOCTEXT("FirewoodTask", "MissingLogsFeedback", "Collect all the firewood first ({0}/{1})."),
				FText::AsNumber(GetCollectedLogCount()), FText::AsNumber(GetRequiredLogCount()))
			: NSLOCTEXT("FirewoodTask", "OtherCarrierFeedback", "Only the carrier can place the firewood.");
	return false;
}

void AFirewoodTask::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearCarrier();
	Super::EndPlay(EndPlayReason);
}
