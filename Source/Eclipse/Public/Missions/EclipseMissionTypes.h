// PROJECT ECLIPSE - Mission value types.
//
// Purpose
//   Objectives and their progress. Every objective type has a real handler in
//   UEclipseMissionSubsystem; there is no objective that merely displays text.
//
// Progress rules
//   Objectives advance in order: objective N+1 only accepts progress once objective N is
//   complete, unless it is marked as parallel (MustBeLast is the opposite case: it only
//   accepts progress when it is the last objective standing).

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "EclipseMissionTypes.generated.h"

/** One step of a mission. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseObjective
{
	GENERATED_BODY()

	/** What has to happen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Mission")
	EEclipseObjectiveType Type = EEclipseObjectiveType::KillEnemies;

	/** Player-facing description. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Mission")
	FText Description;

	/** How many times the action has to happen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Mission", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	/** Item the objective watches, for CollectItems and Extract. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Mission")
	FName TargetItemId;

	/** Seconds the action must be sustained, for Survive and Defend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Mission", meta = (ClampMin = "0.0"))
	float DurationSeconds = 0.0f;

	/** Radius in centimetres within which the objective counts, for location objectives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Mission", meta = (ClampMin = "0.0"))
	float Radius = 0.0f;

	/** True when this objective only becomes active once every other one is done. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Mission")
	bool bMustBeLast = false;

	/** True when the objective cannot be completed until earlier ones are done. */
	bool CanAcceptProgress(int32 ObjectiveIndex, int32 ActiveIndex, int32 CompletedCount, int32 TotalCount) const
	{
		if (bMustBeLast && CompletedCount < (TotalCount - 1))
		{
			return false;
		}

		return ObjectiveIndex <= ActiveIndex;
	}
};

/** Live progress for one objective. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseObjectiveProgress
{
	GENERATED_BODY()

	/** Index of the objective inside the mission. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Mission")
	int32 ObjectiveIndex = 0;

	/** How much progress has been made. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Mission")
	int32 CurrentCount = 0;

	/** Required count, copied from the objective for the UI. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Mission")
	int32 RequiredCount = 1;

	/** Seconds of sustained progress accumulated. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Mission")
	float AccumulatedSeconds = 0.0f;

	/** True once the objective is satisfied. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Mission")
	bool bComplete = false;

	/** Progress in [0, 1], combining count and time. */
	float GetFraction() const
	{
		const float CountFraction = RequiredCount > 0 ? static_cast<float>(CurrentCount) / static_cast<float>(RequiredCount) : 1.0f;
		return FMath::Clamp(CountFraction, 0.0f, 1.0f);
	}
};

/** Live state of one mission. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseMissionProgress
{
	GENERATED_BODY()

	/** Mission definition id. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Mission")
	FName MissionId;

	/** Current lifecycle state. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Mission")
	EEclipseMissionState State = EEclipseMissionState::Locked;

	/** Per-objective progress, in definition order. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Mission")
	TArray<FEclipseObjectiveProgress> Objectives;

	/** Index of the objective that is currently accepting progress. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Mission")
	int32 ActiveObjectiveIndex = 0;

	/** Seconds elapsed since the mission was accepted. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Mission")
	float ElapsedSeconds = 0.0f;

	/** True when every objective is complete. */
	bool IsComplete() const
	{
		for (const FEclipseObjectiveProgress& Objective : Objectives)
		{
			if (!Objective.bComplete)
			{
				return false;
			}
		}
		return Objectives.Num() > 0;
	}

	/** Number of completed objectives. */
	int32 GetCompletedCount() const
	{
		int32 Count = 0;
		for (const FEclipseObjectiveProgress& Objective : Objectives)
		{
			if (Objective.bComplete)
			{
				++Count;
			}
		}
		return Count;
	}
};
