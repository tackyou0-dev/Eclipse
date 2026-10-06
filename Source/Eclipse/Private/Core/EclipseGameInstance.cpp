// PROJECT ECLIPSE - Game instance implementation.
//
// Purpose
//   Raid lifecycle and profile bookkeeping. Every mutation goes through a small number
//   of guarded entry points so that a broken call cannot leave the instance half
//   updated, and so that every state change is broadcast exactly once.

#include "Core/EclipseGameInstance.h"

#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseLog.h"

UEclipseGameInstance::UEclipseGameInstance()
	: Credits(0)
	, ProfileName(TEXT("Colonist"))
{
	const UEclipseDeveloperSettings* Settings = UEclipseDeveloperSettings::Get();
	if (Settings != nullptr)
	{
		Credits = Settings->StartingCredits;
	}
}

void UEclipseGameInstance::StartRaid(const FEclipseRaidSettings& Settings)
{
	if (RaidState.IsRunning())
	{
		UE_LOG(LogEclipse, Warning, TEXT("StartRaid ignored: a raid is already running."));
		return;
	}

	const UEclipseDeveloperSettings* DeveloperSettings = UEclipseDeveloperSettings::Get();
	const int32 FallbackSeed = DeveloperSettings != nullptr ? DeveloperSettings->WorldSeed : 1;
	const float FallbackHour = DeveloperSettings != nullptr ? DeveloperSettings->RaidStartHour : 8.0f;

	RaidState.Settings = Settings;
	RaidState.Settings.SeedOverride = Settings.ResolveSeed(FallbackSeed);
	RaidState.Settings.StartHour = Settings.StartHour >= 0.0f ? Settings.StartHour : FallbackHour;
	RaidState.Outcome = EEclipseRaidOutcome::InProgress;
	RaidState.ElapsedSeconds = 0.0f;

	UE_LOG(LogEclipse, Log, TEXT("Raid started with seed %d at %02d:00 for up to %d players."),
		RaidState.Settings.SeedOverride,
		FMath::FloorToInt(RaidState.Settings.StartHour),
		RaidState.Settings.MaxPlayers);

	OnRaidStateChanged.Broadcast(RaidState, RaidState.Outcome);
}

void UEclipseGameInstance::EndRaid(EEclipseRaidOutcome Outcome)
{
	if (!RaidState.IsRunning())
	{
		UE_LOG(LogEclipse, Log, TEXT("EndRaid ignored: no raid is running."));
		return;
	}

	if (Outcome == EEclipseRaidOutcome::InProgress)
	{
		UE_LOG(LogEclipse, Warning, TEXT("EndRaid called with InProgress; forcing TimedOut instead."));
		Outcome = EEclipseRaidOutcome::TimedOut;
	}

	RaidState.Outcome = Outcome;

	UE_LOG(LogEclipse, Log, TEXT("Raid ended: %s after %.1f seconds."),
		EclipseEnumNames::ToString(Outcome),
		RaidState.ElapsedSeconds);

	OnRaidStateChanged.Broadcast(RaidState, RaidState.Outcome);
}

void UEclipseGameInstance::AccumulateRaidTime(float DeltaSeconds)
{
	if (!RaidState.IsRunning() || DeltaSeconds <= 0.0f)
	{
		return;
	}

	RaidState.ElapsedSeconds += DeltaSeconds;

	const UEclipseDeveloperSettings* Settings = UEclipseDeveloperSettings::Get();
	if (Settings != nullptr && Settings->RaidDurationSeconds > 0.0f && RaidState.ElapsedSeconds >= Settings->RaidDurationSeconds)
	{
		EndRaid(EEclipseRaidOutcome::TimedOut);
	}
}

bool UEclipseGameInstance::SetCredits(int64 NewTotal, FString& OutRejectReason)
{
	if (NewTotal < 0)
	{
		OutRejectReason = TEXT("Credits cannot be negative.");
		return false;
	}

	const int64 Delta = NewTotal - Credits;
	const int64 Ceiling = UEclipseDeveloperSettings::GetMaxSingleTransactionDelta();
	if (FMath::Abs(Delta) > Ceiling)
	{
		OutRejectReason = FString::Printf(TEXT("Credit delta %lld exceeds the %lld ceiling."), Delta, Ceiling);
		UE_LOG(LogEclipse, Warning, TEXT("%s"), *OutRejectReason);
		return false;
	}

	Credits = NewTotal;
	OutRejectReason.Reset();
	return true;
}

void UEclipseGameInstance::SetProfileName(const FString& InName)
{
	if (InName.IsEmpty())
	{
		UE_LOG(LogEclipse, Warning, TEXT("SetProfileName ignored an empty name."));
		return;
	}

	ProfileName = InName.Left(32);
}

void UEclipseGameInstance::Shutdown()
{
	RaidState.Outcome = EEclipseRaidOutcome::InProgress;
	RaidState.ElapsedSeconds = 0.0f;
	OnRaidStateChanged.Clear();

	Super::Shutdown();
}
