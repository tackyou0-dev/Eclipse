// PROJECT ECLIPSE - AI subsystem implementation.
//
// Purpose
//   Budget, tiers, noise and target scoring. The scoring and tier rules are pure functions
//   from EclipseAITypes.h, so this file mostly deals with bookkeeping.

#include "AI/EclipseAISubsystem.h"

#include "AI/EclipseEnemyDefinition.h"
#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseLog.h"
#include "Core/EclipseTeamAgent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

void UEclipseAISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SquadCoordinator = NewObject<UEclipseSquadCoordinator>(this, TEXT("SquadCoordinator"));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			TierRefreshTimer,
			FTimerDelegate::CreateUObject(this, &UEclipseAISubsystem::RefreshTiers),
			TierRefreshInterval,
			true);
	}
}

void UEclipseAISubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TierRefreshTimer);
	}

	Agents.Reset();
	NoiseEvents.Reset();
	SquadCoordinator = nullptr;

	Super::Deinitialize();
}

void UEclipseAISubsystem::RegisterAgent(AActor* Agent, UEclipseEnemyDefinition* Definition)
{
	if (Agent == nullptr || Definition == nullptr)
	{
		return;
	}

	for (FRegisteredAgent& Registered : Agents)
	{
		if (Registered.Agent.Get() == Agent)
		{
			Registered.Definition = Definition;
			return;
		}
	}

	FRegisteredAgent& Registered = Agents.AddDefaulted_GetRef();
	Registered.Agent = Agent;
	Registered.Definition = Definition;
	Registered.Tier = EEclipseAITier::Dormant;
}

void UEclipseAISubsystem::UnregisterAgent(AActor* Agent)
{
	if (Agent == nullptr)
	{
		return;
	}

	LeaveSquad(Agent);

	Agents.RemoveAll([Agent](const FRegisteredAgent& Registered)
	{
		return Registered.Agent.Get() == Agent;
	});
}

EEclipseAITier UEclipseAISubsystem::GetTier(const AActor* Agent) const
{
	for (const FRegisteredAgent& Registered : Agents)
	{
		if (Registered.Agent.Get() == Agent)
		{
			return Registered.Tier;
		}
	}
	return EEclipseAITier::Dormant;
}

float UEclipseAISubsystem::GetActiveThreatWeight() const
{
	float Total = 0.0f;
	for (const FRegisteredAgent& Registered : Agents)
	{
		if (Registered.Tier == EEclipseAITier::HighFrequency && Registered.Definition.IsValid())
		{
			Total += Registered.Definition->ThreatWeight;
		}
	}
	return Total;
}

bool UEclipseAISubsystem::CanAffordAgent(float ThreatWeight) const
{
	const UEclipseDeveloperSettings* Settings = UEclipseDeveloperSettings::Get();
	const float Budget = Settings != nullptr ? static_cast<float>(Settings->MaxActiveAIAgents) : 64.0f;
	return (GetActiveThreatWeight() + FMath::Max(0.0f, ThreatWeight)) <= Budget;
}

int32 UEclipseAISubsystem::CountAgentsNear(const FVector& Location, float RadiusCentimetres, EEclipseFaction ExcludeFaction) const
{
	int32 Count = 0;
	const float RadiusSquared = FMath::Square(FMath::Max(0.0f, RadiusCentimetres));

	for (const FRegisteredAgent& Registered : Agents)
	{
		const AActor* Agent = Registered.Agent.Get();
		if (Agent == nullptr)
		{
			continue;
		}

		const IEclipseTeamAgent* TeamAgent = Cast<const IEclipseTeamAgent>(Agent);
		if (TeamAgent != nullptr && TeamAgent->GetFaction() == ExcludeFaction)
		{
			continue;
		}

		if (FVector::DistSquared(Agent->GetActorLocation(), Location) <= RadiusSquared)
		{
			++Count;
		}
	}

	return Count;
}

