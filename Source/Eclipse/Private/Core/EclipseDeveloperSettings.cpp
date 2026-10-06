// PROJECT ECLIPSE - Developer settings implementation.
//
// Purpose
//   Defaults and accessors for UEclipseDeveloperSettings. Defaults here must match
//   Config/DefaultGame.ini; the ini is authoritative at runtime and this constructor
//   only defines what a fresh checkout sees before the ini is applied.

#include "Core/EclipseDeveloperSettings.h"

UEclipseDeveloperSettings::UEclipseDeveloperSettings()
	: RealSecondsPerGameHour(60.0f)
	, RaidStartHour(8.0f)
	, WorldSeed(20261006)
	, DefaultMap(TEXT("/Game/Eclipse/Maps/L_FallenBasin.L_FallenBasin"))
	, RaidDurationSeconds(2400.0f)
	, MaxActiveAIAgents(64)
	, AIHighFrequencyRadius(4000.0f)
	, AILowFrequencyTickInterval(0.5f)
	, MaxSingleTransactionDelta(1000000)
	, StartingCredits(500)
	, bEnableMovementSanityChecks(true)
	, MaxAcceptedHorizontalSpeed(2000.0f)
	, MaxClientActionsPerSecond(20)
	, MasterVolumeScale(1.0f)
	, bShowDebugHUD(false)
{
}

const UEclipseDeveloperSettings* UEclipseDeveloperSettings::Get()
{
	const UEclipseDeveloperSettings* Settings = GetDefault<UEclipseDeveloperSettings>();
	check(Settings != nullptr);
	return Settings;
}

uint32 UEclipseDeveloperSettings::GetWorldSeed()
{
	return static_cast<uint32>(Get()->WorldSeed);
}

float UEclipseDeveloperSettings::GetAIHighFrequencyRadius()
{
	return Get()->AIHighFrequencyRadius;
}

bool UEclipseDeveloperSettings::AreMovementSanityChecksEnabled()
{
	return Get()->bEnableMovementSanityChecks;
}

int64 UEclipseDeveloperSettings::GetMaxSingleTransactionDelta()
{
	return Get()->MaxSingleTransactionDelta;
}

#if WITH_EDITOR
FName UEclipseDeveloperSettings::GetCategoryName() const
{
	return TEXT("Game");
}
#endif
