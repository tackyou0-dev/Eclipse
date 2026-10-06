// PROJECT ECLIPSE - Session subsystem implementation.
//
// Purpose
//   Host, search, join, travel. Every asynchronous step has a callback that either moves
//   the state forward or records a failure, so the front end never has to guess whether a
//   request is still running.

#include "Multiplayer/EclipseSessionSubsystem.h"

#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseGameInstance.h"
#include "Core/EclipseLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

const FName UEclipseSessionSubsystem::RaidMapKey = TEXT("ECLIPSE_RAID_MAP");
const FName UEclipseSessionSubsystem::RaidPlayerCountKey = TEXT("ECLIPSE_RAID_PLAYERS");
const FName UEclipseSessionSubsystem::RaidSessionName = NAME_GameSession;

void UEclipseSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	State = EEclipseSessionState::Idle;
}

void UEclipseSessionSubsystem::Deinitialize()
{
	if (State == EEclipseSessionState::InSession && bIsHost)
	{
		// The instance is going away: do not leak an advertised session behind us.
		FString Error;
		LeaveSession(RaidSessionName, Error);
	}

	SearchSettings.Reset();

	Super::Deinitialize();
}

IOnlineSessionPtr UEclipseSessionSubsystem::GetSessionInterface() const
{
	IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	return OnlineSubsystem != nullptr ? OnlineSubsystem->GetSessionInterface() : nullptr;
}

void UEclipseSessionSubsystem::SetState(EEclipseSessionState NewState, const FString& Detail)
{
	State = NewState;

	if (NewState == EEclipseSessionState::Failed)
	{
		LastError = Detail;
	}
	else if (NewState != EEclipseSessionState::Idle)
	{
		LastError.Reset();
	}

	UE_LOG(LogEclipseNet, Log, TEXT("Session state: %d (%s)."), static_cast<int32>(NewState), *Detail);
	OnSessionStateChanged.Broadcast(NewState, Detail);
}

void UEclipseSessionSubsystem::SetLateJoinAllowed(bool bAllowed)
{
	bLateJoinAllowed = bAllowed;

	// The host is the only side that can change what the session advertises.
	if (bIsHost && State == EEclipseSessionState::InSession)
	{
		IOnlineSessionPtr SessionInterface = GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			FNamedOnlineSession* Session = SessionInterface->GetNamedSession(RaidSessionName);
			if (Session != nullptr)
			{
				Session->SessionSettings.bAllowJoinInProgress = bLateJoinAllowed;
				SessionInterface->UpdateSession(RaidSessionName, Session->SessionSettings, true);
			}
		}
	}
}

bool UEclipseSessionSubsystem::HostSession(int32 MaxPlayers, bool bLan, FString& OutError)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OutError = TEXT("No online subsystem is available for hosting.");
		SetState(EEclipseSessionState::Failed, OutError);
		return false;
	}

	if (State == EEclipseSessionState::Creating || State == EEclipseSessionState::Searching || State == EEclipseSessionState::Joining)
	{
		OutError = TEXT("A session request is already running.");
		return false;
	}

	// The player cap comes from the raid settings, which is the same number the game mode
	// enforces on PostLogin; the clamp keeps a UI bug from advertising a 64-player raid.
	int32 ResolvedMaxPlayers = MaxPlayers;
	if (const UEclipseGameInstance* GameInstance = Cast<UEclipseGameInstance>(GetGameInstance()))
	{
		ResolvedMaxPlayers = FMath::Min(ResolvedMaxPlayers, GameInstance->GetRaidState().Settings.MaxPlayers);
	}
	ResolvedMaxPlayers = FMath::Clamp(ResolvedMaxPlayers, 1, 8);

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = ResolvedMaxPlayers;
	Settings.NumPrivateConnections = 0;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = bLateJoinAllowed;
	Settings.bIsLANMatch = bLan;
	Settings.bUsesPresence = !bLan;
	Settings.bAllowInvites = true;

	// The advertised map name is the actual map asset name, so a browser groups sessions
	// by the map they will load rather than by a display string that can drift.
	const UEclipseDeveloperSettings* DeveloperSettings = UEclipseDeveloperSettings::Get();
	const FString AdvertisedMap = DeveloperSettings != nullptr
		? DeveloperSettings->DefaultMap.GetAssetName()
		: FString(TEXT("L_FallenBasin"));

	Settings.Set(RaidMapKey, AdvertisedMap, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(RaidPlayerCountKey, ResolvedMaxPlayers, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	CreateSessionCompleteHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UEclipseSessionSubsystem::HandleCreateSessionComplete));

	bUsingLan = bLan;
	bIsHost = true;
	SetState(EEclipseSessionState::Creating, FString::Printf(TEXT("Hosting for %d player(s)."), ResolvedMaxPlayers));

	if (!SessionInterface->CreateSession(0, RaidSessionName, Settings))
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		OutError = TEXT("CreateSession was refused by the online subsystem.");
		SetState(EEclipseSessionState::Failed, OutError);
		return false;
	}

	OutError.Reset();
	return true;
}

