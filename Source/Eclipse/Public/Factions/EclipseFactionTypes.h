// PROJECT ECLIPSE - Faction value types.
//
// Purpose
//   Reputation storage and the standing bands derived from it. Every system that cares
//   about "how do the Iron Wolves feel about this squad" reads a band, never a raw number:
//   bands are what the vendor pricing, mission gating and dialogue all agree on.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "EclipseFactionTypes.generated.h"

/** Standing bands, resolved from a reputation value and the faction's own thresholds. */
UENUM(BlueprintType)
enum class EEclipseReputationBand : uint8
{
	Hostile			UMETA(DisplayName = "Hostile"),
	Neutral			UMETA(DisplayName = "Neutral"),
	Friendly		UMETA(DisplayName = "Friendly"),
	Allied			UMETA(DisplayName = "Allied"),

	Count			UMETA(Hidden)
};

/** One faction's standing with the player. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseReputation
{
	GENERATED_BODY()

	/** Which faction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Faction")
	EEclipseFaction Faction = EEclipseFaction::None;

	/** Raw reputation. Negative is hostile, positive is friendly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Faction")
	float Value = 0.0f;

	/** Band the value currently falls into, resolved against the faction definition. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Faction")
	EEclipseReputationBand Band = EEclipseReputationBand::Neutral;
};

/** Fired whenever a faction's standing changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEclipseReputationChangedSignature, EEclipseFaction, Faction, float, NewValue, EEclipseReputationBand, NewBand);

/** Fired when two factions change relation state (war declared, alliance formed). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FEclipseFactionRelationChanged, EEclipseFaction /*A*/, EEclipseFaction /*B*/);
