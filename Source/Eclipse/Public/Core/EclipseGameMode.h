// PROJECT ECLIPSE - Game mode.
//
// Purpose
//   Raid authority. The game mode stands up the world (seed, clock, weather), admits
//   players, tracks how long the raid has run, and decides when it ends: extraction,
//   total party kill, boss defeat or timeout.
//
// Authority
//   Runs on the server only. Clients infer raid state from AEclipseGameState.
//
// Decoupling
//   Anti-cheat admission is plugged in through OnPlayerJoining rather than called
//   directly, so the security feature owns its own policy and the game mode stays
//   readable.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseGameInstance.h"
#include "Core/EclipseTypes.h"
#include "GameFramework/GameModeBase.h"
#include "EclipseGameMode.generated.h"

/**
 * Validation hook for player admission.
 *
 * Return false to reject the player. Handlers must fill OutRejectReason; the game mode
 * logs it and, on a listen server, kicks the controller.
 */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FEclipsePlayerJoiningValidator, APlayerController* /*Controller*/, FString& /*OutRejectReason*/);

/** Broadcast after a player has been admitted and its raid participation registered. */
DECLARE_MULTICAST_DELEGATE_OneParam(FEclipsePlayerJoined, APlayerController* /*Controller*/);

/** Broadcast when the raid ends, with the reason. */
DECLARE_MULTICAST_DELEGATE_OneParam(FEclipseRaidEnded, EEclipseRaidOutcome /*Outcome*/);

/**
 * Game mode for THE FALLEN BASIN.
 *
 * Lifecycle: InitGame applies the raid settings to the world, PostLogin admits players,
 * Tick accumulates raid time and evaluates the end conditions, and EndRaid settles the
 * result exactly once.
 */
UCLASS()
class ECLIPSE_API AEclipseGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEclipseGameMode();

	/** AGameModeBase: apply raid settings and configure the session. */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/** AGameModeBase: admit a player, subject to the validator and the player cap. */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** AGameModeBase: release raid resources for a departing player. */
	virtual void Logout(AController* Exiting) override;

	/** AGameModeBase: keep the raid clock running. */
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Finish the raid. Safe to call more than once and from any server system: the second
	 * call is ignored and logged at verbose level.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Raid")
	void EndRaid(EEclipseRaidOutcome Outcome);

	/** Called by the extraction zone when a player completes the extraction objective. */
	void NotifyPlayerExtracted(APlayerController* Controller);

	/** Called by the boss encounter when WARDEN PRIME dies. */
	void NotifyBossDefeated();

	/** Called by the combat layer when a player character dies. */
	void NotifyPlayerKilled(APlayerController* Controller);

	/** Registered validator, or an unbound delegate when security is not present. */
	FEclipsePlayerJoiningValidator OnPlayerJoining;

	/** Fired after a player is admitted. */
	FEclipsePlayerJoined OnPlayerJoined;

	/** Fired once when the raid ends. */
	FEclipseRaidEnded OnRaidEnded;

	/** Live count of admitted players. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Raid")
	int32 GetAdmittedPlayerCount() const { return AdmittedControllers.Num(); }

	/** Maximum players this raid admits, seeded from the raid settings. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Raid")
	int32 GetRaidPlayerCap() const { return RaidPlayerCap; }

protected:
	/** AGameModeBase: pick a start point that is not occupied and not blocked. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Server-only per-frame work: raid clock, end conditions. */
	void UpdateRaid(float DeltaSeconds);

	/** Apply the raid settings to the world subsystem, if one exists. */
	void ApplyWorldSettings();

private:
	/** Controllers admitted to this raid, in join order. */
	UPROPERTY()
	TArray<TObjectPtr<APlayerController>> AdmittedControllers;

	/** Maximum players admitted to this raid. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Raid")
	int32 RaidPlayerCap = 4;

	/** True once EndRaid has settled a result. */
	bool bRaidSettled = false;
};
