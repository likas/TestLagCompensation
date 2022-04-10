// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LagCompensationPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class LAGCOMPENSATION_API ALagCompensationPlayerController : public APlayerController
{
	GENERATED_BODY()
	
	ALagCompensationPlayerController(const class FObjectInitializer& ObjectInitializer);
	
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

public:
	/** Return amount of time to tick or simulate to make up for network lag */
	virtual float GetPredictionTime();

	// Perceived latency reduction
	/** Used to correct prediction error. */
	UPROPERTY(EditAnywhere, Replicated, GlobalConfig, Category=Network)
	float PredictionFudgeFactor;
};
