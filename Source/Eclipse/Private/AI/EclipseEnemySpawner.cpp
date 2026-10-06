// PROJECT ECLIPSE - Enemy spawner implementation.
//
// Purpose
//   Weather-, biome- and hour-aware enemy placement. The weighting is deliberately simple
//   and deterministic: score each definition, pick with the seed, then hand the choice to
//   the AI subsystem's budget. No definition is ever spawned twice by accident because
//   the alive count is checked before the choice is used.

#include "AI/EclipseEnemySpawner.h"

#include "AI/EclipseAISubsystem.h"
#include "Characters/EclipseEnemyCharacter.h"
#include "Components/SphereComponent.h"
#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseLog.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "World/EclipseWorldSubsystem.h"

AEclipseEnemySpawner::AEclipseEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	BoundsComponent = CreateDefaultSubobject<USphereComponent>(TEXT("BoundsComponent"));
	BoundsComponent->SetSphereRadius(SpawnRadiusCentimetres);
	BoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundsComponent->SetGenerateOverlapEvents(false);
	RootComponent = BoundsComponent;
}

void AEclipseEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		// Clients never spawn: the server's actors replicate in.
		return;
	}

	if (BoundsComponent != nullptr)
	{
		BoundsComponent->SetSphereRadius(SpawnRadiusCentimetres);
	}

	if (!bSpawningEnabled || EnemyPool.Num() == 0)
	{
		return;
	}

	const float FirstDelay = FMath::Max(0.0f, InitialDelaySeconds);
	World->GetTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&AEclipseEnemySpawner::HandleSpawnTimer,
		FMath::Max(1.0f, SpawnIntervalSeconds),
		true,
		FirstDelay);
}

void AEclipseEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}

	SpawnedAgents.Reset();

	Super::EndPlay(EndPlayReason);
}

void AEclipseEnemySpawner::SetSpawningEnabled(bool bEnabled)
{
	if (bSpawningEnabled == bEnabled)
	{
		return;
	}

	bSpawningEnabled = bEnabled;

	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		return;
	}

	if (bSpawningEnabled)
	{
		World->GetTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&AEclipseEnemySpawner::HandleSpawnTimer,
			FMath::Max(1.0f, SpawnIntervalSeconds),
			true,
			FMath::Max(0.0f, InitialDelaySeconds));
	}
	else
	{
		World->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
}

int32 AEclipseEnemySpawner::GetAliveSpawnedCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AEclipseEnemyCharacter>& Agent : SpawnedAgents)
	{
		if (Agent.IsValid())
		{
			++Count;
		}
	}

	return Count;
}

void AEclipseEnemySpawner::PruneSpawnedAgents()
{
	SpawnedAgents.RemoveAll([](const TWeakObjectPtr<AEclipseEnemyCharacter>& Agent)
	{
		return !Agent.IsValid();
	});
}

float AEclipseEnemySpawner::ScoreDefinitionForWorldState(const UEclipseEnemyDefinition* Definition) const
{
	if (Definition == nullptr || !Definition->IsValidDefinition())
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	const UEclipseWorldSubsystem* WorldSubsystem = World != nullptr ? World->GetSubsystem<UEclipseWorldSubsystem>() : nullptr;

	// Weather is the strongest gate: a definition with a zero multiplier for the current
	// weather simply does not exist for that weather.
	float Weight = FMath::Max(0.1f, Definition->ThreatWeight);

	if (WorldSubsystem != nullptr)
	{
		const EEclipseWeather Weather = WorldSubsystem->GetCurrentWeather();
		const float WeatherMultiplier = Definition->GetWeatherSpawnMultiplier(Weather);
		if (WeatherMultiplier <= 0.0f)
		{
			return 0.0f;
		}

		Weight *= WeatherMultiplier;

		if (Definition->bNightOnly && !WorldSubsystem->IsNight())
		{
			return 0.0f;
		}

		// Biome mismatch is a soft gate: a definition that lists biomes is expected to
		// spawn in them, so it keeps a small chance elsewhere rather than being banned.
		if (Definition->Biomes.Num() > 0)
		{
			const EEclipseBiome Biome = WorldSubsystem->GetBiomeAt(GetActorLocation());
			Weight *= Definition->Biomes.Contains(Biome) ? 1.0f : 0.15f;
		}
	}
	else if (Definition->bNightOnly)
	{
		return 0.0f;
	}

	return Weight;
}

