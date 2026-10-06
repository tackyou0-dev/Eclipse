// PROJECT ECLIPSE - Loot value types.
//
// Purpose
//   The roll inputs and outputs. A loot table is authored as data (mirroring
//   Content/Eclipse/Data/VerticalSlice/loot.json) and rolled by UEclipseLootTable::Roll
//   using FEclipseDeterministicRandom, so the same container in the same raid always
//   contains the same thing.
//
// Determinism rules
//   * Rarity bands are always iterated in enum order, never in TMap order, because map
//     iteration order is not guaranteed and a loot table that rolls differently between
//     runs is a bug report waiting to happen.
//   * Entries are iterated in array order, which is the order a designer authored them.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Inventory/EclipseItemTypes.h"
#include "EclipseLootTypes.generated.h"

/**
 * One possible result of a loot roll.
 *
 * Weight is relative to the other entries of the same rarity, so a designer can tune a
 * single item without rebalancing the whole table.
 */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseLootEntry
{
	GENERATED_BODY()

	/** Item definition id granted by this entry. Must exist in the item catalogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Loot")
	FName ItemId;

	/** Design-time note about when this entry should appear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Loot")
	FText Comment;

	/** Relative weight inside its rarity band. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Loot", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	/** Fewest items this entry can grant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Loot", meta = (ClampMin = "1"))
	int32 MinCount = 1;

	/** Most items this entry can grant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Loot", meta = (ClampMin = "1"))
	int32 MaxCount = 1;

	/** Rarity band this entry belongs to. Rolled against the table's rarity weights first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Loot")
	EEclipseRarity Rarity = EEclipseRarity::Common;

	/** True when the entry is usable. */
	bool IsValidEntry() const
	{
		return !ItemId.IsNone() && Weight > 0.0f && MinCount >= 1 && MaxCount >= MinCount;
	}
};

/** A rolled result: the stacks a table produced, merged by item id. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseLootResult
{
	GENERATED_BODY()

	/** Item stacks produced by the roll. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Loot")
	TArray<FEclipseItemStack> Stacks;

	/** How many rolls the table performed. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Loot")
	int32 RollCount = 0;

	/** Sum of every stack count, for analytics and tests. */
	int32 GetTotalItemCount() const;

	/** Merge a stack into the result, combining entries with the same item id. */
	void Merge(FName ItemId, int32 Count);

	/** True when the roll produced nothing. */
	bool IsEmpty() const { return Stacks.Num() == 0; }
};
