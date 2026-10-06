// PROJECT ECLIPSE - Loot table implementation.
//
// Purpose
//   The roll itself. Keep this in step with eclipse_reference_model.roll_loot_table: the
//   reference tests roll the same tables with the same seeds and compare.

#include "Loot/EclipseLootTable.h"

#include "Core/EclipseLog.h"
#include "Inventory/EclipseItemDefinition.h"

int32 FEclipseLootResult::GetTotalItemCount() const
{
	int32 Total = 0;
	for (const FEclipseItemStack& Stack : Stacks)
	{
		Total += Stack.Count;
	}
	return Total;
}

void FEclipseLootResult::Merge(FName ItemId, int32 Count)
{
	if (ItemId.IsNone() || Count <= 0)
	{
		return;
	}

	for (FEclipseItemStack& Stack : Stacks)
	{
		if (Stack.ItemId == ItemId)
		{
			Stack.Count += Count;
			return;
		}
	}

	Stacks.Add(FEclipseItemStack::Make(ItemId, Count));
}

float UEclipseLootTable::GetRarityWeight(EEclipseRarity Rarity) const
{
	if (const float* Found = RarityWeights.Find(Rarity))
	{
		return FMath::Max(0.0f, *Found);
	}
	return 0.0f;
}

EEclipseRarity UEclipseLootTable::GetHighestRarity() const
{
	EEclipseRarity Highest = EEclipseRarity::Common;
	for (const FEclipseLootEntry& Entry : Entries)
	{
		if (Entry.Rarity > Highest)
		{
			Highest = Entry.Rarity;
		}
	}
	return Highest;
}

EEclipseRarity UEclipseLootTable::PickRarity(FEclipseDeterministicRandom& Random) const
{
	TArray<float> Weights;
	Weights.Reserve(static_cast<int32>(EEclipseRarity::Count));

	// Enum order, always. Iterating RarityWeights directly would make the roll depend on
	// TMap ordering, which is not stable between runs.
	for (int32 Index = 0; Index < static_cast<int32>(EEclipseRarity::Count); ++Index)
	{
		Weights.Add(GetRarityWeight(static_cast<EEclipseRarity>(Index)));
	}

	const int32 Picked = FEclipseDeterministicRandom::WeightedPick(Weights, Random);
	if (Picked == INDEX_NONE)
	{
		// No bands configured: fall back to the most common band so a mis-authored table
		// still drops something rather than silently dropping nothing.
		return EEclipseRarity::Common;
	}

	return static_cast<EEclipseRarity>(Picked);
}

const FEclipseLootEntry* UEclipseLootTable::PickEntry(EEclipseRarity Rarity, FEclipseDeterministicRandom& Random) const
{
	TArray<const FEclipseLootEntry*> Candidates;
	TArray<float> Weights;

	for (const FEclipseLootEntry& Entry : Entries)
	{
		if (Entry.IsValidEntry() && Entry.Rarity == Rarity)
		{
			Candidates.Add(&Entry);
			Weights.Add(Entry.Weight);
		}
	}

	if (Candidates.Num() == 0)
	{
		// A band with no entries is a content mistake, but the player should still get
		// loot: fall back to the whole table.
		for (const FEclipseLootEntry& Entry : Entries)
		{
			if (Entry.IsValidEntry())
			{
				Candidates.Add(&Entry);
				Weights.Add(Entry.Weight);
			}
		}
	}

	const int32 Picked = FEclipseDeterministicRandom::WeightedPick(Weights, Random);
	return Picked == INDEX_NONE ? nullptr : Candidates[Picked];
}

FEclipseLootResult UEclipseLootTable::Roll(FEclipseDeterministicRandom& Random) const
{
	FEclipseLootResult Result;

	const int32 MinRolls = FMath::Max(0, FMath::Min(RollCountMin, RollCountMax));
	const int32 MaxRolls = FMath::Max(0, FMath::Max(RollCountMin, RollCountMax));

	// The engine's RangeInt requires Min <= Max, which the clamps above guarantee.
	Result.RollCount = MinRolls == MaxRolls ? MinRolls : Random.RangeInt(MinRolls, MaxRolls);

	for (int32 RollIndex = 0; RollIndex < Result.RollCount; ++RollIndex)
	{
		const EEclipseRarity Band = PickRarity(Random);
		const FEclipseLootEntry* Entry = PickEntry(Band, Random);
		if (Entry == nullptr)
		{
			continue;
		}

		const int32 Count = Entry->MinCount == Entry->MaxCount
			? Entry->MinCount
			: Random.RangeInt(Entry->MinCount, Entry->MaxCount);

		Result.Merge(Entry->ItemId, Count);
	}

	return Result;
}

bool UEclipseLootTable::ValidateTable(TArray<FString>& OutErrors) const
{
	const int32 FirstError = OutErrors.Num();

	if (TableId.IsNone())
	{
		OutErrors.Add(TEXT("Loot table has no id."));
	}

	if (Entries.Num() == 0)
	{
		OutErrors.Add(FString::Printf(TEXT("Loot table '%s' has no entries."), *TableId.ToString()));
	}

	if (RollCountMax < RollCountMin)
	{
		OutErrors.Add(FString::Printf(TEXT("Loot table '%s' has RollCountMax below RollCountMin."), *TableId.ToString()));
	}

	float TotalRarityWeight = 0.0f;
	for (int32 Index = 0; Index < static_cast<int32>(EEclipseRarity::Count); ++Index)
	{
		TotalRarityWeight += GetRarityWeight(static_cast<EEclipseRarity>(Index));
	}
	if (TotalRarityWeight <= 0.0f)
	{
		OutErrors.Add(FString::Printf(TEXT("Loot table '%s' has no rarity weights."), *TableId.ToString()));
	}

	for (const FEclipseLootEntry& Entry : Entries)
	{
		if (!Entry.IsValidEntry())
		{
			OutErrors.Add(FString::Printf(TEXT("Loot table '%s' has an invalid entry for '%s'."),
				*TableId.ToString(), *Entry.ItemId.ToString()));
			continue;
		}

		if (GetRarityWeight(Entry.Rarity) <= 0.0f)
		{
			OutErrors.Add(FString::Printf(TEXT("Loot table '%s' entry '%s' uses rarity '%s', which has no weight."),
				*TableId.ToString(), *Entry.ItemId.ToString(), EclipseEnumNames::ToString(Entry.Rarity)));
		}

		// Item references are checked against the catalogue when one is available. In a
		// commandlet or in CI there is no catalogue loaded, and the JSON validator covers
		// that case instead.
		if (const UEclipseItemCatalog* Catalog = UEclipseItemCatalog::GetCatalog())
		{
			if (!Catalog->Contains(Entry.ItemId))
			{
				OutErrors.Add(FString::Printf(TEXT("Loot table '%s' references unknown item '%s'."),
					*TableId.ToString(), *Entry.ItemId.ToString()));
			}
		}
	}

	return OutErrors.Num() == FirstError;
}
