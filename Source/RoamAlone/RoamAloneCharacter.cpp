// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoamAloneCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "RoamAlone.h"
#include "GreyboxVan.h"
#include "VanPromptWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "InteractionComponent.h"

ARoamAloneCharacter::ARoamAloneCharacter()
{
	Interaction = CreateDefaultSubobject<UInteractionComponent>(TEXT("Interaction"));
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(InteriorEyeOffset);
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetAutoActivate(false);
	FirstPersonCamera->SetFieldOfView(InteriorFieldOfView);

	// Local body has its own pose so hiding the neck cannot alter world shadows/reflections.
	FirstPersonBody = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonBody"));
	FirstPersonBody->SetupAttachment(GetMesh());
	FirstPersonBody->SetCollisionProfileName(TEXT("NoCollision"));
	FirstPersonBody->SetGenerateOverlapEvents(false);
	FirstPersonBody->SetOnlyOwnerSee(true);
	FirstPersonBody->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
	FirstPersonBody->SetCastShadow(false);
	FirstPersonBody->SetVisibility(false);

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void ARoamAloneCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ARoamAloneCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ARoamAloneCharacter::DoJumpEnd);
		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, Interaction.Get(), &UInteractionComponent::Interact);
		}

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ARoamAloneCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ARoamAloneCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ARoamAloneCharacter::Look);
	}
	else
	{
		UE_LOG(LogRoamAlone, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ARoamAloneCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ARoamAloneCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ARoamAloneCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ARoamAloneCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ARoamAloneCharacter::DoJumpStart()
{
	if (VanViewMode == EVanViewMode::Outside)
	{
		Jump();
	}
}

void ARoamAloneCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void ARoamAloneCharacter::BeginPlay()
{
	Super::BeginPlay();
	FirstPersonCamera->SetRelativeLocation(InteriorEyeOffset);
	FirstPersonCamera->SetFieldOfView(InteriorFieldOfView);
	FirstPersonCamera->Deactivate();
	FollowCamera->Activate();
	FirstPersonBody->SetSkeletalMeshAsset(GetMesh()->GetSkeletalMeshAsset());
	FirstPersonBody->SetAnimInstanceClass(GetMesh()->GetAnimClass());
	for (int32 Index = 0; Index < GetMesh()->GetNumMaterials(); ++Index)
	{
		FirstPersonBody->SetMaterial(Index, GetMesh()->GetMaterial(Index));
	}
	FirstPersonBody->HideBoneByName(TEXT("neck_01"), EPhysBodyOp::PBO_None);
	FirstPersonBody->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Interaction->OnPresentationChanged.AddDynamic(this, &ARoamAloneCharacter::HandleInteractionPresentationChanged);
	Interaction->SetViewSource(FollowCamera);
	UpdatePrototypePrompt();
}

void ARoamAloneCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Interaction->OnPresentationChanged.RemoveDynamic(this, &ARoamAloneCharacter::HandleInteractionPresentationChanged);
	SetVanViewMode(EVanViewMode::Outside);
	if (VanPrompt)
	{
		VanPrompt->RemoveFromParent();
		VanPrompt = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ARoamAloneCharacter::SetNearbyVan(AGreyboxVan* Van)
{
	NearbyVan = Van;
	if (!Van)
	{
		SetVanViewMode(EVanViewMode::Outside);
	}
}

AGreyboxVan* ARoamAloneCharacter::GetNearbyVan() const
{
	return NearbyVan.Get();
}

void ARoamAloneCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AGreyboxVan* Van = NearbyVan.Get();
	const bool bInside = Van && Van->ContainsInteriorPoint(GetActorLocation(), VanViewMode == EVanViewMode::Inside);
	SetVanViewMode(bInside ? EVanViewMode::Inside : EVanViewMode::Outside);
}

void ARoamAloneCharacter::SetVanViewMode(EVanViewMode NewMode)
{
	if (VanViewMode == NewMode || !FirstPersonCamera->IsRegistered() || !FollowCamera->IsRegistered())
	{
		return;
	}
	APlayerController* Player = Cast<APlayerController>(GetController());
	const bool bInside = NewMode == EVanViewMode::Inside;
	if (bInside)
	{
		OutsideWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
		bOutsideOrientToMovement = GetCharacterMovement()->bOrientRotationToMovement;
		bOutsideUseControllerYaw = bUseControllerRotationYaw;
		if (Player && Player->PlayerCameraManager)
		{
			OutsidePitchMin = Player->PlayerCameraManager->ViewPitchMin;
			OutsidePitchMax = Player->PlayerCameraManager->ViewPitchMax;
		}
		StopJumping();
	}
	GetCharacterMovement()->MaxWalkSpeed = bInside ? InteriorWalkSpeed : OutsideWalkSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = bInside ? false : bOutsideOrientToMovement;
	bUseControllerRotationYaw = bInside ? true : bOutsideUseControllerYaw;
	if (bInside && GetController())
	{
		SetActorRotation(FRotator(0, GetController()->GetControlRotation().Yaw, 0));
	}
	if (Player && Player->PlayerCameraManager)
	{
		Player->PlayerCameraManager->ViewPitchMin = bInside ? -75 : OutsidePitchMin;
		Player->PlayerCameraManager->ViewPitchMax = bInside ? 80 : OutsidePitchMax;
	}
	GetMesh()->SetFirstPersonPrimitiveType(bInside ? EFirstPersonPrimitiveType::WorldSpaceRepresentation : EFirstPersonPrimitiveType::None);
	FirstPersonBody->SetVisibility(bInside);
	FollowCamera->SetActive(!bInside);
	FirstPersonCamera->SetActive(bInside);
	VanViewMode = NewMode;
	Interaction->SetViewSource(bInside ? FirstPersonCamera.Get() : FollowCamera);
	UpdatePrototypePrompt();
}

void ARoamAloneCharacter::UpdatePrototypePrompt()
{
	APlayerController* Player = Cast<APlayerController>(GetController());
	if (!Player || !Player->IsLocalController() || !Player->GetLocalPlayer())
	{
		return;
	}
	if (!VanPrompt)
	{
		VanPrompt = CreateWidget<UVanPromptWidget>(Player, UVanPromptWidget::StaticClass());
		if (!VanPrompt)
		{
			return;
		}
		VanPrompt->AddToPlayerScreen();
	}
	FString Message = VanViewMode == EVanViewMode::Inside
		? TEXT("INSIDE | First person\nWalk through the doorway to return outside.")
		: TEXT("OUTSIDE | Third person\nOpen the van door and walk inside.");
	const FText ActionPrompt = Interaction->GetPrompt();
	if (!ActionPrompt.IsEmpty())
	{
		Message += TEXT("\n[E / X] ") + ActionPrompt.ToString();
	}
	const FText Feedback = Interaction->GetFeedback();
	if (!Feedback.IsEmpty())
	{
		Message += TEXT("\n") + Feedback.ToString();
	}
	VanPrompt->SetPrompt(FText::FromString(Message));
}

void ARoamAloneCharacter::HandleInteractionPresentationChanged(const FText& Prompt, const FText& Feedback)
{
	UpdatePrototypePrompt();
}

void ARoamAloneCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	UpdatePrototypePrompt();
}
