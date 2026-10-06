// PROJECT ECLIPSE - Security implementation.
//
// Purpose
//   Trust accounting, action rate limiting and the movement plausibility check. The
//   thresholds come from the developer settings, so a tuning pass does not have to touch
//   this file to change how strict the server is.

#include "Security/EclipseSecuritySubsystem.h"

#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseLog.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"

void UEclipseSecuritySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PlayerTrust.Reset();
	ViolationLog.Reset();
	SessionViolationCount = 0;
}

void UEclipseSecuritySubsystem::Deinitialize()
{
	PlayerTrust.Reset();
	ViolationLog.Reset();

	Super::Deinitialize();
}

void UEclipseSecuritySubsystem::RegisterPlayer(APlayerController* Controller)
{
	if (Controller == nullptr || FindTrust(Controller) != nullptr)
	{
		return;
	}

	FEclipsePlayerTrust& Trust = PlayerTrust.AddDefaulted_GetRef();
	Trust.Controller = Controller;
	Trust.WindowStartSeconds = FPlatformTime::Seconds();

	UE_LOG(LogEclipseSecurity, Log, TEXT("Tracking %s (trust %.2f)."), *Controller->GetName(), Trust.TrustScore);
}

void UEclipseSecuritySubsystem::UnregisterPlayer(APlayerController* Controller)
{
	if (Controller == nullptr)
	{
		return;
	}

	PlayerTrust.RemoveAll([Controller](const FEclipsePlayerTrust& Trust)
	{
		return Trust.Controller.Get() == Controller;
	});
}

FEclipsePlayerTrust* UEclipseSecuritySubsystem::FindTrust(const APlayerController* Controller)
{
	return const_cast<FEclipsePlayerTrust*>(static_cast<const UEclipseSecuritySubsystem*>(this)->FindTrust(Controller));
}

const FEclipsePlayerTrust* UEclipseSecuritySubsystem::FindTrust(const APlayerController* Controller) const
{
	for (const FEclipsePlayerTrust& Trust : PlayerTrust)
	{
		if (Trust.Controller.Get() == Controller)
		{
			return &Trust;
		}
	}

	return nullptr;
}

bool UEclipseSecuritySubsystem::ValidateClientAction(APlayerController* Controller, EEclipseClientAction Action, FString& OutRefusalReason)
{
	if (Controller == nullptr)
	{
		OutRefusalReason = TEXT("No controller.");
		return false;
	}

	// Every player is rate-limited, host included: a limit that exempts the listen server
	// cannot be tested, and the host's own client is still a client.
	FEclipsePlayerTrust* Trust = FindTrust(Controller);
	if (Trust == nullptr)
	{
		// Unknown player: admit the action but start tracking, so a client that floods
		// before registering cannot dodge the limiter forever.
		RegisterPlayer(Controller);
		Trust = FindTrust(Controller);
		if (Trust == nullptr)
		{
			OutRefusalReason = TEXT("Player is not tracked.");
			return false;
		}
	}

	if (Trust->IsQuarantined())
	{
		OutRefusalReason = TEXT("Player is quarantined.");
		return false;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - Trust->WindowStartSeconds >= 1.0)
	{
		Trust->WindowStartSeconds = Now;
		Trust->ActionsInWindow = 0;
	}

	++Trust->ActionsInWindow;

	const UEclipseDeveloperSettings* Settings = UEclipseDeveloperSettings::Get();
	const int32 MaxActionsPerSecond = Settings != nullptr ? FMath::Max(1, Settings->MaxClientActionsPerSecond) : 20;

	if (Trust->ActionsInWindow > MaxActionsPerSecond)
	{
		FString Reason = FString::Printf(
			TEXT("%d actions in one second (limit %d), action code %d."),
			Trust->ActionsInWindow, MaxActionsPerSecond, static_cast<int32>(Action));

		RecordViolation(Controller, Reason);
		OutRefusalReason = Reason;
		return false;
	}

	OutRefusalReason.Reset();
	return true;
}

