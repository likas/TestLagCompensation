// Copyright Epic Games, Inc. All Rights Reserved.

#include "LagCompensationGameMode.h"
#include "LagCompensationHUD.h"
#include "LagCompensationCharacter.h"
#include "LagCompensationPlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "UObject/ConstructorHelpers.h"

ALagCompensationGameMode::ALagCompensationGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;
	PlayerControllerClass = ALagCompensationPlayerController::StaticClass();

	// use our custom HUD class
	HUDClass = ALagCompensationHUD::StaticClass();
}

UClass* ALagCompensationGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if(InController->IsLocalController())
	{
		return SpectatorClass;
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}
