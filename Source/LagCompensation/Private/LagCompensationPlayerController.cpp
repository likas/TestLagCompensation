// Fill out your copyright notice in the Description page of Project Settings.


#include "LagCompensationPlayerController.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

ALagCompensationPlayerController::ALagCompensationPlayerController(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxPing = 200.f;
	PredictionFudgeFactor = 0.f;
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
	return (PlayerState && (GetNetMode() != NM_Standalone)) ? (0.001f*FMath::Clamp(GetPlayerState<APlayerState>()->ExactPing - PredictionFudgeFactor, 0.f, MaxPing)) : 0.f;
}

void ALagCompensationPlayerController::ClientDebugRewind_Implementation(FVector_NetQuantize TargetLocation,
	FVector_NetQuantize RewindLocation, FVector_NetQuantize PrePosition, FVector_NetQuantize PostPosition,
	float TargetCapsuleHeight, float PredictionTime, float Percent, bool bTeleported)
{
	DrawDebugCapsule(GetWorld(), TargetLocation, TargetCapsuleHeight, 33.f, FQuat::Identity, FColor::Red, false, 8.f);
	DrawDebugCapsule(GetWorld(), RewindLocation, TargetCapsuleHeight, 33.f, FQuat::Identity, FColor::Yellow, false, 8.f);
	DrawDebugCapsule(GetWorld(), PrePosition, TargetCapsuleHeight - 20.f, 33.f, FQuat::Identity, FColor::Blue, false, 8.f);
	DrawDebugCapsule(GetWorld(), PostPosition, TargetCapsuleHeight + 20.f, 33.f, FQuat::Identity, FColor::White, false, 8.f);
	
}
