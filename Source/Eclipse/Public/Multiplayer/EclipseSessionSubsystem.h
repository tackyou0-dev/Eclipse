// PROJECT ECLIPSE - Session subsystem.
//
// Purpose
//   Hosting and joining a raid. The raid is an OnlineSubsystem session: the host creates
//   one, sets it up with the raid's settings and travels into the Basin; clients search,
//   join and travel to the resolved connect string. Late joining is a settable policy
//   because a raid that has already started may or may not want another squad member.
//
// Authority
//   Only the host creates or destroys a session, and only the host writes the raid's
//   advertised settings. Clients send intents (join, leave) and follow the host's travel.
//
// Failure handling
//   Every entry point returns a reason string and sets a state; a UI can bind to
//   OnSessionStateChanged and show it. Nothing here silently succeeds.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EclipseSessionSubsystem.generated.h"

class FOnlineSessionSearch;

/** Where the session flow currently is. */
UENUM(BlueprintType)
enum class EEclipseSessionState : uint8
{
	Idle				UMETA(DisplayName = "Idle"),
	Creating			UMETA(DisplayName = "Creating"),
	Searching			UMETA(DisplayName = "Searching"),
	Joining				UMETA(DisplayName = "Joining"),
	InSession			UMETA(DisplayName = "In Session"),
	Destroying			UMETA(DisplayName = "Destroying"),
	Failed				UMETA(DisplayName = "Failed"),

	Count				UMETA(Hidden)
};

/** Broadcast whenever the session flow changes state. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FEclipseSessionStateChanged, EEclipseSessionState /*NewState*/, const FString& /*Detail*/);

/**
 * Game-instance-scoped session service.
 */
UCLASS()
class ECLIPSE_API UEclipseSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** UGameInstanceSubsystem. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** UGameInstanceSubsystem: leaves any session that is still open. */
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	// Hosting and joining
	// ------------------------------------------------------------------

	/**
	 * Create a session for a raid. MaxPlayers is clamped to the raid player cap; the raid
	 * settings are advertised so a joining client can show them while loading.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Session")
	bool HostSession(int32 MaxPlayers, bool bLan, FString& OutError);

	/** Start an asynchronous search. Results arrive through HandleFindSessionsComplete. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Session")
	bool FindSessions(int32 MaxResults, bool bLan, FString& OutError);

	/** Join a result from the last search and travel to it. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Session")
	bool JoinFoundSession(int32 ResultIndex, FString& OutError);

	/** Leave the session, destroying it when we are the host. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Session")
	bool LeaveSession(FName SessionName, FString& OutError);

	/** Travel into the raid map. Called by the game mode and by the front end. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Session")
	bool TravelToRaid(FString& OutError);

	// ------------------------------------------------------------------
	// State
	// ------------------------------------------------------------------

	/** Current state. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Session")
	EEclipseSessionState GetState() const { return State; }

	/** True while a session exists. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Session")
	bool IsInSession() const { return State == EEclipseSessionState::InSession; }

	/** True when this instance created the session. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Session")
	bool IsHost() const { return bIsHost; }

	/** True when the session was created as a LAN session. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Session")
	bool IsLanSession() const { return bUsingLan; }

	/** Results returned by the last search. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Session")
	int32 GetFoundSessionCount() const;

	/** Human-readable summary of a search result, for the server browser. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Session")
	FString GetSessionSummary(int32 ResultIndex) const;

	/** Last failure reason, cleared on the next successful action. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Session")
	const FString& GetLastError() const { return LastError; }

	/** Allow or forbid joining a raid that has already started. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Session")
	void SetLateJoinAllowed(bool bAllowed);

	/** True when the host currently accepts late joins. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Session")
	bool IsLateJoinAllowed() const { return bLateJoinAllowed; }

	/** Fired on every state change, with a human-readable detail string. */
	FEclipseSessionStateChanged OnSessionStateChanged;

	/** Advertised key for the raid map, so a browser can group Fallen Basin sessions. */
	static const FName RaidMapKey;

	/** Advertised key for the number of players the host expects. */
	static const FName RaidPlayerCountKey;

	/** Session name used for the raid. There is only one. */
	static const FName RaidSessionName;

protected:
	/** Session interface from the current online subsystem, or null. */
	IOnlineSessionPtr GetSessionInterface() const;

	/** Move to a new state and broadcast it. */
	void SetState(EEclipseSessionState NewState, const FString& Detail);

	/** Online subsystem callbacks. */
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	/** Current state. */
	EEclipseSessionState State = EEclipseSessionState::Idle;

	/** True when this instance hosts the session. */
	bool bIsHost = false;

	/** True when the last create/search was a LAN query. */
	bool bUsingLan = false;

	/** True when the host accepts late joins. */
	bool bLateJoinAllowed = true;

	/** Last failure reason. */
	FString LastError;

	/** Last search's results. */
	TSharedPtr<FOnlineSessionSearch> SearchSettings;

	/** Delegate handles, so they can be cleared on the way out. */
	FDelegateHandle CreateSessionCompleteHandle;
	FDelegateHandle FindSessionsCompleteHandle;
	FDelegateHandle JoinSessionCompleteHandle;
	FDelegateHandle DestroySessionCompleteHandle;
};
