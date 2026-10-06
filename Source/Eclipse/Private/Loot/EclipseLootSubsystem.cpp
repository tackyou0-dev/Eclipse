// PROJECT ECLIPSE - Loot subsystem implementation.
//
// Purpose
//   Seed derivation and spawning. The roll itself lives in UEclipseLootTable so that it
//   can be tested without a world.

#include "Loot/EclipseLootSubsystem.h"

#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseLog.h"
#include "Engine/World.h"
#include "Inventory/EclipseInventoryComponent.h"
#include "Inventory/EclipsePickupActor.h"

int32 UEclipseLootSubsystem::MakeStableId(const AActor* Actor)
{
	if (Actor == nullptr)
	{
		return 0;
	}

	// The actor name is stable for level-placed actors across runs of the same build, which
	// is exactly the property container contents need.
	return static_cast<int32>(FEclipseDeterministicRandom::StableIdFromString(Actor->GetName()));
}

int32 UEclipseLootSubsystem::MakeStableIdFromLocation(const FVector& Location)
{
	// Two centimetres of quantisation: enough to be stable for a snapped container, fine
	// enough to distinguish neighbouring crates.
	const FIntVector Quantised(
		FMath::RoundToInt(Location.X / 2.0f),
		FMath::RoundToInt(Location.Y / 2.0f),
		FMath::RoundToInt(Location.Z / 2.0f));

	const FString Key = FString::Printf(TEXT("%d:%d:%d"), Quantised.X, Quantised.Y, Quantised.Z);
	return static_cast<int32>(FEclipseDeterministicRandom::StableIdFromString(Key));
}

int32 UEclipseLootSubsystem::GetWorldSeed() const
{
	return static_cast<int32>(UEclipseDeveloperSettings::GetWorldSeed());
}

FEclipseLootResult UEclipseLootSubsystem::RollWithStream(UEclipseLootTable* Table, FEclipseDeterministicRandom& Random) const
{
	if (Table == nullptr)
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("RollWithStream called without a table."));
		return FEclipseLootResult();
	}

	return Table->Roll(Random);
}

FEclipseLootResult UEclipseLootSubsystem::RollForStableId(UEclipseLootTable* Table, int32 StableId) const
{
	if (Table == nullptr)
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("RollForStableId called without a table."));
		return FEclipseLootResult();
	}

	// The seed contract: WorldSeed ^ StableId, mixed through the deterministic stream.
	FEclipseDeterministicRandom Random = FEclipseDeterministicRandom::ForStableId(
		UEclipseDeveloperSettings::GetWorldSeed(),
		static_cast<uint32>(StableId));

	return Table->Roll(Random);
}

int32 UEclipseLootSubsystem::SpawnLootStacks(const FVector& Origin, const TArray<FEclipseItemStack>& Stacks, float ScatterRadius, TSubclassOf<AEclipsePickupActor> PickupClass)
{
	UWorld* World = GetWorld();
	if (World == nullptr || !World->IsGameWorld() || PickupClass == nullptr)
	{
		return 0;
	}

	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogEclipseSecurity, Warning, TEXT("SpawnLootStacks refused on a client."));
		return 0;
	}

	// Scatter is rolled from the location itself so a re-spawned container (after a save
	// load) puts its loot in the same place.
	FEclipseDeterministicRandom ScatterRandom = FEclipseDeterministicRandom::ForStableId(
		UEclipseDeveloperSettings::GetWorldSeed(),
		static_cast<uint32>(MakeStableIdFromLocation(Origin)) ^ 0xA5A5A5A5u);

	int32 Spawned = 0;
	for (const FEclipseItemStack& Stack : Stacks)
	{
		if (Stack.IsEmpty() || Spawned >= MaxPickupsPerContainer)
		{
			continue;
		}

		FVector Location = Origin;
		if (ScatterRadius > 0.0f)
		{
			Location += FVector(
				ScatterRandom.Range(-ScatterRadius, ScatterRadius),
				ScatterRandom.Range(-ScatterRadius, ScatterRadius),
				0.0f);
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AEclipsePickupActor* Pickup = World->SpawnActor<AEclipsePickupActor>(PickupClass, Location, FRotator::ZeroRotator, SpawnParams);
		if (Pickup == nullptr)
		{
			continue;
		}

		Pickup->InitializePickup(Stack.ItemId, Stack.Count);
		++Spawned;
	}

	return Spawned;
}

int32 UEclipseLootSubsystem::SpawnLootAt(const FVector& Origin, UEclipseLootTable* Table, int32 StableId, float ScatterRadius, TSubclassOf<AEclipsePickupActor> PickupClass)
{
	const FEclipseLootResult Result = RollForStableId(Table, StableId);
	if (Result.IsEmpty())
	{
		return 0;
	}

	return SpawnLootStacks(Origin, Result.Stacks, ScatterRadius, PickupClass);
}

int32 UEclipseLootSubsystem::GrantLootTo(AActor* Recipient, UEclipseLootTable* Table, int32 StableId)
{
	if (Recipient == nullptr)
	{
		return 0;
	}

	UEclipseInventoryComponent* Inventory = Recipient->FindComponentByClass<UEclipseInventoryComponent>();
	if (Inventory == nullptr)
	{
		UE_LOG(LogEclipseItems, Verbose, TEXT("%s has no inventory; loot granted to the void."), *Recipient->GetName());
		return 0;
	}

	const FEclipseLootResult Result = RollForStableId(Table, StableId);
	int32 Granted = 0;
	for (const FEclipseItemStack& Stack : Result.Stacks)
	{
		Granted += Inventory->AddItem(Stack.ItemId, Stack.Count);
	}

	return Granted;
}
