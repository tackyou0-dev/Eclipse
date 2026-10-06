// PROJECT ECLIPSE - Loot subsystem.
//
// Purpose
//   Turns a loot table plus a stable id into items in the world or in someone's pack. This
//   is where the project's determinism contract is actually enforced: every container,
//   every corpse and every crate derives its stream from the world seed and its own
//   stable id, so re-opening the same container never re-rolls it.
//
// Seed contract
//   Container contents use MixSeed(WorldSeed, StableId). Changing either the world seed or
//   a container's id changes its contents; changing anything else does not.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Loot/EclipseLootTable.h"
#include "Subsystems/WorldSubsystem.h"
#include "EclipseLootSubsystem.generated.h"

class AEclipsePickupActor;

/**
 * World-scoped loot service.
 *
 * All rolls are server-side. A client that asks for loot gets the already-rolled stacks
 * from the container's replicated state, never a fresh roll; that is what stops a client
 * from save-scumming a supply crate.
 */
UCLASS()
class ECLIPSE_API UEclipseLootSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Roll a table for a stable id. The same id always produces the same result. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Loot")
	FEclipseLootResult RollForStableId(UEclipseLootTable* Table, int32 StableId) const;

	/** Roll a table with an explicit stream, for tests and for scripted events. */
	FEclipseLootResult RollWithStream(UEclipseLootTable* Table, FEclipseDeterministicRandom& Random) const;

	/**
	 * Spawn one pickup per stack in the world. Returns how many actors were spawned.
	 * Server only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Loot")
	int32 SpawnLootStacks(const FVector& Origin, const TArray<FEclipseItemStack>& Stacks, float ScatterRadius, TSubclassOf<AEclipsePickupActor> PickupClass);

	/**
	 * Roll a table and spawn the result. Convenience used by destructible containers and
	 * by mission rewards that need a physical cache.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Loot")
	int32 SpawnLootAt(const FVector& Origin, UEclipseLootTable* Table, int32 StableId, float ScatterRadius, TSubclassOf<AEclipsePickupActor> PickupClass);

	/** Roll a table and add the result to a recipient's inventory. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Loot")
	int32 GrantLootTo(AActor* Recipient, UEclipseLootTable* Table, int32 StableId);

	/** Stable id for an actor, used by containers that live in the level. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Loot")
	static int32 MakeStableId(const AActor* Actor);

	/** Stable id derived from a world-space position, for procedurally placed containers. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Loot")
	static int32 MakeStableIdFromLocation(const FVector& Location);

	/** World seed this subsystem rolls against. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Loot")
	int32 GetWorldSeed() const;

	/** Largest number of pickups a single container may spawn. Guards runaway tables. */
	static constexpr int32 MaxPickupsPerContainer = 24;
};
