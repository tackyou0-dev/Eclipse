// PROJECT ECLIPSE - Projectile implementation.
//
// Purpose
//   Flight, impact and damage. All arithmetic is delegated to the damage library so a
//   rocket and a rifle round agree on what damage means.

#include "Combat/EclipseProjectile.h"

#include "Combat/EclipseDamageLibrary.h"
#include "Combat/EclipseWeakPoint.h"
#include "Core/EclipseLog.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

AEclipseProjectile::AEclipseProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(4.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetNotifyRigidBodyCollision(false);
	RootComponent = CollisionComponent;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->InitialSpeed = 12000.0f;
	ProjectileMovement->MaxSpeed = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
}

void AEclipseProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionComponent != nullptr)
	{
		CollisionComponent->OnComponentHit.AddDynamic(this, &AEclipseProjectile::HandleHit);
		CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AEclipseProjectile::HandleBeginOverlap);
	}

	// Lifetime is a backstop for a projectile that never hits anything (flying off the
	// map edge, or spawned inside geometry).
	if (MaxLifeSeconds > 0.0f)
	{
		SetLifeSpan(MaxLifeSeconds);
	}
}

void AEclipseProjectile::InitializeProjectile(const FEclipseDamageContext& InContext, const FVector& InDirection, bool bInExplosive)
{
	DamageContext = InContext;
	IgnoredActor = InContext.Instigator;
	bExplosive = bInExplosive;

	if (IgnoredActor != nullptr)
	{
		CollisionComponent->IgnoreActorWhenMoving(IgnoredActor, true);
	}

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->InitialSpeed = InitialSpeed;
		ProjectileMovement->ProjectileGravityScale = GravityScale;
		ProjectileMovement->Velocity = InDirection.GetSafeNormal() * InitialSpeed;
	}
}

void AEclipseProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	Detonate(Hit);
}

void AEclipseProjectile::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// An overlap is only an impact when the swept trace actually produced a hit point;
	// otherwise the projectile is still inside its own spawn volume.
	if (!bFromSweep || OtherActor == IgnoredActor)
	{
		return;
	}

	Detonate(SweepResult);
}

void AEclipseProjectile::Detonate(const FHitResult& Hit)
{
	if (bImpacted)
	{
		return;
	}

	bImpacted = true;

	// Freeze the projectile immediately: the damage below may destroy the target, and a
	// destroyed target must not leave a projectile flying through the corpse.
	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
	if (CollisionComponent != nullptr)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	OnImpact.Broadcast(this, Hit);

	AActor* HitActor = Hit.GetActor();
	AActor* Instigator = DamageContext.Instigator;

	FEclipseDamageContext Context = DamageContext;
	Context.HitLocation = Hit.ImpactPoint.IsNearlyZero() ? GetActorLocation() : Hit.ImpactPoint;
	Context.HitNormal = Hit.ImpactNormal.IsNearlyZero() ? GetActorForwardVector() * -1.0f : Hit.ImpactNormal;
	Context.ShotDirection = GetActorForwardVector();
	Context.HitBoneName = Hit.BoneName;

	if (HitActor != nullptr)
	{
		if (const IEclipseWeakPointOwner* WeakPointOwner = Cast<const IEclipseWeakPointOwner>(HitActor))
		{
			Context.WeakPointMultiplier = FMath::Max(1.0f, WeakPointOwner->GetWeakPointMultiplier(Hit.BoneName));
		}

		UEclipseDamageLibrary::ApplyDamage(Instigator, HitActor, Context);
	}

	if (bExplosive && ExplosionRadius > 0.0f)
	{
		TArray<AActor*> IgnoreActors;
		if (Instigator != nullptr)
		{
			IgnoreActors.Add(Instigator);
		}

		// The direct hit already took full damage; the blast adds splash to everything
		// else in radius, which is what makes grenades dangerous indoors.
		UEclipseDamageLibrary::ApplyRadialDamage(Instigator, Context.HitLocation, ExplosionRadius, Context, IgnoreActors);
	}

	// Small grace period so impact effects can play against a stationary projectile.
	SetLifeSpan(0.15f);
}
