// PROJECT ECLIPSE - Mission definition and catalogue.
//
// Purpose
//   A mission is data: a briefing, objectives, gating and rewards. The vertical slice's
//   chain is First Contact -> Lost Convoy -> Black Signal -> Broken Relay -> The Warden,
//   mirrored from Content/Eclipse/Data/VerticalSlice/missions.json.
//
// Gating
//   A mission is offered when the player's level and the offering faction's standing allow
//   it. Weather-gated missions (Black Signal needs an electromagnetic storm) are a real
//   gate, not flavour: the mission subsystem refuses to start them in calm weather.

#pragma once

#include "Core/EclipseTypes.h"
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Missions/EclipseMissionTypes.h"
#include "EclipseMissionDefinition.generated.h"

/**
 * One mission.
 *
 * Objectives are ordered and mostly sequential; bMustBeLast marks the step that only
 * opens once everything else is done, which is what forces "clear the camp, then talk to
 * the quartermaster" to actually happen in that order.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseMissionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable id, for example "Mission.TheWarden". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission")
	FName MissionId;

	/** Display name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission")
	FText DisplayName;

	/** Briefing text shown when the mission is offered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission", meta = (MultiLine = "true"))
	FText Briefing;

	/** Mission classification. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission")
	EEclipseMissionType MissionType = EEclipseMissionType::SideMission;

	/** Faction that offers the mission and gains standing from it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission")
	EEclipseFaction OfferingFaction = EEclipseFaction::FreeColonists;

	/** Character level required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission", meta = (ClampMin = "1"))
	int32 MinimumLevel = 1;

	/** Reputation with the offering faction required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission")
	float MinimumReputation = 0.0f;

	/** Mission is only offered while the player is in this biome. None means anywhere. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission")
	EEclipseBiome RequiredBiome = EEclipseBiome::None;

	/** Weather states that must be active for the mission to start. Empty means any. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission")
	TArray<EEclipseWeather> RequiredWeather;

	/** True when the mission may only be started inside its time window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission")
	bool bHasTimeWindow = false;

	/** Hour the window opens. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission", meta = (ClampMin = "0.0", ClampMax = "23.99"))
	float TimeWindowStartHour = 0.0f;

	/** Hour the window closes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission", meta = (ClampMin = "0.0", ClampMax = "23.99"))
	float TimeWindowEndHour = 24.0f;

	/** Seconds allowed once started. Zero means no limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission", meta = (ClampMin = "0.0"))
	float TimeLimitSeconds = 0.0f;

	/** Steps of the mission, in order. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission")
	TArray<FEclipseObjective> Objectives;

	/** What completing the mission grants. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission")
	FEclipseMissionReward Rewards;

	/** Reputation granted to the offering faction on completion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission")
	float ReputationReward = 0.0f;

	/** Mission this one unlocks on completion. Next in the chain, or None. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Mission")
	FName NextMissionId;

	/** True when the definition is complete enough to offer. */
	bool IsValidMission() const
	{
		if (MissionId.IsNone() || Objectives.Num() == 0 || MinimumLevel < 1)
		{
			return false;
		}

		if (bHasTimeWindow && TimeWindowEndHour <= TimeWindowStartHour)
		{
			return false;
		}

		for (const FEclipseObjective& Objective : Objectives)
		{
			if (Objective.RequiredCount < 1)
			{
				return false;
			}

			if (Objective.Type == EEclipseObjectiveType::CollectItems && Objective.TargetItemId.IsNone())
			{
				return false;
			}

			if ((Objective.Type == EEclipseObjectiveType::Survive || Objective.Type == EEclipseObjectiveType::Defend)
				&& Objective.DurationSeconds <= 0.0f)
			{
				return false;
			}
		}

		return true;
	}

	/** True when the supplied hour falls inside the mission's window. */
	bool IsWithinTimeWindow(float HourOfDay) const
	{
		if (!bHasTimeWindow)
		{
			return true;
		}

		const float Wrapped = EclipseTimeOfDay::WrapHour(HourOfDay);

		// Windows may wrap past midnight (22:00 to 04:00), which is why this is not a
		// simple between check.
		if (TimeWindowEndHour <= TimeWindowStartHour)
		{
			return Wrapped >= TimeWindowStartHour || Wrapped <= TimeWindowEndHour;
		}

		return Wrapped >= TimeWindowStartHour && Wrapped <= TimeWindowEndHour;
	}

	/** True when the weather satisfies the mission's requirement. */
	bool IsWeatherAcceptable(EEclipseWeather Weather) const
	{
		return RequiredWeather.Num() == 0 || RequiredWeather.Contains(Weather);
	}
};

/** Every mission in the game. */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseMissionCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** All missions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Catalog")
	TArray<TObjectPtr<UEclipseMissionDefinition>> Missions;

	/** Find a mission by id, or null. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Catalog")
	UEclipseMissionDefinition* Find(FName MissionId) const;

	/** The mission that starts the chain: the first one with no prerequisite. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Catalog")
	UEclipseMissionDefinition* GetFirstMission() const;

	/** Static accessor for the shipped catalogue. */
	static UEclipseMissionCatalog* GetCatalog();
};
