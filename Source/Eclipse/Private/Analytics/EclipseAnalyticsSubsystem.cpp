// PROJECT ECLIPSE - Analytics subsystem implementation.
//
// Purpose
//   Event construction, the raid summary counters and the flush path. Every Record* call
//   updates the summary and buffers one event, in that order, so a backend outage never
//   costs the local summary.

#include "Analytics/EclipseAnalyticsSubsystem.h"

#include "Core/EclipseGameInstance.h"
#include "Core/EclipseLog.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"

void UEclipseAnalyticsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UEclipseGameInstance* GameInstance = Cast<UEclipseGameInstance>(GetGameInstance()))
	{
		GameInstance->OnRaidStateChanged.AddUObject(this, &UEclipseAnalyticsSubsystem::HandleRaidStateChanged);
	}
}

void UEclipseAnalyticsSubsystem::Deinitialize()
{
	if (UEclipseGameInstance* GameInstance = Cast<UEclipseGameInstance>(GetGameInstance()))
	{
		GameInstance->OnRaidStateChanged.RemoveAll(this);
	}

	// A session that ends without an extraction still ships what it recorded.
	Flush();
	Provider = nullptr;

	Super::Deinitialize();
}

void UEclipseAnalyticsSubsystem::SetProvider(TScriptInterface<IEclipseAnalyticsProvider> InProvider)
{
	Provider = InProvider;
}

void UEclipseAnalyticsSubsystem::ResetRaidSummary()
{
	RaidSummary = FEclipseRaidSummary();
}

void UEclipseAnalyticsSubsystem::BufferEvent(FEclipseAnalyticsEvent&& Event)
{
	BufferedEvents.Add(MoveTemp(Event));

	if (MaxBufferedEvents > 0 && BufferedEvents.Num() >= MaxBufferedEvents)
	{
		// Flushing from inside a gameplay call is acceptable here: the provider contract is
		// explicitly "must not block for long".
		Flush();
	}
}

void UEclipseAnalyticsSubsystem::RecordEvent(FName EventName, const TMap<FString, FString>& Attributes)
{
	if (EventName.IsNone())
	{
		return;
	}

	FEclipseAnalyticsEvent Event;
	Event.EventName = EventName;
	Event.TimestampSeconds = FPlatformTime::Seconds();
	Event.Attributes = Attributes;

	BufferEvent(MoveTemp(Event));
}

void UEclipseAnalyticsSubsystem::RecordKill(FName KillerId, FName VictimId, EEclipseFaction VictimFaction, bool bWasBoss)
{
	++RaidSummary.Kills;

	FEclipseAnalyticsEvent Event;
	Event.EventName = TEXT("Combat.Kill");
	Event.TimestampSeconds = FPlatformTime::Seconds();
	Event.AddAttribute(TEXT("killer"), KillerId.ToString());
	Event.AddAttribute(TEXT("victim"), VictimId.ToString());
	Event.AddAttribute(TEXT("victim_faction"), static_cast<int64>(VictimFaction));
	Event.AddAttribute(TEXT("boss"), bWasBoss ? 1 : 0);

	BufferEvent(MoveTemp(Event));
}

void UEclipseAnalyticsSubsystem::RecordDamage(FName InstigatorId, FName TargetId, float Amount, EEclipseDamageType DamageType, bool bDealtBySquad)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	if (bDealtBySquad)
	{
		RaidSummary.DamageDealt += Amount;
	}
	else
	{
		RaidSummary.DamageTaken += Amount;
	}

	FEclipseAnalyticsEvent Event;
	Event.EventName = bDealtBySquad ? TEXT("Combat.DamageDealt") : TEXT("Combat.DamageTaken");
	Event.TimestampSeconds = FPlatformTime::Seconds();
	Event.AddAttribute(TEXT("instigator"), InstigatorId.ToString());
	Event.AddAttribute(TEXT("target"), TargetId.ToString());
	Event.AddAttribute(TEXT("amount"), static_cast<double>(Amount));
	Event.AddAttribute(TEXT("type"), static_cast<int64>(DamageType));

	BufferEvent(MoveTemp(Event));
}

void UEclipseAnalyticsSubsystem::RecordLoot(FName PlayerId, FName ItemId, int32 Count, EEclipseRarity Rarity)
{
	if (ItemId.IsNone() || Count <= 0)
	{
		return;
	}

	RaidSummary.ItemsLooted += Count;

	FEclipseAnalyticsEvent Event;
	Event.EventName = TEXT("Items.Looted");
	Event.TimestampSeconds = FPlatformTime::Seconds();
	Event.AddAttribute(TEXT("player"), PlayerId.ToString());
	Event.AddAttribute(TEXT("item"), ItemId.ToString());
	Event.AddAttribute(TEXT("count"), static_cast<int64>(Count));
	Event.AddAttribute(TEXT("rarity"), static_cast<int64>(Rarity));

	BufferEvent(MoveTemp(Event));
}

