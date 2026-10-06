// PROJECT ECLIPSE - AI value types.
//
// Purpose
//   Squads, noise and the tier rule that makes 64 agents affordable. The tier rule is a
//   pure function of distance to the nearest player, which is why the reference model can
//   test it without an engine.
//
// Budget contract
//   Only agents within UEclipseDeveloperSettings::AIHighFrequencyRadius of a player tick
//   at full rate. Everything else either ticks slowly or not at all, and the total number
//   of active agents never exceeds MaxActiveAIAgents.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "EclipseAITypes.generated.h"

/** A noise an AI can hear: a gunshot, a footstep, an explosion. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseNoiseEvent
{
	GENERATED_BODY()

	/** Where the noise came from. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	FVector Location = FVector::ZeroVector;

	/** How far it carries, in centimetres. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	float Radius = 0.0f;

	/** Who made it. May be null for world noise such as alarms. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	TObjectPtr<AActor> Instigator = nullptr;

	/** World seconds at which it happened, so stale noise can be ignored. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	float TimeSeconds = 0.0f;

	/** True when the noise is still worth reacting to. */
	bool IsAudible(const FVector& ListenerLocation, float NowSeconds, float MaxAgeSeconds) const
	{
		if (Radius <= 0.0f || (NowSeconds - TimeSeconds) > MaxAgeSeconds)
		{
			return false;
		}
		return FVector::DistSquared(Location, ListenerLocation) <= FMath::Square(Radius);
	}
};

/** A group of agents that share a target and a role assignment. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseSquad
{
	GENERATED_BODY()

	/** Squad identifier. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	FGuid SquadId;

	/** Faction the squad belongs to. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	EEclipseFaction Faction = EEclipseFaction::None;

	/** Current members, in join order. Index 0 is the leader. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	TArray<TObjectPtr<AActor>> Members;

	/** Target the squad is focusing, shared to every member. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	TObjectPtr<AActor> FocusTarget = nullptr;

	/** True when the squad has at least one living member. */
	bool IsValid() const { return SquadId.IsValid() && Members.Num() > 0; }
};

/** Result of evaluating one candidate target. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseTargetScore
{
	GENERATED_BODY()

	/** Candidate actor. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	TObjectPtr<AActor> Target = nullptr;

	/** Higher is more attractive. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	float Score = 0.0f;

	/** Distance in centimetres at the time of scoring. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|AI")
	float Distance = 0.0f;
};

/**
 * Tier and threat arithmetic.
 *
 * These functions are deliberately pure so that Tools/ci and the reference model can test
 * them, and so that a designer can reason about the AI budget from numbers alone.
 */
namespace EclipseAI
{
	/** Distance multiplier at which an agent drops to low-frequency ticking. */
	constexpr float LowFrequencyRadiusMultiplier = 4.0f;

	/** Base score of a player target. Players are always the most attractive target. */
	constexpr float PlayerTargetPriority = 500.0f;

	/** Base score of a non-player target. */
	constexpr float DefaultTargetPriority = 100.0f;

	/** Score lost per centimetre of distance. 100 metres costs 100 points. */
	constexpr float DistancePenaltyPerCentimetre = 0.01f;

	/** Bonus for the target the squad is already focusing. */
	constexpr float SquadFocusBonus = 150.0f;

	/** Agents further than this from their spawn are leashed back home. */
	constexpr float LeashRadiusCentimetres = 6000.0f;

	/** Seconds a noise stays worth reacting to. */
	constexpr float NoiseMemorySeconds = 6.0f;

	/** Tier for a distance to the nearest player, given the high-frequency radius. */
	FORCEINLINE EEclipseAITier TierForDistance(float DistanceToNearestPlayer, float HighFrequencyRadius)
	{
		if (HighFrequencyRadius <= 0.0f)
		{
			return EEclipseAITier::Dormant;
		}

		if (DistanceToNearestPlayer <= HighFrequencyRadius)
		{
			return EEclipseAITier::HighFrequency;
		}

		if (DistanceToNearestPlayer <= HighFrequencyRadius * LowFrequencyRadiusMultiplier)
		{
			return EEclipseAITier::LowFrequency;
		}

		return EEclipseAITier::Dormant;
	}

	/** Score a candidate target. Higher wins. */
	FORCEINLINE float ScoreTarget(bool bIsPlayer, float DistanceCentimetres, bool bIsSquadFocus)
	{
		float Score = bIsPlayer ? PlayerTargetPriority : DefaultTargetPriority;
		Score -= DistanceCentimetres * DistancePenaltyPerCentimetre;
		if (bIsSquadFocus)
		{
			Score += SquadFocusBonus;
		}
		return Score;
	}
}
