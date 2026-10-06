// PROJECT ECLIPSE - Security subsystem.
//
// Purpose
//   Server-side validation of what a client claims. Every client intent that matters -
//   movement corrections, action spam, impossible transactions - passes through here and
//   is either accepted, refused with a reason, or recorded as a violation that costs the
//   player trust.
//
// Model
//   Trust starts at 1.0. A violation multiplies it down by ViolationPenalty and records
//   the reason; below QuarantineThreshold the player is quarantined, which means the
//   server stops applying their input beyond basic movement until a human looks at the
//   log. Trust is not a ban: an honest player on a bad connection loses a little and
//   recovers it over time, while a scripted client loses all of it in seconds.
//
// This is not a substitute for the engine's own replication checks: it answers "does this
// claim make sense for this player right now", which is exactly what the engine cannot do
// for gameplay actions.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EclipseSecuritySubsystem.generated.h"

class APlayerController;

/** Tuning constants shared by the trust record and the service. */
namespace EclipseSecurity
{
	/** Trust lost per violation. */
	constexpr float ViolationPenalty = 0.2f;

	/** Below this trust a player is quarantined. */
	constexpr float QuarantineThreshold = 0.25f;

	/** Trust recovered per second while no violation is recorded. */
	constexpr float TrustRecoveryPerSecond = 0.01f;

	/** Longest violation log kept for the post-raid report. */
	constexpr int32 MaxRecordedViolations = 64;
}

/** Client intents the server rate-limits. */
UENUM(BlueprintType)
enum class EEclipseClientAction : uint8
{
	Move				UMETA(DisplayName = "Move"),
	Fire				UMETA(DisplayName = "Fire"),
	Reload				UMETA(DisplayName = "Reload"),
	Interact			UMETA(DisplayName = "Interact"),
	Loot				UMETA(DisplayName = "Loot"),
	Craft				UMETA(DisplayName = "Craft"),
	Trade				UMETA(DisplayName = "Trade"),
	MissionAccept		UMETA(DisplayName = "Mission Accept"),
	Respawn				UMETA(DisplayName = "Respawn"),

	Count				UMETA(Hidden)
};

/** A violation was recorded. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FEclipseSecurityViolation, APlayerController* /*Controller*/, const FString& /*Reason*/, float /*TrustScore*/);

/**
 * Per-player trust record.
 */
USTRUCT()
struct FEclipsePlayerTrust
{
	GENERATED_BODY()

	/** Controller this record describes. */
	UPROPERTY()
	TWeakObjectPtr<APlayerController> Controller;

	/** Trust in [0, 1]. Starts at 1.0. */
	UPROPERTY()
	float TrustScore = 1.0f;

	/** How many violations have been recorded. */
	UPROPERTY()
	int32 ViolationCount = 0;

	/** Actions counted in the current one-second window. */
	UPROPERTY()
	int32 ActionsInWindow = 0;

	/** Platform time the current window started. */
	UPROPERTY()
	double WindowStartSeconds = 0.0;

	/** Last reported location, for the movement check. */
	UPROPERTY()
	FVector LastLocation = FVector::ZeroVector;

	/** Platform time of the last reported location. */
	UPROPERTY()
	double LastLocationSeconds = 0.0;

	/** Last violation reason, for the log. */
	UPROPERTY()
	FString LastViolationReason;

	/** True once the player has reported a location. */
	UPROPERTY()
	bool bHasLocation = false;

	/** True when the player has earned a quarantine. */
	bool IsQuarantined() const
	{
		return TrustScore < EclipseSecurity::QuarantineThreshold;
	}
};

/**
 * Game-instance-scoped security service.
 */
UCLASS()
class ECLIPSE_API UEclipseSecuritySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** UGameInstanceSubsystem. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** UGameInstanceSubsystem. */
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	// Registration
	// ------------------------------------------------------------------

	/** Start tracking a player. Called when a player is admitted to the raid. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Security")
	void RegisterPlayer(APlayerController* Controller);

	/** Stop tracking a player. Called when a player leaves. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Security")
	void UnregisterPlayer(APlayerController* Controller);

	/** Number of tracked players. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Security")
	int32 GetTrackedPlayerCount() const { return PlayerTrust.Num(); }

	// ------------------------------------------------------------------
	// Validation
	// ------------------------------------------------------------------

	/**
	 * Rate-limit a client action. Returns false when the player is over the configured
	 * actions per second, in which case the caller must ignore the intent.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Security")
	bool ValidateClientAction(APlayerController* Controller, EEclipseClientAction Action, FString& OutRefusalReason);

	/**
	 * Check a reported location against the fastest the player could plausibly have moved.
	 * Returns false when the claim is impossible; the caller refuses the move.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Security")
	bool ValidateMovement(APlayerController* Controller, const FVector& ReportedLocation, FString& OutRefusalReason);

	/** True when a currency or item delta is within the configured single-transaction ceiling. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Security")
	bool ValidateTransactionDelta(int64 Delta, FString& OutRefusalReason) const;

	/** True when the player's input should no longer be trusted beyond basic movement. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Security")
	bool IsQuarantined(const APlayerController* Controller) const;

	/** Trust score for a player. Unknown players read as 1.0. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Security")
	float GetTrustScore(const APlayerController* Controller) const;

	/** Violation count for a player. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Security")
	int32 GetViolationCount(const APlayerController* Controller) const;

	/** Last recorded violation reason for a player. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Security")
	FString GetLastViolationReason(const APlayerController* Controller) const;

	/** Record a violation: costs trust and broadcasts the reason. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Security")
	void RecordViolation(APlayerController* Controller, const FString& Reason);

	/** Restore trust over time. Called by the game mode's tick for each tracked player. */
	void RecoverTrust(float DeltaSeconds);

	/** Fired whenever a violation is recorded. */
	FEclipseSecurityViolation OnViolation;

	/** Trust lost per violation. */
	static constexpr float ViolationPenalty = EclipseSecurity::ViolationPenalty;

	/** Below this trust a player is quarantined. */
	static constexpr float QuarantineThreshold = EclipseSecurity::QuarantineThreshold;

	/** Trust recovered per second while no violation is recorded. */
	static constexpr float TrustRecoveryPerSecond = EclipseSecurity::TrustRecoveryPerSecond;

	/** Longest violation log kept for the post-raid report. */
	static constexpr int32 MaxRecordedViolations = EclipseSecurity::MaxRecordedViolations;

	/** Violations recorded this session, newest last. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Security")
	const TArray<FString>& GetViolationLog() const { return ViolationLog; }

protected:
	/** Find a player's record, or null. */
	FEclipsePlayerTrust* FindTrust(const APlayerController* Controller);

	/** Find a player's record, or null. */
	const FEclipsePlayerTrust* FindTrust(const APlayerController* Controller) const;

	/** Trust records, one per admitted player. */
	UPROPERTY()
	TArray<FEclipsePlayerTrust> PlayerTrust;

	/** Total violations recorded this session, for the post-raid report. */
	UPROPERTY()
	int32 SessionViolationCount = 0;

	/** Violation log, capped at MaxRecordedViolations. */
	UPROPERTY()
	TArray<FString> ViolationLog;
};
