// PROJECT ECLIPSE - World subsystem.
//
// Purpose
//   The world clock, the weather and the biome lookup. Everything that needs to know what
//   time it is, what the sky is doing or which region the player is standing in asks this
//   subsystem, and it is the only place that advances the clock.
//
// Authority
//   Server-authoritative. The clock and weather are pushed to AEclipseGameState, which
//   replicates them; a client's world subsystem is a mirror that never advances time.
//
// Environmental damage
//   Weather damage is real damage: it goes through UEclipseDamageLibrary on an interval,
//   ignoring armour but respecting resistance, and it stops inside a sheltered biome
//   volume or under solid geometry.

#pragma once

#include "Core/EclipseTypes.h"
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/EclipseBiomeVolume.h"
#include "World/EclipseWeatherTable.h"
#include "World/EclipseWorldTypes.h"
#include "EclipseWorldSubsystem.generated.h"

/** Fired when the time-of-day band changes. */
DECLARE_MULTICAST_DELEGATE_OneParam(FEclipseTimeOfDayChanged, EEclipseTimeOfDay /*NewBand*/);

/**
 * World simulation for one level.
 *
 * Ticks on the world, so it exists in the editor preview and in a PIE session; all
 * mutation is guarded by an authority check.
 */
UCLASS()
class ECLIPSE_API UEclipseWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** UTickableWorldSubsystem. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** UTickableWorldSubsystem. */
	virtual void Deinitialize() override;

	/** UTickableWorldSubsystem: advance the clock and the weather. */
	virtual void Tick(float DeltaTime) override;

	/** FTickableGameObject: fixed stat id so the tick is identifiable in a profile. */
	virtual TStatId GetStatId() const override;

	/** True when the subsystem has a world to simulate. */
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// ------------------------------------------------------------------
	// Raid setup
	// ------------------------------------------------------------------

	/**
	 * Apply the raid's seed, start hour and forced weather. Called by the game mode during
	 * InitGame, which is what makes the raid reproducible from its settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|World")
	void ApplyRaidSettings(int32 Seed, float StartHour, EEclipseWeather ForcedWeather, bool bFreezeTime);

	/** Seed the raid was generated with. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	int32 GetWorldSeed() const { return WorldSeed; }

	// ------------------------------------------------------------------
	// Clock
	// ------------------------------------------------------------------

	/** Current hour, in [0, 24). */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	float GetTimeOfDayHours() const { return TimeOfDayHours; }

	/** Current time-of-day band. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	EEclipseTimeOfDay GetTimeOfDay() const { return EclipseTimeOfDay::FromHour(TimeOfDayHours); }

	/** True between 21:00 and 05:00. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	bool IsNight() const { return GetTimeOfDay() == EEclipseTimeOfDay::Night; }

	/** Jump the clock. Used by debug commands and by mission scripted starts. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|World")
	void SetTimeOfDayHours(float NewHours);

	/** Real seconds one in-game hour takes. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	float GetRealSecondsPerGameHour() const;

	// ------------------------------------------------------------------
	// Weather
	// ------------------------------------------------------------------

	/** Weather published right now. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	EEclipseWeather GetCurrentWeather() const { return CurrentWeather; }

	/** The published profile. Never changes while the weather holds. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	const FEclipseWeatherProfile& GetWeatherProfile() const { return WeatherProfile; }

	/** Force a weather state for a duration. Zero means "until the next transition". */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|World")
	void ForceWeather(EEclipseWeather Weather, float DurationSeconds);

	/** Change the weather if it has run its course. Called by Tick. */
	void EvaluateWeatherTransition();

	/** Fired when the weather changes. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|World")
	FEclipseWeatherChangedSignature OnWeatherChanged;

	/** Fired when the time-of-day band changes. */
	FEclipseTimeOfDayChanged OnTimeOfDayChanged;

	// ------------------------------------------------------------------
	// Biomes
	// ------------------------------------------------------------------

	/** Biome at a world position. None when no volume contains it. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|World")
	EEclipseBiome GetBiomeAt(const FVector& Location) const;

	/** True when a position is sheltered from the weather. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|World")
	bool IsShelteredAt(const FVector& Location) const;

	/** Volumes register themselves so the lookup does not scan the level every call. */
	void RegisterBiomeVolume(AEclipseBiomeVolume* Volume);

	/** Remove a volume. */
	void UnregisterBiomeVolume(AEclipseBiomeVolume* Volume);

	// ------------------------------------------------------------------
	// Save support
	// ------------------------------------------------------------------

	/** Capture the clock and weather for a save file. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	FEclipseWorldSnapshot GetSnapshot() const;

	/** Restore a snapshot. Used when a save is loaded. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|World")
	void ApplySnapshot(const FEclipseWorldSnapshot& Snapshot);

	/** Seconds between environmental damage applications. */
	static constexpr float EnvironmentalDamageInterval = 1.0f;

	/** How far up the shelter trace looks, in centimetres. */
	static constexpr float ShelterTraceDistance = 800.0f;

protected:
	/** Apply weather damage to exposed players. */
	void ApplyEnvironmentalDamage(float DeltaSeconds);

	/** Push the clock and weather to the replicated game state. */
	void PushToGameState();

	/** Take the profile for a weather state from the table and publish it. */
	void PublishWeather(EEclipseWeather Weather, float DurationSeconds);

	/** Current world seed. */
	int32 WorldSeed = 0;

	/** Current hour. */
	float TimeOfDayHours = 8.0f;

	/** True when the clock is frozen (tests, cutscenes). */
	bool bFrozen = false;

	/** Published weather. */
	EEclipseWeather CurrentWeather = EEclipseWeather::Clear;

	/** Published profile. */
	FEclipseWeatherProfile WeatherProfile;

	/** Seconds the current weather has been running. */
	float TimeInCurrentWeather = 0.0f;

	/** How long the current weather lasts. */
	float CurrentWeatherDuration = 420.0f;

	/** Weather table, loaded on first use. */
	UPROPERTY()
	TObjectPtr<UEclipseWeatherTable> WeatherTable = nullptr;

	/** Deterministic stream for weather selection. */
	FEclipseDeterministicRandom WeatherRandom;

	/** How many transitions have happened, mixed into the stream so a save can resume it. */
	int32 WeatherTransitionCount = 0;

	/** Registered biome volumes. */
	UPROPERTY()
	TArray<TObjectPtr<AEclipseBiomeVolume>> BiomeVolumes;

	/** Seconds accumulated towards the next environmental damage application. */
	float EnvironmentalDamageAccumulator = 0.0f;

	/** Last published time-of-day band, so band changes are broadcast once. */
	EEclipseTimeOfDay LastTimeOfDay = EEclipseTimeOfDay::Day;
};
