// PROJECT ECLIPSE - World value types.
//
// Purpose
//   The weather profile and the world clock snapshot. Weather is published as exactly one
//   profile at a time - there is no second source of truth for fog, wind or damage - and
//   every system that cares about weather reads the same struct.
//
// Contract
//   UEclipseWorldSubsystem publishes a FEclipseWeatherProfile whenever the weather
//   changes, and AEclipseGameState replicates the enum to clients. Nothing else is
//   allowed to decide what the weather is.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "EclipseWorldTypes.generated.h"

/**
 * Everything weather does.
 *
 * Values are absolute rather than multipliers from a base, so a designer can answer "what
 * does a blizzard do" by reading one row of data.
 */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseWeatherProfile
{
	GENERATED_BODY()

	/** Which weather this describes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Weather")
	EEclipseWeather Weather = EEclipseWeather::Clear;

	/** Wind strength in metres per second. Drives wind audio, cloth and particle drift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Weather", meta = (ClampMin = "0.0"))
	float WindStrength = 2.0f;

	/** Fog density used by the exponential height fog. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Weather", meta = (ClampMin = "0.0"))
	float FogDensity = 0.01f;

	/** Ambient temperature in degrees Celsius. Drives the cold/heat status effects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Weather")
	float TemperatureCelsius = 18.0f;

	/** Visible distance in centimetres, used by AI sight and the enemy HUD markers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Weather", meta = (ClampMin = "100.0"))
	float VisibilityCentimetres = 20000.0f;

	/** Damage per second applied to exposed players. Zero for harmless weather. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Weather", meta = (ClampMin = "0.0"))
	float EnvironmentalDamagePerSecond = 0.0f;

	/** Damage school of the environmental damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Weather")
	EEclipseDamageType EnvironmentalDamageType = EEclipseDamageType::Thermal;

	/** Movement speed multiplier while exposed. 1.0 is unaffected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Weather", meta = (ClampMin = "0.1"))
	float MovementSpeedMultiplier = 1.0f;

	/** Aim stability multiplier. Below 1.0 means more spread. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Weather", meta = (ClampMin = "0.1"))
	float AimStabilityMultiplier = 1.0f;

	/** True when the weather damages players who are not sheltered. */
	bool IsHazardous() const
	{
		return EnvironmentalDamagePerSecond > 0.0f;
	}

	/** True when the weather reduces visibility below the clear-air default. */
	bool IsObscuring() const
	{
		return VisibilityCentimetres < 20000.0f;
	}
};

/** A snapshot of the world clock and weather, for saving and for the HUD. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseWorldSnapshot
{
	GENERATED_BODY()

	/** World seed the raid was generated with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|World")
	int32 WorldSeed = 0;

	/** Time of day in hours, in [0, 24). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|World")
	float TimeOfDayHours = 8.0f;

	/** Current weather. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|World")
	EEclipseWeather Weather = EEclipseWeather::Clear;

	/** Seconds the current weather has been running. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|World")
	float WeatherDurationSeconds = 0.0f;
};
