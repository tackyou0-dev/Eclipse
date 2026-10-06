// PROJECT ECLIPSE - Squad coordinator implementation.
//
// Purpose
//   Membership, focus sharing and the role assignment rule.

#include "AI/EclipseSquadCoordinator.h"

#include "Core/EclipseLog.h"

FGuid UEclipseSquadCoordinator::CreateSquad(EEclipseFaction Faction)
{
	FEclipseSquad& Squad = Squads.AddDefaulted_GetRef();
	Squad.SquadId = FGuid::NewGuid();
	Squad.Faction = Faction;
	return Squad.SquadId;
}

FGuid UEclipseSquadCoordinator::AddToSquad(AActor* Member, EEclipseFaction Faction)
{
	if (Member == nullptr)
	{
		return FGuid();
	}

	const FGuid Existing = GetSquadFor(Member);
	if (Existing.IsValid())
	{
		return Existing;
	}

	// Join the smallest squad of the same faction, so patrols merge rather than forming a
	// new one-man squad every time an agent spawns.
	FEclipseSquad* BestSquad = nullptr;
	for (FEclipseSquad& Squad : Squads)
	{
		if (Squad.Faction != Faction)
		{
			continue;
		}

		if (BestSquad == nullptr || Squad.Members.Num() < BestSquad->Members.Num())
		{
			BestSquad = &Squad;
		}
	}

	if (BestSquad == nullptr)
	{
		const FGuid NewSquadId = CreateSquad(Faction);
		BestSquad = Squads.FindByPredicate([NewSquadId](const FEclipseSquad& Squad)
		{
			return Squad.SquadId == NewSquadId;
		});
	}

	if (BestSquad == nullptr)
	{
		return FGuid();
	}

	BestSquad->Members.Add(Member);
	return BestSquad->SquadId;
}

bool UEclipseSquadCoordinator::RemoveFromSquad(AActor* Member)
{
	if (Member == nullptr)
	{
		return false;
	}

	for (int32 SquadIndex = Squads.Num() - 1; SquadIndex >= 0; --SquadIndex)
	{
		FEclipseSquad& Squad = Squads[SquadIndex];
		if (Squad.Members.Remove(Member) > 0)
		{
			if (Squad.Members.Num() == 0)
			{
				Squads.RemoveAt(SquadIndex);
			}
			return true;
		}
	}

	return false;
}

FGuid UEclipseSquadCoordinator::GetSquadFor(const AActor* Member) const
{
	if (Member == nullptr)
	{
		return FGuid();
	}

	for (const FEclipseSquad& Squad : Squads)
	{
		if (Squad.Members.Contains(Member))
		{
			return Squad.SquadId;
		}
	}

	return FGuid();
}

TArray<AActor*> UEclipseSquadCoordinator::GetMembers(const FGuid& SquadId) const
{
	TArray<AActor*> Result;
	if (const FEclipseSquad* Squad = GetSquad(SquadId))
	{
		for (const TObjectPtr<AActor>& Member : Squad->Members)
		{
			if (Member != nullptr)
			{
				Result.Add(Member);
			}
		}
	}
	return Result;
}

const FEclipseSquad* UEclipseSquadCoordinator::GetSquad(const FGuid& SquadId) const
{
	return Squads.FindByPredicate([SquadId](const FEclipseSquad& Squad)
	{
		return Squad.SquadId == SquadId;
	});
}

void UEclipseSquadCoordinator::SetFocusTarget(const FGuid& SquadId, AActor* Target)
{
	FEclipseSquad* Squad = Squads.FindByPredicate([SquadId](const FEclipseSquad& Candidate)
	{
		return Candidate.SquadId == SquadId;
	});

	if (Squad == nullptr)
	{
		return;
	}

	Squad->FocusTarget = Target;

	UE_LOG(LogEclipseAI, Verbose, TEXT("Squad %s focused %s."),
		*SquadId.ToString(),
		Target != nullptr ? *Target->GetName() : TEXT("nothing"));
}

AActor* UEclipseSquadCoordinator::GetFocusTarget(const FGuid& SquadId) const
{
	const FEclipseSquad* Squad = GetSquad(SquadId);
	return Squad != nullptr ? Squad->FocusTarget : nullptr;
}

bool UEclipseSquadCoordinator::IsSquadEmpty(const FGuid& SquadId) const
{
	const FEclipseSquad* Squad = GetSquad(SquadId);
	return Squad == nullptr || Squad->Members.Num() == 0;
}

EEclipseSquadRole UEclipseSquadCoordinator::ComputeRole(int32 MemberIndex, const UEclipseEnemyDefinition* Definition, float EffectiveRangeCentimetres)
{
	if (MemberIndex <= 0)
	{
		return EEclipseSquadRole::Leader;
	}

	if (Definition != nullptr && Definition->SquadRole == EEclipseSquadRole::Medic)
	{
		// A dedicated support type keeps its role no matter where it sits in the order.
		return EEclipseSquadRole::Medic;
	}

	if (EffectiveRangeCentimetres >= SniperRangeThreshold)
	{
		return EEclipseSquadRole::Sniper;
	}

	// Alternate assault and flank: a squad that all charge the same cover is a squad that
	// all die to the same grenade.
	return (MemberIndex % 2) == 1 ? EEclipseSquadRole::Flanker : EEclipseSquadRole::Assault;
}
