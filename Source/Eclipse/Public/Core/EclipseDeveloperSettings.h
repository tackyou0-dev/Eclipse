// PROJECT ECLIPSE - Developer settings.
//
// Purpose
//   The one place global, designer-editable tuning lives. The project rule is that no
//   magic number appears in gameplay code: if a system needs a constant that a designer
//   might want to change, it reads it from here.
//
//   Values are stored in Config/DefaultGame.ini under [/Script/Eclipse.EclipseDeveloperSettings]
//   and can be overridden per platform or per user.
//
// Access
//   Always go through UEclipseDeveloperSettings::Get(). It is a UDeveloperSettings
//   object, so it is created by the engine and safe to read from any thread that may
//   touch UObjects (game thread).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EclipseDeveloperSettings.generated.h"

/**
 * Project-wide tuning values for PROJECT ECLIPSE.
 *
 * Changing a value here changes every system that reads it; nothing caches these at
 * startup except where explicitly documented. Values marked "seed-affecting" invalidate
 * procedural layout when changed, and are recorded in save files so a save produced
 * under a different seed is detected rather than silently reloaded into a different
 * world.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Eclipse Developer Settings"))
class ECLIPSE_API UEclipseDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEclipseDeveloperSettings();

	/** Static accessor. Never null in a running game; asserts in development builds. */
	static const UEclipseDeveloperSettings* Get();

	/** Convenience accessor for the world seed as an unsigned value. */
	static uint32 GetWorldSeed();

	/** Convenience accessor for the AI high-frequency radius in centimetres. */
	static float GetAIHighFrequencyRadius();

	/** Convenience accessor for the movement sanity check toggle. */
	static bool AreMovementSanityChecksEnabled();

	/** Convenience accessor for the per-transaction ceiling. */
	static int64 GetMaxSingleTransactionDelta();

	// ---------------------------------------------------------------------
	// World simulation
	// ---------------------------------------------------------------------

	/**
	 * Real seconds that elapse per in-game hour. 60 means a full day is 24 real minutes,
	 * which is the pace the vertical slice is tuned for.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "World", meta = (ClampMin = "1.0", UIMin = "10.0", UIMax = "600.0"))
	float RealSecondsPerGameHour;

	/** Hour of the in-game day at which the raid starts. 8 means 08:00. */
	UPROPERTY(EditAnywhere, Config, Category = "World", meta = (ClampMin = "0.0", ClampMax = "23.99"))
	float RaidStartHour;

	/**
	 * Fixed world seed for procedural layout, loot tables and spawn selection.
	 * Seed-affecting: changing it invalidates every generated container in existing saves.
	 */
	UPROPERTY(EditAnywhere, Config, Category = "World|Procgen")
	int32 WorldSeed;

	/** Map opened by the front end and used by automated tests. */
	UPROPERTY(EditAnywhere, Config, Category = "World", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath DefaultMap;

	/** Total real seconds a raid may last before the extraction window closes. */
	UPROPERTY(EditAnywhere, Config, Category = "World", meta = (ClampMin = "60.0"))
	float RaidDurationSeconds;

	// ---------------------------------------------------------------------
	// AI budget
	// ---------------------------------------------------------------------

	/** Hard ceiling on simultaneously active AI agents in the world. */
	UPROPERTY(EditAnywhere, Config, Category = "AI", meta = (ClampMin = "1", ClampMax = "512"))
	int32 MaxActiveAIAgents;

	/** Agents within this radius (cm) of any player tick at full frequency. */
	UPROPERTY(EditAnywhere, Config, Category = "AI", meta = (ClampMin = "100.0"))
	float AIHighFrequencyRadius;

	/** Seconds between low-frequency AI decision updates. */
	UPROPERTY(EditAnywhere, Config, Category = "AI", meta = (ClampMin = "0.05"))
	float AILowFrequencyTickInterval;

	// ---------------------------------------------------------------------
	// Economy and security
	// ---------------------------------------------------------------------

	/** Largest credit delta a single transaction may move. Guards against overflow abuse. */
	UPROPERTY(EditAnywhere, Config, Category = "Economy", meta = (ClampMin = "0.0"))
	int64 MaxSingleTransactionDelta;

	/** Starting credits for a new profile. */
	UPROPERTY(EditAnywhere, Config, Category = "Economy", meta = (ClampMin = "0"))
	int32 StartingCredits;

	/** Enable server-side movement sanity checks. Never disable in a shipping build. */
	UPROPERTY(EditAnywhere, Config, Category = "Security")
	bool bEnableMovementSanityChecks;

	/** Maximum horizontal speed (cm/s) the server accepts before flagging a correction. */
	UPROPERTY(EditAnywhere, Config, Category = "Security", meta = (ClampMin = "100.0"))
	float MaxAcceptedHorizontalSpeed;

	/** Rate limit: maximum validated actions a client may submit per second. */
	UPROPERTY(EditAnywhere, Config, Category = "Security", meta = (ClampMin = "1"))
	int32 MaxClientActionsPerSecond;

	// ---------------------------------------------------------------------
	// Presentation
	// ---------------------------------------------------------------------

	/** Scale applied to the whole mix, used by the audio subsystem at startup. */
	UPROPERTY(EditAnywhere, Config, Category = "Audio", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MasterVolumeScale;

	/** Show the developer HUD on spawn. Automatically disabled in shipping builds. */
	UPROPERTY(EditAnywhere, Config, Category = "Debug")
	bool bShowDebugHUD;

#if WITH_EDITOR
	/** Category shown in Project Settings. */
	virtual FName GetCategoryName() const override;
#endif
};
