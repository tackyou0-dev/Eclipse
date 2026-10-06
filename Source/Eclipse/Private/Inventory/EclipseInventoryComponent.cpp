// PROJECT ECLIPSE - Inventory component implementation.
//
// Purpose
//   Stack management, weight accounting and consumption. Every mutation funnels through
//   NotifyChanged so that encumbrance, replication and the HUD can never disagree about
//   the contents.

#include "Inventory/EclipseInventoryComponent.h"

#include "Combat/EclipseHealthComponent.h"
#include "Core/EclipseLog.h"
#include "Core/EclipseStatComponent.h"
#include "Engine/World.h"
#include "Inventory/EclipsePickupActor.h"
#include "Net/UnrealNetwork.h"

UEclipseInventoryComponent::UEclipseInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEclipseInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (StartingItems.Num() > 0)
	{
		Stacks = StartingItems;
	}

	EvaluateEncumbrance();
	OnInventoryChanged.Broadcast();
}

void UEclipseInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEclipseInventoryComponent, Stacks);
}

UEclipseItemDefinition* UEclipseInventoryComponent::ResolveDefinition(FName ItemId) const
{
	if (UEclipseItemCatalog* Catalog = UEclipseItemCatalog::GetCatalog())
	{
		return Catalog->Find(ItemId);
	}

	UE_LOG(LogEclipseItems, Warning, TEXT("No item catalogue available; cannot resolve '%s'."), *ItemId.ToString());
	return nullptr;
}

int32 UEclipseInventoryComponent::CountItem(FName ItemId) const
{
	int32 Total = 0;
	for (const FEclipseItemStack& Stack : Stacks)
	{
		if (Stack.ItemId == ItemId)
		{
			Total += Stack.Count;
		}
	}
	return Total;
}

const FEclipseItemStack* UEclipseInventoryComponent::FindStack(FName ItemId) const
{
	return Stacks.FindByPredicate([ItemId](const FEclipseItemStack& Stack)
	{
		return Stack.ItemId == ItemId;
	});
}

float UEclipseInventoryComponent::GetTotalWeight() const
{
	if (const UEclipseItemCatalog* Catalog = UEclipseItemCatalog::GetCatalog())
	{
		float Total = 0.0f;
		for (const FEclipseItemStack& Stack : Stacks)
		{
			if (const UEclipseItemDefinition* Definition = Catalog->Find(Stack.ItemId))
			{
				Total += Definition->GetStackWeight(Stack.Count);
			}
			else
			{
				// An unknown item still weighs something: one kilogram per unit, so a
				// content error shows up as encumbrance rather than as free loot.
				Total += static_cast<float>(Stack.Count);
			}
		}
		return Total;
	}

	return 0.0f;
}

float UEclipseInventoryComponent::GetCarryCapacity() const
{
	const AActor* Owner = GetOwner();
	if (Owner != nullptr)
	{
		if (const UEclipseStatComponent* StatComponent = Owner->FindComponentByClass<UEclipseStatComponent>())
		{
			const float Capacity = StatComponent->GetStat(EEclipseStat::CarryCapacity);
			if (Capacity > 0.0f)
			{
				return Capacity;
			}
		}
	}

	// A container with no stat component (a crate, a corpse) is limited by its configured
	// capacity instead.
	return DefaultCapacity;
}

bool UEclipseInventoryComponent::IsEncumbered() const
{
	const float Capacity = GetCarryCapacity();
	return Capacity > 0.0f && GetTotalWeight() > Capacity;
}

float UEclipseInventoryComponent::GetRemainingCapacity() const
{
	return FMath::Max(0.0f, GetCarryCapacity() - GetTotalWeight());
}

