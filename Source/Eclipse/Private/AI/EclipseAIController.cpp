// PROJECT ECLIPSE - AI controller implementation.
//
// Purpose
//   The state machine. Read EvaluateState first: it is the whole decision surface, and
//   everything else in this file exists to carry out what it decides.

#include "AI/EclipseAIController.h"

#include "AI/EclipseAISubsystem.h"
#include "Characters/EclipseEnemyCharacter.h"
#include "Combat/EclipseCombatComponent.h"
#include "Combat/EclipseDamageLibrary.h"
#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseLog.h"
#include "Core/EclipseStatComponent.h"
#include "Engine/World.h"
#include "Weapons/EclipseWeaponComponent.h"

AEclipseAIController::AEclipseAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

AEclipseEnemyCharacter* AEclipseAIController::GetEnemyCharacter() const
{
	return Cast<AEclipseEnemyCharacter>(GetPawn());
}

UEclipseAISubsystem* AEclipseAIController::GetAISubsystem() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UEclipseAISubsystem>() : nullptr;
}

void AEclipseAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AEclipseEnemyCharacter* Character = Cast<AEclipseEnemyCharacter>(InPawn);
	if (Character == nullptr)
	{
		UE_LOG(LogEclipseAI, Warning, TEXT("%s possessed %s, which is not an enemy character; the AI will idle."),
			*GetName(), InPawn != nullptr ? *InPawn->GetName() : TEXT("<null>"));
		return;
	}

	HomeLocation = Character->GetActorLocation();
	CombatComponent = Character->FindComponentByClass<UEclipseCombatComponent>();

	if (UEclipseAISubsystem* AISubsystem = GetAISubsystem())
	{
		AISubsystem->RegisterAgent(Character, Character->GetEnemyDefinition());
		AISubsystem->JoinSquad(Character, Character->GetFaction());

		UEclipseSquadCoordinator* Coordinator = AISubsystem->GetSquadCoordinator();
		const FGuid SquadId = Coordinator != nullptr ? Coordinator->GetSquadFor(Character) : FGuid();

		if (SquadId.IsValid() && Coordinator != nullptr)
		{
			const TArray<AActor*> Members = Coordinator->GetMembers(SquadId);
			const int32 MemberIndex = Members.IndexOfByKey(Character);

			// The sniper decision needs the weapon's effective range, which only exists once
			// a weapon is equipped; a weaponless enemy has no range and takes a melee role.
			float WeaponRange = 0.0f;
			if (const UEclipseWeaponComponent* WeaponComponent = Character->FindComponentByClass<UEclipseWeaponComponent>())
			{
				WeaponRange = WeaponComponent->GetStat(EEclipseStat::EffectiveRange);
			}

			SquadRole = UEclipseSquadCoordinator::ComputeRole(MemberIndex, Character->GetEnemyDefinition(), WeaponRange);

			UE_LOG(LogEclipseAI, Verbose, TEXT("%s joined squad %s as role %d."),
				*Character->GetName(), *SquadId.ToString(), static_cast<int32>(SquadRole));
		}
	}

	TransitionTo(EEclipseAIState::Patrol);
	ApplyTierTickRate();
}

void AEclipseAIController::OnUnPossess()
{
	AEclipseEnemyCharacter* Character = GetEnemyCharacter();
	if (Character != nullptr)
	{
		if (UEclipseAISubsystem* AISubsystem = GetAISubsystem())
		{
			AISubsystem->UnregisterAgent(Character);
		}
	}

	if (CombatComponent != nullptr)
	{
		CombatComponent->StopFire();
	}

	Super::OnUnPossess();
}

void AEclipseAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetPawn() == nullptr || DeltaSeconds <= 0.0f)
	{
		return;
	}

	// A dormant agent does not tick at all; the subsystem re-enables it when a player
	// comes near.
	if (GetAISubsystem() != nullptr && GetAISubsystem()->GetTier(GetPawn()) == EEclipseAITier::Dormant)
	{
		return;
	}

	StateTimeSeconds += DeltaSeconds;
	TimeSinceAttack += DeltaSeconds;

	UpdatePerception(DeltaSeconds);
	EvaluateState(DeltaSeconds);
	TickState(DeltaSeconds);
}

