// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FakeCharacterCapsule.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LagCompensationCharacter.generated.h"

class ALagCompensationPlayerController;
class UInputComponent;
class USkeletalMeshComponent;
class USceneComponent;
class UCameraComponent;
class UMotionControllerComponent;
class UAnimMontage;
class USoundBase;

USTRUCT(BlueprintType)
struct FSavedPosition
{
	GENERATED_USTRUCT_BODY()

	FSavedPosition() : Position(FVector(0.f)), Rotation(FRotator(0.f)), bTeleported(false), Time(0.f), TimeStamp(0.f) {};

	FSavedPosition(FVector InPos, FRotator InRot, bool InTeleported, float InTime, float InTimeStamp) : Position(InPos), Rotation(InRot), bTeleported(InTeleported), Time(InTime), TimeStamp(InTimeStamp) {};

	/** Position of player at time Time. */
	UPROPERTY()
	FVector Position;

	/** Rotation of player at time Time. */
	UPROPERTY()
	FRotator Rotation;

	/** true if teleport occurred getting to current position (so don't interpolate) */
	UPROPERTY()
	bool bTeleported;

	/** Current server world time when this position was updated. */
	float Time;

	/** Client timestamp associated with this position. */
	float TimeStamp;
};

UCLASS(config=Game)
class ALagCompensationCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category=Mesh)
	USkeletalMeshComponent* Mesh1P;

	/** Gun mesh: 1st person view (seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	USkeletalMeshComponent* FP_Gun;

	/** Location on gun mesh where projectiles should spawn. */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	USceneComponent* FP_MuzzleLocation;

	/** Gun mesh: VR view (attached to the VR controller directly, no arm, just the actual gun) */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	USkeletalMeshComponent* VR_Gun;

	/** Location on VR gun mesh where projectiles should spawn. */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
	USceneComponent* VR_MuzzleLocation;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FirstPersonCameraComponent;

	/** Motion controller (right hand) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UMotionControllerComponent* R_MotionController;

	/** Motion controller (left hand) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UMotionControllerComponent* L_MotionController;

	FTimerHandle AutoShootTimer;

	UPROPERTY()
	float MaxSavedPositionAge;

public:
	ALagCompensationCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;
	
	void PrintSavedMove(FSavedMovePtr Move);
	void DrawDebugMove(FSavedMovePtr Move);

public:
	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

	/** Gun muzzle's offset from the characters location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	FVector GunOffset;

	/** Projectile class to spawn */
	UPROPERTY(EditDefaultsOnly, Category=Projectile)
	TSubclassOf<class ALagCompensationProjectile> ProjectileClass;

	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Gameplay)
	USoundBase* FireSound;

	/** AnimMontage to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	UAnimMontage* FireAnimation;

	/** Whether to use motion controller location for aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
	uint8 bUsingMotionControllers : 1;

	UPROPERTY()
	TArray<FSavedPosition> SavedMoves;

	void FindClosestPosition(FVector Position);
	void GetPositionForTime(float Time, FVector& OutPosition, ALagCompensationPlayerController* DebugViewer);

	virtual void PositionUpdated();

protected:
	
	/** Fires a projectile. */
	void OnFire();
	
	UFUNCTION(Server, Unreliable)
	void OnFire_Server(float PredictionAmount, FVector StartLocation, FVector EndLocation, ALagCompensationPlayerController* FireInitiator, bool bClientHit, ALagCompensationCharacter* Victim, FVector ClientPosition);
	void OnFire_Server_Implementation(float PredictionAmount, FVector StartLocation, FVector EndLocation, ALagCompensationPlayerController* FireInitiator, bool bClientHit, ALagCompensationCharacter* Victim, FVector ClientPosition);

	/** Resets HMD orientation and position in VR. */
	void OnResetVR();

	/** Handles moving forward/backward */
	void MoveForward(float Val);

	/** Handles stafing movement, left and right */
	void MoveRight(float Val);

	/**
	 * Called via input to turn at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	UPROPERTY()
	AFakeCharacterCapsule* TestCapsule;
	
protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

public:
	/** Returns Mesh1P subobject **/
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }
	/** Returns FirstPersonCameraComponent subobject **/
	UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	UFUNCTION(BlueprintCallable)
	void AutoFire();

	void DrawDebugRewind(FVector_NetQuantize TargetLocation, FVector_NetQuantize RewindLocation,
		float TargetCapsuleHeight, FVector HitPoint, FVector StartLocation, FVector EndLocation);
		
	UFUNCTION(Client, Unreliable)
	void ClientDrawDebugRewind(FVector_NetQuantize TargetLocation, FVector_NetQuantize RewindLocation,
    		float TargetCapsuleHeight, FVector HitPoint, FVector StartLocation, FVector EndLocation);

	UFUNCTION(Client, Unreliable)
	void ClientDrawDebugCapsule(FVector Location, float HalfHeight, FColor DrawColor);

};

