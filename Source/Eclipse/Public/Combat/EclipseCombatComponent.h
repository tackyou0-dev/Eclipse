// PROJECT ECLIPSE - Combat component.
//
// Purpose
//   Firing: spread, recoil, pellet resolution, projectile spawning, noise and damage
//   application. This is the bridge between the weapon data and the damage pipeline.
//
// Authority
//   The client sends intent (start fire, stop fire, reload, aim) and predicts its own
//   muzzle effects. The server resolves every shot: it consumes ammunition, rolls spread,
//   traces, applies damage and reports noise to the AI. A client cannot deal damage by
//   calling into this component.

#pragma once

#include "Combat/EclipseDamageTypes.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Core/EclipseDeterministicRandom.h"
#include "Core/EclipseTypes.h"
#include "Weapons/EclipseWeaponComponent.h"
#include "EclipseCombatComponent.generated.h"

/** Fired on the server when a shot resolves, for analytics and the kill feed. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FEclipseShotResolved, AActor* /*Target*/, const FEclipseDamageResult& /*Result*/, FName /*WeaponId*/);

/**
 * Firing behaviour for one character.
 *
 * Spread model: BaseSpread * AimMultiplier * MovementMultiplier, where the movement
 * multiplier grows linearly with horizontal speed up to MovementSpreadScale. Recoil is
 * applied to the controller rotation immediately and partially recovered over time.
 */
UCLASS(ClassGroup = (Eclipse), meta = (BlueprintSpawnableComponent))
class ECLIPSE_API UEclipseCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEclipseCombatComponent();

	/** UActorComponent: seed the deterministic spread stream and resolve dependencies. */
	virtual void BeginPlay() override;

	/** UActorComponent: automatic fire, recoil recovery. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	// Intent (client safe)
	// ------------------------------------------------------------------

	/** Pull the trigger. Forwards to the server when called on a client. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Combat")
	void StartFire();

	/** Release the trigger. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Combat")
	void StopFire();

	/** Raise or lower the weapon. Forwards to the server when called on a client. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Combat")
	void SetAiming(bool bNewAiming);

	/** Ask for a reload. Forwards to the server when called on a client. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Combat")
	bool RequestReload();

	/** True while the trigger is held. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Combat")
	bool IsFiring() const { return bFiring; }

	/** True while aiming down sights. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Combat")
	bool IsAiming() const { return bAiming; }

	// ------------------------------------------------------------------
	// Queries used by the HUD and the AI
	// ------------------------------------------------------------------

	/** Current cone half-angle in degrees, including aim and movement modifiers. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Combat")
	float GetCurrentSpreadDegrees() const;

	/** True when the weapon could fire this instant. OutReason explains a refusal. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Combat")
	bool CanFireNow(FString& OutReason) const;

	/** Muzzle world location, or the owner's eye height when there is no weapon mesh. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Combat")
	FVector GetMuzzleLocation() const;

	/** Direction the next shot would take, excluding spread. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Combat")
	FVector GetAimDirection() const;

	/** Shots resolved since spawn. Used by analytics and by the debug overlay. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Combat")
	int32 GetShotsFired() const { return ShotsFired; }

	/** Damage applied to other actors since spawn. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Combat")
	float GetDamageDealt() const { return DamageDealt; }

	/** Resolve a single shot on the server. Public so automated tests can drive it. */
	void FireOnce();

	/** Fired when a shot resolves. */
	FEclipseShotResolved OnShotResolved;

	// ------------------------------------------------------------------
	// Tuning constants
	// ------------------------------------------------------------------

	/** Spread added at full sprint speed, as a multiplier of the base spread. */
	static constexpr float MovementSpreadScale = 1.5f;

	/** Spread multiplier while crouching. */
	static constexpr float CrouchSpreadMultiplier = 0.6f;

	/** Spread multiplier while airborne, on top of the movement term. */
	static constexpr float AirborneSpreadMultiplier = 2.0f;

	/** Fraction of a recoil kick that is recovered automatically. */
	static constexpr float RecoilRecoveryFraction = 0.65f;

	/** Degrees of recoil recovered per second. */
	static constexpr float RecoilRecoveryDegreesPerSecond = 25.0f;

	/** Impulse applied per point of damage to physics-simulating targets. */
	static constexpr float ImpulsePerDamage = 20.0f;

	/** Trace channel used for hitscan shots. */
	static constexpr ECollisionChannel ShotTraceChannel = ECollisionChannel::ECC_Visibility;

protected:
	/** Server: resolve the shot. Client: play the local prediction effects. */
	void HandleStartFire();

	/** Server: stop automatic fire. */
	void HandleStopFire();

	/** Resolve one pellet or projectile along Direction. */
	void ResolveShot(const FVector& MuzzleLocation, const FVector& Direction);

	/** Spawn a projectile for projectile weapons. Returns false when it could not spawn. */
	bool SpawnProjectile(const FVector& MuzzleLocation, const FVector& Direction, const FEclipseDamageContext& Context);

	/** Apply a damage context from a resolved hit. */
	FEclipseDamageResult ApplyHitDamage(AActor* HitActor, const FHitResult& Hit, const FEclipseDamageContext& Context) const;

	/** Build the recurring part of the damage context from the active weapon. */
	bool BuildBaseDamageContext(FEclipseDamageContext& OutContext) const;

	/** Weak point multiplier for a hit bone, looked up through IEclipseWeakPointOwner. */
	static float ResolveWeakPointMultiplier(const AActor* Target, FName BoneName);

	/** Kick the controller rotation and remember how much to recover. */
	void ApplyRecoil(float VerticalDegrees, float HorizontalDegrees);

	/** Recover part of the outstanding recoil. */
	void RecoverRecoil(float DeltaTime);

	/** Horizontal recoil direction, deterministic per shot. */
	float RollRecoilSign();

	/** Report the shot to the AI subsystem so enemies can hear it. */
	void ReportShotNoise(const FVector& Location, float Radius) const;

	/** Server RPCs: the only way a client can influence a shot. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStartFire();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStopFire();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSetAiming(bool bNewAiming);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerReload();

private:
	/** Weapon rack of the owning character. */
	UPROPERTY()
	TObjectPtr<UEclipseWeaponComponent> WeaponComponent = nullptr;

	/** Deterministic stream for spread and recoil direction. */
	FEclipseDeterministicRandom ShotRandom;

	/** World-space direction the last shot travelled, for tracers. */
	FVector LastShotDirection = FVector::ForwardVector;

	/** Outstanding recoil that still has to be given back to the player. */
	float PendingRecoilPitch = 0.0f;

	/** Outstanding horizontal recoil. */
	float PendingRecoilYaw = 0.0f;

	/** Seconds since the last shot, used for the cyclic rate limit. */
	float TimeSinceLastShot = 0.0f;

	/** True while the trigger is held. */
	bool bFiring = false;

	/** True while aiming. */
	bool bAiming = false;

	/** True once this trigger pull has fired, for single-shot and burst weapons. */
	bool bShotThisTriggerPull = false;

	/** Count of shots resolved. */
	int32 ShotsFired = 0;

	/** Total damage applied by this component. */
	float DamageDealt = 0.0f;
};
