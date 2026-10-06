// PROJECT ECLIPSE - AI controller.
//
// Purpose
//   The behaviour of a single enemy. PROJECT ECLIPSE uses a C++ state machine rather than
//   a behaviour tree for the vertical slice: the archetypes in the slice are simple enough
//   that a state machine is smaller, it diffs readably in review, and every transition can
//   be described in one page of documentation.
//
// States
//   Idle       - nothing known, watching
//   Patrol     - walking a route, or holding a guard post
//   Investigate- moving to the last noise or last known position
//   Chase      - closing on a target it has lost line of sight to
//   Attack     - target visible and in range
//   Flee       - badly hurt, breaking contact
//   Leash      - too far from home, returning
//
// Tiers
//   The controller reads its tier from UEclipseAISubsystem and sets its own tick interval,
//   so a dormant agent costs nothing per frame.

#pragma once

#include "AI/EclipseEnemyDefinition.h"
#include "AIController.h"
#include "CoreMinimal.h"
#include "EclipseAIController.generated.h"

class AEclipseEnemyCharacter;
class UEclipseAISubsystem;
class UEclipseCombatComponent;

/** States of the AI state machine. */
UENUM(BlueprintType)
enum class EEclipseAIState : uint8
{
	Idle			UMETA(DisplayName = "Idle"),
	Patrol			UMETA(DisplayName = "Patrol"),
	Investigate		UMETA(DisplayName = "Investigate"),
	Chase			UMETA(DisplayName = "Chase"),
	Attack			UMETA(DisplayName = "Attack"),
	Flee			UMETA(DisplayName = "Flee"),
	Leash			UMETA(DisplayName = "Leash"),

	Count			UMETA(Hidden)
};

/**
 * Behaviour for one AI-controlled enemy.
 *
 * Perception is explicit rather than component-based: a distance-and-field-of-view check
 * plus a line trace, refreshed on a throttle. That keeps the cost predictable with 64
 * agents and makes the AI testable without a perception system.
 */
UCLASS()
class ECLIPSE_API AEclipseAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEclipseAIController();

	/** AController: register with the AI subsystem and join a squad. */
	virtual void OnPossess(APawn* InPawn) override;

	/** AController: leave the squad and unregister. */
	virtual void OnUnPossess() override;

	/** AActor: run the state machine. */
	virtual void Tick(float DeltaSeconds) override;

	/** Current state, for debugging and for tests. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|AI")
	EEclipseAIState GetState() const { return State; }

	/** Current target, or null. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|AI")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	/** Squad role assigned by the coordinator. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|AI")
	EEclipseSquadRole GetSquadRole() const { return SquadRole; }

	/** Force a state. Used by scripted encounters and by tests. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|AI")
	void ForceState(EEclipseAIState NewState);

	/** Distance at which an enemy notices a player, in centimetres. */
	static constexpr float SightRadius = 6500.0f;

	/** Half-angle of the vision cone, in degrees. */
	static constexpr float SightHalfAngleDegrees = 70.0f;

	/** Distance at which an enemy notices a player even outside the cone. */
	static constexpr float ProximityRadius = 900.0f;

	/** Seconds between perception refreshes. */
	static constexpr float PerceptionInterval = 0.25f;

	/** Health fraction below which a non-aggressive enemy breaks contact. */
	static constexpr float FleeHealthFraction = 0.25f;

	/** Seconds an enemy keeps searching after losing a target. */
	static constexpr float SearchDurationSeconds = 8.0f;

	/** Distance in centimetres at which a moving enemy considers itself arrived. */
	static constexpr float ArrivalRadius = 120.0f;

	/** Attack range used when an enemy definition does not specify one. */
	static constexpr float AttackDistanceFallback = 1500.0f;

	/** How far a patrolling agent walks either side of its home position. */
	static constexpr float PatrolOffsetCentimetres = 1200.0f;

	/** How far a fleeing agent runs before re-evaluating. */
	static constexpr float FleeDistanceCentimetres = 2500.0f;

protected:
	/** Choose the next state from what the agent knows. */
	void EvaluateState(float DeltaSeconds);

	/** Refresh sight and hearing. */
	void UpdatePerception(float DeltaSeconds);

	/** True when the target is inside the vision cone and not blocked by geometry. */
	bool HasLineOfSight(const AActor* Target) const;

	/** Run a state for one frame. */
	void TickState(float DeltaSeconds);

	/** Move towards a world position, requesting a path when needed. */
	void MoveTowards(const FVector& Destination);

	/** Attack the current target with whatever the pawn has: a weapon or a melee hit. */
	void AttackCurrentTarget();

	/** Melee or ability attack for a pawn with no weapon component. */
	void PerformIntrinsicAttack();

	/** Switch state and reset the state timer. */
	void TransitionTo(EEclipseAIState NewState);

	/** Apply the tick interval that matches the current tier. */
	void ApplyTierTickRate();

	/** The enemy character this controller drives, or null. */
	AEclipseEnemyCharacter* GetEnemyCharacter() const;

	/** AI subsystem of the current world, or null. */
	UEclipseAISubsystem* GetAISubsystem() const;

	/** Current state. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	EEclipseAIState State = EEclipseAIState::Idle;

	/** Target the agent is engaging. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	TWeakObjectPtr<AActor> CurrentTarget;

	/** Last position the target was seen at. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	/** Position the agent spawned at, and the centre of its leash. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	FVector HomeLocation = FVector::ZeroVector;

	/** Role assigned by the squad coordinator. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	EEclipseSquadRole SquadRole = EEclipseSquadRole::Assault;

	/** Cached combat component of the pawn, if it has one. */
	UPROPERTY()
	TObjectPtr<UEclipseCombatComponent> CombatComponent = nullptr;

	/** Seconds spent in the current state. */
	float StateTimeSeconds = 0.0f;

	/** Seconds since the last perception refresh. */
	float TimeSincePerception = 0.0f;

	/** Seconds since the last attack. */
	float TimeSinceAttack = 0.0f;

	/** Seconds remaining of a search after losing the target. */
	float SearchTimeRemaining = 0.0f;
};
