// Copyright Epic Games, Inc. All Rights Reserved.

#include "LagCompensationCharacter.h"

#include "AudioWaveFormatParser.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "LagCompensationProjectile.h"
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

	// Uncomment the following line to turn motion controllers on by default:
	//bUsingMotionControllers = true;

	MaxSavedPositionAge = 0.3f;
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
	
	//GetWorldTimerManager().SetTimer(AutoShootTimer, this, &ALagCompensationCharacter::OnFire, 2.f, true);
}

void ALagCompensationCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	//GetWorldTimerManager().ClearTimer(AutoShootTimer);
	//AutoShootTimer.Invalidate();
}

void ALagCompensationCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	//UE_LOG(LogTemp, Log, TEXT("%s: Mode: %s"), *GetName(), );
	if(GetOwner() ? GetOwner()->GetLocalRole() : GetLocalRole() == ROLE_AutonomousProxy)
	{
		UCharacterMovementComponent* MovementComponent = Cast<UCharacterMovementComponent>(GetMovementComponent());
		FNetworkPredictionData_Client_Character* Data = MovementComponent->GetPredictionData_Client_Character();
		for (auto SavedMove : Data->SavedMoves)
		{
			//PrintSavedMove(SavedMove);
			DrawDebugMove(SavedMove);
		}
	}
}

