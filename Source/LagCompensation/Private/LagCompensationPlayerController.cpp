// Fill out your copyright notice in the Description page of Project Settings.


#include "LagCompensationPlayerController.h"

#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

ALagCompensationPlayerController::ALagCompensationPlayerController(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PredictionFudgeFactor = 20.f;
}

void ALagCompensationPlayerController::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//DOREPLIFETIME_CONDITION(AUTPlayerController, MaxPredictionPing, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ALagCompensationPlayerController, PredictionFudgeFactor, COND_OwnerOnly);
}

void ALagCompensationPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void ALagCompensationPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	
}

float ALagCompensationPlayerController::GetPredictionTime()
{
	// exact ping is in msec, divide by 1000 to get time in seconds
	if (GetLocalRole() == ROLE_Authority) { UE_LOG(LogTemp, Warning, TEXT("Server ExactPing %f"), PlayerState->ExactPing); }
	return (PlayerState && (GetNetMode() != NM_Standalone)) ? (0.0005f*FMath::Clamp(GetPlayerState<APlayerState>()->ExactPing - PredictionFudgeFactor, 0.f, 120.f)) : 0.f;
}