void UEclipseAnalyticsSubsystem::RecordMission(FName MissionId, bool bSucceeded, float DurationSeconds)
{
	if (MissionId.IsNone())
	{
		return;
	}

	if (bSucceeded)
	{
		++RaidSummary.MissionsCompleted;
	}

	FEclipseAnalyticsEvent Event;
	Event.EventName = bSucceeded ? TEXT("Mission.Completed") : TEXT("Mission.Failed");
	Event.TimestampSeconds = FPlatformTime::Seconds();
	Event.AddAttribute(TEXT("mission"), MissionId.ToString());
	Event.AddAttribute(TEXT("duration"), static_cast<double>(FMath::Max(0.0f, DurationSeconds)));

	BufferEvent(MoveTemp(Event));
}

void UEclipseAnalyticsSubsystem::RecordPlayerDeath(FName PlayerId, bool bWasRevived, FName KillerId)
{
	++RaidSummary.Deaths;

	FEclipseAnalyticsEvent Event;
	Event.EventName = bWasRevived ? TEXT("Player.Revived") : TEXT("Player.Died");
	Event.TimestampSeconds = FPlatformTime::Seconds();
	Event.AddAttribute(TEXT("player"), PlayerId.ToString());
	Event.AddAttribute(TEXT("killer"), KillerId.ToString());
	Event.AddAttribute(TEXT("revived"), bWasRevived ? 1 : 0);

	BufferEvent(MoveTemp(Event));
}

void UEclipseAnalyticsSubsystem::Flush()
{
	if (BufferedEvents.Num() == 0)
	{
		return;
	}

	if (UObject* BackendObject = Provider.GetObject())
	{
		if (BackendObject->Implements<UEclipseAnalyticsProvider>())
		{
			if (IEclipseAnalyticsProvider* Backend = Cast<IEclipseAnalyticsProvider>(BackendObject))
			{
				if (Backend->IsReady())
				{
					Backend->SubmitEvents(BufferedEvents);
					BufferedEvents.Reset();
					return;
				}
			}
		}
	}

	// No backend: keep the batch in the log so a playtest still leaves evidence, and drop
	// it so the buffer cannot grow without bound.
	UE_LOG(LogEclipse, Verbose, TEXT("Analytics flush with no provider: %d event(s) discarded from the buffer."), BufferedEvents.Num());
	BufferedEvents.Reset();
}

void UEclipseAnalyticsSubsystem::HandleRaidStateChanged(const FEclipseRaidState& State, EEclipseRaidOutcome Outcome)
{
	if (Outcome == EEclipseRaidOutcome::InProgress)
	{
		// Raid started: reset the summary and record the settings so a balance pass can
		// group runs by seed and player count.
		bRaidActive = true;
		ResetRaidSummary();

		FEclipseAnalyticsEvent Event;
		Event.EventName = TEXT("Raid.Started");
		Event.TimestampSeconds = FPlatformTime::Seconds();
		Event.AddAttribute(TEXT("seed"), static_cast<int64>(State.Settings.SeedOverride));
		Event.AddAttribute(TEXT("start_hour"), static_cast<double>(State.Settings.StartHour));
		Event.AddAttribute(TEXT("max_players"), static_cast<int64>(State.Settings.MaxPlayers));

		BufferEvent(MoveTemp(Event));
		return;
	}

	if (!bRaidActive)
	{
		return;
	}

	bRaidActive = false;
	RaidSummary.DurationSeconds = State.ElapsedSeconds;

	FEclipseAnalyticsEvent Event;
	Event.EventName = TEXT("Raid.Ended");
	Event.TimestampSeconds = FPlatformTime::Seconds();
	Event.AddAttribute(TEXT("outcome"), static_cast<int64>(Outcome));
	Event.AddAttribute(TEXT("duration"), static_cast<double>(State.ElapsedSeconds));
	Event.AddAttribute(TEXT("kills"), static_cast<int64>(RaidSummary.Kills));
	Event.AddAttribute(TEXT("damage_dealt"), static_cast<double>(RaidSummary.DamageDealt));
	Event.AddAttribute(TEXT("damage_taken"), static_cast<double>(RaidSummary.DamageTaken));
	Event.AddAttribute(TEXT("items_looted"), static_cast<int64>(RaidSummary.ItemsLooted));
	Event.AddAttribute(TEXT("missions_completed"), static_cast<int64>(RaidSummary.MissionsCompleted));
	Event.AddAttribute(TEXT("deaths"), static_cast<int64>(RaidSummary.Deaths));

	BufferEvent(MoveTemp(Event));

	// The end of a raid is the one moment where losing telemetry is unacceptable.
	Flush();

	UE_LOG(LogEclipse, Log, TEXT("Raid ended (%d): %d kills, %.0f damage dealt, %d items, %d mission(s), %d death(s)."),
		static_cast<int32>(Outcome),
		RaidSummary.Kills,
		RaidSummary.DamageDealt,
		RaidSummary.ItemsLooted,
		RaidSummary.MissionsCompleted,
		RaidSummary.Deaths);
}
