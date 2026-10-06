// PROJECT ECLIPSE - Combat component implementation.
//
// Purpose
//   Shot resolution. Read this file together with UEclipseDamageLibrary: this decides
//   where a shot goes, the library decides what it does when it lands.

#include "Combat/EclipseCombatComponent.h"

#include "AI/EclipseAISubsystem.h"
#include "Combat/EclipseDamageLibrary.h"
#include "Combat/EclipseProjectile.h"
#include "Combat/EclipseWeakPoint.h"
#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseLog.h"
#include "Core/EclipseStatComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Weapons/EclipseWeaponInstance.h"

UEclipseCombatComponent::UEclipseCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UEclipseCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	WeaponComponent = Owner->FindComponentByClass<UEclipseWeaponComponent>();
	if (WeaponComponent == nullptr)
	{
		UE_LOG(LogEclipseCombat, Warning, TEXT("%s has a combat component but no weapon component; it cannot fire."),
			*Owner->GetName());
	}

	// Spread and recoil direction are seeded from the world seed and the actor's name, so
	// a replay of the same raid produces the same shot pattern on every machine.
	const uint32 Seed = FEclipseDeterministicRandom::MixSeed(
		UEclipseDeveloperSettings::GetWorldSeed(),
		FEclipseDeterministicRandom::StableIdFromString(Owner->GetName()));
	ShotRandom.Reset(Seed);
}

void UEclipseCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	TimeSinceLastShot += DeltaTime;
	RecoverRecoil(DeltaTime);

	// Only the server fires. A client keeps bFiring for animation and prediction.
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority() || !bFiring)
	{
		return;
	}

	const UEclipseWeaponInstance* Weapon = WeaponComponent != nullptr ? WeaponComponent->GetActiveWeapon() : nullptr;
	if (Weapon == nullptr || Weapon->GetDefinition() == nullptr)
	{
		return;
	}

	const bool bAutomatic = Weapon->GetDefinition()->IsAutomatic();
	if (!bAutomatic && bShotThisTriggerPull)
	{
		return;
	}

	FString Reason;
	if (!CanFireNow(Reason))
	{
		return;
	}

	FireOnce();
}

void UEclipseCombatComponent::StartFire()
{
	if (bFiring)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	if (Owner->HasAuthority())
	{
		HandleStartFire();
		return;
	}

	// Prediction: the client plays the trigger state immediately so the animation does not
	// wait for a round trip, and asks the server to resolve the shot.
	bFiring = true;
	bShotThisTriggerPull = false;
	ServerStartFire();
}

void UEclipseCombatComponent::StopFire()
{
	if (!bFiring)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (Owner != nullptr && Owner->HasAuthority())
	{
		HandleStopFire();
		return;
	}

	bFiring = false;
	ServerStopFire();
}

void UEclipseCombatComponent::HandleStartFire()
{
	bFiring = true;
	bShotThisTriggerPull = false;
}

void UEclipseCombatComponent::HandleStopFire()
{
	bFiring = false;
}

void UEclipseCombatComponent::SetAiming(bool bNewAiming)
{
	if (bAiming == bNewAiming)
	{
		return;
	}

	bAiming = bNewAiming;

	AActor* Owner = GetOwner();
	if (Owner != nullptr && !Owner->HasAuthority())
	{
		ServerSetAiming(bNewAiming);
	}
}

bool UEclipseCombatComponent::RequestReload()
{
	if (WeaponComponent == nullptr)
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (Owner != nullptr && !Owner->HasAuthority())
	{
		ServerReload();
		// The client cannot know whether the reload will be accepted, so it reports
		// success optimistically and the HUD corrects itself on the next replicated state.
		return true;
	}

	return WeaponComponent->StartReload();
}

