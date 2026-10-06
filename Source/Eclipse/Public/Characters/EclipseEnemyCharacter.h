// PROJECT ECLIPSE - Enemy character.
//
// Purpose
//   The actor side of an enemy: applies its UEclipseEnemyDefinition to the health, stats
//   and movement components, answers weak point queries, drives boss phases and drops
//   loot on death.
//
// Boss phases
//   The Warden's four phases are data on the enemy definition. This class watches the
//   health fraction, advances the phase, applies the phase's speed and weak point
//   multipliers and reports the change so the game state can replicate it to the HUD.

#pragma once

#include "AI/EclipseEnemyDefinition.h"
#include "Characters/EclipseCharacterBase.h"
#include "Combat/EclipseWeakPoint.h"
#include "Inventory/EclipsePickupActor.h"
#include "CoreMinimal.h"
#include "EclipseEnemyCharacter.generated.h"

class UEclipseLootSubsystem;

/** Fired when a boss advances a phase. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FEclipseBossPhaseChanged, AEclipseEnemyCharacter* /*Boss*/, int32 /*PhaseIndex*/, const FEclipseBossPhase* /*Phase*/);

/**
 * An AI-controlled enemy.
 *
 * The character is deliberately passive: it applies data and reacts to damage. Decisions
 * live in AEclipseAIController, which is what keeps the enemy type table and the
 * behaviour code readable as two separate things.
 */
UCLASS()
class ECLIPSE_API AEclipseEnemyCharacter : public AEclipseCharacterBase, public IEclipseWeakPointOwner
{
	GENERATED_BODY()

public:
	AEclipseEnemyCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** AActor: apply the definition. */
	virtual void BeginPlay() override;

	/** Apply an enemy definition: pools, resistances, faction, speed and initial phase. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Enemy")
	void InitializeFromDefinition(UEclipseEnemyDefinition* Definition);

	/** Definition in use, or null. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Enemy")
	UEclipseEnemyDefinition* GetEnemyDefinition() const { return EnemyDefinition; }

	/** IEclipseWeakPointOwner: bone damage multiplier, phase-multiplied for bosses. */
	virtual float GetWeakPointMultiplier(FName BoneName) const override;

	/** Current boss phase index. Always 0 for non-boss enemies. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Enemy")
	int32 GetBossPhaseIndex() const { return BossPhaseIndex; }

	/** Current phase data, or null for a non-boss. */
	const FEclipseBossPhase* GetCurrentBossPhase() const;

	/** Health fraction in [0, 1], for the HUD. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Enemy")
	float GetHealthFraction() const;

	/** Called when the health component reports damage, to advance boss phases. */
	void EvaluateBossPhase();

	/** Fired when a phase advances. */
	FEclipseBossPhaseChanged OnBossPhaseChanged;

protected:
	/** AEclipseCharacterBase: apply the definition's stats through the base setup path. */
	virtual void InitializeCharacter() override;

	/** AEclipseCharacterBase: drop loot when the enemy dies. */
	virtual void HandleDeath(AActor* Killer, EEclipseDamageType KillingDamageType) override;

	/** Health changed: advance the boss phase when a threshold is crossed. */
	UFUNCTION()
	void HandleHealthChangedForPhase(float CurrentHealth, float MaxHealth, float Delta, AActor* DamageInstigator);

	/** Bone that takes extra damage, from the definition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Enemy")
	TObjectPtr<UEclipseEnemyDefinition> EnemyDefinition;

	/** Current boss phase index. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Enemy")
	int32 BossPhaseIndex = 0;

	/** Actor class used to spawn loot stacks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	TSubclassOf<AEclipsePickupActor> PickupClass;

	/** True once the death loot has been granted, so it can never drop twice. */
	bool bLootGranted = false;
};
