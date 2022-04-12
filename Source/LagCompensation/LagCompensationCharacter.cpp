// Copyright Epic Games, Inc. All Rights Reserved.

#include "LagCompensationCharacter.h"

#include "AudioWaveFormatParser.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "LagCompensationPlayerController.h"
#include "LCCharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "XRMotionControllerBase.h" // for FXRMotionControllerBase::RightHandSourceId
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// ALagCompensationCharacter

ALagCompensationCharacter::ALagCompensationCharacter(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer.SetDefaultSubobjectClass<ULCCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(33.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetupAttachment(FirstPersonCameraComponent);
	Mesh1P->bCastDynamicShadow = false;
	Mesh1P->CastShadow = false;
	Mesh1P->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	Mesh1P->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));

	// Create a gun mesh component
	FP_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FP_Gun"));
	FP_Gun->SetOnlyOwnerSee(false);			// otherwise won't be visible in the multiplayer
	FP_Gun->bCastDynamicShadow = false;
	FP_Gun->CastShadow = false;
	// FP_Gun->SetupAttachment(Mesh1P, TEXT("GripPoint"));
	FP_Gun->SetupAttachment(RootComponent);

	FP_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FP_MuzzleLocation->SetupAttachment(FP_Gun);
	FP_MuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 0.0f, 10.0f);

	// Note: The ProjectileClass and the skeletal mesh/anim blueprints for Mesh1P, FP_Gun, and VR_Gun 
	// are set in the derived blueprint asset named MyCharacter to avoid direct content references in C++.

	// Create VR Controllers.
	R_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("R_MotionController"));
	R_MotionController->MotionSource = FXRMotionControllerBase::RightHandSourceId;
	R_MotionController->SetupAttachment(RootComponent);
	L_MotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("L_MotionController"));
	L_MotionController->SetupAttachment(RootComponent);

	// Create a gun and attach it to the right-hand VR controller.
	// Create a gun mesh component
	VR_Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VR_Gun"));
	VR_Gun->SetOnlyOwnerSee(false);			// otherwise won't be visible in the multiplayer
	VR_Gun->bCastDynamicShadow = false;
	VR_Gun->CastShadow = false;
	VR_Gun->SetupAttachment(R_MotionController);
	VR_Gun->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

	VR_MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("VR_MuzzleLocation"));
	VR_MuzzleLocation->SetupAttachment(VR_Gun);
	VR_MuzzleLocation->SetRelativeLocation(FVector(0.000004, 53.999992, 10.000000));
	VR_MuzzleLocation->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));		// Counteract the rotation of the VR gun model.

	MaxSavedPositionAge = 1.f;
}

void ALagCompensationCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FP_Gun->AttachToComponent(Mesh1P, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));

	// Show or hide the two versions of the gun based on whether or not we're using motion controllers.
	if (bUsingMotionControllers)
	{
		VR_Gun->SetHiddenInGame(false, true);
		Mesh1P->SetHiddenInGame(true, true);
	}
	else
	{
		VR_Gun->SetHiddenInGame(true, true);
		Mesh1P->SetHiddenInGame(false, true);
	}

	const FVector SpawnLocation = FVector(5000.f, 5000.f, 0.f);
	const FRotator SpawnRotation = FRotator::ZeroRotator;
	FActorSpawnParameters Parms;
	Parms.Owner = this;
	Parms.Instigator = this;
	AActor* TestCapsuleActor = GetWorld()->SpawnActor(AFakeCharacterCapsule::StaticClass(), &SpawnLocation, &SpawnRotation, Parms);
	TestCapsule = TestCapsuleActor? Cast<AFakeCharacterCapsule>(TestCapsuleActor) : nullptr;
}

void ALagCompensationCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void ALagCompensationCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ALagCompensationCharacter::DrawDebugMove(FSavedMovePtr Move)
{
	DrawDebugSphere(GetWorld(), Move->StartLocation, 5.f, 12, FColor::Magenta, false, 10.f);
}