void AEclipseAIController::UpdatePerception(float DeltaSeconds)
{
	TimeSincePerception += DeltaSeconds;
	if (TimeSincePerception < PerceptionInterval)
	{
		return;
	}

	TimeSincePerception = 0.0f;

	UEclipseAISubsystem* AISubsystem = GetAISubsystem();
	APawn* Pawn = GetPawn();
	if (AISubsystem == nullptr || Pawn == nullptr)
	{
		return;
	}

	// Sight beats hearing beats memory.
	AActor* Sighted = AISubsystem->FindBestTarget(Pawn, SightRadius);
	if (Sighted != nullptr && HasLineOfSight(Sighted))
	{
		CurrentTarget = Sighted;
		LastKnownTargetLocation = Sighted->GetActorLocation();
		SearchTimeRemaining = SearchDurationSeconds;
		return;
	}

	// Hearing: the nearest recent noise within its radius. Noise made by an ally is
	// ignored, otherwise a squad would spend the whole fight investigating each other.
	const TArray<FEclipseNoiseEvent> Noise = AISubsystem->GetAudibleNoise(Pawn);
	const IEclipseTeamAgent* SelfAgent = Cast<const IEclipseTeamAgent>(Pawn);

	for (const FEclipseNoiseEvent& Event : Noise)
	{
		const IEclipseTeamAgent* InstigatorAgent = Cast<const IEclipseTeamAgent>(Event.Instigator.Get());
		if (SelfAgent != nullptr && InstigatorAgent != nullptr && InstigatorAgent->GetFaction() == SelfAgent->GetFaction())
		{
			continue;
		}

		LastKnownTargetLocation = Event.Location;
		SearchTimeRemaining = SearchDurationSeconds;
		break;
	}

	if (SearchTimeRemaining > 0.0f)
	{
		SearchTimeRemaining -= PerceptionInterval;
	}

	if (SearchTimeRemaining <= 0.0f && CurrentTarget.IsValid() && !HasLineOfSight(CurrentTarget.Get()))
	{
		CurrentTarget = nullptr;
	}
}

bool AEclipseAIController::HasLineOfSight(const AActor* Target) const
{
	const APawn* Pawn = GetPawn();
	const UWorld* World = GetWorld();
	if (Pawn == nullptr || Target == nullptr || World == nullptr)
	{
		return false;
	}

	const FVector EyeLocation = Pawn->GetPawnViewLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector ToTarget = TargetLocation - EyeLocation;
	const float Distance = ToTarget.Size();
	if (Distance <= 0.0f)
	{
		return false;
	}

	// Close targets are noticed regardless of facing: an enemy that walks past a sentry's
	// shoulder should be seen.
	if (Distance <= ProximityRadius)
	{
		return true;
	}

	const FVector Direction = ToTarget / Distance;
	const float Dot = FVector::DotProduct(Pawn->GetActorForwardVector(), Direction);
	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(SightHalfAngleDegrees));
	if (Dot < CosHalfAngle)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(EclipseAISight), false, Pawn);
	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, EyeLocation, TargetLocation, ECC_Visibility, Params);
	return !bBlocked || Hit.GetActor() == Target;
}

void AEclipseAIController::EvaluateState(float DeltaSeconds)
{
	AEclipseEnemyCharacter* Character = GetEnemyCharacter();
	const float DistanceFromHome = FVector::Dist(GetPawn()->GetActorLocation(), HomeLocation);

	// Leash first: an enemy that has been dragged across the map goes home, and it does so
	// even while it is winning, otherwise a chase never ends.
	if (DistanceFromHome > EclipseAI::LeashRadiusCentimetres)
	{
		TransitionTo(EEclipseAIState::Leash);
		return;
	}

	const UEclipseEnemyDefinition* Definition = Character != nullptr ? Character->GetEnemyDefinition() : nullptr;
	const bool bAggressive = Definition != nullptr
		&& (Definition->Behaviour == EEclipseEnemyBehaviour::Assault || Definition->Behaviour == EEclipseEnemyBehaviour::Squad || Definition->bIsBoss);

	// Fleeing is a real behaviour, not a cosmetic retreat: the agent breaks contact and
	// stays away until its health recovers above the threshold.
	if (Character != nullptr && !bAggressive && Character->GetHealthFraction() < FleeHealthFraction)
	{
		TransitionTo(EEclipseAIState::Flee);
		return;
	}

	if (CurrentTarget.IsValid())
	{
		const float DistanceToTarget = FVector::Dist(GetPawn()->GetActorLocation(), CurrentTarget->GetActorLocation());
		const float AttackRange = Definition != nullptr ? Definition->AttackRange : AttackDistanceFallback;

		if (HasLineOfSight(CurrentTarget.Get()) && DistanceToTarget <= AttackRange)
		{
			TransitionTo(EEclipseAIState::Attack);
			return;
		}

		TransitionTo(EEclipseAIState::Chase);
		return;
	}

	if (SearchTimeRemaining > 0.0f)
	{
		TransitionTo(EEclipseAIState::Investigate);
		return;
	}

	// Nothing to do: patrol, or guard in place. A guard that wanders off is a bug.
	if (Definition != nullptr && Definition->Behaviour == EEclipseEnemyBehaviour::Guard)
	{
		TransitionTo(EEclipseAIState::Idle);
		return;
	}

	TransitionTo(EEclipseAIState::Patrol);
}

