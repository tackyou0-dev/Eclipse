// PROJECT ECLIPSE - AI subsystem.
//
// Purpose
//   The AI budget and the shared blackboard: who is registered, which tier each agent is
//   in, what noise has been made and which target a squad is focusing.
//
// The budget rule
//   The vertical slice targets 64 active agents. Tiers are assigned from distance to the
//   nearest player, and the subsystem refuses to activate an agent when the accumulated
//   threat weight would exceed UEclipseDeveloperSettings::MaxActiveAIAgents. Tier
//   assignment runs on a timer rather than per frame, because it is a global decision and
//   recomputing it 64 times per frame would defeat its own purpose.

#pragma once

#include "AI/EclipseAITypes.h"
#include "AI/EclipseSquadCoordinator.h"
#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "EclipseAISubsystem.generated.h"

class UEclipseEnemyDefinition;

/**
 * World-scoped AI service.
 *
 * Nothing here controls an individual agent: the controllers do that. This is the shared
 * state they all consult, which is what keeps a squad coordinated without agents talking
 * to each other directly.
 */
UCLASS()
class ECLIPSE_API UEclipseAISubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** UWorldSubsystem: start the tier refresh timer. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** UWorldSubsystem: clear the registry and the timer. */
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	// Registration and budget
	// ------------------------------------------------------------------

	/** Register an agent and its definition. */
	void RegisterAgent(AActor* Agent, UEclipseEnemyDefinition* Definition);

	/** Remove an agent. */
	void UnregisterAgent(AActor* Agent);

	/** Tier an agent is currently assigned. Dormant for unknown agents. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|AI")
	EEclipseAITier GetTier(const AActor* Agent) const;

	/** Number of registered agents. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|AI")
	int32 GetRegisteredAgentCount() const { return Agents.Num(); }

	/** Sum of the threat weight of every high-frequency agent. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|AI")
	float GetActiveThreatWeight() const;

	/** True when the budget can afford one more agent of this weight. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|AI")
	bool CanAffordAgent(float ThreatWeight) const;

	/** How many registered agents of a faction other than ExcludeFaction are near a point. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|AI")
	int32 CountAgentsNear(const FVector& Location, float RadiusCentimetres, EEclipseFaction ExcludeFaction) const;

	/** Refresh every agent's tier. Called on a timer and by tests. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|AI")
	void RefreshTiers();

	// ------------------------------------------------------------------
	// Noise
	// ------------------------------------------------------------------

	/** Record a noise event and wake any agent that can hear it. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|AI")
	void ReportNoise(const FVector& Location, float Radius, AActor* Instigator);

	/** Noise events this agent can currently hear. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|AI")
	TArray<FEclipseNoiseEvent> GetAudibleNoise(const AActor* Agent) const;

	/** Drop noise older than EclipseAI::NoiseMemorySeconds. Called by the timer. */
	void PruneNoise();

	// ------------------------------------------------------------------
	// Targets and squads
	// ------------------------------------------------------------------

	/**
	 * Best target for an agent: the highest scoring hostile actor within range. Scoring is
	 * EclipseAI::ScoreTarget, so preference for players and for the squad focus is explicit
	 * rather than accidental.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|AI")
	AActor* FindBestTarget(AActor* Agent, float MaxRangeCentimetres) const;

	/** Squad coordinator owned by this subsystem. Never null after initialisation. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|AI")
	UEclipseSquadCoordinator* GetSquadCoordinator() const { return SquadCoordinator; }

	/** Join (or create) a squad for an agent. */
	void JoinSquad(AActor* Agent, EEclipseFaction Faction);

	/** Leave whatever squad the agent is in. */
	void LeaveSquad(AActor* Agent);

	/** Interval between tier refreshes, in seconds. */
	static constexpr float TierRefreshInterval = 0.5f;

	/** Hard cap on remembered noise events, so a firefight cannot grow the list forever. */
	static constexpr int32 MaxRecordedNoiseEvents = 64;

protected:
	/** One registered agent. */
	struct FRegisteredAgent
	{
		/** Agent actor. */
		TWeakObjectPtr<AActor> Agent;

		/** Definition driving its behaviour. */
		TWeakObjectPtr<const UEclipseEnemyDefinition> Definition;

		/** Assigned tier. */
		EEclipseAITier Tier = EEclipseAITier::Dormant;
	};

	/** Registered agents. */
	TArray<FRegisteredAgent> Agents;

	/** Recent noise events. */
	TArray<FEclipseNoiseEvent> NoiseEvents;

	/** Squad bookkeeping. */
	UPROPERTY()
	TObjectPtr<UEclipseSquadCoordinator> SquadCoordinator = nullptr;

	/** Timer handle for the tier refresh. */
	FTimerHandle TierRefreshTimer;
};
