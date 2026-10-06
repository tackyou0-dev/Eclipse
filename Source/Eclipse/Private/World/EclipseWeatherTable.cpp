// PROJECT ECLIPSE - Weather table implementation.
//
// Purpose
//   Weighted weather selection and profile lookup.

#include "World/EclipseWeatherTable.h"

#include "Core/EclipseLog.h"

EEclipseWeather UEclipseWeatherTable::PickWeather(EEclipseBiome Biome, FEclipseDeterministicRandom& Random) const
{
	const FEclipseBiomeWeatherWeights* Row = BiomeWeights.FindByPredicate([Biome](const FEclipseBiomeWeatherWeights& Candidate)
	{
		return Candidate.Biome == Biome;
	});

	if (Row == nullptr)
	{
		return EEclipseWeather::Clear;
	}

	// Enum order, so the pick is a pure function of the stream and the weights.
	TArray<float> Weights;
	TArray<EEclipseWeather> Candidates;
	Weights.Reserve(static_cast<int32>(EEclipseWeather::Count));
	Candidates.Reserve(static_cast<int32>(EEclipseWeather::Count));

	for (int32 Index = 0; Index < static_cast<int32>(EEclipseWeather::Count); ++Index)
	{
		const EEclipseWeather Weather = static_cast<EEclipseWeather>(Index);
		const float Weight = Row->Weights.Contains(Weather) ? FMath::Max(0.0f, Row->Weights[Weather]) : 0.0f;

		Weights.Add(Weight);
		Candidates.Add(Weather);
	}

	const int32 Picked = FEclipseDeterministicRandom::WeightedPick(Weights, Random);
	if (Picked == INDEX_NONE)
	{
		return EEclipseWeather::Clear;
	}

	return Candidates[Picked];
}

FEclipseWeatherProfile UEclipseWeatherTable::GetProfile(EEclipseWeather Weather) const
{
	if (const FEclipseWeatherProfile* Found = Profiles.Find(Weather))
	{
		return *Found;
	}

	// An unconfigured weather still has to behave: a clear, harmless profile is the safest
	// default and it makes the missing row visible in the log rather than in gameplay.
	UE_LOG(LogEclipseWorld, Warning, TEXT("No weather profile configured for %s; using clear weather."),
		EclipseEnumNames::ToString(Weather));

	FEclipseWeatherProfile Fallback;
	Fallback.Weather = EEclipseWeather::Clear;
	return Fallback;
}

bool UEclipseWeatherTable::ValidateTable(TArray<FString>& OutErrors) const
{
	const int32 FirstError = OutErrors.Num();

	if (BiomeWeights.Num() == 0)
	{
		OutErrors.Add(TEXT("Weather table has no biome rows."));
	}

	for (const FEclipseBiomeWeatherWeights& Row : BiomeWeights)
	{
		float Total = 0.0f;
		for (const TPair<EEclipseWeather, float>& Pair : Row.Weights)
		{
			Total += FMath::Max(0.0f, Pair.Value);
		}

		if (Total <= 0.0f)
		{
			OutErrors.Add(FString::Printf(TEXT("Biome '%s' has no positive weather weights."),
				EclipseEnumNames::ToString(Row.Biome)));
		}
	}

	if (Profiles.Num() == 0)
	{
		OutErrors.Add(TEXT("Weather table has no profiles."));
	}

	if (WeatherDurationSeconds < 10.0f)
	{
		OutErrors.Add(TEXT("WeatherDurationSeconds is too short to be playable."));
	}

	return OutErrors.Num() == FirstError;
}

UEclipseWeatherTable* UEclipseWeatherTable::GetTable()
{
	static TWeakObjectPtr<UEclipseWeatherTable> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	const FSoftObjectPath TablePath(TEXT("/Game/Eclipse/Data/World/DA_WeatherTable.DA_WeatherTable"));
	UEclipseWeatherTable* Table = Cast<UEclipseWeatherTable>(TablePath.TryLoad());
	if (Table == nullptr)
	{
		UE_LOG(LogEclipseWorld, Warning, TEXT("Weather table could not be loaded from '%s'."), *TablePath.ToString());
		return nullptr;
	}

	Cached = Table;
	return Table;
}