int32 UEclipseInventoryComponent::GetAcceptableCount(FName ItemId) const
{
	UEclipseItemDefinition* Definition = ResolveDefinition(ItemId);
	if (Definition == nullptr)
	{
		return 0;
	}

	// Stack space first: either an existing partial stack, or room for a new one.
	int32 StackSpace = 0;
	for (const FEclipseItemStack& Stack : Stacks)
	{
		if (Stack.ItemId == ItemId)
		{
			StackSpace += FMath::Max(0, Definition->MaxStackSize - Stack.Count);
		}
	}

	// A new stack is allowed as long as the item is not already occupying its maximum
	// number of stacks, which only matters for stack size 1 items.
	const int32 ExistingStacks = Stacks.FilterByPredicate([ItemId](const FEclipseItemStack& Stack)
	{
		return Stack.ItemId == ItemId;
	}).Num();
	if (ExistingStacks == 0 || Definition->MaxStackSize > 1)
	{
		StackSpace += Definition->MaxStackSize;
	}

	// Then weight.
	int32 WeightSpace = StackSpace;
	if (Definition->Weight > 0.0f)
	{
		const float Capacity = GetCarryCapacity();
		if (Capacity > 0.0f)
		{
			const float Remaining = Capacity - GetTotalWeight();
			WeightSpace = FMath::Max(0, FMath::FloorToInt(Remaining / Definition->Weight));
		}
	}

	return FMath::Min(StackSpace, WeightSpace);
}

int32 UEclipseInventoryComponent::AddItem(FName ItemId, int32 Count)
{
	if (Count <= 0)
	{
		return 0;
	}

	if (!HasAuthority())
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("AddItem refused on %s: not authoritative."), *GetName());
		return 0;
	}

	UEclipseItemDefinition* Definition = ResolveDefinition(ItemId);
	if (Definition == nullptr)
	{
		return 0;
	}

	const int32 Before = CountItem(ItemId);
	int32 Remaining = FMath::Min(Count, GetAcceptableCount(ItemId));

	// Fill partial stacks first so that a stack of 30/60 rounds takes the next 30 before a
	// new stack is created.
	for (FEclipseItemStack& Stack : Stacks)
	{
		if (Remaining <= 0)
		{
			break;
		}

		if (Stack.ItemId != ItemId || Stack.Count >= Definition->MaxStackSize)
		{
			continue;
		}

		const int32 Space = Definition->MaxStackSize - Stack.Count;
		const int32 ToAdd = FMath::Min(Space, Remaining);
		Stack.Count += ToAdd;
		Remaining -= ToAdd;
	}

	while (Remaining > 0)
	{
		const int32 ToAdd = FMath::Min(Definition->MaxStackSize, Remaining);
		FEclipseItemStack& NewStack = Stacks.AddDefaulted_GetRef();
		NewStack.ItemId = ItemId;
		NewStack.Count = ToAdd;
		NewStack.Durability = Definition->MaxDurability;
		Remaining -= ToAdd;
	}

	// The accepted count is measured rather than tracked through both loops: the contents
	// changed, so how much went in is simply the difference.
	const int32 Added = CountItem(ItemId) - Before;

	if (Added < Count)
	{
		OnInventoryOverflow.Broadcast(ItemId, Count, Added);
		UE_LOG(LogEclipseItems, Verbose, TEXT("%s could only accept %d of %d %s."),
			*GetName(), Added, Count, *ItemId.ToString());
	}

	if (Added > 0)
	{
		NotifyChanged();
	}

	return Added;
}

bool UEclipseInventoryComponent::RemoveItem(FName ItemId, int32 Count)
{
	if (Count <= 0)
	{
		return true;
	}

	if (!HasAuthority())
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("RemoveItem refused on %s: not authoritative."), *GetName());
		return false;
	}

	if (CountItem(ItemId) < Count)
	{
		return false;
	}

	int32 Remaining = Count;

	// Consume from the back so that partially used stacks survive longer, which is what
	// players expect when they take a few rounds out of a magazine.
	for (int32 Index = Stacks.Num() - 1; Index >= 0 && Remaining > 0; --Index)
	{
		FEclipseItemStack& Stack = Stacks[Index];
		if (Stack.ItemId != ItemId)
		{
			continue;
		}

		const int32 Taken = FMath::Min(Stack.Count, Remaining);
		Stack.Count -= Taken;
		Remaining -= Taken;
	}

	Stacks.RemoveAll([](const FEclipseItemStack& Stack)
	{
		return Stack.IsEmpty();
	});

	NotifyChanged();
	return true;
}

