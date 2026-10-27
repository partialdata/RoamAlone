#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GreyboxVan.h"
#include "RoamAloneCharacter.h"
#include "InteractionComponent.h"
#include "Interactable.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"

namespace
{
	struct FVanTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AGreyboxVan* Van = nullptr;
		ARoamAloneCharacter* Character = nullptr;
		APlayerController* Controller = nullptr;
		USceneComponent* InteractionView = nullptr;

		FVanTestWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			UClass* VanClass = LoadClass<AGreyboxVan>(nullptr, TEXT("/Game/VanPrototype/Blueprints/BP_GreyboxVan.BP_GreyboxVan_C"));
			if (!VanClass) return;
			Van = World->SpawnActor<AGreyboxVan>(VanClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
			if (!Van) return;
			UClass* PawnClass = LoadClass<ARoamAloneCharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
			if (PawnClass)
			{
				Character = World->SpawnActor<ARoamAloneCharacter>(PawnClass, FVector(-15, -400, 161.1), FRotator(0, 90, 0), Params);
				Controller = World->SpawnActor<APlayerController>();
				// This transient world has no GameMode login flow to designate the local player.
				Controller->SetAsLocalPlayerController();
				Controller->Possess(Character);
				World->InitializeActorsForPlay(FURL());
				World->GetWorldSettings()->NotifyBeginPlay();
				InteractionView = NewObject<USceneComponent>(Character);
				InteractionView->RegisterComponent();
			}
		}

