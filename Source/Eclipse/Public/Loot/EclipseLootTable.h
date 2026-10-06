// PROJECT ECLIPSE - Loot table.
//
// Purpose
//   A weighted, rarity-banded loot table. Nine of them ship with the vertical slice:
//   one per enemy tier, supply cache, military crate, relic vault and the Warden.
//
// Roll order (mirrored by the reference model)
//   1. Roll the number of rolls from [RollCountMin, RollCountMax].
//   2. For each roll, pick a rarity band from RarityWeights, iterating bands in enum
//      order and using the weighted-pick helper.
//   3. Pick an entry inside that band by relative weight, in authored order. If a band has
//      no entries the roll falls back to the whole table rather than producing nothing.
//   4. Roll the count from [MinCount, MaxCount] and merge it into the result.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseDeterministicRandom.h"
#include "Core/EclipseTypes.h"
#include "Engine/DataAsset.h"
#include "Loot/EclipseLootTypes.h"
#include "EclipseLootTable.generated.h"

/**
 * A rollable loot table.
 *
 * Tables are data assets so that a designer can retune drop rates without a programmer,
 * and so that CI can validate every item reference before anyone runs the game.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseLootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable table id, for example "Loot.EliteSoldier". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Loot")
	FName TableId;

	/** Designer-facing description of when this table is used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Loot")
	FText Description;

	/** Fewest rolls performed per opening. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Loot", meta = (ClampMin = "0"))
	int32 RollCountMin = 2;

	/** Most rolls performed per opening. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Loot", meta = (ClampMin = "0"))
	int32 RollCountMax = 2;

	/** Lowest threat level at which this table may be used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Loot", meta = (ClampMin = "0.0"))
	float MinimumThreatLevel = 0.0f;

	/** Weight per rarity band. Bands iterate in enum order. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Loot")
	TMap<EEclipseRarity, float> RarityWeights;

	/** The entries. Authored order is the tie-break order. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Loot")
	TArray<FEclipseLootEntry> Entries;

	/**
	 * Roll the table. The caller owns the stream, which is what makes a container's
	 * contents reproducible from its stable id.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Loot")
	FEclipseLootResult Roll(FEclipseDeterministicRandom& Random) const;

	/**
	 * Structural validation, used by the content validator and by the editor's data
	 * validation. Appends one message per problem and returns true when the table is sound.
	 */
	bool ValidateTable(TArray<FString>& OutErrors) const;

	/** Rarest band present in the table, for the threat-level guard. */
	EEclipseRarity GetHighestRarity() const;

	/** Weight for a rarity band, or zero when the band is not used. */
	float GetRarityWeight(EEclipseRarity Rarity) const;

private:
	/** Pick a rarity band, iterating bands in enum order. */
	EEclipseRarity PickRarity(FEclipseDeterministicRandom& Random) const;

	/** Pick an entry inside a band, falling back to the whole table. */
	const FEclipseLootEntry* PickEntry(EEclipseRarity Rarity, FEclipseDeterministicRandom& Random) const;
};
