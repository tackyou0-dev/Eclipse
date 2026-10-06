// PROJECT ECLIPSE - Audio subsystem.
//
// Purpose
//   Decides what the world should sound like and plays the one-shots gameplay code asks
//   for. The decision itself is pure data: an audio state is (ambience, wind gain, reverb
//   wet level, music intensity) resolved from the weather and the time of day, both of
//   which the world subsystem publishes. A storm at night is a different mix from a clear
//   morning, and that is authored in the state tables rather than hard-coded in code.
//
// Authority
//   Audio is cosmetic, so it runs on whoever has a listener: the subsystem publishes the
//   audio state on clients from the replicated weather, and on the server it does nothing
//   but resolve the state for tests and for the headless-server case.

#pragma once

#include "Core/EclipseTypes.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Sound/SoundBase.h"
#include "Subsystems/WorldSubsystem.h"
#include "EclipseAudioSubsystem.generated.h"

class USoundMix;

/** Resolved audio state for the world right now. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseAudioState
{
	GENERATED_BODY()

	/** Ambience bed id, resolved to a sound asset by the soundscape Blueprint or by the map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Audio")
	FName AmbienceId = TEXT("Ambience.Clear");

	/** Music intensity in [0, 1]: 0 is a drone, 1 is the combat layer at full level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MusicIntensity = 0.0f;

	/** Wind layer gain, scaled by the weather. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float WindGain = 0.0f;

	/** Reverb wet level in [0, 1]: interiors and storms are wetter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReverbWetLevel = 0.15f;

	/** True when the state has something to play. */
	bool IsAudible() const
	{
		return !AmbienceId.IsNone() || MusicIntensity > 0.0f || WindGain > 0.0f;
	}
};

/** Broadcast whenever the resolved audio state changes. */
DECLARE_MULTICAST_DELEGATE_OneParam(FEclipseAudioStateChanged, const FEclipseAudioState& /*NewState*/);

/**
 * World-scoped audio service.
 *
 * Footsteps and impacts go through PlayOneShot so that the surface lookup, the attenuation
 * and the master volume are applied in exactly one place.
 */
UCLASS()
class ECLIPSE_API UEclipseAudioSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** UTickableWorldSubsystem. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** UTickableWorldSubsystem. */
	virtual void Deinitialize() override;

	/** UTickableWorldSubsystem: watches the world clock and weather. */
	virtual void Tick(float DeltaTime) override;

	/** FTickableGameObject. */
	virtual TStatId GetStatId() const override;

	/** Resolve the audio state for a weather/time pair. Pure function: no world access. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Audio")
	FEclipseAudioState ResolveAudioState(EEclipseWeather Weather, EEclipseTimeOfDay TimeOfDay) const;

	/** Audio state currently published. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Audio")
	const FEclipseAudioState& GetCurrentState() const { return CurrentState; }

	/** Force a state. Used by interiors and scripted sequences. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Audio")
	void OverrideAudioState(const FEclipseAudioState& State, float DurationSeconds);

	/** Clear any override and go back to the world state. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Audio")
	void ClearOverride();

	/** True while an override is in force. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Audio")
	bool HasOverride() const { return OverrideRemainingSeconds > 0.0f; }

	/** Master volume multiplier applied to every one-shot and every mix override. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Audio")
	float GetMasterVolumeScale() const;

	/** Play a one-shot with the surface-appropriate attenuation. Returns the audio component. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Audio")
	UAudioComponent* PlayOneShot(USoundBase* Sound, const FVector& Location, AActor* Instigator = nullptr);

	/** Sound for a physical surface, or null when the map has no entry. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Audio")
	USoundBase* GetSurfaceSound(EPhysicalSurface Surface) const;

	/** Fired whenever the published state changes. */
	FEclipseAudioStateChanged OnAudioStateChanged;

	/** Sound mix the master volume is applied to. Optional. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Audio")
	TSoftObjectPtr<USoundMix> MasterSoundMix;

	/** Per-weather audio contributions. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Audio")
	TMap<EEclipseWeather, FEclipseAudioState> WeatherStates;

	/** Per-band audio contributions; music intensity comes from here. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Audio")
	TMap<EEclipseTimeOfDay, FEclipseAudioState> TimeOfDayStates;

	/** Footstep and impact sounds by physical surface. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Audio")
	TMap<TEnumAsByte<EPhysicalSurface>, TSoftObjectPtr<USoundBase>> SurfaceSounds;

	/** Seconds between world-state checks. Audio does not need to be frame-perfect. */
	static constexpr float StateRefreshInterval = 0.5f;

protected:
	/** Re-resolve the state from the world and publish it if it changed. */
	void RefreshAudioState();

	/** Apply the master volume through the sound mix, when one is configured. */
	void ApplyMasterVolume();

	/** Published state. */
	FEclipseAudioState CurrentState;

	/** Seconds left on an override. */
	float OverrideRemainingSeconds = 0.0f;

	/** State the override replaced, restored when it expires. */
	FEclipseAudioState OverrideReturnState;

	/** Accumulator for the refresh interval. */
	float RefreshAccumulator = 0.0f;
};
