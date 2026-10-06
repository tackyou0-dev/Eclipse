// PROJECT ECLIPSE - World subsystem implementation.
//
// Purpose
//   Clock, weather, biomes and weather damage. The tick order matters: time first, then
//   the weather transition, then the damage, then the push to the game state, so a client
//   always sees a consistent clock/weather pair.

#include "World/EclipseWorldSubsystem.h"

#include "Characters/EclipseCharacterBase.h"
#include "Combat/EclipseDamageLibrary.h"
#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseGameState.h"
#include "Core/EclipseLog.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UEclipseWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UEclipseDeveloperSettings* Settings = UEclipseDeveloperSettings::Get();
	WorldSeed = Settings != nullptr ? Settings->WorldSeed : 1;
	TimeOfDayHours = Settings != nullptr ? Settings->RaidStartHour : 8.0f;

	WeatherTable = UEclipseWeatherTable::GetTable();
	WeatherRandom.Reset(FEclipseDeterministicRandom::MixSeed(static_cast<uint32>(WorldSeed), 0x5EEDu));

	WeatherProfile = WeatherTable != nullptr
		? WeatherTable->GetProfile(EEclipseWeather::Clear)
		: FEclipseWeatherProfile();

	CurrentWeather = WeatherProfile.Weather;
	CurrentWeatherDuration = WeatherTable != nullptr ? WeatherTable->WeatherDurationSeconds : 420.0f;
	LastTimeOfDay = EclipseTimeOfDay::FromHour(TimeOfDayHours);
}

void UEclipseWorldSubsystem::Deinitialize()
{
	BiomeVolumes.Reset();
	WeatherTable = nullptr;

	Super::Deinitialize();
}

bool UEclipseWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Game and PIE only: the clock has no meaning in an editor preview of an asset.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UEclipseWorldSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEclipseWorldSubsystem, STATGROUP_Tickables);
}

void UEclipseWorldSubsystem::ApplyRaidSettings(int32 Seed, float StartHour, EEclipseWeather ForcedWeather, bool bFreezeTime)
{
	WorldSeed = Seed;
	TimeOfDayHours = EclipseTimeOfDay::WrapHour(StartHour);
	bFrozen = bFreezeTime;

	WeatherRandom.Reset(FEclipseDeterministicRandom::MixSeed(static_cast<uint32>(WorldSeed), 0x5EEDu));
	WeatherTransitionCount = 0;

	if (!bFrozen)
	{
		PublishWeather(ForcedWeather, WeatherTable != nullptr ? WeatherTable->WeatherDurationSeconds : 420.0f);
	}

	PushToGameState();

	UE_LOG(LogEclipseWorld, Log, TEXT("Raid world applied: seed %d, start %02d:00, weather %s, frozen %s."),
		WorldSeed,
		FMath::FloorToInt(TimeOfDayHours),
		EclipseEnumNames::ToString(CurrentWeather),
		bFrozen ? TEXT("yes") : TEXT("no"));
}

float UEclipseWorldSubsystem::GetRealSecondsPerGameHour() const
{
	const UEclipseDeveloperSettings* Settings = UEclipseDeveloperSettings::Get();
	return Settings != nullptr ? FMath::Max(1.0f, Settings->RealSecondsPerGameHour) : 60.0f;
}

void UEclipseWorldSubsystem::Tick(float DeltaTime)
{
	if (DeltaTime <= 0.0f || UWorld* World = GetWorld(); World == nullptr || !World->IsGameWorld())
	{
		return;
	}

	// Clients do not advance the world; they receive the clock from the game state.
	if (GetWorld()->GetNetMode() == NM_Client || bFrozen)
	{
		return;
	}

	TimeOfDayHours = EclipseTimeOfDay::WrapHour(TimeOfDayHours + DeltaTime / GetRealSecondsPerGameHour());

	const EEclipseTimeOfDay NewBand = EclipseTimeOfDay::FromHour(TimeOfDayHours);
	if (NewBand != LastTimeOfDay)
	{
		LastTimeOfDay = NewBand;
		OnTimeOfDayChanged.Broadcast(NewBand);
	}

	TimeInCurrentWeather += DeltaTime;
	EvaluateWeatherTransition();
	ApplyEnvironmentalDamage(DeltaTime);
	PushToGameState();
}

