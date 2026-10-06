// PROJECT ECLIPSE - Projectile actor.
//
// Purpose
//   The travel and impact half of a projectile weapon. It carries a fully formed
//   FEclipseDamageContext from the shooter and hands it to the damage library on impact,
//   so a projectile hit and a hitscan hit go through exactly the same pipeline.
//
// Explosives
//   When bExplosive is set the projectile applies radial damage on impact using
//   UEclipseDamageLibrary::ApplyRadialDamage and then destroys itself.

#pragma once

#include "Combat/EclipseDamageTypes.h"
#include "Components/SphereComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "EclipseProjectile.generated.h"

/**
 * A single in-flight projectile.
 *
 * Pooling: the actor is destroyed on impact rather than pooled. The slice fires hundreds
 * of projectiles per raid, which is far below the point where pooling pays for its
 * complexity; if that changes, AEclipseProjectile is the only file to touch.
 */
UCLASS()
class ECLIPSE_API AEclipseProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEclipseProjectile();

	/** AActor: register the impact delegates. */
	virtual void BeginPlay() override;

	/**
	 * Configure the projectile. Must be called immediately after spawning, before the
	 * first tick, otherwise the projectile flies with an empty damage context.
	 */
	void InitializeProjectile(const FEclipseDamageContext& InContext, const FVector& InDirection, bool bInExplosive);

	/** Damage context carried by this projectile. */
	const FEclipseDamageContext& GetDamageContext() const { return DamageContext; }

	/** True after the projectile has detonated or been absorbed. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Projectile")
	bool HasImpacted() const { return bImpacted; }

	/** Speed in centimetres per second. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Projectile")
	float GetSpeed() const { return InitialSpeed; }

	/** Gravity scale applied to the flight path. 0 for most energy weapons. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Projectile")
	float GetGravityScale() const { return GravityScale; }

	/** Radius of the explosion, in centimetres. Zero for non-explosive rounds. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Projectile")
	float GetExplosionRadius() const { return ExplosionRadius; }

	/** Collision component used for impacts and overlaps. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Projectile")
	TObjectPtr<USphereComponent> CollisionComponent;

	/** Movement component driving the flight. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Fired when the projectile lands, before damage is applied. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FEclipseProjectileImpact, AEclipseProjectile*, const FHitResult&);
	FEclipseProjectileImpact OnImpact;

protected:
	/** Impact callback. Applies direct damage, and radial damage when explosive. */
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Overlap callback used by fast projectiles whose sweep missed a thin target. */
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** Apply the damage and destroy the projectile. Safe to call twice. */
	void Detonate(const FHitResult& Hit);

	/** Ignore actor, captured at initialisation. */
	UPROPERTY()
	TObjectPtr<AActor> IgnoredActor = nullptr;

	/** Reflection of the shot's parameters, kept for the HUD and for tests. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Projectile")
	float InitialSpeed = 12000.0f;

	/** Gravity scale. Energy rounds fly flat. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Projectile")
	float GravityScale = 0.0f;

	/** Seconds before the projectile gives up and destroys itself. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Projectile")
	float MaxLifeSeconds = 8.0f;

	/** Explosion radius for explosive delivery. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Projectile")
	float ExplosionRadius = 300.0f;

	/** True when this projectile applies radial damage on impact. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Projectile")
	bool bExplosive = false;

	/** True once the projectile has resolved its impact. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Projectile")
	bool bImpacted = false;

	/** Damage context handed over by the shooter. */
	UPROPERTY()
	FEclipseDamageContext DamageContext;
};
