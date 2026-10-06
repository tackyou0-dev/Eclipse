// PROJECT ECLIPSE - Mission subsystem.
//
// Purpose
//   Mission lifecycle and objective progress. Every objective type has a handler: kill
//   counts arrive from the damage system, collect counts from the inventory, interaction
//   counts from the interaction interface, and survive/defend accumulate time while their
//   condition holds.
//
// Authority
//   Server only. Progress arrives as reports from server-side systems; the replicated
//   game state carries the active mission list so clients can render the tracker.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Missions/EclipseMissionDefinition.h"
#include "Subsystems/WorldSubsystem.h"
#include "EclipseMissionSubsystem.generated.h"

/** Fired whenever a mission changes state. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FEclipseMissionStateChanged, FName /*MissionId*/, EEclipseMissionState /*OldState*/, EEclipseMissionState /*NewState*/);

/** Fired when an objective advances. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FEclipseObjectiveUpdated, FName /*MissionId*/, int32 /*ObjectiveIndex*/, const FEclipseObjectiveProgress& /*Progress*/);

/**
 * World-scoped mission service.
 *
 * The chain is driven by NextMissionId: completing a mission offers the next one, which is
 * what makes the vertical slice a campaign rather than five unrelated errands.
 */
UCLASS()
class ECLIPSE_API UEclipseMissionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** UTickableWorldSubsystem: seed the state table. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** UTickableWorldSubsystem: accumulate survive/defend time and time limits. */
	virtual void Tick(float DeltaTime) override;

	/** FTickableGameObject. */
	virtual TStatId GetStatId() const override;

	// ------------------------------------------------------------------
	// Lifecycle
	// ------------------------------------------------------------------

	/** Offer a mission to the squad. Fails when its gating is not satisfied. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Mission")
	bool OfferMission(FName MissionId, FString& OutRejectReason);

	/** Accept an offered mission and make it active. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Mission")
	bool StartMission(FName MissionId, FString& OutRejectReason);

	/** Complete a mission that has satisfied every objective. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Mission")
	bool CompleteMission(FName MissionId);

	/** Fail a mission, for example when its timer runs out or the squad is wiped. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Mission")
	bool FailMission(FName MissionId, const FString& Reason);

	/** State of a mission. Locked when it is unknown. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Mission")
	EEclipseMissionState GetMissionState(FName MissionId) const;

	/** Progress record for a mission, or null. */
	const FEclipseMissionProgress* GetProgress(FName MissionId) const;

	/** Missions that are currently active, in accept order. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Mission")
	TArray<FName> GetActiveMissionIds() const;

	// ------------------------------------------------------------------
	// Progress reporting
	// ------------------------------------------------------------------

	/**
	 * Report progress against the type of the active objective. Returns true when it
	 * advanced something.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Mission")
	bool ReportProgress(EEclipseObjectiveType Type, int32 Amount, AActor* Context, FName TargetItemId);

	/** Convenience: an enemy died. Credits kill objectives and gives the killer experience. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Mission")
	void ReportEnemyKilled(AActor* Killer, AActor* Victim);

	/** Convenience: a player interacted with something. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Mission")
	void ReportInteraction(AActor* Interactor, AActor* Target);

	/** Fired on state changes. */
	FEclipseMissionStateChanged OnMissionStateChanged;

	/** Fired when an objective advances. */
	FEclipseObjectiveUpdated OnObjectiveUpdated;

	/** Experience granted per enemy kill, before any multiplier. */
	static constexpr int32 ExperiencePerKill = 25;

	/** Frequency in seconds at which timed objectives are evaluated. */
	static constexpr float TimedObjectiveInterval = 0.5f;

protected:
	/** Build the initial progress record for a mission. */
	FEclipseMissionProgress MakeProgress(const UEclipseMissionDefinition& Mission) const;

	/** Advance the active objective index to the first incomplete objective. */
	void AdvanceActiveObjective(FEclipseMissionProgress& Progress, const UEclipseMissionDefinition& Mission);

	/** Grant rewards and unlock the next mission in the chain. */
	void GrantRewards(const UEclipseMissionDefinition& Mission);

	/** Set a mission's state and broadcast if it changed. */
	void SetMissionState(FName MissionId, EEclipseMissionState NewState);

	/** Accumulate survive/defend objectives while their condition holds. */
	void TickTimedObjectives(float DeltaTime);

	/** True when every gating condition for a mission is satisfied. */
	bool CheckGating(const UEclipseMissionDefinition& Mission, FString& OutRejectReason) const;

	/** Progress records by mission id. */
	UPROPERTY()
	TMap<FName, FEclipseMissionProgress> MissionStates;

	/** Seconds accumulated towards the next timed objective evaluation. */
	float TimedObjectiveAccumulator = 0.0f;
};
