// PROJECT ECLIPSE - Enemy spawner.
//
// Purpose
//   Places enemies in the world without a hand-placed pawn for every one of them. A
//   spawner owns a pool of UEclipseEnemyDefinition assets and spawns from it on an
//   interval, weighted by the definition's threat weight, the biome it is standing in and
//   the weather: the Warden's drones do not appear in calm weather, and the Buried do not
//   appear in daylight.
//
// Authority
//   Server only. The spawner ticks on the server (and standalone), spawns actors with
//   authority and registers them with the AI subsystem, which budgets the global agent
//   count. A client never spawns an enemy.
//
// Determinism
//   Selection uses FEclipseDeterministicRandom seeded from the raid seed and the spawner's
//   index, so the same raid seed produces the same waves. That is a test contract: the
//   reference model mirrors the weighting.

#pragma once

#include "AI/EclipseEnemyDefinition.h"
#include "Core/EclipseTypes.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EclipseEnemySpawner.generated.h"

class AEclipseEnemyCharacter;
class USphereComponent;

/**
 * A point in the world that produces enemies.
 *
 * Spawners are placed by level designers and by the procedural pass; mission script can
 * call SpawnWave to force a fight, which is what the defence objectives use.
 */
UCLASS()
class ECLIPSE_API AEclipseEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEclipseEnemySpawner();

	/** AActor: starts the spawn timer on the server. */
	virtual void BeginPlay() override;

	/** AActor: stops the timer. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** True when this spawner is allowed to produce enemies right now. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Spawning")
	bool CanSpawnNow() const;

	/**
	 * Spawn one enemy chosen from the pool. Returns null when nothing can be spawned:
	 * disabled, over budget, out of biome, or the navigation system has no floor here.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Spawning")
	AEclipseEnemyCharacter* SpawnOne(UEclipseEnemyDefinition* DefinitionOverride = nullptr);

	/** Spawn up to Count enemies, stopping at the first refusal. Returns how many spawned. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Spawning")
	int32 SpawnWave(int32 Count);

	/** How many enemies from this spawner are still alive. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Spawning")
	int32 GetAliveSpawnedCount() const;

	/** Choose a definition from the pool for the current biome, weather and hour. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Spawning")
	UEclipseEnemyDefinition* SelectDefinition() const;

	/** Weight of a definition for the current world state. Zero means "not eligible". */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Spawning")
	float ScoreDefinitionForWorldState(const UEclipseEnemyDefinition* Definition) const;

	/** True when this spawner is enabled. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Spawning")
	bool IsSpawningEnabled() const { return bSpawningEnabled; }

	/** Enable or disable the spawner (encounter script and debug commands use this). */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Spawning")
	void SetSpawningEnabled(bool bEnabled);

	/** Definitions this spawner may spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Spawning")
	TArray<TObjectPtr<UEclipseEnemyDefinition>> EnemyPool;

	/** How many enemies this spawner keeps alive at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Spawning", meta = (ClampMin = "1", ClampMax = "32"))
	int32 MaxAliveFromThisSpawner = 3;

	/** Seconds between spawn attempts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Spawning", meta = (ClampMin = "1.0"))
	float SpawnIntervalSeconds = 30.0f;

	/** Radius, in centimetres, around the spawner that spawn points are drawn from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Spawning", meta = (ClampMin = "100.0"))
	float SpawnRadiusCentimetres = 2000.0f;

	/** Delay before the first spawn attempt, so a raid does not start inside a fight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Spawning", meta = (ClampMin = "0.0"))
	float InitialDelaySeconds = 5.0f;

	/** How far above the spawn point the navigation projection starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Spawning", meta = (ClampMin = "100.0"))
	float SpawnTraceHeightCentimetres = 600.0f;

	/** The character class spawned. Defaults to AEclipseEnemyCharacter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Spawning")
	TSoftClassPtr<AEclipseEnemyCharacter> CharacterClass;

	/** Interval multiplier while the weather is hazardous. Below 1.0 spawns faster. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Spawning", meta = (ClampMin = "0.1"))
	float HazardousWeatherIntervalScale = 0.5f;

protected:
	/** Spawn timer callback. */
	void HandleSpawnTimer();

	/** Project a random point in the spawn radius onto the navigation mesh. */
	bool FindSpawnLocation(FVector& OutLocation) const;

	/** Drop dead and destroyed entries from SpawnedAgents. */
	void PruneSpawnedAgents();

	/** Sphere used as the designer-facing bounds handle in the level. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Spawning")
	TObjectPtr<USphereComponent> BoundsComponent;

	/** Enemies this spawner produced and that have not been pruned yet. */
	UPROPERTY()
	TArray<TWeakObjectPtr<AEclipseEnemyCharacter>> SpawnedAgents;

	/** True while the spawn timer is running. */
	bool bSpawningEnabled = true;

	/** Number of spawn attempts made, mixed into the deterministic stream. */
	int32 SpawnAttemptCount = 0;

	/** Timer handle for the spawn loop. */
	FTimerHandle SpawnTimerHandle;
};
