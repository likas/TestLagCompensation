// Fill out your copyright notice in the Description page of Project Settings.


#include "LCCharacterMovementComponent.h"

#include "DrawDebugHelpers.h"
#include "LagCompensation/LagCompensationCharacter.h"

float ULCCharacterMovementComponent::GetCurrentMovementTime() const
{
	return ((GetOwner()->GetLocalRole() == ROLE_AutonomousProxy) || (GetNetMode() == NM_DedicatedServer) || ((GetNetMode() == NM_ListenServer) && !CharacterOwner->IsLocallyControlled()))
		? CurrentServerMoveTime
		: CharacterOwner->GetWorld()->GetTimeSeconds();
}

float ULCCharacterMovementComponent::GetCurrentSynchTime() const
{
	if (GetOwner() && GetOwner()->GetLocalRole() < ROLE_Authority)
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData)
		{
			return ClientData->CurrentTimeStamp;
		}
	}
	return GetCurrentMovementTime();
}



void ULCCharacterMovementComponent::PerformMovement(float DeltaTime)
{
	Super::PerformMovement(DeltaTime);

	ALagCompensationCharacter* Owner = CharacterOwner? Cast<ALagCompensationCharacter>(CharacterOwner) : nullptr;
	if(Owner)
	{
		Owner->PositionUpdated();
	}
}

void ULCCharacterMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags,
	const FVector& NewAccel)
{
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);

	CurrentServerMoveTime = ClientTimeStamp;
}

