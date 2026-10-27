#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FirewoodTask.h"
#include "InteractionComponent.h"
#include "RoamAloneCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"

namespace
{
	struct FFirewoodTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AFirewoodTask* Task = nullptr;
		ARoamAloneCharacter* Character = nullptr;
		USceneComponent* View = nullptr;

		FFirewoodTestWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			UClass* TaskClass = LoadClass<AFirewoodTask>(nullptr, TEXT("/Game/VanPrototype/Blueprints/BP_FirewoodTask.BP_FirewoodTask_C"));
			UClass* PawnClass = LoadClass<ARoamAloneCharacter>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C"));
			if (!TaskClass || !PawnClass) return;
			Task = World->SpawnActor<AFirewoodTask>(TaskClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
			Character = World->SpawnActor<ARoamAloneCharacter>(PawnClass, FVector(0, 0, 92), FRotator::ZeroRotator, Params);
			APlayerController* Controller = World->SpawnActor<APlayerController>();
			Controller->SetAsLocalPlayerController();
			// No GameMode login runs in this transient world. UE 5.8 identifies a human pawn by its PlayerState.
			Controller->SetPlayerState(World->SpawnActor<APlayerState>());
			Controller->Possess(Character);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();
			View = NewObject<USceneComponent>(Character);
			View->RegisterComponent();
		}

