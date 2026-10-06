// PROJECT ECLIPSE - Equipment component implementation.
//
// Purpose
//   Slot resolution, stat modifier application and durability.

#include "Inventory/EclipseEquipmentComponent.h"

#include "Core/EclipseLog.h"
#include "Core/EclipseStatComponent.h"
#include "Inventory/EclipseInventoryComponent.h"
#include "Inventory/EclipseItemDefinition.h"
#include "Net/UnrealNetwork.h"
#include "Weapons/EclipseWeaponComponent.h"

namespace
{
	/** Human-readable slot name for logs. */
	FString SlotNameString(EEclipseEquipmentSlot Slot)
	{
		return StaticEnum<EEclipseEquipmentSlot>() != nullptr
			? StaticEnum<EEclipseEquipmentSlot>()->GetNameStringByValue(static_cast<int64>(Slot))
			: FString::FromInt(static_cast<int32>(Slot));
	}

	/** Gameplay tag names, kept next to the mapping so the two cannot drift apart. */
	const TCHAR* GSlotTags[] =
	{
		TEXT("Equipment.None"),
		TEXT("Equipment.Weapon.Primary"),
		TEXT("Equipment.Weapon.Secondary"),
		TEXT("Equipment.Weapon.Sidearm"),
		TEXT("Equipment.Weapon.Melee"),
		TEXT("Equipment.Armor.Head"),
		TEXT("Equipment.Armor.Chest"),
		TEXT("Equipment.Armor.Legs"),
		TEXT("Equipment.Backpack"),
		TEXT("Equipment.Quick.1"),
		TEXT("Equipment.Quick.2"),
		TEXT("Equipment.Quick.3"),
		TEXT("Equipment.Quick.4")
	};
}

UEclipseEquipmentComponent::UEclipseEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEclipseEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEclipseEquipmentComponent, EquippedItems);
}

const TArray<EEclipseEquipmentSlot>& UEclipseEquipmentComponent::GetEquippableSlots()
{
	static const TArray<EEclipseEquipmentSlot> Slots = {
		EEclipseEquipmentSlot::Head,
		EEclipseEquipmentSlot::Chest,
		EEclipseEquipmentSlot::Legs,
		EEclipseEquipmentSlot::Backpack,
		EEclipseEquipmentSlot::PrimaryWeapon,
		EEclipseEquipmentSlot::SecondaryWeapon,
		EEclipseEquipmentSlot::Sidearm,
		EEclipseEquipmentSlot::Melee
	};
	return Slots;
}

FGameplayTag UEclipseEquipmentComponent::ResolveTagFromSlot(EEclipseEquipmentSlot Slot)
{
	const int32 Index = static_cast<int32>(Slot);
	if (Index < 0 || Index >= UE_ARRAY_COUNT(GSlotTags))
	{
		return FGameplayTag::EmptyTag;
	}
	return FGameplayTag::RequestGameplayTag(FName(GSlotTags[Index]), false);
}

EEclipseEquipmentSlot UEclipseEquipmentComponent::ResolveSlotFromTag(const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return EEclipseEquipmentSlot::None;
	}

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(GSlotTags); ++Index)
	{
		if (Tag.ToString().Equals(GSlotTags[Index], ESearchCase::IgnoreCase))
		{
			return static_cast<EEclipseEquipmentSlot>(Index);
		}
	}

	UE_LOG(LogEclipseItems, Warning, TEXT("Equipment tag '%s' does not map to a slot."), *Tag.ToString());
	return EEclipseEquipmentSlot::None;
}

FName UEclipseEquipmentComponent::MakeModifierSourceId(EEclipseEquipmentSlot Slot)
{
	// The display name (Chest, Head, ...) is stable across enum reordering, unlike the
	// numeric value, and it reads well in the debug overlay.
	const FString SlotName = StaticEnum<EEclipseEquipmentSlot>() != nullptr
		? StaticEnum<EEclipseEquipmentSlot>()->GetNameStringByValue(static_cast<int64>(Slot))
		: FString::FromInt(static_cast<int32>(Slot));
	return FName(*FString::Printf(TEXT("Equipment.%s"), *SlotName));
}

const FEclipseEquippedItem* UEclipseEquipmentComponent::GetEquipped(EEclipseEquipmentSlot Slot) const
{
	return EquippedItems.FindByPredicate([Slot](const FEclipseEquippedItem& Entry)
	{
		return Entry.Slot == Slot;
	});
}

bool UEclipseEquipmentComponent::IsSlotOccupied(EEclipseEquipmentSlot Slot) const
{
	return GetEquipped(Slot) != nullptr;
}