//////////////////////////////////////////////////////////////////////////
// Input

void ALagCompensationCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ALagCompensationCharacter::OnFire);

	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &ALagCompensationCharacter::OnResetVR);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &ALagCompensationCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ALagCompensationCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ALagCompensationCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ALagCompensationCharacter::LookUpAtRate);
}

void ALagCompensationCharacter::AutoFire()
{
	OnFire();
}

void ALagCompensationCharacter::DrawDebugRewind(FVector_NetQuantize TargetLocation, FVector_NetQuantize RewindLocation,
	float TargetCapsuleHeight, FVector HitPoint, FVector StartLocation, FVector EndLocation)
{
	DrawDebugLine(GetWorld(), StartLocation, HitPoint, FColor::Magenta, true);
	DrawDebugSphere(GetWorld(), HitPoint, 5.f, 10, FColor::Orange, true);
	DrawDebugCapsule(GetWorld(), TargetLocation, TargetCapsuleHeight, 33.f, FQuat::Identity, FColor::Black, true);
	DrawDebugCapsule(GetWorld(), RewindLocation, TargetCapsuleHeight - 20, 33.f, FQuat::Identity, FColor::White, true);
}

void ALagCompensationCharacter::ClientDrawDebugCapsule_Implementation(FVector Location, float HalfHeight, FColor DrawColor)
{
	DrawDebugCapsule(GetWorld(), Location, HalfHeight, 33.f, FQuat::Identity, DrawColor, true);
}

void ALagCompensationCharacter::ClientDrawDebugRewind_Implementation(FVector_NetQuantize TargetLocation,
                                                                     FVector_NetQuantize RewindLocation, float TargetCapsuleHeight, FVector HitPoint, FVector StartLocation, FVector EndLocation)
{
	DrawDebugRewind(TargetLocation, RewindLocation, TargetCapsuleHeight, HitPoint, StartLocation, EndLocation);
}

void ALagCompensationCharacter::FindClosestPosition(FVector Position)
{
	if(Position == FVector::ZeroVector) return;
	FVector ClosestPosition = Position - 1000.f;
	int ClosestPositionIndex = 0;
	float MinDistance = 10000000000000;
	
	for (int32 i = SavedMoves.Num() - 1; i >= 0; i--)
	{
		if(FVector::Dist(SavedMoves[i].Position, Position) < MinDistance)
		{
			MinDistance = FVector::Dist(SavedMoves[i].Position, Position);
			ClosestPositionIndex = i;
			ClosestPosition = SavedMoves[i].Position;
		}
	}
	UE_LOG(LogTemp, Log, TEXT("TargetPosition: %s"), *ClosestPosition.ToString());
	UE_LOG(LogTemp, Log, TEXT("%s: closest position is at index %d and time %f"), *GetName(), ClosestPositionIndex, SavedMoves[ClosestPositionIndex].Time);
}

void ALagCompensationCharacter::GetPositionForTime(float PredictionTime, FVector& OutPosition, ALagCompensationPlayerController* DebugViewer)
{
	FVector TargetLocation = GetActorLocation();
	float TargetTime = GetWorld()->GetTimeSeconds() - PredictionTime;
	float Percent = 0.999f;
	if (PredictionTime > 0.f)
	{
		for (int32 i = SavedMoves.Num() - 1; i >= 0; i--)
		{
			//DrawDebugSphere(GetWorld(), SavedMoves[i].Position, 5.f, 12, FColor::Red, true);
			TargetLocation = SavedMoves[i].Position;
			if (SavedMoves[i].Time < TargetTime)
			{
				if (!SavedMoves[i].bTeleported && (i < SavedMoves.Num() - 1))
				{
					if (SavedMoves[i + 1].Time == SavedMoves[i].Time)
					{
						TargetLocation = SavedMoves[i + 1].Position;
					}
					else
					{
						Percent = (TargetTime - SavedMoves[i].Time) / (SavedMoves[i + 1].Time - SavedMoves[i].Time);
						TargetLocation = SavedMoves[i].Position + Percent * (SavedMoves[i + 1].Position - SavedMoves[i].Position);
						//UE_LOG(LogTemp, Log, TEXT("%s: Location found at i = %d + %f percents (%d i's overall count). Time is %f"), *GetName(), i, Percent, SavedMoves.Num(), SavedMoves[i].Time + Percent * (SavedMoves[i + 1].Time - SavedMoves[i].Time));
					}
				}
				break;
			}
		}
	}
	OutPosition = TargetLocation;
}