bool UEclipseCombatComponent::CanFireNow(FString& OutReason) const
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		OutReason = TEXT("No owner.");
		return false;
	}

	if (WeaponComponent == nullptr)
	{
		OutReason = TEXT("No weapon component.");
		return false;
	}

	if (WeaponComponent->IsReloading())
	{
		OutReason = TEXT("Reloading.");
		return false;
	}

	if (WeaponComponent->IsOverheated())
	{
		OutReason = TEXT("Overheated.");
		return false;
	}

	const UEclipseWeaponInstance* Weapon = WeaponComponent->GetActiveWeapon();
	if (Weapon == nullptr || Weapon->GetDefinition() == nullptr)
	{
		OutReason = TEXT("No weapon equipped.");
		return false;
	}

	if (Weapon->IsEmpty())
	{
		OutReason = TEXT("Empty magazine.");
		return false;
	}

	const float ShotInterval = Weapon->GetDefinition()->GetShotIntervalSeconds();
	if (ShotInterval > 0.0f && TimeSinceLastShot < ShotInterval)
	{
		OutReason = TEXT("Cyclic rate limit.");
		return false;
	}

	OutReason.Reset();
	return true;
}

FVector UEclipseCombatComponent::GetMuzzleLocation() const
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return FVector::ZeroVector;
	}

	// The weapon mesh is optional while the slice is grey-boxed, so fall back to the eye
	// height of the owner.
	const APawn* Pawn = Cast<APawn>(Owner);
	if (Pawn != nullptr && Pawn->GetController() != nullptr)
	{
		return Pawn->GetPawnViewLocation();
	}

	return Owner->GetActorLocation();
}

FVector UEclipseCombatComponent::GetAimDirection() const
{
	const AActor* Owner = GetOwner();
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		if (const AController* Controller = Pawn->GetController())
		{
			return Controller->GetControlRotation().Vector();
		}
	}

	return Owner != nullptr ? Owner->GetActorForwardVector() : FVector::ForwardVector;
}

float UEclipseCombatComponent::GetCurrentSpreadDegrees() const
{
	const UEclipseWeaponInstance* Weapon = WeaponComponent != nullptr ? WeaponComponent->GetActiveWeapon() : nullptr;
	if (Weapon == nullptr)
	{
		return 0.0f;
	}

	float Spread = Weapon->GetStat(EEclipseStat::SpreadDegrees);
	if (Spread <= 0.0f)
	{
		return 0.0f;
	}

	if (bAiming && Weapon->GetDefinition() != nullptr)
	{
		Spread *= Weapon->GetDefinition()->AimSpreadMultiplier;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn != nullptr)
	{
		float ReferenceSpeed = 0.0f;
		if (const UEclipseStatComponent* StatComponent = Pawn->FindComponentByClass<UEclipseStatComponent>())
		{
			ReferenceSpeed = StatComponent->GetStat(EEclipseStat::MoveSpeed);
		}

		if (ReferenceSpeed > 0.0f)
		{
			const float SpeedRatio = FMath::Clamp(Pawn->GetVelocity().Size2D() / ReferenceSpeed, 0.0f, 1.0f);
			Spread *= 1.0f + MovementSpreadScale * SpeedRatio;
		}

		if (const UCharacterMovementComponent* Movement = Pawn->FindComponentByClass<UCharacterMovementComponent>())
		{
			if (Movement->IsFalling())
			{
				Spread *= AirborneSpreadMultiplier;
			}
			if (Movement->IsCrouching())
			{
				Spread *= CrouchSpreadMultiplier;
			}
		}
	}

	return Spread;
}

FVector UEclipseCombatComponent::ApplySpread(const FVector& BaseDirection, float SpreadDegrees)
{
	if (SpreadDegrees <= 0.0f)
	{
		return BaseDirection;
	}

	// Uniform sample inside the cone: a square of random pitch/yaw would over-represent
	// the corners, so the radius is square-rooted.
	const float Angle = ShotRandom.Range(0.0f, 360.0f);
	const float Radius = SpreadDegrees * FMath::Sqrt(ShotRandom.NextFloat());

	const FRotator BaseRotation = BaseDirection.Rotation();
	const FRotator Offset(0.0f, Angle, 0.0f);
	const FVector OffsetAxis = Offset.RotateVector(FVector::UpVector);
	const FVector OffsetDirection = BaseDirection.RotateAngleAxis(Radius, FVector::CrossProduct(BaseDirection, OffsetAxis).GetSafeNormal());

	return OffsetDirection.GetSafeNormal();
}