UEclipseEnemyDefinition* AEclipseEnemySpawner::SelectDefinition() const
{
	TArray<UEclipseEnemyDefinition*> Eligible;
	TArray<float> Weights;

	for (const TObjectPtr<UEclipseEnemyDefinition>& Definition : EnemyPool)
	{
		const float Weight = ScoreDefinitionForWorldState(Definition);
		if (Weight > 0.0f)
		{
			Eligible.Add(Definition);
			Weights.Add(Weight);
		}
	}

	if (Eligible.Num() == 0)
	{
		return nullptr;
	}

	const UEclipseDeveloperSettings* Settings = UEclipseDeveloperSettings::Get();
	const UWorld* World = GetWorld();
	const UEclipseWorldSubsystem* WorldSubsystem = World != nullptr ? World->GetSubsystem<UEclipseWorldSubsystem>() : nullptr;
	const uint32 Seed = WorldSubsystem != nullptr ? WorldSubsystem->GetWorldSeed() : (Settings != nullptr ? Settings->WorldSeed : 1u);

	// The stream is keyed by the spawner's identity and the attempt counter: two spawners
	// with the same pool do not produce identical waves, and a save/load that replays the
	// same attempt count gets the same pick.
	const uint32 StableId = FEclipseDeterministicRandom::StableIdFromString(GetName());
	FEclipseDeterministicRandom Random(FEclipseDeterministicRandom::MixSeed(Seed, StableId + static_cast<uint32>(SpawnAttemptCount)));

	const int32 Picked = FEclipseDeterministicRandom::WeightedPick(Weights, Random);
	return Eligible.IsValidIndex(Picked) ? Eligible[Picked] : Eligible[0];
}

bool AEclipseEnemySpawner::CanSpawnNow() const
{
	if (!bSpawningEnabled || EnemyPool.Num() == 0)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	return GetAliveSpawnedCount() < MaxAliveFromThisSpawner;
}

bool AEclipseEnemySpawner::FindSpawnLocation(FVector& OutLocation) const
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	// Draw from a fresh stream per attempt so successive attempts do not march around the
	// circle in the same pattern every raid.
	FEclipseDeterministicRandom PlacementRandom(FEclipseDeterministicRandom::MixSeed(
		static_cast<uint32>(SpawnAttemptCount + 1),
		FEclipseDeterministicRandom::StableIdFromString(GetActorLocation().ToString())));

	const float Angle = PlacementRandom.Range(0.0f, 2.0f * PI);
	const float Radius = PlacementRandom.Range(SpawnRadiusCentimetres * 0.35f, SpawnRadiusCentimetres);
	const FVector Candidate = GetActorLocation() + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, SpawnTraceHeightCentimetres);

	UNavigationSystemV1* Navigation = UNavigationSystemV1::GetCurrent(World);
	if (Navigation != nullptr)
	{
		FNavLocation Projected;
		if (Navigation->ProjectPointToNavigation(Candidate, Projected, FVector(400.0f, 400.0f, SpawnTraceHeightCentimetres)))
		{
			OutLocation = Projected.Location;
			return true;
		}
	}

	// No navigation system (or no mesh under the point): fall back to a ground trace so a
	// greybox map still spawns enemies.
	FHitResult Hit;
	const FVector Start = Candidate;
	const FVector End = Candidate - FVector(0.0f, 0.0f, SpawnTraceHeightCentimetres * 4.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EclipseSpawnerTrace), false, this);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		OutLocation = Hit.ImpactPoint + FVector(0.0f, 0.0f, 50.0f);
		return true;
	}

	return false;
}