bool UEclipseSessionSubsystem::FindSessions(int32 MaxResults, bool bLan, FString& OutError)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OutError = TEXT("No online subsystem is available for searching.");
		SetState(EEclipseSessionState::Failed, OutError);
		return false;
	}

	SearchSettings = MakeShared<FOnlineSessionSearch>();
	SearchSettings->MaxSearchResults = FMath::Clamp(MaxResults, 1, 200);
	SearchSettings->bIsLanQuery = bLan;

	// Only Eclipse raid sessions are interesting, so the search is filtered by the raid key.
	SearchSettings->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

	FindSessionsCompleteHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(this, &UEclipseSessionSubsystem::HandleFindSessionsComplete));

	bUsingLan = bLan;
	SetState(EEclipseSessionState::Searching, TEXT("Searching for raids."));

	if (!SessionInterface->FindSessions(0, SearchSettings.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
		OutError = TEXT("FindSessions was refused by the online subsystem.");
		SetState(EEclipseSessionState::Failed, OutError);
		return false;
	}

	OutError.Reset();
	return true;
}

bool UEclipseSessionSubsystem::JoinFoundSession(int32 ResultIndex, FString& OutError)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OutError = TEXT("No online subsystem is available for joining.");
		return false;
	}

	if (!SearchSettings.IsValid() || !SearchSettings->SearchResults.IsValidIndex(ResultIndex))
	{
		OutError = TEXT("That search result no longer exists; search again.");
		return false;
	}

	// A session that has already started may refuse joins; check the advertised policy
	// before spending a round trip on it.
	const FOnlineSessionSearchResult& Result = SearchSettings->SearchResults[ResultIndex];
	if (!Result.Session.SessionSettings.bAllowJoinInProgress && Result.Session.NumOpenPublicConnections == 0)
	{
		OutError = TEXT("That raid is full and is not accepting late joins.");
		return false;
	}

	JoinSessionCompleteHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(this, &UEclipseSessionSubsystem::HandleJoinSessionComplete));

	bIsHost = false;
	SetState(EEclipseSessionState::Joining, FString::Printf(TEXT("Joining '%s'."), *Result.Session.OwningUserName));

	if (!SessionInterface->JoinSession(0, RaidSessionName, Result))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
		OutError = TEXT("JoinSession was refused by the online subsystem.");
		SetState(EEclipseSessionState::Failed, OutError);
		return false;
	}

	OutError.Reset();
	return true;
}

bool UEclipseSessionSubsystem::LeaveSession(FName SessionName, FString& OutError)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (!SessionInterface.IsValid())
	{
		OutError = TEXT("No online subsystem is available.");
		return false;
	}

	DestroySessionCompleteHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(
		FOnDestroySessionCompleteDelegate::CreateUObject(this, &UEclipseSessionSubsystem::HandleDestroySessionComplete));

	SetState(EEclipseSessionState::Destroying, TEXT("Leaving the session."));

	if (!SessionInterface->DestroySession(SessionName))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
		OutError = TEXT("DestroySession was refused by the online subsystem.");
		SetState(EEclipseSessionState::Failed, OutError);
		return false;
	}

	OutError.Reset();
	return true;
}