void UEclipseWorldSubsystem::EvaluateWeatherTransition()
{
	if (TimeInCurrentWeather < CurrentWeatherDuration)
	{
		return;
	}

	const EEclipseBiome Biome = GetBiomeAt(GetWorld() != nullptr && GetWorld()->GetFirstPlayerController() != nullptr
		&& GetWorld()->GetFirstPlayerController()->GetPawn() != nullptr
		? GetWorld()->GetFirstPlayerController()->GetPawn()->GetActorLocation()
		: FVector::ZeroVector);

	if (WeatherTable == nullptr)
	{
		return;
	}

	// The stream is advanced by the number of transitions so far, so weather is
	// reproducible from the seed and the elapsed duration.
	WeatherRandom.Reset(FEclipseDeterministicRandom::MixSeed(static_cast<uint32>(WorldSeed), static_cast<uint32>(WeatherTransitionCount + 1)));

	const EEclipseWeather Next = WeatherTable->PickWeather(Biome, WeatherRandom);
	++WeatherTransitionCount;

	PublishWeather(Next, WeatherTable->WeatherDurationSeconds);
}

void UEclipseWorldSubsystem::ForceWeather(EEclipseWeather Weather, float DurationSeconds)
{
	PublishWeather(Weather, DurationSeconds > 0.0f ? DurationSeconds : CurrentWeatherDuration);
}

void UEclipseWorldSubsystem::PublishWeather(EEclipseWeather Weather, float DurationSeconds)
{
	CurrentWeather = Weather;
	WeatherProfile = WeatherTable != nullptr ? WeatherTable->GetProfile(Weather) : FEclipseWeatherProfile();
	TimeInCurrentWeather = 0.0f;
	CurrentWeatherDuration = FMath::Max(1.0f, DurationSeconds);

	OnWeatherChanged.Broadcast(CurrentWeather);

	if (AEclipseGameState* GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AEclipseGameState>() : nullptr)
	{
		GameState->SetCurrentWeather(CurrentWeather);
	}

	UE_LOG(LogEclipseWorld, Log, TEXT("Weather is now %s for %.0f seconds."),
		EclipseEnumNames::ToString(CurrentWeather), CurrentWeatherDuration);
}

void UEclipseWorldSubsystem::SetTimeOfDayHours(float NewHours)
{
	TimeOfDayHours = EclipseTimeOfDay::WrapHour(NewHours);
	LastTimeOfDay = EclipseTimeOfDay::FromHour(TimeOfDayHours);
	PushToGameState();
}

void UEclipseWorldSubsystem::PushToGameState()
{
	AEclipseGameState* GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AEclipseGameState>() : nullptr;
	if (GameState == nullptr)
	{
		return;
	}

	GameState->SetWorldHours(TimeOfDayHours);

	if (GameState->GetCurrentWeather() != CurrentWeather)
	{
		GameState->SetCurrentWeather(CurrentWeather);
	}
}

void UEclipseWorldSubsystem::RegisterBiomeVolume(AEclipseBiomeVolume* Volume)
{
	if (Volume != nullptr && !BiomeVolumes.Contains(Volume))
	{
		BiomeVolumes.Add(Volume);
	}
}

void UEclipseWorldSubsystem::UnregisterBiomeVolume(AEclipseBiomeVolume* Volume)
{
	BiomeVolumes.Remove(Volume);
}

