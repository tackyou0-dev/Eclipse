// PROJECT ECLIPSE - Squad coordinator.
//
// Purpose
//   Squads are what make a group of enemies feel like a squad rather than four identical
//   individuals: they share a focus target, they are assigned roles, and the roles change
//   what each member does (a sniper holds distance, a flanker takes the long way round).
//
// Determinism
//   Role assignment is a pure function of membership order and member capability, so a
//   squad behaves identically on the server and in the reference model's tests.

#pragma once

#include "AI/EclipseAITypes.h"
#include "AI/EclipseEnemyDefinition.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EclipseSquadCoordinator.generated.h"

/**
 * Squads for one world.
 *
 * Owned by UEclipseAISubsystem, which is the only thing that creates and destroys squads.
 */
UCLASS()
class ECLIPSE_API UEclipseSquadCoordinator : public UObject
{
	GENERATED_BODY()

public:
	/** Create a squad for a faction. */
	FGuid CreateSquad(EEclipseFaction Faction);

	/** Add a member. Returns the squad it joined, creating one when needed. */
	FGuid AddToSquad(AActor* Member, EEclipseFaction Faction);

	/** Remove a member from whatever squad it is in. */
	bool RemoveFromSquad(AActor* Member);

	/** Squad a member belongs to, or an invalid GUID. */
	FGuid GetSquadFor(const AActor* Member) const;

	/** Members of a squad, leader first. */
	TArray<AActor*> GetMembers(const FGuid& SquadId) const;

	/** Squad data, or null. */
	const FEclipseSquad* GetSquad(const FGuid& SquadId) const;

	/** Every living squad, for the debug overlay. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|AI")
	TArray<FEclipseSquad> GetAllSquads() const { return Squads; }

	/**
	 * Set the squad's shared focus target and hand it to every member, so a squad that has
	 * decided to commit does so together.
	 */
	void SetFocusTarget(const FGuid& SquadId, AActor* Target);

	/** Squad's current focus target, or null. */
	AActor* GetFocusTarget(const FGuid& SquadId) const;

	/** True when the whole squad is dead or has left. */
	bool IsSquadEmpty(const FGuid& SquadId) const;

	/**
	 * Role for a member: the leader leads, a long-ranged member snipes, a support-typed
	 * member is the medic, and the rest alternate between assault and flanking.
	 */
	UFUNCTION(BlueprintPure, Category = "Eclipse|AI")
	static EEclipseSquadRole ComputeRole(int32 MemberIndex, const UEclipseEnemyDefinition* Definition, float EffectiveRangeCentimetres);

	/** Effective range above which a member is assigned the sniper role. */
	static constexpr float SniperRangeThreshold = 8000.0f;

private:
	/** Living squads. */
	UPROPERTY()
	TArray<FEclipseSquad> Squads;
};
