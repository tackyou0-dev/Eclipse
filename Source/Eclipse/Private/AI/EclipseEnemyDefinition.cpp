// PROJECT ECLIPSE - Enemy definition implementation.
//
// Purpose
//   Weak points, weather spawn modifiers and boss phase resolution.

#include "AI/EclipseEnemyDefinition.h"

float UEclipseEnemyDefinition::GetWeakPointMultiplier(FName BoneName) const
{
	if (BoneName.IsNone())
	{
		return 1.0f;
	}

	for (const FEclipseWeakPoint& WeakPoint : WeakPoints)
	{
		if (WeakPoint.IsValidWeakPoint() && WeakPoint.BoneName.IsEqual(BoneName, ENameCase::IgnoreCase))
		{
			return WeakPoint.DamageMultiplier;
		}
	}

	return 1.0f;
}

float UEclipseEnemyDefinition::GetWeatherSpawnMultiplier(EEclipseWeather Weather) const
{
	if (const float* Found = WeatherSpawnModifiers.Find(Weather))
	{
		return FMath::Max(0.0f, *Found);
	}
	return 1.0f;
}

int32 UEclipseEnemyDefinition::GetPhaseIndexForHealth(float HealthFraction) const
{
	if (Phases.Num() == 0)
	{
		return 0;
	}

	const float Fraction = FMath::Clamp(HealthFraction, 0.0f, 1.0f);

	// Highest index whose threshold the boss has already reached. With thresholds
	// 1.0 / 0.75 / 0.5 / 0.25 this yields 0, 1, 2, 3 as health falls.
	int32 Result = 0;
	for (int32 Index = 0; Index < Phases.Num(); ++Index)
	{
		if (Fraction <= Phases[Index].HealthThreshold)
		{
			Result = Index;
		}
	}

	return Result;
}

const FEclipseBossPhase* UEclipseEnemyDefinition::GetPhase(int32 PhaseIndex) const
{
	return Phases.IsValidIndex(PhaseIndex) ? &Phases[PhaseIndex] : nullptr;
}