		~FFirewoodTestWorld()
		{
			World->EndPlay(EEndPlayReason::EndPlayInEditor);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		UPrimitiveComponent* Primitive(FName Name) const
		{
			TArray<UPrimitiveComponent*> Components;
			Task->GetComponents(Components);
			for (UPrimitiveComponent* Component : Components)
			{
				if (Component->GetFName() == Name) return Component;
			}
			return nullptr;
		}

		bool InteractAt(UPrimitiveComponent* Target, float Distance = 125)
		{
			const FVector Center = Target->Bounds.Origin;
			Character->SetActorLocation(FVector(Center.X, Center.Y - Distance, 92));
			const FVector Eye = Character->GetPawnViewLocation();
			View->SetWorldLocationAndRotation(Eye, (Center - Eye).Rotation());
			Character->GetInteractionComponent()->SetViewSource(View);
			return Character->GetInteractionComponent()->TryInteract();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirewoodStateTest, "RoamAlone.Firewood.StateAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFirewoodStateTest::RunTest(const FString& Parameters)
{
	for (int32 Run = 0; Run < 2; ++Run)
	{
		FFirewoodTestWorld Test;
		if (!TestNotNull(TEXT("Authored task loads"), Test.Task) || !TestNotNull(TEXT("Player loads"), Test.Character)) return false;
		if (!TestTrue(TEXT("Test pawn is initialized as a human player"), Test.Character->IsPlayerControlled())) return false;
		UPrimitiveComponent* Pickup = Test.Primitive(TEXT("PickupLog1"));
		UPrimitiveComponent* Placed = Test.Primitive(TEXT("PlacedLog1"));
		if (!TestNotNull(TEXT("Pickup log exists"), Pickup) || !TestNotNull(TEXT("Placed log exists"), Placed)) return false;
		const FText InitialObjective = Test.Task->GetObjectiveText();
		TestFalse(TEXT("Initial objective is available"), InitialObjective.IsEmpty());
		TestEqual(TEXT("Each clean world starts available"), Test.Task->GetTaskState(), EFirewoodTaskState::Available);
		TestFalse(TEXT("Cannot place wood before collecting it"), Test.Task->TryPlaceFirewood(Test.Character));
		TestFalse(TEXT("Null collector is rejected"), Test.Task->TryCollectFirewood(nullptr));
		TestTrue(TEXT("Pickup starts visible"), !Pickup->bHiddenInGame);
		TestTrue(TEXT("Placed stack starts hidden"), Placed->bHiddenInGame);
		TestTrue(TEXT("Player collects once"), Test.Task->TryCollectFirewood(Test.Character));
		TestFalse(TEXT("Repeated collection cannot duplicate wood"), Test.Task->TryCollectFirewood(Test.Character));
		TestEqual(TEXT("Task remembers carrying state"), Test.Task->GetTaskState(), EFirewoodTaskState::Carrying);
		TestTrue(TEXT("Collected bundle is hidden"), Pickup->bHiddenInGame);
		TestEqual(TEXT("Collected bundle no longer intercepts traces"), Pickup->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Objective changes when carrying"), InitialObjective.EqualTo(Test.Task->GetObjectiveText()));
		APawn* OtherPawn = Test.World->SpawnActor<APawn>();
		APlayerController* OtherController = Test.World->SpawnActor<APlayerController>();
		OtherController->SetPlayerState(Test.World->SpawnActor<APlayerState>());
		OtherController->Possess(OtherPawn);
		TestFalse(TEXT("Another pawn cannot place the carrier's bundle"), Test.Task->TryPlaceFirewood(OtherPawn));
		TestTrue(TEXT("Carrier places the bundle"), Test.Task->TryPlaceFirewood(Test.Character));
		TestFalse(TEXT("Repeated placement is a no-op"), Test.Task->TryPlaceFirewood(Test.Character));
		TestFalse(TEXT("Completed task cannot be collected again"), Test.Task->TryCollectFirewood(Test.Character));
		TestEqual(TEXT("Task completes"), Test.Task->GetTaskState(), EFirewoodTaskState::Placed);
		TestTrue(TEXT("Placed stack becomes visible"), !Placed->bHiddenInGame);
		Test.Task->ResetTask();
		Test.Task->ResetTask();
		TestEqual(TEXT("Reset restores initial state safely"), Test.Task->GetTaskState(), EFirewoodTaskState::Available);
		TestTrue(TEXT("Reset restores objective"), InitialObjective.EqualTo(Test.Task->GetObjectiveText()));
		TestTrue(TEXT("Reset restores pickup visibility"), !Pickup->bHiddenInGame);
		TestTrue(TEXT("Reset empties pit"), Placed->bHiddenInGame);
		TestTrue(TEXT("Bundle can be collected after reset"), Test.Task->TryCollectFirewood(Test.Character));
		Test.Character->Destroy();
		TestEqual(TEXT("Losing the carrier restores a collectible bundle"), Test.Task->GetTaskState(), EFirewoodTaskState::Available);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFirewoodInteractionTest, "RoamAlone.Firewood.InteractionRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFirewoodInteractionTest::RunTest(const FString& Parameters)
{
	FFirewoodTestWorld Test;
	if (!TestNotNull(TEXT("Task loads"), Test.Task) || !TestNotNull(TEXT("Player loads"), Test.Character)) return false;
	if (!TestTrue(TEXT("Test pawn is initialized as a human player"), Test.Character->IsPlayerControlled())) return false;
	UPrimitiveComponent* Pickup = Test.Primitive(TEXT("PickupLog1"));
	UPrimitiveComponent* Pit = Test.Primitive(TEXT("PitTarget"));
	if (!TestNotNull(TEXT("Pickup target exists"), Pickup) || !TestNotNull(TEXT("Pit target exists"), Pit)) return false;
	TestFalse(TEXT("Empty-handed pit interaction refuses clearly"), Test.InteractAt(Pit));
	TestFalse(TEXT("Missing bundle gives feedback"), Test.Character->GetInteractionComponent()->GetFeedback().IsEmpty());
	TestFalse(TEXT("Far-away collection is refused"), Test.InteractAt(Pickup, 400));
	AActor* WallActor = Test.World->SpawnActor<AActor>();
	UBoxComponent* Wall = NewObject<UBoxComponent>(WallActor);
	WallActor->SetRootComponent(Wall);
	Wall->SetBoxExtent(FVector(100, 5, 120));
	Wall->SetCollisionProfileName(TEXT("BlockAll"));
	Wall->RegisterComponent();
	WallActor->SetActorLocation(Pickup->Bounds.Origin + FVector(0, -60, 80));
	TestFalse(TEXT("Collection through an obstruction is refused"), Test.InteractAt(Pickup));
	TestEqual(TEXT("Failed interaction leaves task available"), Test.Task->GetTaskState(), EFirewoodTaskState::Available);
	WallActor->Destroy();
	TestTrue(TEXT("Existing interaction component collects the bundle"), Test.InteractAt(Pickup));
	TestFalse(TEXT("Hidden pickup cannot be collected again"), Test.InteractAt(Pickup));
	TestTrue(TEXT("Existing interaction component places at the pit"), Test.InteractAt(Pit));
	TestEqual(TEXT("Route completes task"), Test.Task->GetTaskState(), EFirewoodTaskState::Placed);
	TestFalse(TEXT("Completed pit does not accept a second placement"), Test.InteractAt(Pit));
	TestFalse(TEXT("Completed pit gives brief feedback"), Test.Character->GetInteractionComponent()->GetFeedback().IsEmpty());
	Test.Task->ResetTask();
	TestTrue(TEXT("Route can start again after reset"), Test.InteractAt(Pickup));
	Test.Task->ResetTask();
	TestFalse(TEXT("Reset while carrying prevents stale placement"), Test.InteractAt(Pit));
	return true;
}

#endif
