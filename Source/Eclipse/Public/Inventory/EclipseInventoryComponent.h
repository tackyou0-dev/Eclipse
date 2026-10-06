// PROJECT ECLIPSE - Inventory component.
//
// Purpose
//   Carrying things: stacks, weight, capacity and consumption. Loot hands items to this
//   component, crafting takes them away, and the HUD reads it.
//
// Encumbrance
//   Over the carry capacity the component applies a real penalty - a movement speed
//   modifier on the owner's stat component - rather than merely warning the player. The
//   modifier is added and removed by source id, so it cannot stack with itself.
//
// Authority
//   The server owns the contents. Clients see the replicated stacks and never mutate
//   them; every mutation entry point is guarded by a net mode check.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Inventory/EclipseItemDefinition.h"
#include "Inventory/EclipseItemTypes.h"
#include "EclipseInventoryComponent.generated.h"

/** Fired whenever the contents change. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEclipseInventoryChangedSignature);

/** Fired when an item cannot be added in full. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEclipseInventoryOverflowSignature, FName, ItemId, int32, RequestedCount, int32, AcceptedCount);

/**
 * Item storage for a character, a container or a vehicle.
 *
 * Capacity is by weight, not by slot count: a colonist can carry forty rounds of .338 and
 * nothing else, and the encumbrance penalty makes that choice visible in movement rather
 * than in a greyed-out button.
 */
UCLASS(ClassGroup = (Eclipse), meta = (BlueprintSpawnableComponent))
class ECLIPSE_API UEclipseInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEclipseInventoryComponent();

	/** UActorComponent: apply the initial contents and evaluate encumbrance. */
	virtual void BeginPlay() override;

	/** UActorComponent: replicate the contents. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------------------------
	// Queries
	// ------------------------------------------------------------------

	/** How many of an item are carried across all stacks. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Inventory")
	int32 CountItem(FName ItemId) const;

	/** True when at least one is carried. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Inventory")
	bool HasItem(FName ItemId) const { return CountItem(ItemId) > 0; }

	/** Every stack. Treat as read-only. */
	const TArray<FEclipseItemStack>& GetStacks() const { return Stacks; }

	/** Total weight in kilograms. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Inventory")
	float GetTotalWeight() const;

	/** Carry capacity in kilograms, from EEclipseStat::CarryCapacity. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Inventory")
	float GetCarryCapacity() const;

	/** True when the carried weight exceeds the capacity. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Inventory")
	bool IsEncumbered() const;

	/** Free weight before becoming encumbered. Never negative. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Inventory")
	float GetRemainingCapacity() const;

	/**
	 * How many more of an item could be accepted right now, considering both stack space
	 * and remaining weight.
	 */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Inventory")
	int32 GetAcceptableCount(FName ItemId) const;

	/** First stack holding the item, or nullptr. */
	const FEclipseItemStack* FindStack(FName ItemId) const;

	// ------------------------------------------------------------------
	// Mutation (server only)
	// ------------------------------------------------------------------

	/**
	 * Add items, filling existing stacks first. Returns the number accepted, which may be
	 * less than requested when the inventory is full or too heavy.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Inventory")
	int32 AddItem(FName ItemId, int32 Count);

	/** Remove items across stacks. Returns true when the full amount was removed. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Inventory")
	bool RemoveItem(FName ItemId, int32 Count);

	/** Remove one unit of an item and apply its consumption effect to the owner. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Inventory")
	bool ConsumeItem(FName ItemId);

	/** Drop a stack into the world. Returns the number actually dropped. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Inventory")
	int32 DropItem(FName ItemId, int32 Count);

	/** Reduce durability on a stack, destroying it at zero. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Inventory")
	bool DamageItemDurability(FName ItemId, float Amount);

	/** Replace the whole contents. Used when a save is applied. */
	void SetStacks(const TArray<FEclipseItemStack>& NewStacks);

	/** Fired whenever the contents change. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|Inventory")
	FEclipseInventoryChangedSignature OnInventoryChanged;

	/** Fired when part of an addition was refused. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|Inventory")
	FEclipseInventoryOverflowSignature OnInventoryOverflow;

	/** Movement multiplier applied while encumbered. */
	static constexpr float EncumberedSpeedMultiplier = 0.6f;

	/** Source id of the encumbrance modifier on the stat component. */
	static FName EncumberedModifierSource() { return TEXT("Inventory.Encumbered"); }

protected:
	/** Resolve the definition for an item id, logging when it is missing. */
	UEclipseItemDefinition* ResolveDefinition(FName ItemId) const;

	/** Add or remove the encumbrance modifier to match the current weight. */
	void EvaluateEncumbrance();

	/** Broadcast the change delegate and evaluate encumbrance. */
	void NotifyChanged();

	/** What the character is carrying. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Inventory")
	TArray<FEclipseItemStack> Stacks;

	/** Contents the character starts with. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Inventory")
	TArray<FEclipseItemStack> StartingItems;

	/** Items dropped by this inventory are spawned from this class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Inventory")
	TSubclassOf<AActor> PickupActorClass;

	/** Capacity used when the owner has no stat component (crates, corpses, vehicles). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Inventory", meta = (ClampMin = "0.0"))
	float DefaultCapacity = 60.0f;

	/** True while the encumbrance modifier is applied. */
	bool bEncumbranceApplied = false;
};
