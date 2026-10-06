// PROJECT ECLIPSE - Game state implementation.
//
// Purpose
//   Server-authoritative setters plus the RepNotify fan-out that keeps clients in sync
//   with the world clock, weather, missions and the boss encounter.

#include "Core/EclipseGameState.h"

#include "Core/EclipseLog.h"
#include "Net/UnrealNetwork.h"

AEclipseGameState::AEclipseGameState()
{
	bReplicates = true;
}

void AEclipseGameState::SetWorldHours(float NewHours)
{
	if (!HasAuthority())
	{
		UE_LOG(LogEclipse, Verbose, TEXT("SetWorldHours ignored on a non-authoritative game state."));
		return;
	}

	const float Wrapped = EclipseTimeOfDay::WrapHour(NewHours);
	if (FMath::IsNearlyEqual(Wrapped, ServerWorldHours, 0.001f))
	{
		return;
	}

	ServerWorldHours = Wrapped;
	OnRep_ServerWorldHours();
}

void AEclipseGameState::SetCurrentWeather(EEclipseWeather NewWeather)
{
	if (!HasAuthority())
	{
		UE_LOG(LogEclipse, Verbose, TEXT("SetCurrentWeather ignored on a non-authoritative game state."));
		return;
	}

	if (NewWeather == CurrentWeather)
	{
		return;
	}

	CurrentWeather = NewWeather;
	OnRep_CurrentWeather();

	UE_LOG(LogEclipse, Log, TEXT("Weather is now %s."), EclipseEnumNames::ToString(CurrentWeather));
}

void AEclipseGameState::SetActiveMissionIds(const TArray<FName>& MissionIds)
{
	if (!HasAuthority())
	{
		return;
	}

	ActiveMissionIds = MissionIds;
}

void AEclipseGameState::SetBossState(const FEclipseBossState& NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	BossState = NewState;
}

void AEclipseGameState::SetRaidActive(bool bActive)
{
	if (!HasAuthority())
	{
		return;
	}

	bRaidActive = bActive;
}

void AEclipseGameState::OnRep_CurrentWeather()
{
	OnWeatherChanged.Broadcast(CurrentWeather);
}

void AEclipseGameState::OnRep_ServerWorldHours()
{
	// The clock is polled by the HUD, so nothing to broadcast. The RepNotify exists so a
	// client that wants an exact transition (nightfall, meal times) can hook it later.
}

void AEclipseGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEclipseGameState, ServerWorldHours);
	DOREPLIFETIME(AEclipseGameState, CurrentWeather);
	DOREPLIFETIME(AEclipseGameState, ActiveMissionIds);
	DOREPLIFETIME(AEclipseGameState, BossState);
	DOREPLIFETIME(AEclipseGameState, bRaidActive);
}
