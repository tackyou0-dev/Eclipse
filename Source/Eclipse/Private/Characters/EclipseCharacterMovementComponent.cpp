// PROJECT ECLIPSE - Character movement component implementation.
//
// Purpose
//   Speed resolution, sprint economy and fall damage.

#include "Characters/EclipseCharacterMovementComponent.h"

#include "Combat/EclipseDamageLibrary.h"
#include "Combat/EclipseDamageTypes.h"
#include "Core/EclipseLog.h"
#include "Core/EclipseStatComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Security/EclipseSecuritySubsystem.h"

UEclipseCharacterMovementComponent::UEclipseCharacterMovementComponent()
{
	// Base tuning only. The authoritative walk speed comes from the stat component, so a
	// character with no stats still moves at a sane pace instead of standing still.
	MaxWalkSpeed = 600.0f;
	MaxWalkSpeedCrouched = 300.0f;
	JumpZVelocity = 520.0f;
	AirControl = 0.35f;
	BrakingDecelerationWalking = 2048.0f;
	BrakingDecelerationFalling = 0.0f;
	GroundFriction = 8.0f;
	FallingLateralFriction = 0.0f;
}

UEclipseStatComponent* UEclipseCharacterMovementComponent::GetStatComponent() const
{
	const AActor* Owner = GetOwner();
	return Owner != nullptr ? Owner->FindComponentByClass<UEclipseStatComponent>() : nullptr;
}

float UEclipseCharacterMovementComponent::GetWalkSpeed() const
{
	if (const UEclipseStatComponent* StatComponent = GetStatComponent())
	{
		const float StatSpeed = StatComponent->GetStat(EEclipseStat::MoveSpeed);
		if (StatSpeed > 0.0f)
		{
			return StatSpeed;
		}
	}

	return MaxWalkSpeed;
}

float UEclipseCharacterMovementComponent::GetMaxSpeed() const
{
	const float WalkSpeed = GetWalkSpeed();

	if (!bSprinting)
	{
		return Super::GetMaxSpeed();
	}

	// SprintSpeed is a multiplier on the walk speed so a "Sprinter" skill and a heavy
	// backpack stack predictably.
	float SprintMultiplier = 1.3f;
	if (const UEclipseStatComponent* StatComponent = GetStatComponent())
	{
		const float StatMultiplier = StatComponent->GetStat(EEclipseStat::SprintSpeed);
		if (StatMultiplier > 0.0f)
		{
			SprintMultiplier = StatMultiplier;
		}
	}

	return WalkSpeed * SprintMultiplier;
}

bool UEclipseCharacterMovementComponent::TryStartSprint()
{
	if (bSprinting)
	{
		return true;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character == nullptr)
	{
		return false;
	}

	if (!IsMovingOnGround())
	{
		return false;
	}

	if (Character->bIsCrouched)
	{
		return false;
	}

	UEclipseStatComponent* StatComponent = GetStatComponent();
	if (StatComponent == nullptr)
	{
		// A character without stats can always sprint; this is the case for simple
		// scripted actors and for automated tests.
		bSprinting = true;
		return true;
	}

	if (StatComponent->IsStaminaDepleted())
	{
		return false;
	}

	// Sprinting needs some forward intent: sprinting in place is a bug, not a feature.
	if (Velocity.Size2D() < GetWalkSpeed() * MinimumSprintSpeedRatio)
	{
		return false;
	}

	bSprinting = true;
	return true;
}

void UEclipseCharacterMovementComponent::StopSprint()
{
	bSprinting = false;
}

void UEclipseCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bSprinting || DeltaTime <= 0.0f)
	{
		return;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character == nullptr || !Character->HasAuthority())
	{
		// Clients predict; the server decides when the pool runs out.
		return;
	}

	UEclipseStatComponent* StatComponent = GetStatComponent();
	if (StatComponent == nullptr)
	{
		return;
	}

	if (!IsMovingOnGround() || Velocity.Size2D() < GetWalkSpeed() * MinimumSprintSpeedRatio)
	{
		StopSprint();
		return;
	}

	if (!StatComponent->ConsumeSprintStamina(DeltaTime))
	{
		StopSprint();
	}
}

EEclipseMovementState UEclipseCharacterMovementComponent::GetMovementState() const
{
	if (IsFalling())
	{
		return EEclipseMovementState::Falling;
	}

	if (IsSwimming())
	{
		return EEclipseMovementState::Swimming;
	}

	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (Character->bIsCrouched)
		{
			return EEclipseMovementState::Crouching;
		}
	}

	if (bSprinting)
	{
		return EEclipseMovementState::Sprinting;
	}

	return Velocity.Size2D() > 10.0f ? EEclipseMovementState::Walking : EEclipseMovementState::Idle;
}

bool UEclipseCharacterMovementComponent::HandleFallDamage(float FallDistance, float DamageMultiplier, const FHitResult& Hit)
{
	const bool bBaseResult = Super::HandleFallDamage(FallDistance, DamageMultiplier, Hit);

	if (FallDistance <= SafeFallDistance)
	{
		return bBaseResult;
	}

	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (Character == nullptr || !Character->HasAuthority())
	{
		return bBaseResult;
	}

	// Fall damage deliberately ignores armour: padding does not help you land badly. The
	// resistance profile still applies, so a Hazmat suit does not help either, but a
	// creature with Kinetic resistance does.
	const float ExcessDistance = FallDistance - SafeFallDistance;
	float Damage = ExcessDistance * FallDamagePerCentimetre * FMath::Max(0.0f, DamageMultiplier);

	// Environmental resistance trims the rest, which is what makes it worth a skill point.
	if (const UEclipseStatComponent* StatComponent = GetStatComponent())
	{
		const float Resistance = FMath::Clamp(StatComponent->GetStat(EEclipseStat::EnvironmentalResistance), 0.0f, 0.9f);
		Damage *= (1.0f - Resistance);
	}

	if (Damage <= 0.0f)
	{
		return bBaseResult;
	}

	FEclipseDamageContext Context;
	Context.BaseDamage = Damage;
	Context.DamageType = FallDamageType;
	Context.bIgnoreArmor = true;
	Context.bIgnoreShields = true;
	Context.Instigator = nullptr;
	Context.DamageSourceId = TEXT("System.FallDamage");
	Context.HitLocation = Hit.ImpactPoint;
	Context.HitNormal = Hit.ImpactNormal;
	Context.ShotDirection = FVector::DownVector;

	UEclipseDamageLibrary::ApplyDamage(nullptr, Character, Context);
	return true;
}

bool UEclipseCharacterMovementComponent::ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	if (Super::ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel, ClientWorldLocation, RelativeClientLocation, ClientMovementBase, ClientBaseBoneName, ClientMovementMode))
	{
		return true;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const APlayerController* Controller = Character != nullptr ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (Controller == nullptr)
	{
		return false;
	}

	// A dedicated server has no local view of this controller, so this is the
	// authoritative check rather than a prediction.
	UEclipseSecuritySubsystem* Security = Controller->GetGameInstance() != nullptr
		? Controller->GetGameInstance()->GetSubsystem<UEclipseSecuritySubsystem>()
		: nullptr;

	if (Security == nullptr)
	{
		return false;
	}

	FString RefusalReason;
	if (!Security->ValidateMovement(const_cast<APlayerController*>(Controller), ClientWorldLocation, RefusalReason))
	{
		UE_LOG(LogEclipseSecurity, Warning, TEXT("Refusing a client move from %s: %s"), *Controller->GetName(), *RefusalReason);
		return true;
	}

	return false;
}
