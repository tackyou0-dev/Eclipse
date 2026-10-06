// PROJECT ECLIPSE - Enemy definition.
//
// Purpose
//   Everything that makes one enemy type different from another: pools, resistances,
//   behaviour archetype, spawn rules and loot. The vertical slice ships six enemies plus
//   the four-phase WARDEN PRIME boss, mirrored from
//   Content/Eclipse/Data/VerticalSlice/enemies.json.
//
// Weak points
//   A weak point maps a hit bone to a damage multiplier. The combat component asks for it
//   through IEclipseWeakPointOwner, which the enemy character implements by reading this
//   asset.

#pragma once

#include "CoreMinimal.h"
#include "Combat/EclipseDamageTypes.h"
#include "Core/EclipseTypes.h"
#include "Engine/DataAsset.h"
#include "Loot/EclipseLootTable.h"
#include "EclipseEnemyDefinition.generated.h"

/** A bone that takes extra damage. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseWeakPoint
{
	GENERATED_BODY()

	/** Bone name, matching the skeletal mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Enemy")
	FName BoneName;

	/** Damage multiplier applied to hits on this bone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Enemy", meta = (ClampMin = "1.0"))
	float DamageMultiplier = 2.0f;

	/** True when the entry is usable. */
	bool IsValidWeakPoint() const
	{
		return !BoneName.IsNone() && DamageMultiplier >= 1.0f;
	}
};

/** One phase of a boss encounter. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseBossPhase
{
	GENERATED_BODY()

	/** Zero-based phase index. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Boss", meta = (ClampMin = "0"))
	int32 PhaseIndex = 0;

	/** Name shown in the boss bar. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Boss")
	FText DisplayName;

	/** Health fraction at or below which this phase begins. Phases are ordered descending. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Boss", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthThreshold = 1.0f;

	/** Movement speed multiplier during this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Boss", meta = (ClampMin = "0.1"))
	float MoveSpeedMultiplier = 1.0f;

	/** Weak point multiplier during this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Boss", meta = (ClampMin = "1.0"))
	float WeakPointMultiplier = 1.0f;

	/** Drones summoned when the phase begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Boss", meta = (ClampMin = "0"))
	int32 DronesToDeploy = 0;

	/** Ability ids the boss may use in this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Boss")
	TArray<FName> Abilities;

	/** Arena hazard ids active in this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Boss")
	TArray<FName> ArenaHazards;
};

/**
 * An enemy or boss archetype.
 *
 * ThreatLevel drives loot gates and the spawn director's budget: a Heavy Brute costs more
 * of the AI budget than a Scout Drone, which is what stops a raid from being overrun.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseEnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable id, for example "EliteSoldier". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	FName EnemyId;

	/** Display name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	FText DisplayName;

	/** Designer-facing description. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy", meta = (MultiLine = "true"))
	FText Description;

	/** Faction the enemy belongs to. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	EEclipseFaction Faction = EEclipseFaction::Wildlife;

	/** Relative danger. Gates loot quality and AI budget cost. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy", meta = (ClampMin = "0.0"))
	float ThreatLevel = 1.0f;

	/** Health pool. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	/** Flat armour value used by the mitigation curve. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy", meta = (ClampMin = "0.0"))
	float Armor = 0.0f;

	/** Shield pool. Regenerates after a delay; Void damage bypasses it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy", meta = (ClampMin = "0.0"))
	float Shield = 0.0f;

	/** Walk speed in centimetres per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy", meta = (ClampMin = "1.0"))
	float MoveSpeed = 300.0f;

	/** Cost in the AI budget. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy", meta = (ClampMin = "0.1"))
	float ThreatWeight = 1.0f;

	/** Radius in centimetres at which this enemy's own noise attracts others. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy", meta = (ClampMin = "0.0"))
	float NoiseRadius = 300.0f;

	/** Damage school this enemy deals. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	EEclipseDamageType DamageType = EEclipseDamageType::Physical;

	/** Damage per hit. Zero for enemies that cannot attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy", meta = (ClampMin = "0.0"))
	float AttackDamage = 12.0f;

	/** Range at which the enemy will attack, in centimetres. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy", meta = (ClampMin = "0.0"))
	float AttackRange = 1500.0f;

	/** Seconds between attacks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy", meta = (ClampMin = "0.05"))
	float AttackInterval = 1.5f;

	/** Resistance profile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	FEclipseResistanceProfile Resistances;

	/** Behaviour archetype the AI controller runs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	EEclipseEnemyBehaviour Behaviour = EEclipseEnemyBehaviour::Patrol;

	/** Preferred role when the enemy joins a squad. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	EEclipseSquadRole SquadRole = EEclipseSquadRole::Assault;

	/** Biomes the enemy may spawn in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	TArray<EEclipseBiome> Biomes;

	/** True when the enemy only spawns at night. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	bool bNightOnly = false;

	/** Spawn chance multiplier per weather type. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	TMap<EEclipseWeather, float> WeatherSpawnModifiers;

	/** Loot granted on death. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	TObjectPtr<UEclipseLootTable> LootTable;

	/** Bones that take extra damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	TArray<FEclipseWeakPoint> WeakPoints;

	/** True when this is a boss with phases. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	bool bIsBoss = false;

	/** Phase table for a boss, ordered by descending health threshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Enemy")
	TArray<FEclipseBossPhase> Phases;

	/** True when the definition is complete enough to spawn. */
	bool IsValidDefinition() const
	{
		return !EnemyId.IsNone() && MaxHealth > 0.0f && ThreatWeight > 0.0f;
	}

	/** Weak point multiplier for a bone. Returns 1.0 for anything not listed. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Enemy")
	float GetWeakPointMultiplier(FName BoneName) const;

	/** Spawn chance multiplier for a weather type. Missing entries mean 1.0. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Enemy")
	float GetWeatherSpawnMultiplier(EEclipseWeather Weather) const;

	/**
	 * Phase index for a health fraction. Phases are authored with descending thresholds
	 * (1.0, 0.75, 0.5, 0.25 for the Warden) and the last matching phase wins.
	 */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Enemy")
	int32 GetPhaseIndexForHealth(float HealthFraction) const;

	/** Phase entry for an index, or null. */
	const FEclipseBossPhase* GetPhase(int32 PhaseIndex) const;
};
