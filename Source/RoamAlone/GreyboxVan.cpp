#include "GreyboxVan.h"

#include "RoamAlone.h"
#include "RoamAloneCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

AGreyboxVan::AGreyboxVan()
{
	PrimaryActorTick.bCanEverTick = false;
	VanRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VanRoot"));
	VanRoot->SetMobility(EComponentMobility::Static);
	SetRootComponent(VanRoot);
	Interior = CreateDefaultSubobject<UBoxComponent>(TEXT("Interior"));
	Interior->SetupAttachment(VanRoot);
	Interior->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Interior->SetCollisionResponseToAllChannels(ECR_Ignore);
	Interior->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Interior->SetGenerateOverlapEvents(true);
	Interior->OnComponentBeginOverlap.AddDynamic(this, &AGreyboxVan::OnInteriorBegin);
	Interior->OnComponentEndOverlap.AddDynamic(this, &AGreyboxVan::OnInteriorEnd);
}

void AGreyboxVan::BeginPlay()
{
	Super::BeginPlay();
	ResolvedDoorRoot = Cast<USceneComponent>(DoorRootReference.GetComponent(this));
	ResolvedDoorPanel = Cast<UStaticMeshComponent>(DoorPanelReference.GetComponent(this));
	ResolvedInsideSwitch = Cast<UPrimitiveComponent>(InsideSwitchReference.GetComponent(this));
	ResolvedLockControl = Cast<UPrimitiveComponent>(LockControlReference.GetComponent(this));
	if (!ResolvedLockControl)
	{
		UE_LOG(LogRoamAlone, Warning, TEXT("%s: assign an inside lock control; locking is disabled until it is available."), *GetName());
	}
	bDoorReady = ResolvedDoorRoot && ResolvedDoorRoot->GetAttachParent() && ResolvedDoorPanel
		&& ResolvedDoorPanel->GetStaticMesh() && ResolvedDoorPanel->IsAttachedTo(ResolvedDoorRoot)
		&& ResolvedInsideSwitch;
	if (!bDoorReady)
	{
		UE_LOG(LogRoamAlone, Error, TEXT("%s: assign the door assembly, panel mesh, and inside switch in the van Blueprint."), *GetName());
		return;
	}
	ClosedDoorTransform = ResolvedDoorRoot->GetRelativeTransform();
}

bool AGreyboxVan::ContainsInteriorPoint(FVector WorldPoint, bool bAlreadyInside) const
{
	const FVector Local = Interior->GetComponentTransform().InverseTransformPosition(WorldPoint);
	const FVector Extent = Interior->GetUnscaledBoxExtent() + FVector(bAlreadyInside ? 3.0 : -3.0);
	return FMath::Abs(Local.X) < Extent.X && FMath::Abs(Local.Y) < Extent.Y && FMath::Abs(Local.Z) < Extent.Z;
}

bool AGreyboxVan::IsDoorComponent(const UPrimitiveComponent* Component) const
{
	return bDoorReady && IsValid(Component)
		&& (Component == ResolvedInsideSwitch || Component->IsAttachedTo(ResolvedDoorRoot));
}

bool AGreyboxVan::TrySetDoorOpen(bool bOpen)
{
	if (bDoorOpen == bOpen || (bOpen && bDoorLocked) || !bDoorReady
		|| !IsValid(ResolvedDoorRoot) || !IsValid(ResolvedDoorPanel))
	{
		return false;
	}
	FTransform Destination = ClosedDoorTransform;
	if (bOpen)
	{
		Destination.AddToTranslation(DoorOpenOffset);
	}
	const FTransform RootWorld = Destination * ResolvedDoorRoot->GetAttachParent()->GetComponentTransform();
	const FTransform PanelInRoot = ResolvedDoorPanel->GetComponentTransform().GetRelativeTransform(ResolvedDoorRoot->GetComponentTransform());
	const FTransform PanelWorld = PanelInRoot * RootWorld;
	const FBox Bounds = ResolvedDoorPanel->GetStaticMesh()->GetBoundingBox();
	const FVector Extent = Bounds.GetExtent() * PanelWorld.GetScale3D().GetAbs() + FVector(1);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(VanDoorClearance), false, this);
	if (GetWorld()->OverlapAnyTestByObjectType(PanelWorld.TransformPosition(Bounds.GetCenter()),
		PanelWorld.GetRotation(), FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeBox(Extent), Query))
	{
		return false;
	}
	ResolvedDoorRoot->SetRelativeTransform(Destination);
	bDoorOpen = bOpen;
	return true;
}

