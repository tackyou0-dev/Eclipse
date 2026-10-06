// PROJECT ECLIPSE - Game state.
//
// Purpose
//   The replicated snapshot of a running raid: world clock, weather, active missions and
//   the boss encounter state. Clients read this and never write it; the server is the
//   only writer. Anything a client needs to display about the world comes from here
//   rather than from a client-side subsystem, which is what keeps the HUD honest in a
//   four-player session.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "GameFramework/GameStateBase.h"
#include "EclipseGameState.generated.h"

/** Replicated boss encounter snapshot. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseBossState
{
	GENERATED_BODY()

	/** True while the boss is alive and engaged. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Boss")
	bool bEngaged = false;

	/** Current phase index, 0-based. Phase 0 is ground combat. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Boss")
	int32 PhaseIndex = 0;

	/** Display name of the current phase, resolved from the boss definition. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Boss")
	FName PhaseName;

	/** Health fraction in [0, 1] for the boss bar. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Boss")
	float HealthFraction = 1.0f;

	/** True when the boss is dead and the arena is safe. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Boss")
	bool bDefeated = false;
};

/**
 * Replicated game state.
 *
 * Replication is property-based with RepNotify so the UI and audio layers can react
 * without polling. Every setter is server-only and guarded with a net mode check.
 */
UCLASS()
class ECLIPSE_API AEclipseGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AEclipseGameState();

	/** Server world clock in hours, already wrapped into [0, 24). */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	float GetWorldHours() const { return ServerWorldHours; }

	/** Time-of-day band derived from the world clock. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	EEclipseTimeOfDay GetTimeOfDay() const { return EclipseTimeOfDay::FromHour(ServerWorldHours); }

	/** True between 21:00 and 05:00. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	bool IsNight() const { return GetTimeOfDay() == EEclipseTimeOfDay::Night; }

	/** Weather currently published by the world subsystem. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	EEclipseWeather GetCurrentWeather() const { return CurrentWeather; }

	/** Missions currently active for the squad. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Mission")
	const TArray<FName>& GetActiveMissionIds() const { return ActiveMissionIds; }

	/** Boss snapshot. bEngaged is false outside the Warden arena. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Boss")
	const FEclipseBossState& GetBossState() const { return BossState; }

	/** True while the raid is running (as opposed to after extraction). */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Raid")
	bool IsRaidActive() const { return bRaidActive; }

	/**
	 * Server only: set the world clock. Values outside [0, 24) are wrapped, so callers
	 * never have to worry about midnight arithmetic.
	 */
	void SetWorldHours(float NewHours);

	/** Server only: publish a new weather profile. */
	void SetCurrentWeather(EEclipseWeather NewWeather);

	/** Server only: replace the active mission list (order preserved for the UI). */
	void SetActiveMissionIds(const TArray<FName>& MissionIds);

	/** Server only: update the boss snapshot. */
	void SetBossState(const FEclipseBossState& NewState);

	/** Server only: mark the raid as running or finished. */
	void SetRaidActive(bool bActive);

	/** Broadcast on clients and server whenever the weather changes. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|World")
	FEclipseWeatherChangedSignature OnWeatherChanged;

protected:
	/** RepNotify: forwards to the weather delegate. */
	UFUNCTION()
	void OnRep_CurrentWeather();

	/** RepNotify: kept so UI can refresh when the clock crosses a band boundary. */
	UFUNCTION()
	void OnRep_ServerWorldHours();

	/** Server clock in hours. */
	UPROPERTY(ReplicatedUsing = OnRep_ServerWorldHours, BlueprintReadOnly, Category = "Eclipse|World")
	float ServerWorldHours = 8.0f;

	/** Published weather. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeather, BlueprintReadOnly, Category = "Eclipse|World")
	EEclipseWeather CurrentWeather = EEclipseWeather::Clear;

	/** Active missions, in display order. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Mission")
	TArray<FName> ActiveMissionIds;

	/** Boss encounter snapshot. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Boss")
	FEclipseBossState BossState;

	/** Raid running flag. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Raid")
	bool bRaidActive = false;

	/** AGameStateBase. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
