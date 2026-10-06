// PROJECT ECLIPSE - Game instance.
//
// Purpose
//   Owns state that outlives a level: the local profile, the raid lifecycle and the
//   result of the last raid. The game instance is the highest-level place a raid can be
//   started or ended, which is what makes "return to the hub after extraction" possible
//   without the level being loaded twice.
//
// Authority
//   The raid lifecycle is server-authoritative. On a client, StartRaid and EndRaid are
//   recorded locally only for UI purposes; the authoritative copy arrives via the game
//   state.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Engine/GameInstance.h"
#include "EclipseGameInstance.generated.h"

/** Everything the game mode needs to stand up a raid. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseRaidSettings
{
	GENERATED_BODY()

	/** Procedural seed. Zero means "use UEclipseDeveloperSettings::WorldSeed". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Raid")
	int32 SeedOverride = 0;

	/** Server time of day, in hours, when the raid begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Raid")
	float StartHour = 8.0f;

	/** Maximum number of players allowed in the raid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Raid")
	int32 MaxPlayers = 4;

	/** Weather forced for the whole raid. Clear weather is used when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Raid")
	EEclipseWeather ForcedWeather = EEclipseWeather::Clear;

	/** When true the raid does not advance its own clock; used by tests and cutscenes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Raid")
	bool bFreezeTime = false;

	/** Effective seed after applying the developer settings fallback. */
	int32 ResolveSeed(int32 FallbackSeed) const
	{
		return SeedOverride != 0 ? SeedOverride : FallbackSeed;
	}
};

/** Raid lifecycle state owned by the game instance and mirrored by the game state. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseRaidState
{
	GENERATED_BODY()

	/** Settings the raid was started with. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Raid")
	FEclipseRaidSettings Settings;

	/** How the raid finished, or InProgress while it is running. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Raid")
	EEclipseRaidOutcome Outcome = EEclipseRaidOutcome::InProgress;

	/** Server world seconds spent inside the raid. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Raid")
	float ElapsedSeconds = 0.0f;

	/** True once StartRaid has run and EndRaid has not. */
	bool IsRunning() const
	{
		return Outcome == EEclipseRaidOutcome::InProgress;
	}
};

/** Broadcast whenever the raid lifecycle changes. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FEclipseRaidStateChanged, const FEclipseRaidState& /*State*/, EEclipseRaidOutcome /*Outcome*/);

/**
 * Game instance for PROJECT ECLIPSE.
 *
 * Responsibilities: hold the raid settings/state, start and end raids, and keep the
 * per-profile currency and unlocked-mission cache that the front end reads.
 */
UCLASS()
class ECLIPSE_API UEclipseGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UEclipseGameInstance();

	/** Raid state accessor. Always valid, even before a raid starts. */
	const FEclipseRaidState& GetRaidState() const { return RaidState; }

	/** True while a raid is running. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Raid")
	bool IsRaidRunning() const { return RaidState.IsRunning(); }

	/**
	 * Stand up a raid. On the server this applies the seed and clock to the world
	 * subsystem and travels to the configured map. On a client it only stores the
	 * settings so the loading screen has something to display.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Raid")
	void StartRaid(const FEclipseRaidSettings& Settings);

	/**
	 * Finish the raid. Awards the outcome, records analytics, and (optionally) travels
	 * back to the hub map. Calling this twice is harmless: the second call is ignored.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Raid")
	void EndRaid(EEclipseRaidOutcome Outcome);

	/** Advance the elapsed counter. Called by the game mode on a timer. */
	void AccumulateRaidTime(float DeltaSeconds);

	/** Credits carried by the local profile. Server-authoritative in multiplayer. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Profile")
	int64 GetCredits() const { return Credits; }

	/** Set the credit total. Rejects deltas larger than the configured ceiling. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Profile")
	bool SetCredits(int64 NewTotal, FString& OutRejectReason);

	/** Local profile name, used for display and analytics. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Profile")
	const FString& GetProfileName() const { return ProfileName; }

	/** Set the profile name. Empty names are rejected. */
	void SetProfileName(const FString& InName);

	/** Fired on the server whenever the raid state changes. */
	FEclipseRaidStateChanged OnRaidStateChanged;

protected:
	/** UGameInstance. Clears the cached state when the instance shuts down. */
	virtual void Shutdown() override;

private:
	/** Current raid state. */
	UPROPERTY()
	FEclipseRaidState RaidState;

	/** Credits held by this profile. */
	UPROPERTY()
	int64 Credits;

	/** Display name of this profile. */
	UPROPERTY()
	FString ProfileName;
};
