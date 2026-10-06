// PROJECT ECLIPSE - Team agent interface.
//
// Purpose
//   Everything that belongs to a faction implements this: the player character, every
//   enemy, turrets, and the boss. Hostility, AI target selection, friendly-fire rules
//   and mission credit all go through it, so an actor that implements it is never
//   accidentally treated as an ally or an enemy.
//
// Note
//   Pure C++ virtuals only: factions are gameplay data, not Blueprint composition.
//   Faction definitions live in data assets and are resolved by UEclipseFactionSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "UObject/Interface.h"
#include "EclipseTeamAgent.generated.h"

/** Bare UInterface so IEclipseTeamAgent is visible to the reflection system. */
UINTERFACE(MinimalAPI, BlueprintType)
class UEclipseTeamAgent : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by every actor that participates in faction relations. */
class ECLIPSE_API IEclipseTeamAgent
{
	GENERATED_BODY()

public:
	/** Faction this agent belongs to. Wildlife and Neutral are legitimate answers. */
	virtual EEclipseFaction GetFaction() const = 0;

	/** Stable squad id, or an invalid GUID when the agent is not in a squad. */
	virtual FGuid GetSquadId() const = 0;

	/** False once the agent is dead, so AI stops targeting it. */
	virtual bool IsAlive() const = 0;

	/** True when this agent is a player-controlled character. */
	virtual bool IsPlayerAgent() const
	{
		return false;
	}
};
