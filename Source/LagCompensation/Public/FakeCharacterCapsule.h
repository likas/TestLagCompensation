// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Actor.h"
#include "FakeCharacterCapsule.generated.h"

UCLASS()
class LAGCOMPENSATION_API AFakeCharacterCapsule : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	UCapsuleComponent* FakeCapsule;
	
public:	
	// Sets default values for this actor's properties
	AFakeCharacterCapsule(const FObjectInitializer& ObjectInitializer);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
