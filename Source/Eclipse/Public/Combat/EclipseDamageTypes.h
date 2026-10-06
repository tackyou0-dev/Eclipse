// PROJECT ECLIPSE - Damage value types.
//
// Purpose
//   The context that describes one damage event and the result of applying it. Every
//   damage source in the game - bullets, melee, explosions, weather, radiation, boss
//   abilities - builds a FEclipseDamageContext and hands it to
//   UEclipseDamageLibrary::ApplyDamage. Nothing applies damage by calling
//   AActor::TakeDamage directly.
//
// Mirrored by
//   Tests/Reference/eclipse_reference_model.py: compute_armor_mitigation,
//   compute_resistance_multiplier and compute_final_damage implement exactly the same
//   arithmetic as UEclipseDamageLibrary.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "EclipseDamageTypes.generated.h"

/**
 * One damage event.
 *
 * The pipeline is: BaseDamage * DistanceFalloff * WeakPointMultiplier *
 * CriticalMultiplier * (1 - ArmorMitigation) * ResistanceMultiplier. Shields absorb
 * before health unless bIgnoreShields is set; armour is skipped when bIgnoreArmor is set,
 * which is what environmental damage uses.
 */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseDamageContext
{
	GENERATED_BODY()

	/** Authoritative damage before any mitigation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage")
	float BaseDamage = 0.0f;

	/** Damage school, used to look up resistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage")
	EEclipseDamageType DamageType = EEclipseDamageType::Physical;

	/** 0..1. Reduces the target's effective armour for this hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ArmorPenetration = 0.0f;

	/** 0..1 multiplier from range: 1.0 inside effective range, falling off to MinimumRangeFalloff. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DistanceFalloff = 1.0f;

	/** Multiplier from hitting a weak point. 1.0 when no weak point was hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage", meta = (ClampMin = "0.0"))
	float WeakPointMultiplier = 1.0f;

	/** Multiplier from a critical hit. 1.0 when the hit was not critical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage", meta = (ClampMin = "0.0"))
	float CriticalMultiplier = 1.0f;

	/** Skip the shield pool entirely. Used by Void damage and EMP effects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage")
	bool bIgnoreShields = false;

	/** Skip armour mitigation. Used by environmental and fall damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage")
	bool bIgnoreArmor = false;

	/** World-space impact point, for decals, blood and audio. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage")
	FVector HitLocation = FVector::ZeroVector;

	/** Surface normal at the impact point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage")
	FVector HitNormal = FVector::UpVector;

	/** Normalised direction the damage travelled, from attacker to target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage")
	FVector ShotDirection = FVector::ForwardVector;

	/** Physical impulse applied to ragdolls and physics objects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage", meta = (ClampMin = "0.0"))
	float ImpactImpulse = 0.0f;

	/** Bone or weak-point name that was hit, or None for a body hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage")
	FName HitBoneName;

	/** Actor that caused the damage. May be null for world hazards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage")
	TObjectPtr<AActor> Instigator = nullptr;

	/** Weapon, hazard or ability id, for analytics and kill feed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage")
	FName DamageSourceId;

	/** True when there is anything to apply. */
	bool IsValid() const
	{
		return BaseDamage > 0.0f && DamageType != EEclipseDamageType::Count;
	}

	/** Convenience: build a simple direct-hit context. */
	static FEclipseDamageContext MakeDirect(float InDamage, EEclipseDamageType InType, AActor* InInstigator)
	{
		FEclipseDamageContext Context;
		Context.BaseDamage = InDamage;
		Context.DamageType = InType;
		Context.Instigator = InInstigator;
		return Context;
	}
};

/** What actually happened when damage was applied. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseDamageResult
{
	GENERATED_BODY()

	/** False when the target had no health component or was already dead. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Damage")
	bool bApplied = false;

	/** True when this hit reduced the target to zero health. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Damage")
	bool bKilled = false;

	/** Damage before mitigation. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Damage")
	float RawDamage = 0.0f;

	/** Damage after armour and resistance, before it was split across shield and health. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Damage")
	float MitigatedDamage = 0.0f;

	/** Fraction of the incoming damage that armour removed, in [0, MaxArmorMitigation]. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Damage")
	float ArmorMitigation = 0.0f;

	/** Resistance multiplier that was applied. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Damage")
	float ResistanceMultiplier = 1.0f;

	/** Damage absorbed by the shield pool. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Damage")
	float ShieldDamage = 0.0f;

	/** Damage taken off the health pool. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Damage")
	float HealthDamage = 0.0f;

	/** Health remaining after the hit. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Damage")
	float HealthRemaining = 0.0f;

	/** Shield remaining after the hit. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Damage")
	float ShieldRemaining = 0.0f;
};

/**
 * Per-damage-type resistance multipliers.
 *
 * 1.0 means "takes normal damage", 0.0 means immune, 2.0 means double damage. Values are
 * clamped to [MinimumResistance, MaximumResistance] before use, so a data entry typo
 * cannot make something invulnerable or one-shot.
 */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseResistanceProfile
{
	GENERATED_BODY()

	/** Multiplier per damage type. Missing types read as 1.0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Damage")
	TMap<EEclipseDamageType, float> Resistances;

	/** Lowest multiplier the profile can report. */
	static constexpr float MinimumResistance = 0.0f;

	/** Highest multiplier the profile can report. */
	static constexpr float MaximumResistance = 2.0f;

	/** Clamped multiplier for a damage type. */
	float Get(EEclipseDamageType DamageType) const;

	/** Set a multiplier, clamped to the legal range. */
	void Set(EEclipseDamageType DamageType, float Multiplier);
};
