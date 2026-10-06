// PROJECT ECLIPSE - Health component.
//
// Purpose
//   The standard IEclipseDamageable implementation: health, shield, armour, resistances,
//   death and revival. Characters, vehicles and destructibles all use it, which is why
//   the boss's four phases and a wooden crate's destruction go through the same code.
//
// Integration with stats
//   Maximum health and armour are read from the owner's UEclipseStatComponent when one
//   exists, so a skill that adds +25 MaxHealth is visible here without the health
//   component knowing anything about skills.

#pragma once

#include "Combat/EclipseDamageTypes.h"
#include "Combat/EclipseDamageable.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "EclipseHealthComponent.generated.h"

/** Health changed, with the delta and who caused it. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FEclipseHealthChangedSignature, float, CurrentHealth, float, MaxHealth, float, Delta, AActor*, Instigator);

/** Shield changed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEclipseShieldChangedSignature, float, CurrentShield, float, MaxShield);

/** The owner died. Killer can be null for environmental deaths. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEclipseDeathSignature, AActor*, Killer, EEclipseDamageType, KillingDamageType);

/** The owner came back. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEclipseRevivedSignature);

/**
 * Health, shield, armour and death for one actor.
 *
 * Shield regeneration is delayed after damage so that chip damage stays meaningful;
 * armour is a static pool here rather than a durability-tracked item, because equipment
 * durability is a separate system.
 */
UCLASS(ClassGroup = (Eclipse), meta = (BlueprintSpawnableComponent))
class ECLIPSE_API UEclipseHealthComponent : public UActorComponent, public IEclipseDamageable
{
	GENERATED_BODY()

public:
	UEclipseHealthComponent();

	/** UActorComponent: clamp pools and start shield regeneration ticking. */
	virtual void BeginPlay() override;

	/** UActorComponent: shield regeneration. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	// IEclipseDamageable
	// ------------------------------------------------------------------

	virtual FEclipseDamageResult ApplyEclipseDamage(const FEclipseDamageContext& Context) override;
	virtual bool IsDamageable() const override;
	virtual bool IsDead() const override;
	virtual float GetCurrentHealth() const override;
	virtual float GetMaximumHealth() const override;

	// ------------------------------------------------------------------
	// Setup
	// ------------------------------------------------------------------

	/** Configure the pools from an enemy or item definition. Clamps to the max values. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Health")
	void InitializeHealth(float InMaxHealth, float InMaxShield, float InArmor, const FEclipseResistanceProfile& InResistances);

	/** Override the configured maximum health (used by stat modifiers and buffs). */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Health")
	void SetMaxHealth(float InMaxHealth);

	/** Armour value used by the mitigation curve. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Health")
	void SetArmor(float InArmor);

	/** Replace the resistance profile. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Health")
	void SetResistances(const FEclipseResistanceProfile& InResistances);

	/** Shield regeneration rate in points per second. Zero disables it. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Health")
	void SetShieldRegeneration(float PointsPerSecond, float DelaySeconds);

	/**
	 * While invulnerable the component refuses damage but still reports IsDamageable as
	 * false, which the AI reads to stop shooting. Used by boss transitions.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Health")
	void SetInvulnerable(bool bInInvulnerable);

	// ------------------------------------------------------------------
	// Queries
	// ------------------------------------------------------------------

	/** Current health, after stat modifiers. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Health")
	float GetHealth() const { return CurrentHealth; }

	/** Health as a fraction of the current maximum, in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Health")
	float GetHealthFraction() const;

	/** Current shield. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Health")
	float GetShield() const { return CurrentShield; }

	/** Shield as a fraction of the current maximum, in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Health")
	float GetShieldFraction() const;

	/** Armour value, from the stat component when present. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Health")
	float GetArmor() const;

	/** Resistance profile in use. */
	const FEclipseResistanceProfile& GetResistances() const { return Resistances; }

	/** True while damage is refused. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Health")
	bool IsInvulnerable() const { return bInvulnerable; }

	// ------------------------------------------------------------------
	// Mutation
	// ------------------------------------------------------------------

	/** Add health, clamped to the maximum. Returns the amount actually restored. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Health")
	float Heal(float Amount);

	/** Add shield, clamped to the maximum. Returns the amount actually restored. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Health")
	float RestoreShield(float Amount);

	/** Set the shield pool directly. Used by EMP effects and by loading a save. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Health")
	void SetShield(float NewShield);

	/** Set the health pool directly. Used by loading a save. */
	void SetHealth(float NewHealth);

	/** Kill the owner without a damage event, for example falling out of the world. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Health")
	void Kill(AActor* Killer);

	/** Bring the owner back at a fraction of maximum health. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Health")
	void Revive(float HealthFraction);

	/** Health changed. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|Health")
	FEclipseHealthChangedSignature OnHealthChanged;

	/** Shield changed. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|Health")
	FEclipseShieldChangedSignature OnShieldChanged;

	/** Owner died. Broadcasts at most once per life. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|Health")
	FEclipseDeathSignature OnDeath;

	/** Owner revived. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|Health")
	FEclipseRevivedSignature OnRevived;

protected:
	/** Configured maximum health, used when no stat component overrides it. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Health", meta = (ClampMin = "1.0"))
	float ConfiguredMaxHealth = 100.0f;

	/** Configured maximum shield, used when no stat component overrides it. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Health", meta = (ClampMin = "0.0"))
	float ConfiguredMaxShield = 0.0f;

	/** Configured armour value. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Health", meta = (ClampMin = "0.0"))
	float ConfiguredArmor = 0.0f;

	/** Resistance profile. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Health")
	FEclipseResistanceProfile Resistances;

	/** Current health. */
	UPROPERTY(VisibleAnywhere, Category = "Eclipse|Health")
	float CurrentHealth = 100.0f;

	/** Current shield. */
	UPROPERTY(VisibleAnywhere, Category = "Eclipse|Health")
	float CurrentShield = 0.0f;

	/** Shield regeneration rate, points per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Health", meta = (ClampMin = "0.0"))
	float ShieldRegenPerSecond = 8.0f;

	/** Seconds after taking damage before the shield regenerates. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Health", meta = (ClampMin = "0.0"))
	float ShieldRegenDelay = 6.0f;

private:
	/** Recompute shield regeneration tick state. */
	void RefreshTickState();

	/** Read maximum health from the stat component when one is present. */
	float ResolveMaxHealth() const;

	/** Seconds since the last damage event. */
	float TimeSinceDamage = 0.0f;

	/** True while damage is refused. */
	bool bInvulnerable = false;

	/** Guards OnDeath so it fires exactly once per life. */
	bool bDeathBroadcast = false;
};