EEclipseBiome UEclipseWorldSubsystem::GetBiomeAt(const FVector& Location) const
{
	// Smallest containing volume wins, so a designer can nest an interior inside a region.
	const AEclipseBiomeVolume* BestVolume = nullptr;
	float BestSize = TNumericLimits<float>::Max();

	for (const TObjectPtr<AEclipseBiomeVolume>& Volume : BiomeVolumes)
	{
		if (Volume == nullptr || !Volume->ContainsLocation(Location))
		{
			continue;
		}

		const float Size = Volume->GetVolumeSize();
		if (Size < BestSize)
		{
			BestSize = Size;
			BestVolume = Volume;
		}
	}

	return BestVolume != nullptr ? BestVolume->GetBiome() : EEclipseBiome::None;
}

bool UEclipseWorldSubsystem::IsShelteredAt(const FVector& Location) const
{
	// A sheltered volume is explicit shelter: an interior the designer marked.
	for (const TObjectPtr<AEclipseBiomeVolume>& Volume : BiomeVolumes)
	{
		if (Volume != nullptr && Volume->IsSheltered() && Volume->ContainsLocation(Location))
		{
			return true;
		}
	}

	// Otherwise, solid geometry overhead counts: standing under a rock face is shelter.
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(EclipseShelterTrace), false);
	FHitResult Hit;
	const FVector Start = Location;
	const FVector End = Location + FVector(0.0f, 0.0f, ShelterTraceDistance);
	return World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params);
}

void UEclipseWorldSubsystem::ApplyEnvironmentalDamage(float DeltaSeconds)
{
	if (!WeatherProfile.IsHazardous())
	{
		return;
	}

	EnvironmentalDamageAccumulator += DeltaSeconds;
	if (EnvironmentalDamageAccumulator < EnvironmentalDamageInterval)
	{
		return;
	}

	const float Interval = EnvironmentalDamageAccumulator;
	EnvironmentalDamageAccumulator = 0.0f;

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float Damage = WeatherProfile.EnvironmentalDamagePerSecond * Interval;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			continue;
		}

		const AActor* PlayerActor = Cast<AEclipseCharacterBase>(Pawn);
		if (PlayerActor == nullptr)
		{
			continue;
		}

		if (IsShelteredAt(Pawn->GetActorLocation()))
		{
			continue;
		}

		FEclipseDamageContext Context;
		Context.BaseDamage = Damage;
		Context.DamageType = WeatherProfile.EnvironmentalDamageType;
		Context.bIgnoreArmor = true;
		Context.bIgnoreShields = true;
		Context.Instigator = nullptr;
		Context.DamageSourceId = FName(*FString::Printf(TEXT("Weather.%s"), EclipseEnumNames::ToString(CurrentWeather)));
		Context.HitLocation = Pawn->GetActorLocation();

		// Damage goes through the same pipeline as a bullet: resistance still applies, which
		// is what makes a hazmat suit worth wearing in a toxic storm.
		UEclipseDamageLibrary::ApplyDamage(nullptr, Pawn, Context);
	}
}

FEclipseWorldSnapshot UEclipseWorldSubsystem::GetSnapshot() const
{
	FEclipseWorldSnapshot Snapshot;
	Snapshot.WorldSeed = WorldSeed;
	Snapshot.TimeOfDayHours = TimeOfDayHours;
	Snapshot.Weather = CurrentWeather;
	Snapshot.WeatherDurationSeconds = TimeInCurrentWeather;
	return Snapshot;
}

void UEclipseWorldSubsystem::ApplySnapshot(const FEclipseWorldSnapshot& Snapshot)
{
	WorldSeed = Snapshot.WorldSeed != 0 ? Snapshot.WorldSeed : WorldSeed;
	TimeOfDayHours = EclipseTimeOfDay::WrapHour(Snapshot.TimeOfDayHours);
	LastTimeOfDay = EclipseTimeOfDay::FromHour(TimeOfDayHours);

	PublishWeather(Snapshot.Weather, CurrentWeatherDuration);
	TimeInCurrentWeather = FMath::Max(0.0f, Snapshot.WeatherDurationSeconds);
}