float UEclipseCombatComponent::RollRecoilSign()
{
	return ShotRandom.Chance(0.5f) ? 1.0f : -1.0f;
}

void UEclipseCombatComponent::ApplyRecoil(float VerticalDegrees, float HorizontalDegrees)
{
	if (VerticalDegrees == 0.0f && HorizontalDegrees == 0.0f)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Controller = Pawn != nullptr ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (Controller == nullptr)
	{
		// AI weapons kick their aim directly; there is no camera to shake.
		return;
	}

	FRotator Rotation = Controller->GetControlRotation();
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch - VerticalDegrees, -89.0f, 89.0f);
	Rotation.Yaw += HorizontalDegrees;
	Controller->SetControlRotation(Rotation);

	PendingRecoilPitch += VerticalDegrees * RecoilRecoveryFraction;
	PendingRecoilYaw += HorizontalDegrees * RecoilRecoveryFraction;
}

void UEclipseCombatComponent::RecoverRecoil(float DeltaTime)
{
	if (FMath::IsNearlyZero(PendingRecoilPitch) && FMath::IsNearlyZero(PendingRecoilYaw))
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Controller = Pawn != nullptr ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (Controller == nullptr)
	{
		PendingRecoilPitch = 0.0f;
		PendingRecoilYaw = 0.0f;
		return;
	}

	const float Step = RecoilRecoveryDegreesPerSecond * DeltaTime;
	const float PitchStep = FMath::Sign(PendingRecoilPitch) * FMath::Min(FMath::Abs(PendingRecoilPitch), Step);
	const float YawStep = FMath::Sign(PendingRecoilYaw) * FMath::Min(FMath::Abs(PendingRecoilYaw), Step);

	if (FMath::IsNearlyZero(PitchStep) && FMath::IsNearlyZero(YawStep))
	{
		return;
	}

	FRotator Rotation = Controller->GetControlRotation();
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch + PitchStep, -89.0f, 89.0f);
	Rotation.Yaw -= YawStep;
	Controller->SetControlRotation(Rotation);

	PendingRecoilPitch -= PitchStep;
	PendingRecoilYaw -= YawStep;
}

bool UEclipseCombatComponent::BuildBaseDamageContext(FEclipseDamageContext& OutContext) const
{
	const UEclipseWeaponInstance* Weapon = WeaponComponent != nullptr ? WeaponComponent->GetActiveWeapon() : nullptr;
	if (Weapon == nullptr || Weapon->GetDefinition() == nullptr)
	{
		return false;
	}

	const UEclipseWeaponDefinition* Definition = Weapon->GetDefinition();
	const FEclipseWeaponStats Stats = Weapon->GetEffectiveStats();

	OutContext.BaseDamage = Stats.Damage;
	OutContext.DamageType = Definition->DamageType;
	OutContext.ArmorPenetration = Stats.ArmorPenetration;
	OutContext.DistanceFalloff = 1.0f;
	OutContext.WeakPointMultiplier = 1.0f;
	OutContext.CriticalMultiplier = 1.0f;
	OutContext.bIgnoreShields = Definition->DamageType == EEclipseDamageType::Void;
	OutContext.bIgnoreArmor = false;
	OutContext.ImpactImpulse = Stats.Damage * ImpulsePerDamage;
	OutContext.Instigator = GetOwner();
	OutContext.DamageSourceId = Definition->WeaponId;
	return true;
}

float UEclipseCombatComponent::ResolveWeakPointMultiplier(const AActor* Target, FName BoneName)
{
	if (Target == nullptr || BoneName.IsNone())
	{
		return 1.0f;
	}

	if (const IEclipseWeakPointOwner* WeakPointOwner = Cast<const IEclipseWeakPointOwner>(Target))
	{
		return FMath::Max(1.0f, WeakPointOwner->GetWeakPointMultiplier(BoneName));
	}

	return 1.0f;
}

