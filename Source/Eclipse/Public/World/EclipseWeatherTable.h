// PROJECT ECLIPSE - Weather table.
//
// Purpose
//   Which weather can happen where, how likely each one is, and what each one does. The
//   table is a data asset so that a designer can add a new weather state without a code
//   change, and so that the content validator can check that every biome has at least one
//   possible weather.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseDeterministicRandom.h"
#include "Core/EclipseTypes.h"
#include "Engine/DataAsset.h"
#include "World/EclipseWorldTypes.h"
#include "EclipseWeatherTable.generated.h"

/** Weather weights for one biome. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseBiomeWeatherWeights
{
	GENERATED_BODY()

	/** Biome the weights apply to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Weather")
	EEclipseBiome Biome = EEclipseBiome::None;

	/** Relative chance per weather type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Weather")
	TMap<EEclipseWeather, float> Weights;
};

/**
 * Weather selection and profiles.
 *
 * Selection is deterministic: the subsystem seeds a stream from the world seed and the
 * number of transitions so far, so a raid's weather sequence is reproducible.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseWeatherTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Weather chances per biome. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weather")
	TArray<FEclipseBiomeWeatherWeights> BiomeWeights;

	/** Profile per weather state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weather")
	TMap<EEclipseWeather, FEclipseWeatherProfile> Profiles;

	/** Seconds the weather holds before another transition is considered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weather", meta = (ClampMin = "10.0"))
	float WeatherDurationSeconds = 420.0f;

	/**
	 * Pick the next weather for a biome. Falls back to Clear when the biome has no
	 * weights, which is the correct behaviour for an interior or an unconfigured zone.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weather")
	EEclipseWeather PickWeather(EEclipseBiome Biome, FEclipseDeterministicRandom& Random) const;

	/** Profile for a weather state. Returns the Clear profile when unconfigured. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weather")
	FEclipseWeatherProfile GetProfile(EEclipseWeather Weather) const;

	/** True when every biome the game uses has at least one possible weather. */
	bool ValidateTable(TArray<FString>& OutErrors) const;

	/** Static accessor for the shipped table. */
	static UEclipseWeatherTable* GetTable();
};
