// PROJECT ECLIPSE - Audio subsystem implementation.
//
// Purpose
//   Audio state resolution and one-shot playback. Weather and time of day are combined
//   here, and nowhere else, so a designer changing the storm's wind gain changes it for
//   every map.

#include "Audio/EclipseAudioSubsystem.h"

#include "Components/AudioComponent.h"
#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseLog.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundMix.h"
#include "World/EclipseWorldSubsystem.h"

void UEclipseAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Authored defaults. They live here (rather than in a constructor) so a Blueprint
	// subclass of the subsystem can override them without touching code.
	if (WeatherStates.Num() == 0)
	{
		FEclipseAudioState Clear;
		Clear.AmbienceId = TEXT("Ambience.Clear");
		Clear.WindGain = 0.2f;
		Clear.ReverbWetLevel = 0.1f;
		WeatherStates.Add(EEclipseWeather::Clear, Clear);

		FEclipseAudioState Rain;
		Rain.AmbienceId = TEXT("Ambience.Rain");
		Rain.WindGain = 0.8f;
		Rain.ReverbWetLevel = 0.35f;
		WeatherStates.Add(EEclipseWeather::Rain, Rain);

		FEclipseAudioState Storm;
		Storm.AmbienceId = TEXT("Ambience.Storm");
		Storm.WindGain = 1.2f;
		Storm.ReverbWetLevel = 0.55f;
		WeatherStates.Add(EEclipseWeather::Storm, Storm);

		FEclipseAudioState Sandstorm;
		Sandstorm.AmbienceId = TEXT("Ambience.Sandstorm");
		Sandstorm.WindGain = 0.9f;
		Sandstorm.ReverbWetLevel = 0.3f;
		WeatherStates.Add(EEclipseWeather::Sandstorm, Sandstorm);

		FEclipseAudioState Blizzard;
		Blizzard.AmbienceId = TEXT("Ambience.Blizzard");
		Blizzard.WindGain = 1.1f;
		Blizzard.ReverbWetLevel = 0.5f;
		WeatherStates.Add(EEclipseWeather::Blizzard, Blizzard);

		FEclipseAudioState ToxicStorm;
		ToxicStorm.AmbienceId = TEXT("Ambience.ToxicStorm");
		ToxicStorm.WindGain = 0.7f;
		ToxicStorm.ReverbWetLevel = 0.4f;
		WeatherStates.Add(EEclipseWeather::ToxicStorm, ToxicStorm);

		FEclipseAudioState ElectromagneticStorm;
		ElectromagneticStorm.AmbienceId = TEXT("Ambience.EMStorm");
		ElectromagneticStorm.WindGain = 1.0f;
		ElectromagneticStorm.ReverbWetLevel = 0.45f;
		WeatherStates.Add(EEclipseWeather::ElectromagneticStorm, ElectromagneticStorm);
	}

	if (TimeOfDayStates.Num() == 0)
	{
		FEclipseAudioState Dawn;
		Dawn.MusicIntensity = 0.1f;
		Dawn.WindGain = 0.1f;
		TimeOfDayStates.Add(EEclipseTimeOfDay::Dawn, Dawn);

		FEclipseAudioState Day;
		Day.MusicIntensity = 0.0f;
		Day.WindGain = 0.15f;
		TimeOfDayStates.Add(EEclipseTimeOfDay::Day, Day);

		FEclipseAudioState Dusk;
		Dusk.MusicIntensity = 0.25f;
		Dusk.WindGain = 0.2f;
		TimeOfDayStates.Add(EEclipseTimeOfDay::Dusk, Dusk);

		FEclipseAudioState Night;
		Night.MusicIntensity = 0.4f;
		Night.WindGain = 0.25f;
		Night.ReverbWetLevel = 0.2f;
		TimeOfDayStates.Add(EEclipseTimeOfDay::Night, Night);
	}

	ApplyMasterVolume();
	RefreshAudioState();
}

void UEclipseAudioSubsystem::Deinitialize()
{
	WeatherStates.Reset();
	TimeOfDayStates.Reset();
	SurfaceSounds.Reset();
	MasterSoundMix.Reset();

	Super::Deinitialize();
}

TStatId UEclipseAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEclipseAudioSubsystem, STATGROUP_Tickables);
}

float UEclipseAudioSubsystem::GetMasterVolumeScale() const
{
	const UEclipseDeveloperSettings* Settings = UEclipseDeveloperSettings::Get();
	return Settings != nullptr ? FMath::Clamp(Settings->MasterVolumeScale, 0.0f, 2.0f) : 1.0f;
}

void UEclipseAudioSubsystem::ApplyMasterVolume()
{
	UWorld* World = GetWorld();
	if (World == nullptr || MasterSoundMix.IsNull())
	{
		return;
	}

	USoundMix* Mix = MasterSoundMix.LoadSynchronous();
	if (Mix == nullptr)
	{
		return;
	}

	// The master volume is a gain on the master sound class, applied through the mix so
	// that it does not fight with per-source attenuation.
	UGameplayStatics::SetSoundMixClassOverride(World, Mix, nullptr, GetMasterVolumeScale(), 1.0f, 0.25f, true);
	UGameplayStatics::PushSoundMixModifier(World, Mix);
}