void ALagCompensationCharacter::PositionUpdated()
{
	const float WorldTime = GetWorld()->GetTimeSeconds();
	ULCCharacterMovementComponent* MovementComponent = Cast<ULCCharacterMovementComponent>(GetMovementComponent());
	if (GetCharacterMovement())
	{
		new(SavedMoves)FSavedPosition(GetActorLocation(), GetViewRotation(),
			GetCharacterMovement()->bJustTeleported,WorldTime,
			(MovementComponent ? MovementComponent->GetCurrentSynchTime() : 0.f));
	}

	// maintain one position beyond MaxSavedPositionAge for interpolation
	if (SavedMoves.Num() > 1 && SavedMoves[1].Time < WorldTime - MaxSavedPositionAge)
	{
		SavedMoves.RemoveAt(0);
	}
}

void ALagCompensationCharacter::OnFire()
{
	UWorld* const World = GetWorld();
	if (World != nullptr)
	{
		//getting prediction time based on the RTT and some restraining constants
		ALagCompensationPlayerController* LagCompensationPC = GetOwner() ? Cast<ALagCompensationPlayerController>(GetController()) : nullptr;
		float PredictionTime = LagCompensationPC ? LagCompensationPC->GetPredictionTime() : 0.f;
		
		UE_LOG(LogTemp, Log, TEXT("%s's PredictionTime is %e"), *GetName(), PredictionTime);
		
		const FRotator Rotation = GetControlRotation();
        const FVector StartLocation = ((FirstPersonCameraComponent != nullptr) ? FirstPersonCameraComponent->GetComponentLocation() : GetActorLocation()) + Rotation.RotateVector(GunOffset);
        FVector_NetQuantize EndLocation = StartLocation + (Rotation.Vector() * 10000.f);
		
        FHitResult OutHit;

		TArray<AActor*> ActorsToIgnore;
		ActorsToIgnore.Empty();

		//throwing a trace just to see where we're actually firing a shot
		
		UKismetSystemLibrary::LineTraceSingle(GetWorld(), StartLocation, EndLocation, ETraceTypeQuery::TraceTypeQuery1,
            false, ActorsToIgnore, EDrawDebugTrace::ForDuration, OutHit, true);
		
		ALagCompensationCharacter* HitCharacter = OutHit.Actor.Get() ? Cast<ALagCompensationCharacter>(OutHit.Actor.Get()) : nullptr;
		
		if(GetLocalRole() == ROLE_AutonomousProxy || GetLocalRole() == ROLE_Authority && IsLocallyControlled())
		{
			OnFire_Server(PredictionTime, StartLocation, EndLocation, Cast<ALagCompensationPlayerController>(this->GetController()),
				HitCharacter, HitCharacter ? HitCharacter->GetActorLocation() : FVector::ZeroVector);
		}
	}

	// try and play the sound if specified
	if (FireSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

	// try and play a firing animation if specified
	if (FireAnimation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Mesh1P->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}

void ALagCompensationCharacter::OnFire_Server_Implementation(float PredictionAmount, FVector StartLocation,
	FVector EndLocation, ALagCompensationPlayerController* FireInitiator, ALagCompensationCharacter* Victim,
	FVector ClientPosition)
{
	UWorld* const World = GetWorld();
	if (World != nullptr)
	{
		float CurrentTime = World->GetTimeSeconds();
		UE_LOG(LogTemp, Log, TEXT("%s: \nTimeStamp5: Client fired in %f, now is %f, diff: %f"), *GetName(), CurrentTime - PredictionAmount, CurrentTime, PredictionAmount);

		FHitResult OutHit;
		
		TArray<AActor*> ActorsToIgnore;
		ActorsToIgnore.Empty();
		
		FVector ClientRewoundPosition;

		bool bPositionFound = false;
		bool bClientHit = IsValid(Victim);
		for (TActorIterator<ALagCompensationCharacter> ActorItr(World); ActorItr; ++ActorItr)
		{
			//get the rewind position of a player
			(*ActorItr)->GetPositionForTime(PredictionAmount, ClientRewoundPosition, FireInitiator);

			/*
			if(bClientHit && (*ActorItr) == Victim)
			{
				(*ActorItr)->FindClosestPosition(ClientPosition);
			}
			*/

			//put a capsule in the rewind position of a player
			//TestCapsule->SetActorLocation(CurrentCapsuleLocation);
			(*ActorItr)->TestCapsule->SetActorLocation(ClientRewoundPosition);
			ActorsToIgnore.Add(*ActorItr);
		}
		

		
		//fire a trace from a given spot (yup the player can cheat here. checking against the current muzzle
		//location could help)
		bool bHitOccurred = UKismetSystemLibrary::LineTraceSingle(GetWorld(), StartLocation, EndLocation, ETraceTypeQuery::TraceTypeQuery1,
	false, ActorsToIgnore, EDrawDebugTrace::ForDuration, OutHit, true);

		ALagCompensationCharacter* HitActor = (Cast<ALagCompensationCharacter>(OutHit.Actor.Get()->GetOwner()));
		
		bool ServerRegisterHit = bHitOccurred && HitActor;
		
		if(ServerRegisterHit)
		{
			//we hit something and it's a FakeCapsule representing the player's rewound position!
			
			UCapsuleComponent* ActorCapsule = HitActor->GetCapsuleComponent();
			FVector CurrentCapsuleLocation = ActorCapsule ? ActorCapsule->GetComponentLocation() : HitActor->GetActorLocation();
			float ActorCapsuleHalfHeight = ActorCapsule ? ActorCapsule->GetScaledCapsuleHalfHeight() : 96.f;

			FVector HitRewoundPostion = OutHit.Actor.Get()->GetActorLocation();

			if(!bClientHit)
			{
				UE_LOG(LogTemp, Warning, TEXT("%s: Server: Due to an inconsistent nature of network delays we hit %s on the SERVER but missed on the CLIENT"), *GetName(), *OutHit.Actor.Get()->GetName());
			}
				
			DrawDebugCapsule(GetWorld(), ClientPosition, ActorCapsuleHalfHeight + 20.f, 33.f, FQuat::Identity, FColor::Blue, true);
			DrawDebugRewind(CurrentCapsuleLocation, HitRewoundPostion, ActorCapsuleHalfHeight, OutHit.Location, StartLocation, EndLocation);
				
			ClientDrawDebugCapsule(ClientPosition, ActorCapsuleHalfHeight + 20, FColor::Yellow);
			ClientDrawDebugRewind(CurrentCapsuleLocation, HitRewoundPostion, ActorCapsuleHalfHeight, OutHit.Location, StartLocation, EndLocation);
				
		}
			
		if((HitActor) == Victim && bClientHit && !(ServerRegisterHit) && bHitOccurred)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: Server: Due to an inconsistent nature of network delays we hit %s on CLIENT but missed on the SERVER"), *GetName(), *Victim->GetName());
		}

		for (TActorIterator<ALagCompensationCharacter> ActorItr(World); ActorItr; ++ActorItr)
		{
			TestCapsule->SetActorLocation(FVector(5000.f, 5000.f, 0.f));
		}
	}
}

void ALagCompensationCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void ALagCompensationCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void ALagCompensationCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void ALagCompensationCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ALagCompensationCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}