bool UEclipseSessionSubsystem::TravelToRaid(FString& OutError)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance == nullptr)
	{
		OutError = TEXT("No game instance to travel from.");
		return false;
	}

	// Standalone: travel straight to the configured map.
	if (!bIsHost && !SessionInterface.IsValid())
	{
		OutError = TEXT("No session to travel into.");
		return false;
	}

	APlayerController* Controller = GameInstance->GetFirstLocalPlayerController();
	if (Controller == nullptr)
	{
		OutError = TEXT("No local player controller to travel.");
		return false;
	}

	if (SessionInterface.IsValid())
	{
		FString ConnectString;
		if (!SessionInterface->GetResolvedConnectString(RaidSessionName, ConnectString))
		{
			OutError = TEXT("The session has no resolvable connect string yet.");
			SetState(EEclipseSessionState::Failed, OutError);
			return false;
		}

		UE_LOG(LogEclipseNet, Log, TEXT("Travelling to the raid at '%s'."), *ConnectString);
		Controller->ClientTravel(ConnectString, TRAVEL_Absolute);
		OutError.Reset();
		return true;
	}

	// Host with no session interface (single player): the game instance owns the map.
	if (UEclipseGameInstance* EclipseGameInstance = Cast<UEclipseGameInstance>(GameInstance))
	{
		EclipseGameInstance->StartRaid(EclipseGameInstance->GetRaidState().Settings);
		OutError.Reset();
		return true;
	}

	OutError = TEXT("Nothing to travel into.");
	return false;
}

int32 UEclipseSessionSubsystem::GetFoundSessionCount() const
{
	return SearchSettings.IsValid() ? SearchSettings->SearchResults.Num() : 0;
}

FString UEclipseSessionSubsystem::GetSessionSummary(int32 ResultIndex) const
{
	if (!SearchSettings.IsValid() || !SearchSettings->SearchResults.IsValidIndex(ResultIndex))
	{
		return FString();
	}

	const FOnlineSessionSearchResult& Result = SearchSettings->SearchResults[ResultIndex];
	FString MapName;
	if (!Result.Session.SessionSettings.Get(RaidMapKey, MapName))
	{
		MapName = TEXT("Unknown");
	}

	int32 AdvertisedPlayers = 0;
	if (!Result.Session.SessionSettings.Get(RaidPlayerCountKey, AdvertisedPlayers))
	{
		AdvertisedPlayers = Result.Session.SessionSettings.NumPublicConnections;
	}

	const int32 PingMs = Result.PingInMs;

	return FString::Printf(TEXT("%s  %d/%d  %dms"),
		*MapName,
		AdvertisedPlayers - Result.Session.NumOpenPublicConnections,
		AdvertisedPlayers,
		PingMs);
}

void UEclipseSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
	}

	if (!bWasSuccessful)
	{
		bIsHost = false;
		SetState(EEclipseSessionState::Failed, TEXT("The session could not be created."));
		return;
	}

	SetState(EEclipseSessionState::InSession, FString::Printf(TEXT("Hosting '%s'."), *SessionName.ToString()));
}

void UEclipseSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteHandle);
	}

	if (!bWasSuccessful)
	{
		SetState(EEclipseSessionState::Failed, TEXT("The session search failed."));
		return;
	}

	SetState(EEclipseSessionState::Idle, FString::Printf(TEXT("Found %d raid(s)."), GetFoundSessionCount()));
}

void UEclipseSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteHandle);
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		bIsHost = false;
		SetState(EEclipseSessionState::Failed, FString::Printf(TEXT("Joining failed (result %d)."), static_cast<int32>(Result)));
		return;
	}

	SetState(EEclipseSessionState::InSession, FString::Printf(TEXT("Joined '%s'."), *SessionName.ToString()));

	// Joining without following up with a travel leaves the player in the menu; the whole
	// point of joining is to get into the raid.
	FString TravelError;
	if (!TravelToRaid(TravelError))
	{
		SetState(EEclipseSessionState::Failed, TravelError);
	}
}

void UEclipseSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr SessionInterface = GetSessionInterface();
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteHandle);
	}

	bIsHost = false;

	if (!bWasSuccessful)
	{
		SetState(EEclipseSessionState::Failed, TEXT("The session could not be destroyed."));
		return;
	}

	SetState(EEclipseSessionState::Idle, FString::Printf(TEXT("Left '%s'."), *SessionName.ToString()));
}
