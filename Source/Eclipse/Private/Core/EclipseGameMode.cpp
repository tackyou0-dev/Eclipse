// PROJECT ECLIPSE - Game mode implementation.
//
// Purpose
//   Stands up a raid and settles it. The raid clock, the player cap and the end
//   conditions all live here because they are the only rules that are true for every
//   system at once.

#include "Core/EclipseGameMode.h"

#include "Characters/EclipsePlayerCharacter.h"
#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseGameState.h"
#include "Core/EclipseLog.h"
#include "Security/EclipseSecuritySubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "World/EclipseWorldSubsystem.h"

AEclipseGameMode::AEclipseGameMode()
{
	bStartPlayersAsSpectators = true;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f;

	DefaultPawnClass = AEclipsePlayerCharacter::StaticClass();
	GameStateClass = AEclipseGameState::StaticClass();
}

void AEclipseGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	ApplyWorldSettings();

	if (AEclipseGameState* EclipseGameState = GetGameState<AEclipseGameState>())
	{
		EclipseGameState->SetRaidActive(true);
	}

	UE_LOG(LogEclipse, Log, TEXT("Raid initialised on map %s."), *MapName);
}

void AEclipseGameMode::ApplyWorldSettings()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	UEclipseWorldSubsystem* WorldSubsystem = World->GetSubsystem<UEclipseWorldSubsystem>();
	if (WorldSubsystem == nullptr)
	{
		UE_LOG(LogEclipse, Warning, TEXT("No world subsystem: raid settings were not applied to the world."));
		return;
	}

	const UEclipseGameInstance* GameInstance = GetGameInstance<UEclipseGameInstance>();
	const int32 Seed = GameInstance != nullptr
		? GameInstance->GetRaidState().Settings.ResolveSeed(UEclipseDeveloperSettings::Get()->WorldSeed)
		: UEclipseDeveloperSettings::Get()->WorldSeed;
	const float StartHour = GameInstance != nullptr
		? GameInstance->GetRaidState().Settings.StartHour
		: UEclipseDeveloperSettings::Get()->RaidStartHour;
	const EEclipseWeather ForcedWeather = GameInstance != nullptr
		? GameInstance->GetRaidState().Settings.ForcedWeather
		: EEclipseWeather::Clear;
	const bool bFreezeTime = GameInstance != nullptr && GameInstance->GetRaidState().Settings.bFreezeTime;

	WorldSubsystem->ApplyRaidSettings(Seed, StartHour, ForcedWeather, bFreezeTime);
}

void AEclipseGameMode::PostLogin(APlayerController* NewPlayer)
{
	if (NewPlayer == nullptr)
	{
		return;
	}

	const int32 PlayerCap = RaidPlayerCap;
	if (AdmittedControllers.Num() >= PlayerCap)
	{
		UE_LOG(LogEclipse, Warning, TEXT("Rejecting %s: the raid is full (%d/%d)."),
			*NewPlayer->GetName(), AdmittedControllers.Num(), PlayerCap);
		NewPlayer->ClientReturnToMainMenuWithTextReason(NSLOCTEXT("Eclipse", "RaidFull", "The raid is full."));
		return;
	}

	if (OnPlayerJoining.IsBound())
	{
		FString RejectReason;
		if (!OnPlayerJoining.Execute(NewPlayer, RejectReason))
		{
			UE_LOG(LogEclipse, Warning, TEXT("Rejecting %s: %s"), *NewPlayer->GetName(), *RejectReason);
			NewPlayer->ClientReturnToMainMenuWithTextReason(FText::FromString(RejectReason));
			return;
		}
	}

	Super::PostLogin(NewPlayer);

	AdmittedControllers.Add(NewPlayer);

	// Admission is the moment the server starts trusting this client: from here the
	// security subsystem rate-limits its intents and checks its movement claims.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UEclipseSecuritySubsystem* Security = GameInstance->GetSubsystem<UEclipseSecuritySubsystem>())
		{
			Security->RegisterPlayer(NewPlayer);
		}
	}

	OnPlayerJoined.Broadcast(NewPlayer);

	UE_LOG(LogEclipse, Log, TEXT("%s joined the raid (%d/%d)."),
		*NewPlayer->GetName(), AdmittedControllers.Num(), PlayerCap);
}

void AEclipseGameMode::Logout(AController* Exiting)
{
	APlayerController* ExitingController = Cast<APlayerController>(Exiting);
	AdmittedControllers.Remove(ExitingController);

	if (ExitingController != nullptr)
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UEclipseSecuritySubsystem* Security = GameInstance->GetSubsystem<UEclipseSecuritySubsystem>())
			{
				Security->UnregisterPlayer(ExitingController);
			}
		}
	}

	Super::Logout(Exiting);

	if (AdmittedControllers.Num() == 0 && !bRaidSettled)
	{
		UE_LOG(LogEclipse, Log, TEXT("Every player left; ending the raid."));
		EndRaid(EEclipseRaidOutcome::TimedOut);
	}
}

void AEclipseGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bRaidSettled)
	{
		return;
	}

	UpdateRaid(DeltaSeconds);

	// Trust recovers while a player behaves; a single bad frame does not end a session.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UEclipseSecuritySubsystem* Security = GameInstance->GetSubsystem<UEclipseSecuritySubsystem>())
		{
			Security->RecoverTrust(DeltaSeconds);
		}
	}
}

void AEclipseGameMode::UpdateRaid(float DeltaSeconds)
{
	UEclipseGameInstance* GameInstance = GetGameInstance<UEclipseGameInstance>();
	if (GameInstance == nullptr || !GameInstance->IsRaidRunning())
	{
		return;
	}

	GameInstance->AccumulateRaidTime(DeltaSeconds);

	// The game instance ends the raid itself when the configured duration is exceeded.
	if (!GameInstance->IsRaidRunning())
	{
		EndRaid(GameInstance->GetRaidState().Outcome);
		return;
	}

	const AEclipseGameState* EclipseGameState = GetGameState<AEclipseGameState>();
	if (EclipseGameState != nullptr && EclipseGameState->GetBossState().bDefeated && AdmittedControllers.Num() > 0)
	{
		// Boss defeated is a raid outcome in its own right: the squad may still leave with
		// whatever it looted, but the run is recorded as a boss kill.
		EndRaid(EEclipseRaidOutcome::BossDefeated);
	}
}

void AEclipseGameMode::EndRaid(EEclipseRaidOutcome Outcome)
{
	if (bRaidSettled)
	{
		UE_LOG(LogEclipse, Verbose, TEXT("EndRaid ignored: the raid was already settled."));
		return;
	}

	bRaidSettled = true;

	if (UEclipseGameInstance* GameInstance = GetGameInstance<UEclipseGameInstance>())
	{
		GameInstance->EndRaid(Outcome);
	}

	if (AEclipseGameState* EclipseGameState = GetGameState<AEclipseGameState>())
	{
		EclipseGameState->SetRaidActive(false);
	}

	UE_LOG(LogEclipse, Log, TEXT("Raid settled as %s."), EclipseEnumNames::ToString(Outcome));
	OnRaidEnded.Broadcast(Outcome);
}

void AEclipseGameMode::NotifyPlayerExtracted(APlayerController* Controller)
{
	if (Controller == nullptr)
	{
		UE_LOG(LogEclipse, Warning, TEXT("NotifyPlayerExtracted called without a controller."));
		return;
	}

	UE_LOG(LogEclipse, Log, TEXT("%s reached extraction."), *Controller->GetName());

	// Co-op rule: the raid ends for everyone when the last living player extracts. In the
	// vertical slice every admitted player is a participant, so this is a squad check.
	EndRaid(EEclipseRaidOutcome::Extracted);
}

void AEclipseGameMode::NotifyBossDefeated()
{
	if (AEclipseGameState* EclipseGameState = GetGameState<AEclipseGameState>())
	{
		FEclipseBossState BossState = EclipseGameState->GetBossState();
		BossState.bDefeated = true;
		BossState.bEngaged = false;
		BossState.HealthFraction = 0.0f;
		EclipseGameState->SetBossState(BossState);
	}

	EndRaid(EEclipseRaidOutcome::BossDefeated);
}

void AEclipseGameMode::NotifyPlayerKilled(APlayerController* Controller)
{
	if (Controller == nullptr)
	{
		return;
	}

	UE_LOG(LogEclipse, Log, TEXT("%s was killed."), *Controller->GetName());

	// Total party kill ends the raid; a single death in co-op does not, because the
	// downed player waits for a revive.
	bool bAnyAlive = false;
	for (const TObjectPtr<APlayerController>& Admitted : AdmittedControllers)
	{
		if (Admitted != nullptr && Admitted != Controller)
		{
			bAnyAlive = true;
			break;
		}
	}

	if (!bAnyAlive)
	{
		EndRaid(EEclipseRaidOutcome::KilledInAction);
	}
}

AActor* AEclipseGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	AActor* Chosen = Super::ChoosePlayerStart_Implementation(Player);
	if (Chosen != nullptr)
	{
		return Chosen;
	}

	// Fall back to the first player start that is not already claimed by a living pawn.
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* Start = *It;
		if (Start == nullptr)
		{
			continue;
		}

		bool bOccupied = false;
		for (const TObjectPtr<APlayerController>& Admitted : AdmittedControllers)
		{
			const APawn* Pawn = Admitted != nullptr ? Admitted->GetPawn() : nullptr;
			if (Pawn != nullptr && FVector::DistSquared(Pawn->GetActorLocation(), Start->GetActorLocation()) < FMath::Square(200.0f))
			{
				bOccupied = true;
				break;
			}
		}

		if (!bOccupied)
		{
			return Start;
		}
	}

	return nullptr;
}
