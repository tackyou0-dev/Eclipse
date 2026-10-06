// PROJECT ECLIPSE - Item value types.
//
// Purpose
//   The stack representation used by the inventory, the loot system and save files.
//
// Durability
//   Armour and weapons carry durability. A stack of ammunition does not, so durability is
//   stored per stack and only meaningful for definitions with a positive MaxDurability.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "EclipseItemTypes.generated.h"

/**
 * One stack of items in a container.
 *
 * ItemId is the stable key (for example "Ammo.556"); the definition that describes it is
 * resolved through the item catalogue, never stored here, so a balance change to the
 * definition is picked up by every existing stack.
 */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseItemStack
{
	GENERATED_BODY()

	/** Item definition id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Item")
	FName ItemId;

	/** How many are in this stack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Item", meta = (ClampMin = "1"))
	int32 Count = 1;

	/** Remaining durability, or zero for items that do not wear out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Item", meta = (ClampMin = "0.0"))
	float Durability = 0.0f;

	/** True when the stack is empty and should be removed. */
	bool IsEmpty() const
	{
		return ItemId.IsNone() || Count <= 0;
	}

	/** True when more of this item could be added to the stack. */
	bool CanStackWith(const FEclipseItemStack& Other, int32 MaxStackSize) const
	{
		return ItemId == Other.ItemId && MaxStackSize > 1 && Count < MaxStackSize;
	}

	/** Convenience constructor. */
	static FEclipseItemStack Make(FName InItemId, int32 InCount)
	{
		FEclipseItemStack Stack;
		Stack.ItemId = InItemId;
		Stack.Count = FMath::Max(1, InCount);
		return Stack;
	}
};

/**
 * A single instance of an equipped item, with the modifiers it contributes.
 *
 * Equipment is per instance rather than per definition because two composite vests can
 * have different durability and different fitted modules later.
 */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseEquippedItem
{
	GENERATED_BODY()

	/** Item definition id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Item")
	FName ItemId;

	/** Slot it occupies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Item")
	EEclipseEquipmentSlot Slot = EEclipseEquipmentSlot::None;

	/** Remaining durability. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Item", meta = (ClampMin = "0.0"))
	float Durability = 0.0f;

	/** True when this entry holds an item. */
	bool IsValidEntry() const
	{
		return !ItemId.IsNone() && Slot != EEclipseEquipmentSlot::None;
	}
};