void UEclipseAISubsystem::RefreshTiers()
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float HighFrequencyRadius = UEclipseDeveloperSettings::GetAIHighFrequencyRadius();

	// Gather player positions once: the tier decision is about distance to the nearest
	// player, and there are at most four of them.
	TArray<FVector> PlayerLocations;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
		if (Pawn != nullptr)
		{
			PlayerLocations.Add(Pawn->GetActorLocation());
		}
	}

	for (FRegisteredAgent& Registered : Agents)
	{
		const AActor* Agent = Registered.Agent.Get();
		if (Agent == nullptr)
		{
			Registered.Tier = EEclipseAITier::Dormant;
			continue;
		}

		if (PlayerLocations.Num() == 0)
		{
			// Nobody to react to: park every agent so a raid with an empty server costs
			// nothing.
			Registered.Tier = EEclipseAITier::Dormant;
			continue;
		}

		float NearestDistance = TNumericLimits<float>::Max();
		for (const FVector& PlayerLocation : PlayerLocations)
		{
			NearestDistance = FMath::Min(NearestDistance, FVector::Dist(Agent->GetActorLocation(), PlayerLocation));
		}

		const EEclipseAITier Tier = EclipseAI::TierForDistance(NearestDistance, HighFrequencyRadius);

		// Bosses and hero agents are exempt from the budget: a boss that stops ticking when
		// the budget is full would be a scripted fight that silently stops.
		if (Tier == EEclipseAITier::HighFrequency && Registered.Definition.IsValid() && Registered.Definition->bIsBoss)
		{
			Registered.Tier = EEclipseAITier::Boss;
			continue;
		}

		// Budget check: an agent that would push the total past the ceiling stays at low
		// frequency instead of being destroyed.
		if (Tier == EEclipseAITier::HighFrequency
			&& Registered.Definition.IsValid()
			&& GetActiveThreatWeight() + Registered.Definition->ThreatWeight > static_cast<float>(UEclipseDeveloperSettings::Get()->MaxActiveAIAgents))
		{
			Registered.Tier = EEclipseAITier::LowFrequency;
			continue;
		}

		Registered.Tier = Tier;
	}

	PruneNoise();
}

void UEclipseAISubsystem::ReportNoise(const FVector& Location, float Radius, AActor* Instigator)
{
	if (Radius <= 0.0f)
	{
		return;
	}

	FEclipseNoiseEvent Event;
	Event.Location = Location;
	Event.Radius = Radius;
	Event.Instigator = Instigator;
	Event.TimeSeconds = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;

	NoiseEvents.Add(Event);

	// A hard cap keeps a firefight from growing an unbounded list; the oldest events are
	// the least interesting.
	if (NoiseEvents.Num() > MaxRecordedNoiseEvents)
	{
		NoiseEvents.RemoveAt(0, NoiseEvents.Num() - MaxRecordedNoiseEvents, EAllowShrinking::No);
	}
}

void UEclipseAISubsystem::PruneNoise()
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	NoiseEvents.RemoveAll([Now](const FEclipseNoiseEvent& Event)
	{
		return (Now - Event.TimeSeconds) > EclipseAI::NoiseMemorySeconds;
	});
}

TArray<FEclipseNoiseEvent> UEclipseAISubsystem::GetAudibleNoise(const AActor* Agent) const
{
	TArray<FEclipseNoiseEvent> Audible;
	if (Agent == nullptr)
	{
		return Audible;
	}

	const UWorld* World = GetWorld();
	const float Now = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	const FVector Location = Agent->GetActorLocation();

	for (const FEclipseNoiseEvent& Event : NoiseEvents)
	{
		if (Event.IsAudible(Location, Now, EclipseAI::NoiseMemorySeconds))
		{
			Audible.Add(Event);
		}
	}

	return Audible;
}

AActor* UEclipseAISubsystem::FindBestTarget(AActor* Agent, float MaxRangeCentimetres) const
{
	const UWorld* World = GetWorld();
	if (Agent == nullptr || World == nullptr)
	{
		return nullptr;
	}

	float BestScore = 0.0f;
	AActor* BestTarget = nullptr;

	const FGuid SquadId = SquadCoordinator != nullptr ? SquadCoordinator->GetSquadFor(Agent) : FGuid();
	const AActor* SquadFocus = SquadCoordinator != nullptr && SquadId.IsValid()
		? SquadCoordinator->GetFocusTarget(SquadId)
		: nullptr;

	// Candidates are the players. Enemy-versus-enemy targeting is deliberately out of scope
	// for the slice, and the faction rules are already in place for when it is not.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			continue;
		}

		// A dead player is not a target; the AI should look for whoever is still standing.
		if (const IEclipseTeamAgent* TeamAgent = Cast<const IEclipseTeamAgent>(Pawn))
		{
			if (!TeamAgent->IsAlive())
			{
				continue;
			}
		}

		const float Distance = FVector::Dist(Agent->GetActorLocation(), Pawn->GetActorLocation());
		if (Distance > MaxRangeCentimetres)
		{
			continue;
		}

		const float Score = EclipseAI::ScoreTarget(true, Distance, Pawn == SquadFocus);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Pawn;
		}
	}

	return BestTarget;
}

void UEclipseAISubsystem::JoinSquad(AActor* Agent, EEclipseFaction Faction)
{
	if (SquadCoordinator != nullptr && Agent != nullptr)
	{
		SquadCoordinator->AddToSquad(Agent, Faction);
	}
}

void UEclipseAISubsystem::LeaveSquad(AActor* Agent)
{
	if (SquadCoordinator != nullptr && Agent != nullptr)
	{
		SquadCoordinator->RemoveFromSquad(Agent);
	}
}
