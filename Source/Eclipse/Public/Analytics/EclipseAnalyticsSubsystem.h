// PROJECT ECLIPSE - Analytics subsystem.
//
// Purpose
//   Records gameplay telemetry (raid lifecycle, damage, loot, missions, deaths) into a
//   small in-memory buffer and ships it to a backend through IEclipseAnalyticsProvider.
//   The subsystem works with no provider bound: events are still counted into the raid
//   summary, which is what the post-raid screen reads, and the batch is logged in
//   development builds so a playtest leaves a trail without any backend configured.
//
// Privacy
//   Only gameplay facts are recorded: ids, numbers, timestamps. No player name, no chat,
//   no location data outside the world. That is a deliberate constraint, not an accident.

#pragma once

#include "Core/EclipseGameInstance.h"
#include "Core/EclipseTypes.h"
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EclipseAnalyticsSubsystem.generated.h"

/** One telemetry event. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseAnalyticsEvent
{
	GENERATED_BODY()

	/** Event name, for example "Raid.Ended" or "Combat.PlayerKilled". */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Analytics")
	FName EventName;

	/** Platform time in seconds when the event was recorded. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Analytics")
	double TimestampSeconds = 0.0;

	/** Flat attribute bag. Values are strings so any backend can ingest them. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Analytics")
	TMap<FString, FString> Attributes;

	/** Add or overwrite an attribute. */
	void AddAttribute(const FString& Key, const FString& Value)
	{
		Attributes.Add(Key, Value);
	}

	/** Add or overwrite a numeric attribute. */
	void AddAttribute(const FString& Key, double Value)
	{
		Attributes.Add(Key, FString::SanitizeFloat(Value));
	}

	/** Add or overwrite an integer attribute. */
	void AddAttribute(const FString& Key, int64 Value)
	{
		Attributes.Add(Key, FString::Printf(TEXT("%lld"), Value));
	}
};

/** Counters the post-raid screen shows. Reset when a raid starts. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseRaidSummary
{
	GENERATED_BODY()

	/** Enemies killed by the squad. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Analytics")
	int32 Kills = 0;

	/** Damage the squad dealt. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Analytics")
	float DamageDealt = 0.0f;

	/** Damage the squad took. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Analytics")
	float DamageTaken = 0.0f;

	/** Items picked up. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Analytics")
	int32 ItemsLooted = 0;

	/** Missions completed. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Analytics")
	int32 MissionsCompleted = 0;

	/** Player deaths, including revives. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Analytics")
	int32 Deaths = 0;

	/** Raid duration in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Analytics")
	float DurationSeconds = 0.0f;
};

/**
 * Bare interface for a telemetry backend.
 *
 * The shipped build binds a provider from the platform layer; tests bind a recording
 * provider to assert on the events a system emitted.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UEclipseAnalyticsProvider : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by anything that can accept a batch of events. */
class ECLIPSE_API IEclipseAnalyticsProvider
{
	GENERATED_BODY()

public:
	/** Ship a batch. Called on the game thread; must not block for long. */
	virtual void SubmitEvents(const TArray<FEclipseAnalyticsEvent>& Events) = 0;

	/** True when the backend is able to accept events right now. */
	virtual bool IsReady() const = 0;
};

/**
 * Game-instance-scoped telemetry service.
 */
UCLASS()
class ECLIPSE_API UEclipseAnalyticsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** UGameInstanceSubsystem: hooks the raid lifecycle. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** UGameInstanceSubsystem: flushes what is left. */
	virtual void Deinitialize() override;

	/** Record a bare event. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Analytics")
	void RecordEvent(FName EventName, const TMap<FString, FString>& Attributes);

	/** Record a kill. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Analytics")
	void RecordKill(FName KillerId, FName VictimId, EEclipseFaction VictimFaction, bool bWasBoss);

	/** Record damage, split by direction so the summary is correct from one call site. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Analytics")
	void RecordDamage(FName InstigatorId, FName TargetId, float Amount, EEclipseDamageType DamageType, bool bDealtBySquad);

	/** Record a pickup. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Analytics")
	void RecordLoot(FName PlayerId, FName ItemId, int32 Count, EEclipseRarity Rarity);

	/** Record a mission outcome. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Analytics")
	void RecordMission(FName MissionId, bool bSucceeded, float DurationSeconds);

	/** Record a player death or revive. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Analytics")
	void RecordPlayerDeath(FName PlayerId, bool bWasRevived, FName KillerId);

	/** Replace the telemetry backend. Null clears it. */
	void SetProvider(TScriptInterface<IEclipseAnalyticsProvider> InProvider);

	/** Ship everything buffered. Called automatically when the buffer fills or a raid ends. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Analytics")
	void Flush();

	/** Events waiting to be shipped. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Analytics")
	int32 GetBufferedEventCount() const { return BufferedEvents.Num(); }

	/** Summary of the current (or last) raid. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Analytics")
	const FEclipseRaidSummary& GetRaidSummary() const { return RaidSummary; }

	/** Reset the summary. Called by the raid-started handler. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Analytics")
	void ResetRaidSummary();

	/** How many events may be buffered before an automatic flush. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Analytics", meta = (ClampMin = "16"))
	int32 MaxBufferedEvents = 256;

protected:
	/** Push one event into the buffer, flushing when it is full. */
	void BufferEvent(FEclipseAnalyticsEvent&& Event);

	/** React to the raid lifecycle: reset on start, report and flush on end. */
	void HandleRaidStateChanged(const FEclipseRaidState& State, EEclipseRaidOutcome Outcome);

	/** Buffered events. */
	UPROPERTY()
	TArray<FEclipseAnalyticsEvent> BufferedEvents;

	/** Counters for the post-raid screen. */
	UPROPERTY()
	FEclipseRaidSummary RaidSummary;

	/** Bound backend, if any. */
	TScriptInterface<IEclipseAnalyticsProvider> Provider;

	/** True while a raid is running, so end-of-raid is only reported once. */
	bool bRaidActive = false;
};
