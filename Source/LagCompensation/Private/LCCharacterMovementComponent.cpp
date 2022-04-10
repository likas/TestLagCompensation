// Fill out your copyright notice in the Description page of Project Settings.


#include "LCCharacterMovementComponent.h"

#include "DrawDebugHelpers.h"
#include "LagCompensation/LagCompensationCharacter.h"

void ULCCharacterMovementComponent::ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData)
{
	Super::ServerMove_PerformMovement(MoveData);

	const float CurrentServerTime = GetWorld()->GetTimeSeconds();

	DrawDebugSphere(GetWorld(), MoveData.Location, 5.f, 12, FColor::Blue, false, 5.f);

	ALagCompensationCharacter* LCChar = Cast<ALagCompensationCharacter>(GetOwner());

	FSavedPosition NewPosition;
	NewPosition.Position = MoveData.Location;
	NewPosition.Rotation = MoveData.ControlRotation;
	NewPosition.Velocity = MoveData.Acceleration;
	NewPosition.Time = CurrentServerTime;
	NewPosition.TimeStamp = MoveData.TimeStamp;
	
	if(LCChar)
	{
		if(LCChar->SavedMoves.Num() > 30)
			LCChar->SavedMoves.RemoveAt(0);
		//UE_LOG(LogTemp, Log, TEXT("%s: Num %d"), *GetName(), LCChar->SavedMoves.Num());
		LCChar->SavedMoves.Add(NewPosition);
	}
}