void ALagCompensationCharacter::PrintSavedMove(FSavedMovePtr Move)
{
	UE_LOG(LogTemp, Log, TEXT("%s: TimeStamp: %f, \n DeltaTime: %f, \n StartLocation: %s"),\
	*GetName(),
	Move->TimeStamp,
	Move->DeltaTime,
	*Move->StartLocation.ToString()
	);
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

bool ALagCompensationCharacter::GetPositionForTime(float PredictionTime, FVector& OutPosition)
{
	FVector TargetLocation = GetActorLocation();
	FVector PrePosition = GetActorLocation();
	FVector PostPosition = GetActorLocation();
	float TargetTime = GetWorld()->GetTimeSeconds() - PredictionTime;
	float Percent = 0.999f;
	bool bTeleported = false;
	if (PredictionTime > 0.f)
	{
		for (int32 i = SavedMoves.Num() - 1; i >= 0; i--)
		{
			TargetLocation = SavedMoves[i].Position;
			if (SavedMoves[i].Time < TargetTime)
			{
				if (!SavedMoves[i].bTeleported && (i<SavedMoves.Num() - 1))
				{
					PrePosition = SavedMoves[i].Position;
					PostPosition = SavedMoves[i + 1].Position;
					if (SavedMoves[i + 1].Time == SavedMoves[i].Time)
					{
						Percent = 1.f;
						TargetLocation = SavedMoves[i + 1].Position;
					}
					else
					{
						Percent = (TargetTime - SavedMoves[i].Time) / (SavedMoves[i + 1].Time - SavedMoves[i].Time);
						TargetLocation = SavedMoves[i].Position + Percent * (SavedMoves[i + 1].Position - SavedMoves[i].Position);
					}
				}
				else
				{
					bTeleported = SavedMoves[i].bTeleported;
				}
				break;
			}
		}
	}
	OutPosition = TargetLocation;
	return true;
}

void ALagCompensationCharacter::PositionUpdated()
{
	const float WorldTime = GetWorld()->GetTimeSeconds();
	ULCCharacterMovementComponent* MovementComponent = Cast<ULCCharacterMovementComponent>(GetMovementComponent());
	if (GetCharacterMovement())
	{
		new(SavedMoves)FSavedPosition(GetActorLocation(), GetViewRotation(),
			GetCharacterMovement()->Velocity, GetCharacterMovement()->bJustTeleported, false,
			WorldTime, (MovementComponent ? MovementComponent->GetCurrentSynchTime() : 0.f));
	}

	// maintain one position beyond MaxSavedPositionAge for interpolation
	if (SavedMoves.Num() > 1 && SavedMoves[1].Time < WorldTime - MaxSavedPositionAge)
	{
		SavedMoves.RemoveAt(0);
	}
}

void ALagCompensationCharacter::OnFire()
{
	// try and fire a projectile
	if (ProjectileClass != nullptr)
	{
		UWorld* const World = GetWorld();
		if (World != nullptr)
		{
			if (bUsingMotionControllers)
			{
				const FRotator SpawnRotation = VR_MuzzleLocation->GetComponentRotation();
				const FVector SpawnLocation = VR_MuzzleLocation->GetComponentLocation();
				World->SpawnActor<ALagCompensationProjectile>(ProjectileClass, SpawnLocation, SpawnRotation);
			}
			else
			{
					const FRotator SpawnRotation = GetControlRotation();
            		// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
            		const FVector StartLocation = ((FirstPersonCameraComponent != nullptr) ? FirstPersonCameraComponent->GetComponentLocation() : GetActorLocation()) + SpawnRotation.RotateVector(GunOffset);
            		FVector_NetQuantize EndLocation = StartLocation + (SpawnRotation.Vector() * 5000.f);
            		
            		const FName TraceTag("LineTraceTag");
            		World->DebugDrawTraceTag = TraceTag;
                            
            		FCollisionQueryParams CollisionParams = FCollisionQueryParams::DefaultQueryParam;
            		CollisionParams.TraceTag = TraceTag;
            		FHitResult OutHit;
            				
            		TArray<AActor*> ActorsToIgnore;
            		ActorsToIgnore.Empty();
            		//GetWorld()->LineTraceSingleByChannel(OutHit, StartLocation, EndLocation, ECC_Visibility, CollisionParams);
            		bool bHitOccured = UKismetSystemLibrary::LineTraceSingle(GetWorld(), StartLocation, EndLocation, ETraceTypeQuery::TraceTypeQuery1,
            			false, ActorsToIgnore, EDrawDebugTrace::ForDuration, OutHit, true);
			
				ALagCompensationPlayerController* LCPC = GetOwner() ? Cast<ALagCompensationPlayerController>(GetController()) : NULL;
				float PredictionTime = LCPC ? LCPC->GetPredictionTime() : 0.f;
				UE_LOG(LogTemp, Log, TEXT("%s: %e"), *GetName(), PredictionTime);
				float CurrentClientTime = GetWorld()->GetTimeSeconds();
				
				if(GetLocalRole() == ROLE_AutonomousProxy || GetLocalRole() == ROLE_Authority && IsLocallyControlled())
				{
					OnFire_Server(PredictionTime);
				}
				
				switch( GetLocalRole() )
				{
				case ROLE_Authority:
					UE_LOG(LogTemp, Log, TEXT("%s: Role is Authority"), *GetName());
					return;
				case ROLE_AutonomousProxy:
					UE_LOG(LogTemp, Log, TEXT("%s: Role is AutonomousProxy"), *GetName());
					return;
				case ROLE_SimulatedProxy:
					UE_LOG(LogTemp, Log, TEXT("%s: Role is SimulatedProxy"), *GetName());
					return;
				default:
					return;
				}
			}
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

void ALagCompensationCharacter::OnFire_Server_Implementation(float ClientFireTime)
{
	UWorld* const World = GetWorld();
	if (World != nullptr)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		UE_LOG(LogTemp, Log, TEXT("%s: TimeStamp5: Client fired in %f \n TimeStamp5: Now is %f"), *GetName(), CurrentTime - ClientFireTime, CurrentTime);

		const FRotator SpawnRotation = GetControlRotation();
		// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
		const FVector StartLocation = ((FirstPersonCameraComponent != nullptr) ? FirstPersonCameraComponent->GetComponentLocation() : GetActorLocation()) + SpawnRotation.RotateVector(GunOffset);
		FVector_NetQuantize EndLocation = StartLocation + (SpawnRotation.Vector() * 5000.f);
		
		const FName TraceTag("LineTraceTag");
		World->DebugDrawTraceTag = TraceTag;
                
		FCollisionQueryParams CollisionParams = FCollisionQueryParams::DefaultQueryParam;
		CollisionParams.TraceTag = TraceTag;
		FHitResult OutHit;
				
		TArray<AActor*> ActorsToIgnore;
		ActorsToIgnore.Empty();
		//GetWorld()->LineTraceSingleByChannel(OutHit, StartLocation, EndLocation, ECC_Visibility, CollisionParams);


		DrawDebugSphere(GetWorld(), OutHit.Location, 10.f, 12, FColor::Orange, false, 1.f);

		FVector ClientViewPosition;
		//ALagCompensationCharacter* HitCandidate;
		//get the rewind position of a player
		bool bPositionFound = false;
		for (TActorIterator<ALagCompensationCharacter> ActorItr(GetWorld()); ActorItr; ++ActorItr)
		{
			ALagCompensationCharacter* LCChar = *ActorItr;
			bPositionFound = LCChar->GetPositionForTime(ClientFireTime, ClientViewPosition);
			if(bPositionFound)
			{
				FVector NormalPosition = LCChar->GetActorLocation();
				UCapsuleComponent* ActorCapsule = LCChar->GetCapsuleComponent();
				//LCChar->SetActorLocation(ClientViewPosition);
				
				DrawDebugCapsule(GetWorld(), ActorCapsule->GetComponentLocation(),
					ActorCapsule->GetScaledCapsuleHalfHeight(), ActorCapsule->GetScaledCapsuleRadius(),
			ActorCapsule->GetComponentRotation().Quaternion(), FColor::Black, true);
				
				DrawDebugCapsule(GetWorld(), ClientViewPosition,
					ActorCapsule->GetScaledCapsuleHalfHeight(), ActorCapsule->GetScaledCapsuleRadius(),
					ActorCapsule->GetComponentRotation().Quaternion(), FColor::White, true);
			}
						
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