void UEclipseCombatComponent::FireOnce()
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		// Firing is server-only. A client that reaches this point is misconfigured, and
		// saying so loudly is cheaper than debugging silent no-damage reports.
		UE_LOG(LogEclipseSecurity, Warning, TEXT("FireOnce called on a non-authoritative combat component; shot refused."));
		return;
	}

	FString Reason;
	if (!CanFireNow(Reason))
	{
		UE_LOG(LogEclipseCombat, Verbose, TEXT("Shot refused: %s"), *Reason);
		return;
	}

	UEclipseWeaponInstance* Weapon = WeaponComponent->GetActiveWeapon();
	const UEclipseWeaponDefinition* Definition = Weapon->GetDefinition();
	const FEclipseWeaponStats Stats = Weapon->GetEffectiveStats();

	const FVector MuzzleLocation = GetMuzzleLocation();
	const FVector BaseDirection = GetAimDirection();
	const float Spread = GetCurrentSpreadDegrees();
	const int32 Pellets = FMath::Max(1, Stats.PelletCount);

	for (int32 PelletIndex = 0; PelletIndex < Pellets; ++PelletIndex)
	{
		const FVector Direction = ApplySpread(BaseDirection, Spread);
		ResolveShot(MuzzleLocation, Direction);
	}

	WeaponComponent->ConsumeAmmo(1);
	WeaponComponent->AddHeat(Stats.HeatPerShot);

	ApplyRecoil(Stats.RecoilVertical, Stats.RecoilHorizontal * RollRecoilSign());

	ShotsFired++;
	bShotThisTriggerPull = true;
	TimeSinceLastShot = 0.0f;

	ReportShotNoise(MuzzleLocation, Stats.NoiseRadius);
}

void UEclipseCombatComponent::ResolveShot(const FVector& MuzzleLocation, const FVector& Direction)
{
	UWorld* World = GetWorld();
	UEclipseWeaponInstance* Weapon = WeaponComponent != nullptr ? WeaponComponent->GetActiveWeapon() : nullptr;
	if (World == nullptr || Weapon == nullptr || Weapon->GetDefinition() == nullptr)
	{
		return;
	}

	FEclipseDamageContext Context;
	if (!BuildBaseDamageContext(Context))
	{
		return;
	}

	const UEclipseWeaponDefinition* Definition = Weapon->GetDefinition();
	const FEclipseWeaponStats Stats = Weapon->GetEffectiveStats();

	LastShotDirection = Direction;

	if (Definition->DeliveryMode == EEclipseDeliveryMode::Projectile || Definition->DeliveryMode == EEclipseDeliveryMode::Explosive)
	{
		SpawnProjectile(MuzzleLocation, Direction, Context);
		return;
	}

	const FVector TraceEnd = MuzzleLocation + Direction * Stats.MaxRange;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(EclipseWeaponShot), true, GetOwner());
	FHitResult Hit;

	if (!World->LineTraceSingleByChannel(Hit, MuzzleLocation, TraceEnd, ShotTraceChannel, Params))
	{
		return;
	}

	Context.DistanceFalloff = UEclipseDamageLibrary::ComputeDistanceFalloff(Hit.Distance, Stats.EffectiveRange, Stats.MaxRange);
	Context.HitLocation = Hit.ImpactPoint;
	Context.HitNormal = Hit.ImpactNormal;
	Context.ShotDirection = Direction;
	Context.HitBoneName = Hit.BoneName;
	Context.WeakPointMultiplier = ResolveWeakPointMultiplier(Hit.GetActor(), Hit.BoneName);

	ApplyHitDamage(Hit.GetActor(), Hit, Context);
}