bool AGreyboxVan::TrySetLocked(bool bLocked)
{
	if (bDoorLocked == bLocked || !bDoorReady || (bLocked && (bDoorOpen || !IsValid(ResolvedLockControl))))
	{
		return false;
	}
	bDoorLocked = bLocked;
	return true;
}

FText AGreyboxVan::GetInteractionPrompt_Implementation(APawn* Interactor, const FHitResult& Hit) const
{
	if (!IsValid(Interactor) || Interactor->GetWorld() != GetWorld()
		|| Hit.GetActor() != this || !bDoorReady)
	{
		return FText::GetEmpty();
	}
	if (IsValid(ResolvedLockControl) && Hit.GetComponent() == ResolvedLockControl)
	{
		if (!ContainsInteriorPoint(Interactor->GetActorLocation()))
		{
			return FText::GetEmpty();
		}
		return bDoorLocked ? NSLOCTEXT("VanInteraction", "UnlockDoor", "Unlock door (locked)")
			: NSLOCTEXT("VanInteraction", "LockDoor", "Lock door (unlocked)");
	}
	if (!IsDoorComponent(Hit.GetComponent()))
	{
		return FText::GetEmpty();
	}
	if (bDoorLocked)
	{
		return NSLOCTEXT("VanInteraction", "LockedHandle", "Try door (locked)");
	}
	return bDoorOpen ? NSLOCTEXT("VanInteraction", "CloseDoor", "Close door")
		: NSLOCTEXT("VanInteraction", "OpenDoor", "Open door");
}

bool AGreyboxVan::TryInteract_Implementation(APawn* Interactor, const FHitResult& Hit, FText& Feedback)
{
	Feedback = FText::GetEmpty();
	if (GetInteractionPrompt_Implementation(Interactor, Hit).IsEmpty())
	{
		return false;
	}
	if (Hit.GetComponent() == ResolvedLockControl)
	{
		if (!TrySetLocked(!bDoorLocked))
		{
			Feedback = NSLOCTEXT("VanInteraction", "CloseBeforeLocking", "Close the door before locking it.");
			return false;
		}
		Feedback = bDoorLocked ? NSLOCTEXT("VanInteraction", "DoorLocked", "Door locked.")
			: NSLOCTEXT("VanInteraction", "DoorUnlocked", "Door unlocked. Use the door switch to open it.");
		return true;
	}
	if (bDoorLocked)
	{
		Feedback = NSLOCTEXT("VanInteraction", "LockedFeedback", "Door is locked. Use the inside LOCK tab to unlock it.");
		return false;
	}
	if (!TrySetDoorOpen(!bDoorOpen))
	{
		Feedback = NSLOCTEXT("VanInteraction", "BlockedDoor", "Door is blocked. Move clear of the panel.");
		return false;
	}
	return true;
}

void AGreyboxVan::OnInteriorBegin(UPrimitiveComponent* Overlapped, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bFromSweep, const FHitResult& Hit)
{
	if (ARoamAloneCharacter* Character = Cast<ARoamAloneCharacter>(OtherActor))
	{
		if (OtherComponent == Character->GetCapsuleComponent())
		{
			Character->SetNearbyVan(this);
		}
	}
}

void AGreyboxVan::OnInteriorEnd(UPrimitiveComponent* Overlapped, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 BodyIndex)
{
	if (ARoamAloneCharacter* Character = Cast<ARoamAloneCharacter>(OtherActor))
	{
		if (OtherComponent == Character->GetCapsuleComponent() && Character->GetNearbyVan() == this)
		{
			Character->SetNearbyVan(nullptr);
		}
	}
}

void AGreyboxVan::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TArray<AActor*> Overlapping;
	Interior->GetOverlappingActors(Overlapping, ARoamAloneCharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		ARoamAloneCharacter* Character = CastChecked<ARoamAloneCharacter>(Actor);
		if (Character->GetNearbyVan() == this)
		{
			Character->SetNearbyVan(nullptr);
		}
	}
	Super::EndPlay(EndPlayReason);
}