void AEclipseAIController::TickState(float DeltaSeconds)
{
	switch (State)
	{
	case EEclipseAIState::Idle:
		// Standing watch: nothing to do, perception handles the rest.
		break;

	case EEclipseAIState::Patrol:
	{
		// Patrol is a slow orbit around the home position rather than a baked route, which
		// keeps the slice's grey-box levels navigable without designer-authored patrol
		// paths. Distance keeps it from drifting into the leash.
		const FVector Offset(HomeLocation.X + PatrolOffsetCentimetres, HomeLocation.Y, HomeLocation.Z);
		MoveTowards(FVector::Dist(GetPawn()->GetActorLocation(), Offset) < ArrivalRadius
			? HomeLocation - FVector(PatrolOffsetCentimetres, 0.0f, 0.0f)
			: Offset);
		break;
	}

	case EEclipseAIState::Investigate:
		MoveTowards(LastKnownTargetLocation);
		break;

	case EEclipseAIState::Chase:
		if (CurrentTarget.IsValid())
		{
			MoveTowards(CurrentTarget->GetActorLocation());
		}
		break;

	case EEclipseAIState::Attack:
	{
		// Stop and shoot: an enemy that keeps running while firing is unfair at range.
		StopMovement();

		if (CombatComponent != nullptr)
		{
			CombatComponent->SetAiming(true);
			CombatComponent->StartFire();
		}
		else
		{
			PerformIntrinsicAttack();
		}
		break;
	}

	case EEclipseAIState::Flee:
	{
		// Run directly away from the last known threat, then re-evaluate.
		const FVector Away = (GetPawn()->GetActorLocation() - LastKnownTargetLocation).GetSafeNormal();
		MoveTowards(GetPawn()->GetActorLocation() + Away * FleeDistanceCentimetres);
		break;
	}

	case EEclipseAIState::Leash:
		MoveTowards(HomeLocation);
		if (FVector::Dist(GetPawn()->GetActorLocation(), HomeLocation) <= ArrivalRadius * 2.0f)
		{
			CurrentTarget = nullptr;
			SearchTimeRemaining = 0.0f;
			TransitionTo(EEclipseAIState::Patrol);
		}
		break;

	default:
		break;
	}
}

void AEclipseAIController::PerformIntrinsicAttack()
{
	AEclipseEnemyCharacter* Character = GetEnemyCharacter();
	const UEclipseEnemyDefinition* Definition = Character != nullptr ? Character->GetEnemyDefinition() : nullptr;
	if (Character == nullptr || Definition == nullptr || !CurrentTarget.IsValid())
	{
		return;
	}

	if (TimeSinceAttack < Definition->AttackInterval)
	{
		return;
	}

	const float Distance = FVector::Dist(Character->GetActorLocation(), CurrentTarget->GetActorLocation());
	if (Distance > Definition->AttackRange || !HasLineOfSight(CurrentTarget.Get()))
	{
		return;
	}

	FEclipseDamageContext Context;
	Context.BaseDamage = Definition->AttackDamage;
	Context.DamageType = Definition->DamageType;
	Context.Instigator = Character;
	Context.DamageSourceId = Definition->EnemyId;
	Context.HitLocation = CurrentTarget->GetActorLocation();
	Context.ShotDirection = (CurrentTarget->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal();
	Context.DistanceFalloff = 1.0f;

	UEclipseDamageLibrary::ApplyDamage(Character, CurrentTarget.Get(), Context);

	// Intrinsic attacks run on the definition's cooldown, otherwise a pack of brutes shreds
	// a player within a second.
	TimeSinceAttack = 0.0f;
}

void AEclipseAIController::MoveTowards(const FVector& Destination)
{
	const FVector Origin = GetPawn() != nullptr ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
	if (FVector::DistSquared(Origin, Destination) <= FMath::Square(ArrivalRadius))
	{
		return;
	}

	// MoveToLocation is a request: navigation may not have a path, in which case the agent
	// stands still rather than walking through geometry.
	MoveToLocation(Destination, ArrivalRadius, /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true);
}

void AEclipseAIController::TransitionTo(EEclipseAIState NewState)
{
	if (State == NewState)
	{
		return;
	}

	// Leaving the attack state must release the trigger, otherwise an AI that switches to
	// chase keeps firing while it runs.
	if (State == EEclipseAIState::Attack)
	{
		if (CombatComponent != nullptr)
		{
			CombatComponent->StopFire();
			CombatComponent->SetAiming(false);
		}
	}

	State = NewState;
	StateTimeSeconds = 0.0f;
}

void AEclipseAIController::ForceState(EEclipseAIState NewState)
{
	TransitionTo(NewState);
}

void AEclipseAIController::ApplyTierTickRate()
{
	UEclipseAISubsystem* AISubsystem = GetAISubsystem();
	if (AISubsystem == nullptr || GetPawn() == nullptr)
	{
		return;
	}

	const UEclipseDeveloperSettings* Settings = UEclipseDeveloperSettings::Get();
	const float LowFrequencyInterval = Settings != nullptr ? Settings->AILowFrequencyTickInterval : 0.5f;

	switch (AISubsystem->GetTier(GetPawn()))
	{
	case EEclipseAITier::HighFrequency:
	case EEclipseAITier::Hero:
	case EEclipseAITier::Boss:
		PrimaryActorTick.TickInterval = 0.0f;
		break;

	case EEclipseAITier::LowFrequency:
		PrimaryActorTick.TickInterval = FMath::Max(0.05f, LowFrequencyInterval);
		break;

	case EEclipseAITier::Dormant:
	default:
		// Dormant agents are skipped inside Tick; the interval is left alone so that
		// waking up restores the previous rate without a special case.
		PrimaryActorTick.TickInterval = 0.0f;
		break;
	}
}
