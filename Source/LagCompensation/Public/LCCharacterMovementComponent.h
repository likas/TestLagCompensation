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
	
};
