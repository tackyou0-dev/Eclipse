// PROJECT ECLIPSE - Stat component.
//
// Purpose
//   Aggregates every stat modifier a character is subject to - skills, equipment,
//   attachments, status effects - into one query surface, and simulates stamina.
//
// Why a component rather than fields on the character
//   Modifiers arrive from unrelated systems at unrelated times and must be removable by
//   source id. Keeping them in one place means "why is my movement speed 1180?" has one
//   answer, and it can be printed by the debug command.
//
// Contract
//   GetStat returns (Base + Additive) * Multiplicative, matching
//   FEclipseStatAggregate::Apply and the Python reference model.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/EclipseTypes.h"
#include "EclipseStatComponent.generated.h"

/** One named group of modifiers. Sources are replaced wholesale, not merged. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseModifierSource
{
	GENERATED_BODY()

	/** Stable id, for example "Skill.RecoilControl" or "Attachment.Muzzle.Suppressor". */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Stats")
	FName SourceId;

	/** Modifiers contributed while the source is active. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Stats")
	TArray<FEclipseStatModifier> Modifiers;
};

/** Fired when a stat's final value changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEclipseStatChangedSignature, EEclipseStat, Stat, float, NewValue);

/** Fired when stamina hits zero. Sprint and abilities listen to this. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEclipseStaminaDepletedSignature);

/** Fired when stamina hits zero. Sprint and abilities listen to this. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEclipseStaminaDepletedSignature);

/**
 * Per-character stat aggregation and stamina simulation.
 *
 * Ticks only when it has a stamina pool to simulate, and only on the authority (the
 * server owns stamina; clients make a local prediction that is corrected by the server's
 * replicated value).
 */
UCLASS(ClassGroup = (Eclipse), meta = (BlueprintSpawnableComponent))
class ECLIPSE_API UEclipseStatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEclipseStatComponent();

	/** UActorComponent: seed the default base stats and decide whether to tick. */
	virtual void BeginPlay() override;

	/** UActorComponent: stamina regeneration. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	// Base stats
	// ------------------------------------------------------------------

	/** Set the unmodified value of a stat. Negative values are clamped to zero. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Stats")
	void SetBaseStat(EEclipseStat Stat, float Value);

	/** Unmodified value of a stat. Returns zero for None or unknown stats. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Stats")
	float GetBaseStat(EEclipseStat Stat) const;

	/**
	 * Final value of a stat after all active sources: (Base + Additive) * Multiplicative.
	 * Stats that are ratios (for example SprintSpeed multipliers) still start from a base
	 * of 1.0, so callers multiply rather than add.
	 */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Stats")
	float GetStat(EEclipseStat Stat) const;

	/** The aggregate for a stat, or null when nothing modifies it. */
	const FEclipseStatAggregate* GetAggregate(EEclipseStat Stat) const;

	// ------------------------------------------------------------------
	// Modifier sources
	// ------------------------------------------------------------------

	/**
	 * Register or replace a modifier source. Replacing is deliberate: a skill that is
	 * re-applied must not stack with itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Stats")
	void AddModifierSource(FName SourceId, const TArray<FEclipseStatModifier>& Modifiers);

	/** Remove a source. Returns true when it existed. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Stats")
	bool RemoveModifierSource(FName SourceId);

	/** True when a source with this id is currently applied. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Stats")
	bool HasModifierSource(FName SourceId) const;

	/** Drop every source. Used on respawn and on load. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Stats")
	void ClearModifierSources();

	/** Every active source id, for the debug overlay and save files. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Stats")
	TArray<FName> GetModifierSourceIds() const;

	// ------------------------------------------------------------------
	// Stamina
	// ------------------------------------------------------------------

	/** Current stamina. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Stamina")
	float GetStamina() const { return CurrentStamina; }

	/** Stamina cap, taken from EEclipseStat::MaxStamina. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Stamina")
	float GetMaxStamina() const;

	/** Stamina as a fraction of the cap, in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Stamina")
	float GetStaminaFraction() const;

	/** True when the pool is empty. Sprinting is refused in this state. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Stamina")
	bool IsStaminaDepleted() const { return CurrentStamina <= 0.0f; }

	/** Spend stamina. Returns false (and spends nothing) when the pool is too small. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Stamina")
	bool ConsumeStamina(float Amount);

	/** Add stamina, clamped to the cap. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Stamina")
	void RestoreStamina(float Amount);

	/** Drain stamina for one frame of sprinting. Returns false when the sprint must end. */
	bool ConsumeSprintStamina(float DeltaSeconds);

	/** Refill to the cap. Used on respawn. */
	void RefillStamina();

	/** Stamina regenerated per second once the delay has elapsed. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Stamina")
	void SetStaminaRegenRate(float PerSecond) { StaminaRegenPerSecond = FMath::Max(0.0f, PerSecond); }

	/** Seconds of inactivity before stamina starts regenerating. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Stamina")
	void SetStaminaRegenDelay(float Seconds) { StaminaRegenDelay = FMath::Max(0.0f, Seconds); }

	/** Stamina drained per second of sprinting. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Stamina")
	void SetSprintStaminaPerSecond(float PerSecond) { SprintStaminaPerSecond = FMath::Max(0.0f, PerSecond); }

	/** Directly set the current stamina, used when a save is applied. */
	void SetStamina(float NewStamina);

	/** UActorComponent: replicate the stamina pool to clients. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Fired when a stat's final value changes. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|Stats")
	FEclipseStatChangedSignature OnStatChanged;

	/** Fired when stamina reaches zero. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|Stamina")
	FEclipseStaminaDepletedSignature OnStaminaDepleted;

protected:
	/** Broadcast OnStatChanged for every stat touched by a source. */
	void BroadcastStatChanges(const TArray<FEclipseStatModifier>& Modifiers);

	/** Recompute the per-stat aggregate cache from ActiveSources. */
	void RebuildAggregates();

private:
	/** Unmodified stat values. Missing entries read as zero. */
	UPROPERTY()
	TMap<EEclipseStat, float> BaseStats;

	/** Active modifier sources keyed by source id. */
	UPROPERTY()
	TMap<FName, FEclipseModifierSource> ActiveSources;

	/** Cached aggregate per stat, rebuilt whenever a source changes. */
	TMap<EEclipseStat, FEclipseStatAggregate> Aggregates;

	/** Current stamina. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentStamina)
	float CurrentStamina = 100.0f;

	/** Stamina regenerated per second after the delay. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Stamina")
	float StaminaRegenPerSecond = 18.0f;

	/** Seconds since the last stamina spend before regeneration starts. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Stamina")
	float StaminaRegenDelay = 1.25f;

	/** Stamina drained per second while sprinting. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Stamina")
	float SprintStaminaPerSecond = 12.0f;

	/** Seconds since the last stamina spend. */
	float TimeSinceStaminaSpend = 0.0f;

	/** RepNotify so a client's predicted stamina snaps back to the server value. */
	UFUNCTION()
	void OnRep_CurrentStamina();
};
