// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LCCharacterMovementComponent.generated.h"

/**
 * 
 */
UCLASS()
class LAGCOMPENSATION_API ULCCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	virtual void ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData) override;
	float GetCurrentMovementTime() const;
	
public:
	float GetCurrentSynchTime() const;
private:
	virtual void PerformMovement(float DeltaTime) override;
	
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
public:
	/** Time server is using for this move, from timestamp passed by client */
	UPROPERTY()
	float CurrentServerMoveTime;
	
};