FEclipseDamageResult UEclipseCombatComponent::ApplyHitDamage(AActor* HitActor, const FHitResult& Hit, const FEclipseDamageContext& Context) const
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr || HitActor == nullptr)
	{
		return FEclipseDamageResult();
	}

	// Co-op friendly fire is off: two player-controlled pawns cannot hurt each other. The
	// faction system extends this rule when AI squads are involved.
	const APawn* TargetPawn = Cast<APawn>(HitActor);
	const APawn* OwnerPawn = Cast<APawn>(Owner);
	if (TargetPawn != nullptr && OwnerPawn != nullptr && TargetPawn->IsPlayerControlled() && OwnerPawn->IsPlayerControlled())
	{
		return FEclipseDamageResult();
	}

	const FEclipseDamageResult Result = UEclipseDamageLibrary::ApplyDamage(Owner, HitActor, Context);

	if (Result.bApplied)
	{
		const_cast<UEclipseCombatComponent*>(this)->DamageDealt += Result.MitigatedDamage;

		// Physics props react to the hit; characters handle knockback through their own
		// movement, so only simulating bodies get an impulse here.
		if (Hit.GetComponent() != nullptr && Hit.GetComponent()->IsSimulatingPhysics())
		{
			const FVector ImpulseDirection = (Context.ShotDirection.IsNearlyZero() ? Hit.ImpactNormal : Context.ShotDirection);
			Hit.GetComponent()->AddImpulseAtLocation(ImpulseDirection * Context.ImpactImpulse, Hit.ImpactPoint, Hit.BoneName);
		}

		OnShotResolved.Broadcast(HitActor, Result, Context.DamageSourceId);
	}

	return Result;
}

bool UEclipseCombatComponent::SpawnProjectile(const FVector& MuzzleLocation, const FVector& Direction, const FEclipseDamageContext& Context)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (World == nullptr || Owner == nullptr)
	{
		return false;
	}

	UEclipseWeaponInstance* Weapon = WeaponComponent != nullptr ? WeaponComponent->GetActiveWeapon() : nullptr;
	if (Weapon == nullptr || Weapon->GetDefinition() == nullptr)
	{
		return false;
	}

	const UEclipseWeaponDefinition* Definition = Weapon->GetDefinition();
	const bool bExplosive = Definition->DeliveryMode == EEclipseDeliveryMode::Explosive;

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Owner;
	SpawnParams.Instigator = Cast<APawn>(Owner);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEclipseProjectile* Projectile = World->SpawnActor<AEclipseProjectile>(
		AEclipseProjectile::StaticClass(), MuzzleLocation, Direction.Rotation(), SpawnParams);

	if (Projectile == nullptr)
	{
		UE_LOG(LogEclipseCombat, Warning, TEXT("%s failed to spawn a projectile for %s."),
			*Owner->GetName(), *Definition->WeaponId.ToString());
		return false;
	}

	Projectile->InitializeProjectile(Context, Direction, bExplosive);
	return true;
}

void UEclipseCombatComponent::ReportShotNoise(const FVector& Location, float Radius) const
{
	if (Radius <= 0.0f)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UEclipseAISubsystem* AISubsystem = World->GetSubsystem<UEclipseAISubsystem>())
		{
			AISubsystem->ReportNoise(Location, Radius, GetOwner());
		}
	}
}

void UEclipseCombatComponent::ServerStartFire_Implementation()
{
	HandleStartFire();
}

bool UEclipseCombatComponent::ServerStartFire_Validate()
{
	return IsValid(GetOwner());
}

void UEclipseCombatComponent::ServerStopFire_Implementation()
{
	HandleStopFire();
}

bool UEclipseCombatComponent::ServerStopFire_Validate()
{
	return IsValid(GetOwner());
}

void UEclipseCombatComponent::ServerSetAiming_Implementation(bool bNewAiming)
{
	bAiming = bNewAiming;
}

bool UEclipseCombatComponent::ServerSetAiming_Validate(bool bNewAiming)
{
	return IsValid(GetOwner());
}

void UEclipseCombatComponent::ServerReload_Implementation()
{
	if (WeaponComponent != nullptr)
	{
		WeaponComponent->StartReload();
	}
}

bool UEclipseCombatComponent::ServerReload_Validate()
{
	return IsValid(GetOwner());
}
