#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "NightDirector.h"
#include "FirewoodTask.h"
#include "GreyboxVan.h"
#include "RoamAloneCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "TimerManager.h"

struct FNightEncounterTestWorld
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	ANightDirector* Director = nullptr;
	AFirewoodTask* Task = nullptr;
	AGreyboxVan* Van = nullptr;
	ARoamAloneCharacter* Player = nullptr;

	FNightEncounterTestWorld()
	{
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const auto Load = [](const TCHAR* Path) { return LoadClass<AActor>(nullptr, Path); };
		Van = World->SpawnActor<AGreyboxVan>(Load(TEXT("/Game/VanPrototype/Blueprints/BP_GreyboxVan.BP_GreyboxVan_C")), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		Task = World->SpawnActor<AFirewoodTask>(Load(TEXT("/Game/VanPrototype/Blueprints/BP_FirewoodTask.BP_FirewoodTask_C")), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		Player = World->SpawnActor<ARoamAloneCharacter>(Load(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C")), FVector(-15, -400, 161.1), FRotator::ZeroRotator, Params);
		Director = World->SpawnActor<ANightDirector>(Load(TEXT("/Game/VanPrototype/Blueprints/BP_NightDirector.BP_NightDirector_C")), FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!Van || !Task || !Player || !Director) return;
		Director->FirewoodTask = Task;
		Director->Van = Van;
		APlayerController* Controller = World->SpawnActor<APlayerController>();
		Controller->SetAsLocalPlayerController();
		Controller->SetPlayerState(World->SpawnActor<APlayerState>());
		Controller->Possess(Player);
		World->InitializeActorsForPlay(FURL());
		World->GetWorldSettings()->NotifyBeginPlay();
		Director->InitializeEncounter();
	}

	~FNightEncounterTestWorld()
	{
		World->EndPlay(EEndPlayReason::EndPlayInEditor);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}

	bool Ready() const { return Director && Director->bReady; }
	FHitResult HornHit() const
	{
		return FHitResult(Director, Director->ResolvedHornControl, FVector::ZeroVector, FVector::UpVector);
	}
	void MoveInside() { Player->SetActorLocation(FVector(-15, 0, 161.1)); }
	void Advance()
	{
		World->GetTimerManager().ClearTimer(Director->PhaseTimer);
		Director->AdvanceEncounter();
	}
	void CheckShelter() { Director->CheckShelter(); }
	void Deliver()
	{
		TArray<UPrimitiveComponent*> Meshes;
		Task->GetComponents(Meshes);
		for (UPrimitiveComponent* Mesh : Meshes)
		{
			if (Mesh->GetName().StartsWith(TEXT("PickupLog"))) Task->TryCollectFirewood(Player, Mesh);
		}
		Task->TryPlaceFirewood(Player);
	}
	bool HasPendingCue() const
	{
		return World->GetTimerManager().TimerExists(Director->PhaseTimer)
			|| World->GetTimerManager().TimerExists(Director->FootstepTimer)
			|| World->GetTimerManager().TimerExists(Director->ShelterTimer);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNightEncounterRoutes, "RoamAlone.Night.DoorBranchesAndResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightEncounterRoutes::RunTest(const FString& Parameters)
{
	for (int32 Branch = 0; Branch < 4; ++Branch)
	{
		FNightEncounterTestWorld Test;
		if (!TestTrue(TEXT("Authored director and its required references initialize"), Test.Ready())) return false;
		TestFalse(TEXT("Horn cannot skip the outdoor task"), Test.Director->TrySoundHorn(Test.Player));
		Test.Deliver();
		TestEqual(TEXT("Delivery gives a reason to retreat"), Test.Director->GetPhase(), ENightPhase::Retreat);
		Test.CheckShelter();
		TestEqual(TEXT("Outside player does not start the night event"), Test.Director->GetPhase(), ENightPhase::Retreat);
		Test.MoveInside();
		Test.CheckShelter();
		TestEqual(TEXT("Shelter provides a preparation interval"), Test.Director->GetPhase(), ENightPhase::Shelter);
		Test.Advance();
		TestEqual(TEXT("Footsteps precede knocking"), Test.Director->GetPhase(), ENightPhase::Approach);
		Test.Advance();
		if (Branch == 0) TestTrue(TEXT("Player can lock during the sequence"), Test.Van->TrySetLocked(true));
		if (Branch == 2) TestTrue(TEXT("Player can leave door open"), Test.Van->TrySetDoorOpen(true));
		APawn* Obstruction = nullptr;
		if (Branch == 3)
		{
			// A second character occupies the sliding door destination, not the playable interior.
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Obstruction = Test.World->SpawnActor<ARoamAloneCharacter>(Test.Player->GetClass(), FVector(-145, -107.175, 161.1), FRotator::ZeroRotator, Params);
		}
		Test.Advance();
		const EDoorAttemptResult Expected[] = {EDoorAttemptResult::Locked, EDoorAttemptResult::Opened, EDoorAttemptResult::AlreadyOpen, EDoorAttemptResult::Blocked};
		TestEqual(TEXT("Handle attempt evaluates current door state and clearance"), Test.Director->GetDoorAttemptResult(), Expected[Branch]);
		TestEqual(TEXT("Every branch permits a player response"), Test.Director->GetPhase(), ENightPhase::AwaitingHorn);
		TestEqual(TEXT("Door remains closed in locked and blocked cases"), Test.Van->IsDoorOpen(), Branch == 1 || Branch == 2);
		Test.Advance();
		TestEqual(TEXT("Repeated callback cannot repeat the door attempt"), Test.Director->GetDoorAttemptResult(), Expected[Branch]);
		TestFalse(TEXT("Null pawn cannot sound horn"), Test.Director->TrySoundHorn(nullptr));
		// Exercise Blueprint dispatch as the real interaction system does. Empty child overrides
		// must not silently replace the native prompt or response.
		const FHitResult HornHit = Test.HornHit();
		TestFalse(TEXT("Authored horn exposes its inherited interaction prompt"),
			IInteractable::Execute_GetInteractionPrompt(Test.Director, Test.Player, HornHit).IsEmpty());
		FText Feedback;
		TestTrue(TEXT("Inside player sounds horn through Blueprint interface dispatch"),
			IInteractable::Execute_TryInteract(Test.Director, Test.Player, HornHit, Feedback));
		TestEqual(TEXT("Horn interaction starts withdrawal"), Test.Director->GetPhase(), ENightPhase::Withdrawing);
		TestFalse(TEXT("Repeated horn cannot start duplicate withdrawal"), Test.Director->TrySoundHorn(Test.Player));
		if (Obstruction) Obstruction->Destroy();
		Test.Advance();
		TestEqual(TEXT("Withdrawal reaches fade"), Test.Director->GetPhase(), ENightPhase::DawnFade);
		Test.Advance();
		TestEqual(TEXT("Night resolves at morning"), Test.Director->GetPhase(), ENightPhase::Complete);
		TestTrue(TEXT("Development reset succeeds"), Test.Director->ResetEncounter());
		TestTrue(TEXT("Repeated reset succeeds"), Test.Director->ResetEncounter());
		TestEqual(TEXT("Reset clears task progress"), Test.Task->GetCollectedLogCount(), 0);
		TestEqual(TEXT("Reset clears result"), Test.Director->GetDoorAttemptResult(), EDoorAttemptResult::None);
		TestFalse(TEXT("Reset cancels all pending sequence timers"), Test.HasPendingCue());
		TestFalse(TEXT("Reset restores a closed unlocked door"), Test.Van->IsDoorOpen() || Test.Van->IsLocked());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNightEncounterPrerequisites, "RoamAlone.Night.EarlyReturnAndInterruptedSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNightEncounterPrerequisites::RunTest(const FString& Parameters)
{
	FNightEncounterTestWorld Test;
	if (!TestTrue(TEXT("Director initializes"), Test.Ready())) return false;
	Test.MoveInside();
	Test.CheckShelter();
	TestEqual(TEXT("Entering before the task cannot skip collection"), Test.Director->GetPhase(), ENightPhase::Gathering);
	Test.Deliver();
	Test.CheckShelter();
	Test.Player->SetActorLocation(FVector(-15, -400, 161.1));
	Test.Advance();
	TestEqual(TEXT("Critical cue waits if player steps outside"), Test.Director->GetPhase(), ENightPhase::Shelter);
	Test.MoveInside();
	Test.Advance();
	TestEqual(TEXT("Returning resumes the sequence"), Test.Director->GetPhase(), ENightPhase::Approach);
	TestTrue(TEXT("Reset mid-sequence succeeds"), Test.Director->ResetEncounter());
	TestFalse(TEXT("Interrupted timers are cleared"), Test.HasPendingCue());
	Test.Deliver();
	Test.CheckShelter();
	TestEqual(TEXT("Second run starts cleanly"), Test.Director->GetPhase(), ENightPhase::Shelter);
	return true;
}

#endif