		~FVanTestWorld()
		{
			World->EndPlay(EEndPlayReason::EndPlayInEditor);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		void Move(FVector Position)
		{
			Character->SetActorLocation(Position);
			Character->Tick(1.0f / 60);
		}

		void AimAt(FVector HitLocation)
		{
			const FVector Eye = Character->GetPawnViewLocation();
			InteractionView->SetWorldLocationAndRotation(Eye, (HitLocation - Eye).Rotation());
			Character->GetInteractionComponent()->SetViewSource(InteractionView);
		}

		bool InteractAt(FVector HitLocation)
		{
			AimAt(HitLocation);
			return Character->GetInteractionComponent()->TryInteract();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanDoorSafetyTest, "RoamAlone.Van.DoorSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVanDoorSafetyTest::RunTest(const FString& Parameters)
{
	FVanTestWorld Test;
	if (!TestNotNull(TEXT("Template character loads"), Test.Character)) return false;
	TestFalse(TEXT("Door starts closed"), Test.Van->IsDoorOpen());
	TestFalse(TEXT("Repeated close is a no-op"), Test.Van->TrySetDoorOpen(false));
	TestTrue(TEXT("Clear door opens"), Test.Van->TrySetDoorOpen(true));
	TestFalse(TEXT("Repeated open is a no-op"), Test.Van->TrySetDoorOpen(true));
	Test.Move(FVector(-15, -120, 161.1));
	TestFalse(TEXT("Closing on capsule is refused"), Test.Van->TrySetDoorOpen(false));
	TestTrue(TEXT("Refusal preserves door state"), Test.Van->IsDoorOpen());
	Test.Move(FVector(-15, -400, 161.1));
	TestTrue(TEXT("Clear door closes"), Test.Van->TrySetDoorOpen(false));
	Test.Move(FVector(-15, -155, 161.1));
	TestTrue(TEXT("Normal close-range approach can open the sliding door"), Test.InteractAt(FVector(-15, -101, 185)));
	Test.Move(FVector(-15, -120, 161.1));
	TestFalse(TEXT("Door still cannot close on the player at the threshold"), Test.Van->TrySetDoorOpen(false));
	Test.Move(FVector(-15, -400, 161.1));
	TestTrue(TEXT("Door closes after player moves clear"), Test.Van->TrySetDoorOpen(false));
	Test.Move(FVector(-145, -107.175, 161.1));
	TestFalse(TEXT("Blocked sliding destination refuses opening"), Test.Van->TrySetDoorOpen(true));
	TestFalse(TEXT("Blocked opening preserves closed state"), Test.Van->IsDoorOpen());
	Test.Move(FVector(-15, -400, 161.1));
	TestFalse(TEXT("Out-of-reach interaction refused"), Test.InteractAt(FVector(-15, -101, 185)));
	Test.Move(FVector(-15, -260, 161.1));
	TestTrue(TEXT("In-reach visible door can be opened"), Test.InteractAt(FVector(-15, -101, 185)));
	Test.Van->TrySetDoorOpen(false);
	AActor* Obstruction = Test.World->SpawnActor<AActor>();
	UBoxComponent* Wall = NewObject<UBoxComponent>(Obstruction);
	Obstruction->SetRootComponent(Wall);
	Wall->SetBoxExtent(FVector(150, 10, 120));
	Wall->SetCollisionProfileName(TEXT("BlockAll"));
	Wall->RegisterComponent();
	Obstruction->SetActorLocation(FVector(-15, -210, 185));
	TestFalse(TEXT("Character reach trace refuses interaction through a wall"), Test.InteractAt(FVector(-15, -101, 185)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanCameraRoundTripTest, "RoamAlone.Van.CameraRoundTrips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVanCameraRoundTripTest::RunTest(const FString& Parameters)
{
	FVanTestWorld Test;
	if (!TestNotNull(TEXT("Template character loads"), Test.Character)) return false;
	const float OriginalSpeed = Test.Character->GetCharacterMovement()->MaxWalkSpeed;
	for (int32 Run = 0; Run < 10; ++Run)
	{
		Test.Move(FVector(-15, -7.5, 161.1));
		TestEqual(TEXT("Crossing inside uses first person"), Test.Character->GetVanViewMode(), EVanViewMode::Inside);
		TestTrue(TEXT("Eye camera is active"), Test.Character->GetFirstPersonCamera()->IsActive());
		TestFalse(TEXT("Third-person camera is inactive"), Test.Character->GetFollowCamera()->IsActive());
		TestTrue(TEXT("Embodied local body visible"), Test.Character->GetFirstPersonBody()->IsVisible());
		TestTrue(TEXT("Local neck/head hidden"), Test.Character->GetFirstPersonBody()->IsBoneHiddenByName(TEXT("neck_01")));
		TestEqual(TEXT("World mesh retains full shadow/reflection representation"), Test.Character->GetMesh()->FirstPersonPrimitiveType, EFirstPersonPrimitiveType::WorldSpaceRepresentation);
		TestTrue(TEXT("Same pawn stays possessed"), Test.Controller->GetPawn() == Test.Character);
		Test.Move(FVector(-15, -400, 161.1));
		TestEqual(TEXT("Leaving restores third person"), Test.Character->GetVanViewMode(), EVanViewMode::Outside);
		TestTrue(TEXT("Follow camera restored"), Test.Character->GetFollowCamera()->IsActive());
		TestFalse(TEXT("First-person camera inactive"), Test.Character->GetFirstPersonCamera()->IsActive());
		TestEqual(TEXT("Outside walk speed restored"), Test.Character->GetCharacterMovement()->MaxWalkSpeed, OriginalSpeed);
		TestTrue(TEXT("Template movement orientation restored"), Test.Character->GetCharacterMovement()->bOrientRotationToMovement);
	}
	Test.Move(FVector(-15, -87.35, 161.1));
	TestEqual(TEXT("Outside edge does not enter early"), Test.Character->GetVanViewMode(), EVanViewMode::Outside);
	Test.Move(FVector(-15, -85.35, 161.1));
	TestEqual(TEXT("Crossing inner threshold enters"), Test.Character->GetVanViewMode(), EVanViewMode::Inside);
	Test.Move(FVector(-15, -90.35, 161.1));
	TestEqual(TEXT("Threshold dead band prevents jitter"), Test.Character->GetVanViewMode(), EVanViewMode::Inside);
	Test.Move(FVector(-15, -93.35, 161.1));
	TestEqual(TEXT("Crossing outer threshold leaves"), Test.Character->GetVanViewMode(), EVanViewMode::Outside);
	Test.Move(FVector(-15, -7.5, 161.1));
	Test.Van->Destroy();
	Test.Character->Tick(1.0f / 60);
	TestEqual(TEXT("Removing van safely restores outside view"), Test.Character->GetVanViewMode(), EVanViewMode::Outside);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanExitCameraBlendTest, "RoamAlone.Van.ExitCameraBlend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVanExitCameraBlendTest::RunTest(const FString& Parameters)
{
	FVanTestWorld Test;
	if (!TestNotNull(TEXT("Template character loads"), Test.Character)) return false;
	Test.Van->TrySetDoorOpen(true);
	Test.Move(FVector(-15, -7.5, 161.1));
	FMinimalViewInfo InsideView;
	Test.Character->CalcCamera(0.0f, InsideView);
	// Cross the threshold without a time step so the first outgoing frame is observable.
	Test.Character->SetActorLocation(FVector(-15, -100, 161.1));
	FMinimalViewInfo LastEyeView;
	Test.Character->CalcCamera(0.0f, LastEyeView);
	Test.Character->Tick(0.0f);
	const FVector Target(-15, -420, 225.1);
	Test.Character->GetFollowCamera()->SetWorldLocation(Target);
	FMinimalViewInfo StartView;
	Test.Character->CalcCamera(0.0f, StartView);
	TestTrue(TEXT("Exit begins a camera blend"), Test.Character->IsCameraTransitionActive());
	TestTrue(TEXT("First outgoing frame starts at the eye view without a positional cut"), StartView.Location.Equals(LastEyeView.Location, 0.1));
	TestTrue(TEXT("First outgoing FOV is continuous"), FMath::IsNearlyEqual(StartView.FOV, LastEyeView.FOV));
	Test.Character->Tick(0.2f);
	FMinimalViewInfo MidView;
	Test.Character->CalcCamera(0.0f, MidView);
	TestTrue(TEXT("Intermediate frame moves towards third person"),
		FVector::Dist(MidView.Location, Target) < FVector::Dist(StartView.Location, Target)
		&& FVector::Dist(MidView.Location, Target) > 1.0f);
	TestTrue(TEXT("FOV also eases between the camera settings"), MidView.FOV > StartView.FOV && MidView.FOV < Test.Character->GetFollowCamera()->FieldOfView);
	Test.Character->Tick(1.0f);
	FMinimalViewInfo EndView;
	Test.Character->CalcCamera(0.0f, EndView);
	TestFalse(TEXT("Exit blend finishes"), Test.Character->IsCameraTransitionActive());
	TestTrue(TEXT("Finished view reaches the third-person camera"), EndView.Location.Equals(Target, 0.1));
	TestTrue(TEXT("Pawn possession remains unchanged"), Test.Controller->GetPawn() == Test.Character);

	Test.Move(FVector(-15, -7.5, 161.1));
	Test.Character->SetActorLocation(FVector(-15, -100, 161.1));
	Test.Character->Tick(0.0f);
	Test.Character->GetFollowCamera()->SetWorldLocation(Target);
	Test.Character->Tick(0.2f);
	Test.Character->CalcCamera(0.0f, MidView);
	Test.Character->SetActorLocation(FVector(-15, -7.5, 161.1));
	FMinimalViewInfo BeforeReverse;
	Test.Character->CalcCamera(0.0f, BeforeReverse);
	Test.Character->Tick(0.0f);
	FMinimalViewInfo AfterReverse;
	Test.Character->CalcCamera(0.0f, AfterReverse);
	TestTrue(TEXT("Stepping back inside reverses from the current view"), AfterReverse.Location.Equals(BeforeReverse.Location, 0.1));
	Test.Character->Tick(1.0f);
	Test.Character->CalcCamera(0.0f, EndView);
	TestTrue(TEXT("Interrupted exit settles at first person"), EndView.Location.Equals(InsideView.Location, 0.1));
	TestTrue(TEXT("Embodied first-person body is restored"), Test.Character->GetFirstPersonBody()->IsVisible());

	Test.Character->SetActorLocation(FVector(-15, -100, 161.1));
	Test.Character->Tick(0.0f);
	Test.Character->GetFollowCamera()->SetWorldLocation(Target);
	AActor* Obstruction = Test.World->SpawnActor<AActor>();
	UBoxComponent* Wall = NewObject<UBoxComponent>(Obstruction);
	Obstruction->SetRootComponent(Wall);
	Wall->SetBoxExtent(FVector(150, 5, 200));
	Wall->SetCollisionProfileName(TEXT("BlockAll"));
	Wall->RegisterComponent();
	Obstruction->SetActorLocation(FVector(-15, -150, 200));
	Test.Character->Tick(0.3f);
	Test.Character->CalcCamera(0.0f, MidView);
	TestTrue(TEXT("Intermediate camera sweep stays on the near side of a wall"), MidView.Location.Y > -145.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanWalkThroughTest, "RoamAlone.Van.CollisionRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVanWalkThroughTest::RunTest(const FString& Parameters)
{
	FVanTestWorld Test;
	if (!TestNotNull(TEXT("Template character loads"), Test.Character)) return false;
	FHitResult Hit;
	Test.Character->SetActorLocation(FVector(-15, -7.5, 161.1), true, &Hit);
	TestTrue(TEXT("Closed door physically blocks entry"), Hit.bBlockingHit);
	Test.Character->Tick(1.0f / 60);
	TestEqual(TEXT("Blocked entry keeps third person"), Test.Character->GetVanViewMode(), EVanViewMode::Outside);
	Test.Move(FVector(-15, -400, 161.1));
	TestTrue(TEXT("Door opens from clear approach"), Test.Van->TrySetDoorOpen(true));
	Test.Character->SetActorLocation(FVector(-15, -7.5, 161.1), true, &Hit);
	TestFalse(TEXT("Open doorway admits full Quinn capsule"), Hit.bBlockingHit);
	Test.Character->Tick(1.0f / 60);
	TestEqual(TEXT("Successful entry switches camera"), Test.Character->GetVanViewMode(), EVanViewMode::Inside);
	TestTrue(TEXT("Wall-mounted inside switch can close the parked panel"), Test.InteractAt(FVector(42, -77, 185)));
	Test.Character->Tick(1.0f / 60);
	TestEqual(TEXT("Door position does not change camera mode"), Test.Character->GetVanViewMode(), EVanViewMode::Inside);
	Test.Character->SetActorLocation(FVector(-15, -400, 161.1), true, &Hit);
	TestTrue(TEXT("Closed exit remains physically blocked"), Hit.bBlockingHit);
	Test.Character->Tick(1.0f / 60);
	TestEqual(TEXT("Blocked exit preserves first person"), Test.Character->GetVanViewMode(), EVanViewMode::Inside);
	Test.Move(FVector(-15, -7.5, 161.1));
	TestTrue(TEXT("Wall-mounted inside switch can reopen the door"), Test.InteractAt(FVector(42, -77, 185)));
	Test.Character->SetActorLocation(FVector(-15, -400, 161.1), true, &Hit);
	TestFalse(TEXT("Open exit admits full capsule"), Hit.bBlockingHit);
	Test.Character->Tick(1.0f / 60);
	TestEqual(TEXT("Successful exit restores third person"), Test.Character->GetVanViewMode(), EVanViewMode::Outside);
	Test.Move(FVector(-15, -7.5, 161.1));
	Test.Character->SetActorLocation(FVector(-105, -7.5, 161.1), true, &Hit);
	TestFalse(TEXT("Full Quinn capsule can traverse furnished aisle to the bed"), Hit.bBlockingHit);
	Test.Character->SetActorLocation(FVector(-180, -7.5, 161.1), true, &Hit);
	TestTrue(TEXT("Raised bed blocks walking through furniture"), Hit.bBlockingHit);
	Test.Move(FVector(-15, -7.5, 161.1));
	Test.Character->SetActorLocation(FVector(-15, 65, 161.1), true, &Hit);
	TestTrue(TEXT("Galley collision bounds the narrow aisle"), Hit.bBlockingHit);
	Test.Move(FVector(-15, -7.5, 161.1));
	Test.Character->SetActorLocation(FVector(-15, 400, 161.1), true, &Hit);
	TestTrue(TEXT("Side wall/windows block capsule"), Hit.bBlockingHit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVanHandleMotionTest, "RoamAlone.Van.HandleFollowsPanel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVanHandleMotionTest::RunTest(const FString& Parameters)
{
	FVanTestWorld Test;
	if (!TestNotNull(TEXT("Authored van and character load"), Test.Character)) return false;
	const auto Find = [&Test](FName Name)
	{
		FComponentReference Reference;
		Reference.ComponentProperty = Name;
		return Cast<USceneComponent>(Reference.GetComponent(Test.Van));
	};
	USceneComponent* Root = Find(TEXT("SlidingDoorRoot"));
	USceneComponent* Panel = Find(TEXT("SlidingDoorPanel"));
	USceneComponent* Handle = Find(TEXT("DoorHandle"));
	USceneComponent* Switch = Find(TEXT("InsideDoorSwitch"));
	if (!TestNotNull(TEXT("Door root is authored in Blueprint"), Root)
		|| !TestNotNull(TEXT("Panel is authored in Blueprint"), Panel)
		|| !TestNotNull(TEXT("Handle is authored in Blueprint"), Handle)
		|| !TestNotNull(TEXT("Inside switch is authored in Blueprint"), Switch)) return false;
	TestTrue(TEXT("Handle is attached to the moving assembly"), Handle->IsAttachedTo(Root));
	const FVector ClosedHandle = Handle->GetComponentLocation();
	const FVector FixedSwitch = Switch->GetComponentLocation();
	const FTransform HandleInPanel = Handle->GetComponentTransform().GetRelativeTransform(Panel->GetComponentTransform());
	TestTrue(TEXT("Door opens"), Test.Van->TrySetDoorOpen(true));
	TestFalse(TEXT("Handle moves with the open door"), Handle->GetComponentLocation().Equals(ClosedHandle));
	TestTrue(TEXT("Handle keeps its exact offset from the panel"), HandleInPanel.Equals(Handle->GetComponentTransform().GetRelativeTransform(Panel->GetComponentTransform()), 0.01));
	TestTrue(TEXT("Inside switch remains mounted on the wall"), Switch->GetComponentLocation().Equals(FixedSwitch));
	TestTrue(TEXT("Door closes"), Test.Van->TrySetDoorOpen(false));
	TestTrue(TEXT("Handle returns with the door"), Handle->GetComponentLocation().Equals(ClosedHandle));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionRetargetTest, "RoamAlone.Interaction.Revalidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionRetargetTest::RunTest(const FString& Parameters)
{
	FVanTestWorld Test;
	if (!TestNotNull(TEXT("Authored pawn loads with its interaction component"), Test.Character)) return false;
	UInteractionComponent* Interaction = Test.Character->GetInteractionComponent();
	Test.Move(FVector(-15, -260, 161.1));
	Test.AimAt(FVector(-15, -101, 185));
	TestTrue(TEXT("Interaction component is active"), Interaction->IsActive());
	TestTrue(TEXT("Pawn is locally controlled"), Test.Character->IsLocallyControlled());
	TestTrue(TEXT("Van class implements the native interface"), Test.Van->GetClass()->ImplementsInterface(UInteractable::StaticClass()));
	TestTrue(TEXT("Interface actor is focused"), Interaction->GetFocusedActor() == Test.Van);
	TestFalse(TEXT("Focused door supplies a prompt"), Interaction->GetPrompt().IsEmpty());
	Test.Character->SetActorLocation(FVector(-15, -500, 161.1));
	TestFalse(TEXT("A cached nearby prompt does not authorize a now-distant action"), Interaction->TryInteract());
	TestFalse(TEXT("Rejected stale focus does not open the door"), Test.Van->IsDoorOpen());
	TestTrue(TEXT("Stale prompt clears immediately"), Interaction->GetPrompt().IsEmpty());

	Test.Move(FVector(-15, -260, 161.1));
	Test.AimAt(FVector(-15, -101, 185));
	// The camera can see a target which the pawn cannot physically reach around a wall.
	Interaction->SetViewSource(Test.InteractionView);
	Test.InteractionView->SetWorldLocation(FVector(-15, -150, 220));
	Test.InteractionView->SetWorldRotation((FVector(-15, -101, 185) - Test.InteractionView->GetComponentLocation()).Rotation());
	AActor* Obstruction = Test.World->SpawnActor<AActor>();
	UBoxComponent* Wall = NewObject<UBoxComponent>(Obstruction);
	Obstruction->SetRootComponent(Wall);
	Wall->SetBoxExtent(FVector(100, 5, 150));
	Wall->SetCollisionProfileName(TEXT("BlockAll"));
	Wall->RegisterComponent();
	Obstruction->SetActorLocation(FVector(-15, -210, 180));
	TestFalse(TEXT("Unobstructed camera cannot interact through a pawn-side wall"), Interaction->TryInteract());
	Obstruction->Destroy();
	Test.AimAt(FVector(-15, -101, 185));
	TestTrue(TEXT("Clear target can be used after obstruction is removed"), Interaction->TryInteract());
	Interaction->Deactivate();
	TestTrue(TEXT("Deactivation clears prompt"), Interaction->GetPrompt().IsEmpty());
	TestFalse(TEXT("Disabled component refuses input"), Interaction->TryInteract());
	Interaction->Activate();
	Test.Move(FVector(-15, -7.5, 161.1));
	Test.AimAt(FVector(42, -77, 185));
	TestTrue(TEXT("Reactivation can use the wall switch"), Interaction->TryInteract());
	Test.AimAt(FVector(-15, -101, 185));
	Test.Van->Destroy();
	TestFalse(TEXT("Destroyed target is safe before the next refresh timer"), Interaction->TryInteract());
	TestTrue(TEXT("Destroyed target leaves no prompt"), Interaction->GetPrompt().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionSurfaceTest, "RoamAlone.Interaction.SurfaceFiltering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInteractionSurfaceTest::RunTest(const FString& Parameters)
{
	FVanTestWorld Test;
	if (!TestNotNull(TEXT("Template character loads"), Test.Character)) return false;
	UInteractionComponent* Interaction = Test.Character->GetInteractionComponent();
	Test.Move(FVector(-200, -240, 161.1));
	Test.AimAt(FVector(-200, -101, 120));
	TestTrue(TEXT("A van wall offers no door interaction"), Interaction->GetPrompt().IsEmpty());
	TestFalse(TEXT("Looking at the van actor's non-door component cannot operate its door"), Interaction->TryInteract());
	Test.Move(FVector(-15, -260, 161.1));
	Test.AimAt(FVector(-15, -101, 185));
	TestTrue(TEXT("Panel offers interaction again"), Interaction->GetFocusedActor() == Test.Van);
	Test.InteractionView->SetWorldRotation(FRotator(90, 0, 0));
	TestFalse(TEXT("Action traces again after looking away without a timer update"), Interaction->TryInteract());
	TestFalse(TEXT("Looking away leaves door unchanged"), Test.Van->IsDoorOpen());
	return true;
}

#endif
