// PROJECT ECLIPSE - Faction definition and catalogue.
//
// Purpose
//   Factions are data: who they hate, who they like, where their thresholds sit and how
//   they price goods. The vertical slice ships Helix Corporation, Free Colonists, Iron
//   Wolves, the Ascendants and Wildlife, mirrored from
//   Content/Eclipse/Data/VerticalSlice/factions.json.
//
// Symmetry rule
//   Relations are resolved symmetrically at runtime: A is hostile to B if either side
//   lists the other as an enemy. That is why wildlife can list everyone without everybody
//   having to list wildlife back.

#pragma once

#include "Core/EclipseTypes.h"
#include "CoreMinimal.h"
#include "Economy/EclipseEconomyTypes.h"
#include "Engine/DataAsset.h"
#include "EclipseFactionDefinition.generated.h"

/**
 * Everything that defines one faction.
 *
 * Thresholds are asymmetric on purpose: the Iron Wolves take a lot of convincing before
 * they will trade fairly (hostile -400, allied +800), while the Free Colonists come round
 * sooner (hostile -200, allied +500).
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseFactionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Which faction this describes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Faction")
	EEclipseFaction Faction = EEclipseFaction::None;

	/** Display name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Faction")
	FText DisplayName;

	/** Designer-facing description used in the dossier UI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Faction", meta = (MultiLine = "true"))
	FText Description;

	/** Factions this one shoots on sight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Faction")
	TArray<EEclipseFaction> Enemies;

	/** Factions this one will not shoot. Alliances are symmetric at runtime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Faction")
	TArray<EEclipseFaction> Allies;

	/** Reputation at or below which the faction is hostile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Faction")
	float HostileThreshold = -200.0f;

	/** Reputation at or above which the faction is friendly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Faction")
	float FriendlyThreshold = 200.0f;

	/** Reputation at or above which the faction is allied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Faction")
	float AlliedThreshold = 600.0f;

	/** How this faction prices each item category. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Faction")
	TArray<FEclipseVendorPricing> VendorPricing;

	/** Lowest reputation this faction tracks. */
	static constexpr float MinimumReputation = -1000.0f;

	/** Highest reputation this faction tracks. */
	static constexpr float MaximumReputation = 1000.0f;

	/** True when the definition is complete. */
	bool IsValidDefinition() const
	{
		return Faction != EEclipseFaction::None
			&& HostileThreshold < FriendlyThreshold
			&& FriendlyThreshold <= AlliedThreshold;
	}

	/** Resolve the band a reputation value falls into. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Faction")
	EEclipseReputationBand GetBand(float Reputation) const;

	/** Pricing row for a category, or a neutral default when the faction has none. */
	FEclipseVendorPricing GetPricing(EEclipseItemCategory Category) const;

	/** True when this faction declares the other as an enemy. */
	bool DeclaresEnemy(EEclipseFaction Other) const;

	/** True when this faction declares the other as an ally. */
	bool DeclaresAlly(EEclipseFaction Other) const;

	/** Clamp a reputation value into the tracked range. */
	static float ClampReputation(float Reputation);
};

/** Every faction definition, loaded once and queried by the faction subsystem. */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseFactionCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** All faction definitions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Catalog")
	TArray<TObjectPtr<UEclipseFactionDefinition>> Factions;

	/** Find a faction definition, or null. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Catalog")
	UEclipseFactionDefinition* Find(EEclipseFaction Faction) const;

	/** Static accessor for the shipped catalogue. */
	static UEclipseFactionCatalog* GetCatalog();
};
