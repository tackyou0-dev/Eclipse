// PROJECT ECLIPSE - Faction subsystem.
//
// Purpose
//   The single answer to "who is hostile to whom, and how do they feel about the player".
//   AI targeting, friendly fire rules, vendor pricing, mission gating and the dossier UI
//   all ask this subsystem rather than interpreting faction data themselves.
//
// Lifetime
//   A game instance subsystem: reputation is a profile property and must survive the
//   transition from the hub to the basin and back.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Factions/EclipseFactionDefinition.h"
#include "Factions/EclipseFactionTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EclipseFactionSubsystem.generated.h"

/**
 * Reputation and relations for PROJECT ECLIPSE.
 *
 * Relations are resolved symmetrically: a pair is hostile when *either* definition lists
 * the other as an enemy. Without that rule, wildlife (which lists every faction) would be
 * one-directional and predators would ignore the player.
 */
UCLASS()
class ECLIPSE_API UEclipseFactionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** UGameInstanceSubsystem: seed the reputation table from the catalogue. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// ------------------------------------------------------------------
	// Reputation
	// ------------------------------------------------------------------

	/** Current reputation with a faction. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Faction")
	float GetReputation(EEclipseFaction Faction) const;

	/** Current standing band with a faction. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Faction")
	EEclipseReputationBand GetBand(EEclipseFaction Faction) const;

	/**
	 * Adjust reputation. Deltas are clamped to the faction's tracked range and a single
	 * change larger than MaxReputationDelta is refused as a data error rather than applied.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Faction")
	bool AddReputation(EEclipseFaction Faction, float Delta, FString& OutRejectReason);

	/** Set reputation directly. Used when a save is applied. */
	void SetReputation(EEclipseFaction Faction, float Value);

	/** Every faction's reputation, for saving and for the dossier. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Faction")
	TArray<FEclipseReputation> GetAllReputations() const;

	// ------------------------------------------------------------------
	// Relations
	// ------------------------------------------------------------------

	/** True when the two factions shoot each other on sight. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Faction")
	bool AreHostile(EEclipseFaction A, EEclipseFaction B) const;

	/** True when the two factions are declared allies by either side. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Faction")
	bool AreAllied(EEclipseFaction A, EEclipseFaction B) const;

	/**
	 * True when the player's standing makes this faction hostile to them, over and above
	 * the faction-to-faction relation. Used by the AI director when deciding whether a
	 * patrol attacks on sight.
	 */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Faction")
	bool IsHostileToPlayer(EEclipseFaction Faction) const;

	/** Definition for a faction, or null when the catalogue has no entry. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Faction")
	UEclipseFactionDefinition* GetDefinition(EEclipseFaction Faction) const;

	/** Fired whenever a reputation value changes. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|Faction")
	FEclipseReputationChangedSignature OnReputationChanged;

	/** Largest reputation change a single event may apply. */
	static constexpr float MaxReputationDelta = 250.0f;

private:
	/** Reputation per faction. */
	UPROPERTY()
	TMap<EEclipseFaction, float> ReputationByFaction;
};
