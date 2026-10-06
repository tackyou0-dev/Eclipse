// PROJECT ECLIPSE - Faction subsystem implementation.
//
// Purpose
//   Reputation bookkeeping and the symmetric relation rules.

#include "Factions/EclipseFactionSubsystem.h"

#include "Core/EclipseLog.h"

void UEclipseFactionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Every faction starts neutral. Wildlife is the exception: it is permanently hostile,
	// which its definition expresses with thresholds far outside the reachable range.
	for (int32 Index = 0; Index < static_cast<int32>(EEclipseFaction::Count); ++Index)
	{
		const EEclipseFaction Faction = static_cast<EEclipseFaction>(Index);
		ReputationByFaction.Add(Faction, 0.0f);
	}
}

UEclipseFactionDefinition* UEclipseFactionSubsystem::GetDefinition(EEclipseFaction Faction) const
{
	if (UEclipseFactionCatalog* Catalog = UEclipseFactionCatalog::GetCatalog())
	{
		return Catalog->Find(Faction);
	}
	return nullptr;
}

float UEclipseFactionSubsystem::GetReputation(EEclipseFaction Faction) const
{
	if (const float* Found = ReputationByFaction.Find(Faction))
	{
		return *Found;
	}
	return 0.0f;
}

EEclipseReputationBand UEclipseFactionSubsystem::GetBand(EEclipseFaction Faction) const
{
	const float Value = GetReputation(Faction);
	if (const UEclipseFactionDefinition* Definition = GetDefinition(Faction))
	{
		return Definition->GetBand(Value);
	}

	// No definition: fall back to a generic curve so a missing asset degrades to sane
	// behaviour instead of everyone being neutral forever.
	if (Value <= -200.0f)
	{
		return EEclipseReputationBand::Hostile;
	}
	if (Value >= 600.0f)
	{
		return EEclipseReputationBand::Allied;
	}
	if (Value >= 200.0f)
	{
		return EEclipseReputationBand::Friendly;
	}
	return EEclipseReputationBand::Neutral;
}

bool UEclipseFactionSubsystem::AddReputation(EEclipseFaction Faction, float Delta, FString& OutRejectReason)
{
	if (Faction == EEclipseFaction::None || Faction == EEclipseFaction::Neutral)
	{
		OutRejectReason = TEXT("Reputation is not tracked for this faction.");
		return false;
	}

	if (FMath::Abs(Delta) > MaxReputationDelta)
	{
		OutRejectReason = FString::Printf(TEXT("Reputation delta %.1f exceeds the %.1f limit."), Delta, MaxReputationDelta);
		UE_LOG(LogEclipseSecurity, Warning, TEXT("%s"), *OutRejectReason);
		return false;
	}

	const float Previous = GetReputation(Faction);
	const float NewValue = UEclipseFactionDefinition::ClampReputation(Previous + Delta);
	ReputationByFaction.Add(Faction, NewValue);

	OnReputationChanged.Broadcast(Faction, NewValue, GetBand(Faction));
	OutRejectReason.Reset();
	return true;
}

void UEclipseFactionSubsystem::SetReputation(EEclipseFaction Faction, float Value)
{
	if (Faction == EEclipseFaction::None)
	{
		return;
	}

	const float Clamped = UEclipseFactionDefinition::ClampReputation(Value);
	ReputationByFaction.Add(Faction, Clamped);
	OnReputationChanged.Broadcast(Faction, Clamped, GetBand(Faction));
}

TArray<FEclipseReputation> UEclipseFactionSubsystem::GetAllReputations() const
{
	TArray<FEclipseReputation> Result;
	Result.Reserve(ReputationByFaction.Num());

	// Enum order for a stable save file and a stable UI list.
	for (int32 Index = 0; Index < static_cast<int32>(EEclipseFaction::Count); ++Index)
	{
		const EEclipseFaction Faction = static_cast<EEclipseFaction>(Index);
		if (const float* Found = ReputationByFaction.Find(Faction))
		{
			FEclipseReputation Entry;
			Entry.Faction = Faction;
			Entry.Value = *Found;
			Entry.Band = GetBand(Faction);
			Result.Add(Entry);
		}
	}

	return Result;
}

bool UEclipseFactionSubsystem::AreHostile(EEclipseFaction A, EEclipseFaction B) const
{
	if (A == B || A == EEclipseFaction::None || B == EEclipseFaction::None)
	{
		return false;
	}

	// Neutral means "unaligned security system" rather than "everyone's friend": WARDEN
	// PRIME attacks anything that enters the arena.
	if (A == EEclipseFaction::Neutral || B == EEclipseFaction::Neutral)
	{
		return true;
	}

	const UEclipseFactionDefinition* DefinitionA = GetDefinition(A);
	const UEclipseFactionDefinition* DefinitionB = GetDefinition(B);

	const bool bAThinksBIsEnemy = DefinitionA != nullptr && DefinitionA->DeclaresEnemy(B);
	const bool bBThinksAIsEnemy = DefinitionB != nullptr && DefinitionB->DeclaresEnemy(A);

	return bAThinksBIsEnemy || bBThinksAIsEnemy;
}

bool UEclipseFactionSubsystem::AreAllied(EEclipseFaction A, EEclipseFaction B) const
{
	if (A == B || A == EEclipseFaction::None || B == EEclipseFaction::None)
	{
		return false;
	}

	const UEclipseFactionDefinition* DefinitionA = GetDefinition(A);
	const UEclipseFactionDefinition* DefinitionB = GetDefinition(B);

	const bool bAThinksBIsAlly = DefinitionA != nullptr && DefinitionA->DeclaresAlly(B);
	const bool bBThinksAIsAlly = DefinitionB != nullptr && DefinitionB->DeclaresAlly(A);

	return bAThinksBIsAlly || bBThinksAIsAlly;
}

bool UEclipseFactionSubsystem::IsHostileToPlayer(EEclipseFaction Faction) const
{
	// Wildlife is permanently hostile; everything else depends on reputation.
	if (Faction == EEclipseFaction::Wildlife)
	{
		return true;
	}

	return GetBand(Faction) == EEclipseReputationBand::Hostile;
}