bool UEclipseEquipmentComponent::EquipFromInventory(FName ItemId)
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	UEclipseInventoryComponent* Inventory = Owner->FindComponentByClass<UEclipseInventoryComponent>();
	if (Inventory == nullptr)
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("EquipFromInventory called on an actor with no inventory."));
		return false;
	}

	UEclipseItemCatalog* Catalog = UEclipseItemCatalog::GetCatalog();
	UEclipseItemDefinition* Definition = Catalog != nullptr ? Catalog->Find(ItemId) : nullptr;
	if (Definition == nullptr)
	{
		return false;
	}

	if (!Definition->bEquippable)
	{
		UE_LOG(LogEclipseItems, Verbose, TEXT("%s is not equippable."), *ItemId.ToString());
		return false;
	}

	// Weapons go to the weapon rack; everything else to an equipment slot.
	if (Definition->WeaponDefinition != nullptr)
	{
		UEclipseWeaponComponent* WeaponComponent = Owner->FindComponentByClass<UEclipseWeaponComponent>();
		if (WeaponComponent == nullptr)
		{
			return false;
		}

		const EEclipseEquipmentSlot WeaponSlot = WeaponComponent->GetActiveWeapon() == nullptr
			? EEclipseEquipmentSlot::PrimaryWeapon
			: EEclipseEquipmentSlot::SecondaryWeapon;

		if (!Inventory->RemoveItem(ItemId, 1))
		{
			return false;
		}

		if (WeaponComponent->EquipWeapon(Definition->WeaponDefinition, WeaponSlot) == nullptr)
		{
			Inventory->AddItem(ItemId, 1);
			return false;
		}

		OnEquipmentChanged.Broadcast(WeaponSlot, ItemId);
		return true;
	}

	const EEclipseEquipmentSlot Slot = ResolveSlotFromTag(Definition->EquipSlotTag);
	if (Slot == EEclipseEquipmentSlot::None)
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("%s is marked equippable but has no equipment slot tag."), *ItemId.ToString());
		return false;
	}

	if (IsSlotOccupied(Slot))
	{
		// Swapping is intentional and is the common case (upgrading a vest), so the old
		// item goes back to the inventory first and is restored if the swap fails.
		if (!UnequipToInventory(Slot))
		{
			return false;
		}
	}

	if (!Inventory->RemoveItem(ItemId, 1))
	{
		return false;
	}

	FEclipseEquippedItem& Entry = EquippedItems.AddDefaulted_GetRef();
	Entry.ItemId = ItemId;
	Entry.Slot = Slot;
	Entry.Durability = Definition->MaxDurability;

	ApplySlotModifiers(Entry);
	OnEquipmentChanged.Broadcast(Slot, ItemId);

	UE_LOG(LogEclipseItems, Verbose, TEXT("%s equipped %s into %s."),
		*Owner->GetName(), *ItemId.ToString(), *SlotNameString(Slot));

	return true;
}

bool UEclipseEquipmentComponent::UnequipToInventory(EEclipseEquipmentSlot Slot)
{
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return false;
	}

	const int32 Index = EquippedItems.IndexOfByPredicate([Slot](const FEclipseEquippedItem& Entry)
	{
		return Entry.Slot == Slot;
	});

	if (Index == INDEX_NONE)
	{
		return false;
	}

	const FEclipseEquippedItem Entry = EquippedItems[Index];

	UEclipseInventoryComponent* Inventory = Owner->FindComponentByClass<UEclipseInventoryComponent>();
	if (Inventory == nullptr)
	{
		return false;
	}

	const int32 Accepted = Inventory->AddItem(Entry.ItemId, 1);
	if (Accepted <= 0)
	{
		// Never destroy an item because the inventory was full.
		return false;
	}

	ClearSlotModifiers(Slot);
	EquippedItems.RemoveAt(Index);
	OnEquipmentChanged.Broadcast(Slot, NAME_None);
	return true;
}

void UEclipseEquipmentComponent::ApplySlotModifiers(const FEclipseEquippedItem& Entry)
{
	const AActor* Owner = GetOwner();
	UEclipseStatComponent* StatComponent = Owner != nullptr ? Owner->FindComponentByClass<UEclipseStatComponent>() : nullptr;
	if (StatComponent == nullptr)
	{
		return;
	}

	const UEclipseItemCatalog* Catalog = UEclipseItemCatalog::GetCatalog();
	const UEclipseItemDefinition* Definition = Catalog != nullptr ? Catalog->Find(Entry.ItemId) : nullptr;
	if (Definition == nullptr)
	{
		return;
	}

	StatComponent->AddModifierSource(MakeModifierSourceId(Entry.Slot), Definition->StatModifiers);
}

void UEclipseEquipmentComponent::ClearSlotModifiers(EEclipseEquipmentSlot Slot)
{
	const AActor* Owner = GetOwner();
	UEclipseStatComponent* StatComponent = Owner != nullptr ? Owner->FindComponentByClass<UEclipseStatComponent>() : nullptr;
	if (StatComponent != nullptr)
	{
		StatComponent->RemoveModifierSource(MakeModifierSourceId(Slot));
	}
}

bool UEclipseEquipmentComponent::ApplyDurabilityDamage(EEclipseEquipmentSlot Slot, float Amount)
{
	if (Amount <= 0.0f || !HasAuthority())
	{
		return false;
	}

	for (FEclipseEquippedItem& Entry : EquippedItems)
	{
		if (Entry.Slot != Slot || Entry.Durability <= 0.0f)
		{
			continue;
		}

		Entry.Durability = FMath::Max(0.0f, Entry.Durability - Amount);
		if (Entry.Durability <= 0.0f)
		{
			UE_LOG(LogEclipseItems, Log, TEXT("%s wore out and is gone."), *Entry.ItemId.ToString());
			const FName BrokenItemId = Entry.ItemId;
			ClearSlotModifiers(Slot);
			EquippedItems.RemoveAll([Slot](const FEclipseEquippedItem& Candidate)
			{
				return Candidate.Slot == Slot;
			});
			OnEquipmentChanged.Broadcast(Slot, BrokenItemId);
		}

		return true;
	}

	return false;
}

float UEclipseEquipmentComponent::GetTotalArmorContribution() const
{
	const UEclipseItemCatalog* Catalog = UEclipseItemCatalog::GetCatalog();
	if (Catalog == nullptr)
	{
		return 0.0f;
	}

	float Total = 0.0f;
	for (const FEclipseEquippedItem& Entry : EquippedItems)
	{
		if (const UEclipseItemDefinition* Definition = Catalog->Find(Entry.ItemId))
		{
			for (const FEclipseStatModifier& Modifier : Definition->StatModifiers)
			{
				if (Modifier.Stat == EEclipseStat::Armor)
				{
					Total += Modifier.Additive;
				}
			}
		}
	}

	return Total;
}