AEclipseEnemyCharacter* AEclipseEnemySpawner::SpawnOne(UEclipseEnemyDefinition* DefinitionOverride)
{
	if (!CanSpawnNow())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	UEclipseEnemyDefinition* Definition = DefinitionOverride != nullptr ? DefinitionOverride : SelectDefinition();
	if (World == nullptr || Definition == nullptr)
	{
		return nullptr;
	}

	// The AI subsystem owns the global budget. Asking it before spawning is what stops a
	// level full of spawners from producing an unbounded horde.
	UEclipseAISubsystem* AISubsystem = World->GetSubsystem<UEclipseAISubsystem>();
	if (AISubsystem != nullptr && !AISubsystem->CanAffordAgent(Definition->ThreatWeight))
	{
		UE_LOG(LogEclipseAI, Verbose, TEXT("%s is over the AI budget; skipping spawn of %s."),
			*GetName(), *Definition->EnemyId.ToString());
		return nullptr;
	}

	FVector SpawnLocation;
	if (!FindSpawnLocation(SpawnLocation))
	{
		UE_LOG(LogEclipseAI, Verbose, TEXT("%s could not find a floor to spawn on."), *GetName());
		return nullptr;
	}

	UClass* Class = CharacterClass.IsNull() ? AEclipseEnemyCharacter::StaticClass() : CharacterClass.LoadSynchronous();
	if (Class == nullptr)
	{
		UE_LOG(LogEclipseAI, Warning, TEXT("%s has an unloadable character class; using the default enemy."), *GetName());
		Class = AEclipseEnemyCharacter::StaticClass();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.Owner = this;

	const FRotator SpawnRotation(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);
	AEclipseEnemyCharacter* Spawned = World->SpawnActor<AEclipseEnemyCharacter>(Class, SpawnLocation, SpawnRotation, SpawnParams);
	if (Spawned == nullptr)
	{
		UE_LOG(LogEclipseAI, Warning, TEXT("%s failed to spawn %s."), *GetName(), *Definition->EnemyId.ToString());
		return nullptr;
	}

	Spawned->InitializeFromDefinition(Definition);

	if (AISubsystem != nullptr)
	{
		AISubsystem->RegisterAgent(Spawned, Definition);
	}

	SpawnedAgents.Add(Spawned);
	++SpawnAttemptCount;

	UE_LOG(LogEclipseAI, Log, TEXT("%s spawned %s at %s (alive from this spawner: %d)."),
		*GetName(), *Definition->EnemyId.ToString(), *SpawnLocation.ToCompactString(), GetAliveSpawnedCount());

	return Spawned;
}

int32 AEclipseEnemySpawner::SpawnWave(int32 Count)
{
	int32 Spawned = 0;
	for (int32 Index = 0; Index < FMath::Max(0, Count); ++Index)
	{
		if (SpawnOne() == nullptr)
		{
			break;
		}

		++Spawned;
	}

	return Spawned;
}

void AEclipseEnemySpawner::HandleSpawnTimer()
{
	PruneSpawnedAgents();

	if (!CanSpawnNow())
	{
		return;
	}

	UWorld* World = GetWorld();
	const UEclipseWorldSubsystem* WorldSubsystem = World != nullptr ? World->GetSubsystem<UEclipseWorldSubsystem>() : nullptr;

	// Hazardous weather accelerates the spawn loop; the timer interval itself is left
	// alone and the extra attempts come from this timer re-arming itself.
	if (WorldSubsystem != nullptr && WorldSubsystem->GetWeatherProfile().IsHazardous() && World != nullptr)
	{
		World->GetTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&AEclipseEnemySpawner::HandleSpawnTimer,
			FMath::Max(1.0f, SpawnIntervalSeconds * HazardousWeatherIntervalScale),
			true,
			FMath::Max(1.0f, SpawnIntervalSeconds * HazardousWeatherIntervalScale));
	}

	SpawnOne();
}