FEclipseAudioState UEclipseAudioSubsystem::ResolveAudioState(EEclipseWeather Weather, EEclipseTimeOfDay TimeOfDay) const
{
	FEclipseAudioState Resolved;

	if (const FEclipseAudioState* WeatherState = WeatherStates.Find(Weather))
	{
		Resolved.AmbienceId = WeatherState->AmbienceId;
		Resolved.WindGain = WeatherState->WindGain;
		Resolved.ReverbWetLevel = WeatherState->ReverbWetLevel;
	}

	if (const FEclipseAudioState* TimeState = TimeOfDayStates.Find(TimeOfDay))
	{
		// Music is driven by the time of day; the weather's own music value is a floor, so
		// a storm at noon is still more tense than a clear noon.
		Resolved.MusicIntensity = FMath::Max(TimeState->MusicIntensity, Resolved.MusicIntensity);
		Resolved.WindGain = FMath::Max(Resolved.WindGain, TimeState->WindGain);
		Resolved.ReverbWetLevel = FMath::Max(Resolved.ReverbWetLevel, TimeState->ReverbWetLevel);
	}

	return Resolved;
}

void UEclipseAudioSubsystem::RefreshAudioState()
{
	UWorld* World = GetWorld();
	const UEclipseWorldSubsystem* WorldSubsystem = World != nullptr ? World->GetSubsystem<UEclipseWorldSubsystem>() : nullptr;

	const FEclipseAudioState Resolved = WorldSubsystem != nullptr
		? ResolveAudioState(
			WorldSubsystem->GetCurrentWeather(),
			EclipseTimeOfDay::FromHour(WorldSubsystem->GetTimeOfDayHours()))
		: FEclipseAudioState();

	const bool bChanged = Resolved.AmbienceId != CurrentState.AmbienceId
		|| !FMath::IsNearlyEqual(Resolved.MusicIntensity, CurrentState.MusicIntensity)
		|| !FMath::IsNearlyEqual(Resolved.WindGain, CurrentState.WindGain)
		|| !FMath::IsNearlyEqual(Resolved.ReverbWetLevel, CurrentState.ReverbWetLevel);

	if (!bChanged)
	{
		return;
	}

	CurrentState = Resolved;
	OnAudioStateChanged.Broadcast(CurrentState);

	UE_LOG(LogEclipsePresentation, Verbose, TEXT("Audio state: %s, music %.2f, wind %.2f, reverb %.2f."),
		*CurrentState.AmbienceId.ToString(), CurrentState.MusicIntensity, CurrentState.WindGain, CurrentState.ReverbWetLevel);
}

void UEclipseAudioSubsystem::OverrideAudioState(const FEclipseAudioState& State, float DurationSeconds)
{
	if (!HasOverride())
	{
		OverrideReturnState = CurrentState;
	}

	CurrentState = State;
	OverrideRemainingSeconds = FMath::Max(0.1f, DurationSeconds);
	OnAudioStateChanged.Broadcast(CurrentState);
}

void UEclipseAudioSubsystem::ClearOverride()
{
	if (!HasOverride())
	{
		return;
	}

	OverrideRemainingSeconds = 0.0f;
	CurrentState = OverrideReturnState;
	OnAudioStateChanged.Broadcast(CurrentState);
}

void UEclipseAudioSubsystem::Tick(float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	if (HasOverride())
	{
		OverrideRemainingSeconds = FMath::Max(0.0f, OverrideRemainingSeconds - DeltaTime);

		if (OverrideRemainingSeconds <= 0.0f)
		{
			// Clearing the override goes back to whatever the world state says, not to the
			// state that was current when the override started: the weather may have moved
			// on while the sequence played.
			OverrideRemainingSeconds = 0.0f;
			RefreshAudioState();
		}

		return;
	}

	RefreshAccumulator += DeltaTime;
	if (RefreshAccumulator < StateRefreshInterval)
	{
		return;
	}

	RefreshAccumulator = 0.0f;
	RefreshAudioState();
}

UAudioComponent* UEclipseAudioSubsystem::PlayOneShot(USoundBase* Sound, const FVector& Location, AActor* Instigator)
{
	UWorld* World = GetWorld();
	if (World == nullptr || Sound == nullptr)
	{
		return nullptr;
	}

	const float Volume = FMath::Clamp(GetMasterVolumeScale(), 0.0f, 2.0f);

	// A one-shot with an instigator is attached so it dies with the actor that made it (a
	// footstep stops when the pawn is destroyed); otherwise it is a free-standing sound.
	if (Instigator != nullptr && Instigator->GetRootComponent() != nullptr)
	{
		return UGameplayStatics::SpawnSoundAttached(
			Sound,
			Instigator->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			true,
			Volume);
	}

	return UGameplayStatics::SpawnSoundAtLocation(
		World,
		Sound,
		Location,
		FRotator::ZeroRotator,
		Volume);
}

USoundBase* UEclipseAudioSubsystem::GetSurfaceSound(EPhysicalSurface Surface) const
{
	if (const TSoftObjectPtr<USoundBase>* Found = SurfaceSounds.Find(Surface))
	{
		return Found->LoadSynchronous();
	}

	return nullptr;
}
