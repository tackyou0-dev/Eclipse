// PROJECT ECLIPSE - Player controller implementation.
//
// Purpose
//   Mapping context lifetime and the respawn fallback.

#include "Characters/EclipsePlayerController.h"

#include "Characters/EclipsePlayerCharacter.h"
#include "Core/EclipseGameMode.h"
#include "Core/EclipseLog.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

AEclipsePlayerController::AEclipsePlayerController()
{
	bReplicates = true;
}

void AEclipsePlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Gameplay input is only bound for the local player; a dedicated server has no local
	// player and this is a no-op there.
	if (IsLocalPlayerController())
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		AddGameplayMappingContext();
	}
}

void AEclipsePlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveGameplayMappingContext();

	Super::EndPlay(EndPlayReason);
}

void AEclipsePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	const AEclipsePlayerCharacter* Character = Cast<AEclipsePlayerCharacter>(InPawn);
	if (Character == nullptr)
	{
		UE_LOG(LogEclipse, Verbose, TEXT("%s possessed %s, which is not a player character."),
			*GetName(), InPawn != nullptr ? *InPawn->GetName() : TEXT("<null>"));
		return;
	}

	bAwaitingRespawn = false;

	// Control rotation drives aiming, so the controller must follow the pawn's view.
	SetControlRotation(Character->GetActorRotation());
}

void AEclipsePlayerController::AddGameplayMappingContext()
{
	if (GameplayMappingContext == nullptr)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetLocalPlayer() != nullptr
		? GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr)
	{
		Subsystem->AddMappingContext(GameplayMappingContext, GameplayMappingPriority);
	}
}

void AEclipsePlayerController::RemoveGameplayMappingContext()
{
	if (GameplayMappingContext == nullptr)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = GetLocalPlayer() != nullptr
		? GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr)
	{
		Subsystem->RemoveMappingContext(GameplayMappingContext);
	}
}

void AEclipsePlayerController::RequestRespawn()
{
	if (!bAwaitingRespawn)
	{
		return;
	}

	if (HasAuthority())
	{
		ServerRequestRespawn_Implementation();
		return;
	}

	ServerRequestRespawn();
}

void AEclipsePlayerController::ServerRequestRespawn_Implementation()
{
	UWorld* World = GetWorld();
	AEclipseGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AEclipseGameMode>() : nullptr;
	if (GameMode == nullptr)
	{
		UE_LOG(LogEclipse, Warning, TEXT("%s requested a respawn with no game mode."), *GetName());
		return;
	}

	APawn* CurrentPawn = GetPawn();
	AEclipsePlayerCharacter* Character = Cast<AEclipsePlayerCharacter>(CurrentPawn);
	if (Character == nullptr)
	{
		UE_LOG(LogEclipse, Warning, TEXT("%s requested a respawn without a player character."), *GetName());
		return;
	}

	// Respawn reuses the pawn rather than destroying it, so the squad keeps its loadout,
	// inventory and mission credit. Only the body is reset.
	Character->SetActorLocation(Character->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f));
	Character->ReviveCharacter(AEclipsePlayerController::RespawnHealthFraction);

	bAwaitingRespawn = false;

	UE_LOG(LogEclipse, Log, TEXT("%s respawned."), *GetName());
}

bool AEclipsePlayerController::ServerRequestRespawn_Validate()
{
	return true;
}
