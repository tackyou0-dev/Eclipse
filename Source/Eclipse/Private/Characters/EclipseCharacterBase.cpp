// PROJECT ECLIPSE - Base character implementation.
//
// Purpose
//   Component wiring, faction replication and the death/revive transition. Derived
//   classes add behaviour; they do not re-implement dying.

#include "Characters/EclipseCharacterBase.h"

#include "Components/CapsuleComponent.h"
#include "Core/EclipseLog.h"
#include "Net/UnrealNetwork.h"

AEclipseCharacterBase::AEclipseCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UEclipseCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	StatComponent = CreateDefaultSubobject<UEclipseStatComponent>(TEXT("StatComponent"));
	HealthComponent = CreateDefaultSubobject<UEclipseHealthComponent>(TEXT("HealthComponent"));

	AIControllerClass = nullptr;
}

void AEclipseCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (StatComponent != nullptr)
	{
		StatComponent->OnStatChanged.AddDynamic(this, &AEclipseCharacterBase::HandleStatChanged);
	}

	if (HealthComponent != nullptr)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AEclipseCharacterBase::HandleHealthDepleted);
	}

	InitializeCharacter();
	RefreshMovementSpeed();
}

UEclipseCharacterMovementComponent* AEclipseCharacterBase::GetEclipseMovement() const
{
	return Cast<UEclipseCharacterMovementComponent>(GetCharacterMovement());
}

void AEclipseCharacterBase::InitializeCharacter()
{
	// Derived classes fill in the data-driven stats here. The base implementation only
	// guarantees that the pools agree with each other.
	if (HealthComponent != nullptr && StatComponent != nullptr)
	{
		StatComponent->SetBaseStat(EEclipseStat::MaxHealth, HealthComponent->GetMaximumHealth());
	}

	RefreshMovementSpeed();
}

void AEclipseCharacterBase::HandleStatChanged(EEclipseStat Stat, float NewValue)
{
	switch (Stat)
	{
	case EEclipseStat::MoveSpeed:
	case EEclipseStat::SprintSpeed:
		RefreshMovementSpeed();
		break;

	case EEclipseStat::MaxHealth:
		if (HealthComponent != nullptr)
		{
			HealthComponent->SetMaxHealth(NewValue);
		}
		break;

	default:
		break;
	}
}

void AEclipseCharacterBase::RefreshMovementSpeed()
{
	UEclipseCharacterMovementComponent* Movement = GetEclipseMovement();
	if (Movement == nullptr || StatComponent == nullptr)
	{
		return;
	}

	Movement->MaxWalkSpeed = StatComponent->GetStat(EEclipseStat::MoveSpeed);
	Movement->MaxWalkSpeedCrouched = Movement->MaxWalkSpeed * 0.5f;
}

void AEclipseCharacterBase::SetFaction(EEclipseFaction NewFaction)
{
	if (!HasAuthority())
	{
		UE_LOG(LogEclipse, Warning, TEXT("SetFaction ignored on %s: not authoritative."), *GetName());
		return;
	}

	Faction = NewFaction;
}

void AEclipseCharacterBase::SetSquadId(const FGuid& NewSquadId)
{
	if (!HasAuthority())
	{
		return;
	}

	SquadId = NewSquadId;
}

bool AEclipseCharacterBase::IsAlive() const
{
	return !bDead;
}

bool AEclipseCharacterBase::IsDead() const
{
	return bDead;
}

void AEclipseCharacterBase::HandleHealthDepleted(AActor* Killer, EEclipseDamageType KillingDamageType)
{
	HandleDeath(Killer, KillingDamageType);
}

void AEclipseCharacterBase::HandleDeath(AActor* Killer, EEclipseDamageType KillingDamageType)
{
	if (bDead)
	{
		return;
	}

	bDead = true;

	if (UEclipseCharacterMovementComponent* Movement = GetEclipseMovement())
	{
		Movement->StopSprint();
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
	}

	// The corpse stays collidable as a low obstacle so a downed squad mate is visible and
	// can be found again for the revive.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}

	if (AController* OwningController = GetController())
	{
		OwningController->SetIgnoreMoveInput(true);
	}

	UE_LOG(LogEclipse, Log, TEXT("%s died to %s."),
		*GetName(), Killer != nullptr ? *Killer->GetName() : TEXT("the environment"));

	OnCharacterDeath.Broadcast(this, Killer);
}

void AEclipseCharacterBase::ReviveCharacter(float HealthFraction)
{
	if (!HasAuthority())
	{
		return;
	}

	if (!bDead && HealthComponent != nullptr && HealthComponent->GetHealth() > 0.0f)
	{
		return;
	}

	bDead = false;
	bBeingRevived = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}

	if (AController* OwningController = GetController())
	{
		OwningController->SetIgnoreMoveInput(false);
	}

	if (HealthComponent != nullptr)
	{
		HealthComponent->Revive(HealthFraction);
	}

	if (StatComponent != nullptr)
	{
		StatComponent->RefillStamina();
	}

	OnCharacterRevived.Broadcast(this);
}

void AEclipseCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEclipseCharacterBase, Faction);
	DOREPLIFETIME(AEclipseCharacterBase, SquadId);
	DOREPLIFETIME(AEclipseCharacterBase, bDead);
}