bool UEclipseInventoryComponent::ConsumeItem(FName ItemId)
{
	if (!HasAuthority())
	{
		return false;
	}

	const FEclipseItemStack* Stack = FindStack(ItemId);
	if (Stack == nullptr)
	{
		return false;
	}

	UEclipseItemDefinition* Definition = ResolveDefinition(ItemId);
	if (Definition == nullptr)
	{
		return false;
	}

	if (!RemoveItem(ItemId, 1))
	{
		return false;
	}

	if (Definition->ConsumableHealAmount > 0.0f)
	{
		const AActor* Owner = GetOwner();
		if (UEclipseHealthComponent* HealthComponent = Owner != nullptr ? Owner->FindComponentByClass<UEclipseHealthComponent>() : nullptr)
		{
			const float Restored = HealthComponent->Heal(Definition->ConsumableHealAmount);
			UE_LOG(LogEclipseItems, Verbose, TEXT("%s consumed %s and restored %.0f health."),
				*GetName(), *ItemId.ToString(), Restored);
		}
	}

	return true;
}

int32 UEclipseInventoryComponent::DropItem(FName ItemId, int32 Count)
{
	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (World == nullptr || Owner == nullptr || PickupActorClass == nullptr)
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("%s cannot drop items: no pickup actor class configured."), *GetName());
		return 0;
	}

	const int32 Available = CountItem(ItemId);
	const int32 ToDrop = FMath::Min(Count, Available);
	if (ToDrop <= 0 || !RemoveItem(ItemId, ToDrop))
	{
		return 0;
	}

	// Drop slightly in front of the owner so the pickup is not created inside their body.
	const FVector DropLocation = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 120.0f;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Spawned = World->SpawnActor<AActor>(PickupActorClass, DropLocation, Owner->GetActorRotation(), SpawnParams);
	if (AEclipsePickupActor* Pickup = Cast<AEclipsePickupActor>(Spawned))
	{
		Pickup->InitializePickup(ItemId, ToDrop);
		return ToDrop;
	}

	// The pickup failed to spawn; the items are put back rather than lost.
	AddItem(ItemId, ToDrop);
	return 0;
}

bool UEclipseInventoryComponent::DamageItemDurability(FName ItemId, float Amount)
{
	if (Amount <= 0.0f || !HasAuthority())
	{
		return false;
	}

	const FEclipseItemStack* Stack = FindStack(ItemId);
	if (Stack == nullptr || Stack->Durability <= 0.0f)
	{
		return false;
	}

	for (FEclipseItemStack& MutableStack : Stacks)
	{
		if (MutableStack.ItemId != ItemId)
		{
			continue;
		}

		MutableStack.Durability = FMath::Max(0.0f, MutableStack.Durability - Amount);
		if (MutableStack.Durability <= 0.0f)
		{
			UE_LOG(LogEclipseItems, Log, TEXT("%s broke."), *ItemId.ToString());
		}
		break;
	}

	NotifyChanged();
	return true;
}

void UEclipseInventoryComponent::SetStacks(const TArray<FEclipseItemStack>& NewStacks)
{
	if (!HasAuthority())
	{
		return;
	}

	Stacks = NewStacks;
	Stacks.RemoveAll([](const FEclipseItemStack& Stack)
	{
		return Stack.IsEmpty();
	});

	NotifyChanged();
}

void UEclipseInventoryComponent::EvaluateEncumbrance()
{
	const AActor* Owner = GetOwner();
	UEclipseStatComponent* StatComponent = Owner != nullptr ? Owner->FindComponentByClass<UEclipseStatComponent>() : nullptr;
	if (StatComponent == nullptr)
	{
		return;
	}

	const bool bShouldBeEncumbered = IsEncumbered();
	if (bShouldBeEncumbered == bEncumbranceApplied)
	{
		return;
	}

	if (bShouldBeEncumbered)
	{
		TArray<FEclipseStatModifier> Modifiers;
		Modifiers.Emplace(EEclipseStat::MoveSpeed, 0.0f, EncumberedSpeedMultiplier);
		StatComponent->AddModifierSource(EncumberedModifierSource(), Modifiers);
	}
	else
	{
		StatComponent->RemoveModifierSource(EncumberedModifierSource());
	}

	bEncumbranceApplied = bShouldBeEncumbered;
}

void UEclipseInventoryComponent::NotifyChanged()
{
	EvaluateEncumbrance();
	OnInventoryChanged.Broadcast();
}
