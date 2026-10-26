#include "InteractionComponent.h"

#include "Interactable.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Templates/UnrealTemplate.h"
#include "TimerManager.h"

UInteractionComponent::UInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bAutoActivate = true;
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	StartRefreshTimer();
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	ActiveViewSource.Reset();
	ClearPresentation();
	Super::EndPlay(EndPlayReason);
}

void UInteractionComponent::Activate(bool bReset)
{
	Super::Activate(bReset);
	if (HasBegunPlay())
	{
		StartRefreshTimer();
	}
}

void UInteractionComponent::Deactivate()
{
	Super::Deactivate();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
	}
	ClearPresentation();
}

void UInteractionComponent::StartRefreshTimer()
{
	if (IsActive())
	{
		GetWorld()->GetTimerManager().SetTimer(RefreshTimer, this,
			&UInteractionComponent::RefreshTarget, FMath::Max(0.02f, TargetRefreshInterval), true);
	}
}

void UInteractionComponent::SetViewSource(USceneComponent* ViewSource)
{
	// A pawn may only nominate its own camera/component as its view source.
	ActiveViewSource = IsValid(ViewSource) && ViewSource->GetOwner() == GetOwner() ? ViewSource : nullptr;
	if (HasBegunPlay())
	{
		RefreshTarget();
	}
}

bool UInteractionComponent::FindTarget(FHitResult& OutHit, FText& OutPrompt) const
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsActive() || !IsValid(Pawn) || !Pawn->IsLocallyControlled() || !GetWorld())
	{
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	if (const USceneComponent* View = ActiveViewSource.Get())
	{
		ViewLocation = View->GetComponentLocation();
		ViewRotation = View->GetComponentRotation();
	}
	else if (const APlayerController* Player = Cast<APlayerController>(Pawn->GetController()))
	{
		Player->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else
	{
		return false;
	}

	FCollisionQueryParams Query(SCENE_QUERY_STAT(InteractionTarget), false, Pawn);
	FHitResult AimHit;
	if (!GetWorld()->LineTraceSingleByChannel(AimHit, ViewLocation,
		ViewLocation + ViewRotation.Vector() * AimTraceDistance, TraceChannel, Query))
	{
		return false;
	}
	AActor* Target = AimHit.GetActor();
	if (!IsValid(Target) || !Target->GetClass()->ImplementsInterface(UInteractable::StaticClass()))
	{
		return false;
	}

	// The orbiting camera may see around a wall or beyond the character's reach.
	const FVector Eye = Pawn->GetPawnViewLocation();
	const FVector Delta = AimHit.ImpactPoint - Eye;
	if (Delta.IsNearlyZero() || Delta.SizeSquared() > FMath::Square(ReachDistance))
	{
		return false;
	}
	FHitResult ReachHit;
	if (!GetWorld()->LineTraceSingleByChannel(ReachHit, Eye,
		AimHit.ImpactPoint + Delta.GetSafeNormal() * 3.0f, TraceChannel, Query)
		|| ReachHit.GetActor() != Target || ReachHit.GetComponent() != AimHit.GetComponent())
	{
		return false;
	}
	// Both queries must hit the same surface, not another part of a large actor around an obstruction.
	if (FVector::DistSquared(ReachHit.ImpactPoint, AimHit.ImpactPoint) > FMath::Square(6.0f))
	{
		return false;
	}
	OutPrompt = IInteractable::Execute_GetInteractionPrompt(Target, Pawn, ReachHit);
	if (!IsValid(Target) || OutPrompt.IsEmpty())
	{
		return false;
	}
	OutHit = ReachHit;
	return true;
}

bool UInteractionComponent::TryInteract()
{
	if (bExecutingInteraction)
	{
		return false;
	}
	TGuardValue<bool> Guard(bExecutingInteraction, true);
	FHitResult Hit;
	FText NewPrompt;
	if (!FindTarget(Hit, NewPrompt))
	{
		ClearPresentation();
		return false;
	}

	AActor* Target = Hit.GetActor();
	FText NewFeedback;
	const bool bSucceeded = IInteractable::Execute_TryInteract(Target, Cast<APawn>(GetOwner()), Hit, NewFeedback);
	// Blueprint implementations may destroy themselves or disable interaction during the call.
	if (!IsActive() || !IsValid(Target) || !IsValid(GetOwner()))
	{
		ClearPresentation();
		return bSucceeded;
	}
	FeedbackUntil = GetWorld()->GetTimeSeconds() + 2.0;
	PublishPresentation(Target, NewPrompt, NewFeedback);
	RefreshTarget();
	return bSucceeded;
}

void UInteractionComponent::Interact()
{
	TryInteract();
}

void UInteractionComponent::RefreshTarget()
{
	FHitResult Hit;
	FText NewPrompt;
	if (!FindTarget(Hit, NewPrompt))
	{
		ClearPresentation();
		return;
	}
	const bool bKeepFeedback = Hit.GetActor() == FocusedActor.Get()
		&& GetWorld()->GetTimeSeconds() < FeedbackUntil;
	PublishPresentation(Hit.GetActor(), NewPrompt, bKeepFeedback ? Feedback : FText::GetEmpty());
}

void UInteractionComponent::ClearPresentation()
{
	FeedbackUntil = 0;
	PublishPresentation(nullptr, FText::GetEmpty(), FText::GetEmpty());
}

void UInteractionComponent::PublishPresentation(AActor* Target, const FText& NewPrompt, const FText& NewFeedback)
{
	if (FocusedActor.Get() != Target || !Prompt.EqualTo(NewPrompt) || !Feedback.EqualTo(NewFeedback))
	{
		FocusedActor = Target;
		Prompt = NewPrompt;
		Feedback = NewFeedback;
		OnPresentationChanged.Broadcast(Prompt, Feedback);
	}
}
