// PROJECT ECLIPSE - Damage pipeline.
//
// Purpose
//   The single entry point for damage in PROJECT ECLIPSE. Every weapon, projectile,
//   explosion, environmental hazard and boss ability calls ApplyDamage; nothing calls
//   AActor::TakeDamage.
//
// Why a library
//   The arithmetic is the game's most important balance surface, and it has to be
//   testable without an engine. Keeping it in pure static functions means
//   Tests/Reference/eclipse_reference_model.py can mirror it exactly and the 33 reference
//   tests cover it on every commit.
//
// Pipeline
//   1. Resolve the target's IEclipseDamageable implementation.
//   2. Reject the hit when the target is not damageable.
//   3. Compute armour mitigation: effective armour is Armor * (1 - Penetration), and
//      mitigation is Effective / (Effective + ArmorMitigationConstant), capped.
//   4. Multiply by distance falloff, weak point, critical multiplier and resistance.
//   5. Split the result across shield and health, shield first.
//   6. Report what happened in a FEclipseDamageResult.

#pragma once

#include "Combat/EclipseDamageTypes.h"
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EclipseDamageLibrary.generated.h"

/**
 * Static damage operations.
 *
 * The Compute* functions are pure and are mirrored one-to-one by the reference model.
 */
UCLASS()
class ECLIPSE_API UEclipseDamageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Armour value at which mitigation reaches half of the cap. Balance constant. */
	static constexpr float ArmorMitigationConstant = 100.0f;

	/** Hard ceiling on armour mitigation, so armour never makes a target invulnerable. */
	static constexpr float MaxArmorMitigation = 0.9f;

	/** Damage multiplier applied at or beyond maximum range. */
	static constexpr float MinimumRangeFalloff = 0.35f;

	/**
	 * Apply damage to a target. This is the only sanctioned way to hurt something.
	 *
	 * Returns a result with bApplied == false when the target is not damageable, is
	 * already dead, or the context carried no damage.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Damage", meta = (DefaultToSelf = "Target"))
	static FEclipseDamageResult ApplyDamage(AActor* Instigator, AActor* Target, const FEclipseDamageContext& Context);

	/**
	 * Apply damage to every damageable actor within Radius of Origin, falling off
	 * linearly to MinimumRangeFalloff at the edge. Actors in IgnoreActors are skipped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Damage")
	static TArray<FEclipseDamageResult> ApplyRadialDamage(AActor* Instigator, const FVector& Origin, float Radius, const FEclipseDamageContext& Context, const TArray<AActor*>& IgnoreActors);

	/** Fraction of damage removed by armour, in [0, MaxArmorMitigation]. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Damage")
	static float ComputeArmorMitigation(float Armor, float ArmorPenetration);

	/** Resistance multiplier for a damage type, clamped by the profile. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Damage")
	static float ComputeResistanceMultiplier(const FEclipseResistanceProfile& Profile, EEclipseDamageType DamageType);

	/**
	 * Final damage of a context against a given armour value and resistance profile,
	 * before it is split across shield and health. OutArmorMitigation receives the
	 * mitigation fraction that was used.
	 */
	static float ComputeFinalDamage(const FEclipseDamageContext& Context, float Armor, const FEclipseResistanceProfile& Profile, float& OutArmorMitigation);

	/**
	 * Distance falloff multiplier for a shot fired at Distance from the shooter.
	 * 1.0 inside EffectiveRange, falling linearly to MinimumRangeFalloff at MaxRange,
	 * and held at MinimumRangeFalloff beyond it.
	 */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Damage")
	static float ComputeDistanceFalloff(float Distance, float EffectiveRange, float MaxRange);

	/** Find the damageable implementation on an actor, checking the actor then components. */
	static TScriptInterface<IEclipseDamageable> FindDamageable(AActor* Target);
};
