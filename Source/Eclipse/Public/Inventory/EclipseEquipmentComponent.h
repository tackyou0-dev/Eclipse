// PROJECT ECLIPSE - Equipment component.
//
// Purpose
//   What the character is wearing. Equipment contributes stat modifiers (armour, carry
//   capacity, resistances) and is the thing that wears out: durability lives on the
//   equipped instance, not on the inventory stack.
//
// Weapons
//   Weapons are not handled here. A weapon item is handed to UEclipseWeaponComponent,
//   because a weapon has attachments and ammunition that armour does not.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "GameplayTagContainer.h"
#include "Inventory/EclipseItemTypes.h"
#include "EclipseEquipmentComponent.generated.h"

/** Fired when a slot changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEclipseEquipmentChangedSignature, EEclipseEquipmentSlot, Slot, FName, ItemId);

/**
 * Equipped armour and accessories for one character.
 *
 * The stat modifier source id is "Equipment.<SlotName>", so the same slot can never apply
 * twice and the debug overlay can point at exactly which item is responsible for a stat.
 */
UCLASS(ClassGroup = (Eclipse), meta = (BlueprintSpawnableComponent))
class ECLIPSE_API UEclipseEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEclipseEquipmentComponent();

	/** UActorComponent: replicate the equipment list. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------------------------
	// Equipping
	// ------------------------------------------------------------------

	/**
	 * Equip an item the character is carrying. Consumes it from the inventory, applies its
	 * modifiers and returns false when the item is not equippable or the slot is busy.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Equipment")
	bool EquipFromInventory(FName ItemId);

	/** Unequip a slot, returning the item to the inventory. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Equipment")
	bool UnequipToInventory(EEclipseEquipmentSlot Slot);

	/** Equipped entry in a slot, or nullptr. */
	const FEclipseEquippedItem* GetEquipped(EEclipseEquipmentSlot Slot) const;

	/** Every equipped entry, including empty slots. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Equipment")
	TArray<FEclipseEquippedItem> GetEquippedItems() const { return EquippedItems; }

	/** True when the slot holds something. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Equipment")
	bool IsSlotOccupied(EEclipseEquipmentSlot Slot) const;

	/** Apply durability loss to a slot. Destroys the item when it reaches zero. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Equipment")
	bool ApplyDurabilityDamage(EEclipseEquipmentSlot Slot, float Amount);

	/** Total armour contributed by equipment, for the character sheet. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Equipment")
	float GetTotalArmorContribution() const;

	/** Map an equipment gameplay tag (Equipment.Armor.Chest) to a slot. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Equipment")
	static EEclipseEquipmentSlot ResolveSlotFromTag(const FGameplayTag& Tag);

	/** Map a slot to its tag. Inverse of ResolveSlotFromTag. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Equipment")
	static FGameplayTag ResolveTagFromSlot(EEclipseEquipmentSlot Slot);

	/** Fired whenever a slot changes. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|Equipment")
	FEclipseEquipmentChangedSignature OnEquipmentChanged;

	/** Slots a character can equip into, in display order. */
	static const TArray<EEclipseEquipmentSlot>& GetEquippableSlots();

protected:
	/** Apply or refresh the stat modifier source for a slot. */
	void ApplySlotModifiers(const FEclipseEquippedItem& Entry);

	/** Remove the stat modifier source for a slot. */
	void ClearSlotModifiers(EEclipseEquipmentSlot Slot);

	/** Modifier source id for a slot. */
	static FName MakeModifierSourceId(EEclipseEquipmentSlot Slot);

	/** Equipped items, one entry per occupied slot. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Equipment")
	TArray<FEclipseEquippedItem> EquippedItems;
};
