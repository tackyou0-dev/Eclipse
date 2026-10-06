// PROJECT ECLIPSE - Item definition and catalogue.
//
// Purpose
//   Items are data. An item definition describes weight, stack size, value and what the
//   item does when equipped or consumed; the catalogue is the lookup that turns an item
//   id into a definition.
//
// Content source
//   Content/Eclipse/Data/VerticalSlice/items.json mirrors these fields and is validated
//   by Tools/ci/validate_content.py on every commit.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Weapons/EclipseWeaponDefinition.h"
#include "EclipseItemDefinition.generated.h"

/**
 * A single item type.
 *
 * ItemId is the stable key used by loot tables, recipes, missions and save files.
 * Renaming it breaks saves; add a new definition and migrate instead.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable id, for example "Ammo.556" or "Armor.CompositeVest". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item")
	FName ItemId;

	/** Display name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item")
	FText DisplayName;

	/** Flavour and mechanical description shown in the tooltip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item", meta = (MultiLine = "true"))
	FText Description;

	/** Inventory category. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item")
	EEclipseItemCategory Category = EEclipseItemCategory::Miscellaneous;

	/** Rarity band, for colour and pricing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item")
	EEclipseRarity Rarity = EEclipseRarity::Common;

	/** Grid footprint in the inventory UI, in cells. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item")
	FIntPoint GridSize = FIntPoint(1, 1);

	/** Weight of one unit in kilograms. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item", meta = (ClampMin = "0.0"))
	float Weight = 0.0f;

	/** Volume of one unit in litres, used by vehicle storage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item", meta = (ClampMin = "0.0"))
	float Volume = 0.0f;

	/** Maximum stack size. 1 for anything unique such as a weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item", meta = (ClampMin = "1"))
	int32 MaxStackSize = 1;

	/** Durability of a fresh item. Zero for items that never wear out. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item", meta = (ClampMin = "0.0"))
	float MaxDurability = 0.0f;

	/** Base value in credits, before faction pricing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item", meta = (ClampMin = "0"))
	int32 BaseValue = 0;

	/** Health restored when consumed. Zero for non-medical items. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item", meta = (ClampMin = "0.0"))
	float ConsumableHealAmount = 0.0f;

	/** True when the item can be equipped into a slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item")
	bool bEquippable = false;

	/** Slot the item occupies when equipped, as a gameplay tag such as Equipment.Armor.Chest. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item", meta = (Categories = "Equipment"))
	FGameplayTag EquipSlotTag;

	/** Stat modifiers granted while equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item")
	TArray<FEclipseStatModifier> StatModifiers;

	/**
	 * Weapon this item equips as, or null for non-weapon items. Weapon items must set this
	 * so the equipment component knows to hand them to the weapon rack instead of a slot.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Item")
	TObjectPtr<UEclipseWeaponDefinition> WeaponDefinition;

	/** True when the definition is usable: it must have an id and a sane stack size. */
	bool IsValidDefinition() const
	{
		return !ItemId.IsNone() && MaxStackSize >= 1 && Weight >= 0.0f;
	}

	/** Weight of a stack of this item. */
	float GetStackWeight(int32 Count) const
	{
		return Weight * static_cast<float>(FMath::Max(0, Count));
	}

	/** True when the item restores health when consumed. */
	bool IsConsumable() const
	{
		return ConsumableHealAmount > 0.0f;
	}
};

/**
 * The lookup table used by every system that turns an item id into a definition.
 *
 * Shipped as a single data asset so that a loot roll, a recipe and a vendor all resolve
 * the same object. A missing entry is a content error, and every caller logs it rather
 * than silently returning null.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseItemCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Every item in the game. Populated by the content build step. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Catalog")
	TArray<TObjectPtr<UEclipseItemDefinition>> Items;

	/** Find an item definition by id, or null. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Catalog")
	UEclipseItemDefinition* Find(FName ItemId) const;

	/** True when the catalogue contains the id. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Catalog")
	bool Contains(FName ItemId) const;

	/** Every id in the catalogue, for validation and for the debug browser. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Catalog")
	TArray<FName> GetAllItemIds() const;

	/** Static accessor for the project's catalogue. Null before content is loaded. */
	static UEclipseItemCatalog* GetCatalog();
};
