// PROJECT ECLIPSE - Enemy character implementation.
//
// Purpose
//   Turning an enemy definition into a living actor, and turning its death into loot.

#include "Characters/EclipseEnemyCharacter.h"

#include "Combat/EclipseHealthComponent.h"
#include "Core/EclipseLog.h"
#include "Core/EclipseStatComponent.h"
#include "Engine/World.h"
#include "Loot/EclipseLootSubsystem.h"

AEclipseEnemyCharacter::AEclipseEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	// Enemies always drop physical loot; the class is overridable per spawner for special
	// cases such as a boss cache.
	PickupClass = AEclipsePickupActor::StaticClass();
}

void AEclipseEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (EnemyDefinition != nullptr)
	{
		InitializeFromDefinition(EnemyDefinition);
	}
}

void AEclipseEnemyCharacter::InitializeCharacter()
{
	Super::InitializeCharacter();

	// The spawner usually calls InitializeFromDefinition before BeginPlay. Applying it
	// again from the base class setup keeps a level-placed enemy with a definition set in
	// the editor working too.
	if (EnemyDefinition != nullptr)
	{
		InitializeFromDefinition(EnemyDefinition);
	}
}

void AEclipseEnemyCharacter::InitializeFromDefinition(UEclipseEnemyDefinition* Definition)
{
	if (Definition == nullptr || !Definition->IsValidDefinition())
	{
		UE_LOG(LogEclipseAI, Warning, TEXT("%s has an invalid enemy definition."), *GetName());
		return;
	}

	EnemyDefinition = Definition;

	if (HealthComponent != nullptr)
	{
		HealthComponent->InitializeHealth(Definition->MaxHealth, Definition->Shield, Definition->Armor, Definition->Resistances);

		// Remove before adding: applying a definition twice must not double-bind.
		HealthComponent->OnHealthChanged.RemoveDynamic(this, &AEclipseEnemyCharacter::HandleHealthChangedForPhase);
		HealthComponent->OnHealthChanged.AddDynamic(this, &AEclipseEnemyCharacter::HandleHealthChangedForPhase);
	}

	if (StatComponent != nullptr)
	{
		StatComponent->SetBaseStat(EEclipseStat::MaxHealth, Definition->MaxHealth);
		StatComponent->SetBaseStat(EEclipseStat::MaxShield, Definition->Shield);
		StatComponent->SetBaseStat(EEclipseStat::Armor, Definition->Armor);
		StatComponent->SetBaseStat(EEclipseStat::MoveSpeed, Definition->MoveSpeed);
		StatComponent->SetBaseStat(EEclipseStat::CarryCapacity, Definition->ThreatWeight * 100.0f);
	}

	SetFaction(Definition->Faction);

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = Definition->MoveSpeed;
	}

	BossPhaseIndex = 0;
	bLootGranted = false;
}

const FEclipseBossPhase* AEclipseEnemyCharacter::GetCurrentBossPhase() const
{
	return EnemyDefinition != nullptr ? EnemyDefinition->GetPhase(BossPhaseIndex) : nullptr;
}

float AEclipseEnemyCharacter::GetHealthFraction() const
{
	return HealthComponent != nullptr ? HealthComponent->GetHealthFraction() : 0.0f;
}

float AEclipseEnemyCharacter::GetWeakPointMultiplier(FName BoneName) const
{
	if (EnemyDefinition == nullptr)
	{
		return 1.0f;
	}

	const float Base = EnemyDefinition->GetWeakPointMultiplier(BoneName);
	if (Base <= 1.0f)
	{
		return Base;
	}

	// Later boss phases expose the weak point further: the Overcharge phase doubles the
	// multiplier, which is what turns the last quarter of the fight into a damage race.
	const FEclipseBossPhase* Phase = GetCurrentBossPhase();
	if (Phase != nullptr)
	{
		return Base * Phase->WeakPointMultiplier;
	}

	return Base;
}

void AEclipseEnemyCharacter::EvaluateBossPhase()
{
	if (EnemyDefinition == nullptr || !EnemyDefinition->bIsBoss || HealthComponent == nullptr)
	{
		return;
	}

	const int32 NewPhaseIndex = EnemyDefinition->GetPhaseIndexForHealth(HealthComponent->GetHealthFraction());
	if (NewPhaseIndex == BossPhaseIndex)
	{
		return;
	}

	const int32 PreviousPhase = BossPhaseIndex;
	BossPhaseIndex = NewPhaseIndex;

	const FEclipseBossPhase* Phase = EnemyDefinition->GetPhase(BossPhaseIndex);
	if (Phase != nullptr)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = EnemyDefinition->MoveSpeed * Phase->MoveSpeedMultiplier;
		}

		UE_LOG(LogEclipseAI, Log, TEXT("%s entered phase %d (%s): %d drone(s), hazards %d."),
			*GetName(),
			Phase->PhaseIndex,
			*Phase->DisplayName.ToString(),
			Phase->DronesToDeploy,
			Phase->ArenaHazards.Num());
	}

	OnBossPhaseChanged.Broadcast(this, BossPhaseIndex, Phase);

	if (PreviousPhase < BossPhaseIndex)
	{
		// Phase transitions are invulnerable windows so that a burst cannot skip a whole
		// phase and its scripted beat.
		if (Phase != nullptr)
		{
			HealthComponent->SetInvulnerable(true);
		}
	}
}

void AEclipseEnemyCharacter::HandleHealthChangedForPhase(float CurrentHealth, float MaxHealth, float Delta, AActor* DamageInstigator)
{
	if (EnemyDefinition != nullptr && EnemyDefinition->bIsBoss)
	{
		EvaluateBossPhase();
	}
}

void AEclipseEnemyCharacter::HandleDeath(AActor* Killer, EEclipseDamageType KillingDamageType)
{
	Super::HandleDeath(Killer, KillingDamageType);

	if (bLootGranted || EnemyDefinition == nullptr || EnemyDefinition->LootTable == nullptr)
	{
		return;
	}

	bLootGranted = true;

	UWorld* World = GetWorld();
	UEclipseLootSubsystem* LootSubsystem = World != nullptr ? World->GetSubsystem<UEclipseLootSubsystem>() : nullptr;
	if (LootSubsystem == nullptr)
	{
		return;
	}

	// A corpse's loot is derived from its own stable id, so the same enemy always drops the
	// same thing and a re-load cannot be used to re-roll a drop.
	const int32 StableId = UEclipseLootSubsystem::MakeStableId(this);
	const int32 Spawned = LootSubsystem->SpawnLootAt(GetActorLocation(), EnemyDefinition->LootTable, StableId, 80.0f, PickupClass);

	UE_LOG(LogEclipseAI, Log, TEXT("%s dropped %d stack(s)."), *GetName(), Spawned);
}