bool UEclipseSecuritySubsystem::ValidateMovement(APlayerController* Controller, const FVector& ReportedLocation, FString& OutRefusalReason)
{
	if (Controller == nullptr)
	{
		OutRefusalReason = TEXT("No controller.");
		return false;
	}

	const UEclipseDeveloperSettings* Settings = UEclipseDeveloperSettings::Get();
	if (Settings != nullptr && !Settings->bEnableMovementSanityChecks)
	{
		OutRefusalReason.Reset();
		return true;
	}

	FEclipsePlayerTrust* Trust = FindTrust(Controller);
	if (Trust == nullptr)
	{
		RegisterPlayer(Controller);
		Trust = FindTrust(Controller);
		if (Trust == nullptr)
		{
			OutRefusalReason = TEXT("Player is not tracked.");
			return false;
		}
	}

	const double Now = FPlatformTime::Seconds();

	if (!Trust->bHasLocation)
	{
		Trust->LastLocation = ReportedLocation;
		Trust->LastLocationSeconds = Now;
		Trust->bHasLocation = true;
		OutRefusalReason.Reset();
		return true;
	}

	const double Elapsed = FMath::Max(0.001, Now - Trust->LastLocationSeconds);
	const float MaxSpeed = Settings != nullptr ? FMath::Max(100.0f, Settings->MaxAcceptedHorizontalSpeed) : 2000.0f;

	// Horizontal distance only: falling is vertical and must not be mistaken for a
	// teleport, and a launch pad legitimately moves a pawn upwards.
	const FVector Delta = ReportedLocation - Trust->LastLocation;
	const float HorizontalDistance = FVector(Delta.X, Delta.Y, 0.0f).Size();
	const float AllowedDistance = MaxSpeed * static_cast<float>(Elapsed) * 1.5f;

	Trust->LastLocation = ReportedLocation;
	Trust->LastLocationSeconds = Now;

	if (HorizontalDistance > AllowedDistance)
	{
		FString Reason = FString::Printf(
			TEXT("Moved %.0f cm in %.3f s (limit %.0f cm): %.0f cm/s."),
			HorizontalDistance, Elapsed, AllowedDistance, HorizontalDistance / Elapsed);

		RecordViolation(Controller, Reason);
		OutRefusalReason = Reason;
		return false;
	}

	OutRefusalReason.Reset();
	return true;
}

bool UEclipseSecuritySubsystem::ValidateTransactionDelta(int64 Delta, FString& OutRefusalReason) const
{
	const UEclipseDeveloperSettings* Settings = UEclipseDeveloperSettings::Get();
	const int64 Ceiling = Settings != nullptr ? FMath::Max<int64>(1, Settings->MaxSingleTransactionDelta) : 1000000;

	if (FMath::Abs(Delta) > Ceiling)
	{
		OutRefusalReason = FString::Printf(TEXT("Transaction delta %lld exceeds the ceiling %lld."), Delta, Ceiling);
		return false;
	}

	OutRefusalReason.Reset();
	return true;
}

bool UEclipseSecuritySubsystem::IsQuarantined(const APlayerController* Controller) const
{
	const FEclipsePlayerTrust* Trust = FindTrust(Controller);
	return Trust != nullptr && Trust->IsQuarantined();
}

float UEclipseSecuritySubsystem::GetTrustScore(const APlayerController* Controller) const
{
	const FEclipsePlayerTrust* Trust = FindTrust(Controller);
	return Trust != nullptr ? Trust->TrustScore : 1.0f;
}

int32 UEclipseSecuritySubsystem::GetViolationCount(const APlayerController* Controller) const
{
	const FEclipsePlayerTrust* Trust = FindTrust(Controller);
	return Trust != nullptr ? Trust->ViolationCount : 0;
}

FString UEclipseSecuritySubsystem::GetLastViolationReason(const APlayerController* Controller) const
{
	const FEclipsePlayerTrust* Trust = FindTrust(Controller);
	return Trust != nullptr ? Trust->LastViolationReason : FString();
}

void UEclipseSecuritySubsystem::RecordViolation(APlayerController* Controller, const FString& Reason)
{
	if (Controller == nullptr)
	{
		return;
	}

	FEclipsePlayerTrust* Trust = FindTrust(Controller);
	if (Trust == nullptr)
	{
		RegisterPlayer(Controller);
		Trust = FindTrust(Controller);
		if (Trust == nullptr)
		{
			return;
		}
	}

	Trust->TrustScore = FMath::Clamp(Trust->TrustScore - ViolationPenalty, 0.0f, 1.0f);
	++Trust->ViolationCount;
	Trust->LastViolationReason = Reason;

	++SessionViolationCount;
	ViolationLog.Add(FString::Printf(TEXT("%s: %s"), *Controller->GetName(), *Reason));

	// The log is a report, not a database: keep the newest entries and drop the rest.
	if (ViolationLog.Num() > MaxRecordedViolations)
	{
		ViolationLog.RemoveAt(0, ViolationLog.Num() - MaxRecordedViolations);
	}

	UE_LOG(LogEclipseSecurity, Warning, TEXT("Violation by %s (trust %.2f, %d total): %s"),
		*Controller->GetName(), Trust->TrustScore, Trust->ViolationCount, *Reason);

	OnViolation.Broadcast(Controller, Reason, Trust->TrustScore);
}

void UEclipseSecuritySubsystem::RecoverTrust(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return;
	}

	const float Recovery = TrustRecoveryPerSecond * DeltaSeconds;

	for (FEclipsePlayerTrust& Trust : PlayerTrust)
	{
		if (Trust.TrustScore < 1.0f)
		{
			Trust.TrustScore = FMath::Clamp(Trust.TrustScore + Recovery, 0.0f, 1.0f);
		}
	}
}
