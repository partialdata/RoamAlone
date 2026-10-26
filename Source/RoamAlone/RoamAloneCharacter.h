// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "RoamAloneCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class AGreyboxVan;
class UVanPromptWidget;
class UInteractionComponent;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EVanViewMode : uint8
{
	Outside,
	Inside
};

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class ARoamAloneCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInteractionComponent> Interaction;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(Units="cm"))
	FVector InteriorEyeOffset = FVector(8, 0, 64);

	UPROPERTY(EditDefaultsOnly, Category="Camera", meta=(ClampMin="60", ClampMax="110", Units="deg"))
	float InteriorFieldOfView = 85;

	UPROPERTY(EditDefaultsOnly, Category="Movement", meta=(ClampMin="50", Units="cm/s"))
	float InteriorWalkSpeed = 180;

public:

	/** Constructor */
	ARoamAloneCharacter();	

	virtual void Tick(float DeltaSeconds) override;
	virtual void PawnClientRestart() override;
	void SetNearbyVan(AGreyboxVan* Van);
	AGreyboxVan* GetNearbyVan() const;

	UFUNCTION(BlueprintPure, Category="Camera")
	EVanViewMode GetVanViewMode() const { return VanViewMode; }

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	USkeletalMeshComponent* GetFirstPersonBody() const { return FirstPersonBody; }

	UFUNCTION(BlueprintPure, Category="Interaction")
	UInteractionComponent* GetInteractionComponent() const { return Interaction; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

private:
	void SetVanViewMode(EVanViewMode NewMode);
	void UpdatePrototypePrompt();

	UFUNCTION()
	void HandleInteractionPresentationChanged(const FText& Prompt, const FText& Feedback);

	UPROPERTY(VisibleInstanceOnly, Category="Camera")
	EVanViewMode VanViewMode = EVanViewMode::Outside;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGreyboxVan> NearbyVan;

	UPROPERTY(Transient)
	TObjectPtr<UVanPromptWidget> VanPrompt;

	float OutsideWalkSpeed = 500;
	float OutsidePitchMin = -89.9f;
	float OutsidePitchMax = 89.9f;
	bool bOutsideOrientToMovement = true;
	bool bOutsideUseControllerYaw = false;

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

