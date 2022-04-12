// Fill out your copyright notice in the Description page of Project Settings.


#include "FakeCharacterCapsule.h"

#include "Components/CapsuleComponent.h"

// Sets default values
AFakeCharacterCapsule::AFakeCharacterCapsule(const FObjectInitializer& ObjectInitializer)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	UCapsuleComponent* SomeCapsule = nullptr;
	SomeCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));

	SomeCapsule->InitCapsuleSize(33.f, 96.f);
	SomeCapsule->SetVisibility(false);
	SomeCapsule->SetHiddenInGame(false);
	SomeCapsule->SetCollisionProfileName("OverlapOnlyPawn");

	RootComponent = SomeCapsule;
}

// Called when the game starts or when spawned
void AFakeCharacterCapsule::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AFakeCharacterCapsule::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

