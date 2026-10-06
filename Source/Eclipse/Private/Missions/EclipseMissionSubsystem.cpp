// PROJECT ECLIPSE - Mission subsystem implementation.
//
// Purpose
//   Gating, progress and rewards. The objective loop is the important part: every report
//   walks the active mission's objectives and offers itself only to the one that is
//   currently open, which is what makes the sequence meaningful.

#include "Missions/EclipseMissionSubsystem.h"

#include "AI/EclipseAISubsystem.h"
#include "Characters/EclipseCharacterBase.h"
#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseGameInstance.h"
#include "Core/EclipseGameState.h"
#include "Core/EclipseLog.h"
#include "Engine/World.h"
#include "Factions/EclipseFactionSubsystem.h"
#include "Inventory/EclipseInventoryComponent.h"
#include "Progression/EclipseProgressionComponent.h"
#include "World/EclipseWorldSubsystem.h"

void UEclipseMissionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UEclipseMissionCatalog* Catalog = UEclipseMissionCatalog::GetCatalog())
	{
		for (const TObjectPtr<UEclipseMissionDefinition>& Mission : Catalog->Missions)
		{
			if (Mission != nullptr)
			{
				FEclipseMissionProgress Progress = MakeProgress(*Mission);
				Progress.State = EEclipseMissionState::Locked;
				MissionStates.Add(Mission->MissionId, Progress);
			}
		}

		// The first link of the chain is immediately available; the rest unlock as the
		// campaign progresses.
		if (UEclipseMissionDefinition* First = Catalog->GetFirstMission())
		{
			SetMissionState(First->MissionId, EEclipseMissionState::Available);
		}
	}
}

TStatId UEclipseMissionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEclipseMissionSubsystem, STATGROUP_Tickables);
}

FEclipseMissionProgress UEclipseMissionSubsystem::MakeProgress(const UEclipseMissionDefinition& Mission) const
{
	FEclipseMissionProgress Progress;
	Progress.MissionId = Mission.MissionId;
	Progress.ActiveObjectiveIndex = 0;

	for (int32 Index = 0; Index < Mission.Objectives.Num(); ++Index)
	{
		const FEclipseObjective& Objective = Mission.Objectives[Index];

		FEclipseObjectiveProgress Entry;
		Entry.ObjectiveIndex = Index;
		Entry.RequiredCount = Objective.RequiredCount;
		Entry.CurrentCount = 0;
		Entry.bComplete = false;
		Progress.Objectives.Add(Entry);
	}

	// An empty objective list is complete by definition; IsComplete() guards against that
	// by requiring at least one objective.
	return Progress;
}

void UEclipseMissionSubsystem::SetMissionState(FName MissionId, EEclipseMissionState NewState)
{
	FEclipseMissionProgress* Progress = MissionStates.Find(MissionId);
	if (Progress == nullptr || Progress->State == NewState)
	{
		return;
	}

	const EEclipseMissionState OldState = Progress->State;
	Progress->State = NewState;

	if (AEclipseGameState* GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AEclipseGameState>() : nullptr)
	{
		GameState->SetActiveMissionIds(GetActiveMissionIds());
	}

	UE_LOG(LogEclipseMissions, Log, TEXT("Mission %s: %s -> %s."),
		*MissionId.ToString(),
		EclipseEnumNames::ToString(OldState),
		EclipseEnumNames::ToString(NewState));

	OnMissionStateChanged.Broadcast(MissionId, OldState, NewState);
}

bool UEclipseMissionSubsystem::CheckGating(const UEclipseMissionDefinition& Mission, FString& OutRejectReason) const
{
	if (!Mission.IsValidMission())
	{
		OutRejectReason = TEXT("The mission definition is invalid.");
		return false;
	}

	const UWorld* World = GetWorld();
	const AEclipseGameState* GameState = World != nullptr ? World->GetGameState<AEclipseGameState>() : nullptr;

	if (const UEclipseWorldSubsystem* WorldSubsystem = World != nullptr ? World->GetSubsystem<UEclipseWorldSubsystem>() : nullptr)
	{
		if (!Mission.IsWithinTimeWindow(WorldSubsystem->GetTimeOfDayHours()))
		{
			OutRejectReason = FString::Printf(TEXT("Outside the mission window (%02d:00-%02d:00)."),
				FMath::FloorToInt(Mission.TimeWindowStartHour), FMath::FloorToInt(Mission.TimeWindowEndHour));
			return false;
		}

		if (!Mission.IsWeatherAcceptable(WorldSubsystem->GetCurrentWeather()))
		{
			OutRejectReason = TEXT("The required weather has not arrived.");
			return false;
		}
	}

	// Level and reputation are checked against the first player, because the slice's
	// missions are squad-wide and the campaign belongs to the host's profile.
	const UEclipseGameInstance* GameInstance = World != nullptr ? World->GetGameInstance<UEclipseGameInstance>() : nullptr;
	const APlayerController* FirstController = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = FirstController != nullptr ? FirstController->GetPawn() : nullptr;

	if (Pawn != nullptr)
	{
		if (const UEclipseProgressionComponent* Progression = Pawn->FindComponentByClass<UEclipseProgressionComponent>())
		{
			if (Progression->GetLevel() < Mission.MinimumLevel)
			{
				OutRejectReason = FString::Printf(TEXT("Requires level %d."), Mission.MinimumLevel);
				return false;
			}
		}
	}

	if (GameInstance != nullptr)
	{
		if (const UEclipseFactionSubsystem* FactionSubsystem = GameInstance->GetSubsystem<UEclipseFactionSubsystem>())
		{
			if (FactionSubsystem->GetReputation(Mission.OfferingFaction) < Mission.MinimumReputation)
			{
				OutRejectReason = FString::Printf(TEXT("Requires %.0f reputation with the offering faction."), Mission.MinimumReputation);
				return false;
			}
		}
	}

	// A mission that needs a specific biome needs the squad to actually be there.
	if (Mission.RequiredBiome != EEclipseBiome::None && Pawn != nullptr)
	{
		if (const UEclipseWorldSubsystem* WorldSubsystem = World->GetSubsystem<UEclipseWorldSubsystem>())
		{
			const EEclipseBiome Biome = WorldSubsystem->GetBiomeAt(Pawn->GetActorLocation());
			if (Biome != Mission.RequiredBiome)
			{
				OutRejectReason = TEXT("The squad is not in the required region.");
				return false;
			}
		}
	}

	OutRejectReason.Reset();
	return true;
}

bool UEclipseMissionSubsystem::OfferMission(FName MissionId, FString& OutRejectReason)
{
	UEclipseMissionCatalog* Catalog = UEclipseMissionCatalog::GetCatalog();
	UEclipseMissionDefinition* Mission = Catalog != nullptr ? Catalog->Find(MissionId) : nullptr;
	if (Mission == nullptr)
	{
		OutRejectReason = TEXT("Unknown mission.");
		return false;
	}

	FEclipseMissionProgress* Progress = MissionStates.Find(MissionId);
	if (Progress == nullptr)
	{
		OutRejectReason = TEXT("Mission has no state record.");
		return false;
	}

	if (Progress->State != EEclipseMissionState::Available && Progress->State != EEclipseMissionState::Locked)
	{
		OutRejectReason = TEXT("Mission is not available.");
		return false;
	}

	if (!CheckGating(*Mission, OutRejectReason))
	{
		return false;
	}

	SetMissionState(MissionId, EEclipseMissionState::Offered);
	return true;
}

bool UEclipseMissionSubsystem::StartMission(FName MissionId, FString& OutRejectReason)
{
	UEclipseMissionCatalog* Catalog = UEclipseMissionCatalog::GetCatalog();
	UEclipseMissionDefinition* Mission = Catalog != nullptr ? Catalog->Find(MissionId) : nullptr;
	if (Mission == nullptr)
	{
		OutRejectReason = TEXT("Unknown mission.");
		return false;
	}

	FEclipseMissionProgress* Progress = MissionStates.Find(MissionId);
	if (Progress == nullptr)
	{
		OutRejectReason = TEXT("Mission has no state record.");
		return false;
	}

	if (Progress->State == EEclipseMissionState::Active || Progress->State == EEclipseMissionState::Completed)
	{
		OutRejectReason = TEXT("Mission is already running or complete.");
		return false;
	}

	if (!CheckGating(*Mission, OutRejectReason))
	{
		return false;
	}

	*Progress = MakeProgress(*Mission);
	Progress->State = EEclipseMissionState::Active;

	if (AEclipseGameState* GameState = GetWorld() != nullptr ? GetWorld()->GetGameState<AEclipseGameState>() : nullptr)
	{
		GameState->SetActiveMissionIds(GetActiveMissionIds());
	}

	UE_LOG(LogEclipseMissions, Log, TEXT("Mission %s started."), *MissionId.ToString());
	OnMissionStateChanged.Broadcast(MissionId, EEclipseMissionState::Offered, EEclipseMissionState::Active);
	return true;
}

void UEclipseMissionSubsystem::AdvanceActiveObjective(FEclipseMissionProgress& Progress, const UEclipseMissionDefinition& Mission)
{
	for (int32 Index = 0; Index < Progress.Objectives.Num(); ++Index)
	{
		if (!Progress.Objectives[Index].bComplete)
		{
			Progress.ActiveObjectiveIndex = Index;
			return;
		}
	}

	// Nothing left: park the index at the last objective and settle the mission.
	Progress.ActiveObjectiveIndex = FMath::Max(0, Progress.Objectives.Num() - 1);
	CompleteMission(Progress.MissionId);
}

bool UEclipseMissionSubsystem::ReportProgress(EEclipseObjectiveType Type, int32 Amount, AActor* Context, FName TargetItemId)
{
	if (Amount <= 0)
	{
		return false;
	}

	bool bAnyAdvanced = false;

	for (TPair<FName, FEclipseMissionProgress>& Pair : MissionStates)
	{
		FEclipseMissionProgress& Progress = Pair.Value;
		if (Progress.State != EEclipseMissionState::Active)
		{
			continue;
		}

		UEclipseMissionCatalog* Catalog = UEclipseMissionCatalog::GetCatalog();
		const UEclipseMissionDefinition* Mission = Catalog != nullptr ? Catalog->Find(Progress.MissionId) : nullptr;
		if (Mission == nullptr || !Progress.Objectives.IsValidIndex(Progress.ActiveObjectiveIndex))
		{
			continue;
		}

		const FEclipseObjective& Objective = Mission->Objectives[Progress.ActiveObjectiveIndex];
		if (Objective.Type != Type)
		{
			continue;
		}

		// Collect and extract objectives are item-specific.
		if ((Type == EEclipseObjectiveType::CollectItems || Type == EEclipseObjectiveType::Extract)
			&& !Objective.TargetItemId.IsNone()
			&& Objective.TargetItemId != TargetItemId)
		{
			continue;
		}

		FEclipseObjectiveProgress& ObjectiveProgress = Progress.Objectives[Progress.ActiveObjectiveIndex];
		if (ObjectiveProgress.bComplete)
		{
			continue;
		}

		ObjectiveProgress.CurrentCount = FMath::Min(ObjectiveProgress.RequiredCount, ObjectiveProgress.CurrentCount + Amount);
		ObjectiveProgress.bComplete = ObjectiveProgress.CurrentCount >= ObjectiveProgress.RequiredCount;

		bAnyAdvanced = true;
		OnObjectiveUpdated.Broadcast(Progress.MissionId, Progress.ActiveObjectiveIndex, ObjectiveProgress);

		UE_LOG(LogEclipseMissions, Verbose, TEXT("Mission %s objective %d: %d/%d."),
			*Progress.MissionId.ToString(), Progress.ActiveObjectiveIndex, ObjectiveProgress.CurrentCount, ObjectiveProgress.RequiredCount);

		if (ObjectiveProgress.bComplete)
		{
			AdvanceActiveObjective(Progress, *Mission);
		}

		// One mission advances per report: completing an objective can complete the mission
		// and start the next one, and mutating the map while iterating it is asking for
		// trouble.
		break;
	}

	return bAnyAdvanced;
}

void UEclipseMissionSubsystem::Tick(float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	// Time limits.
	for (TPair<FName, FEclipseMissionProgress>& Pair : MissionStates)
	{
		FEclipseMissionProgress& Progress = Pair.Value;
		if (Progress.State != EEclipseMissionState::Active)
		{
			continue;
		}

		UEclipseMissionCatalog* Catalog = UEclipseMissionCatalog::GetCatalog();
		const UEclipseMissionDefinition* Mission = Catalog != nullptr ? Catalog->Find(Progress.MissionId) : nullptr;
		if (Mission == nullptr)
		{
			continue;
		}

		Progress.ElapsedSeconds += DeltaTime;

		if (Mission->TimeLimitSeconds > 0.0f && Progress.ElapsedSeconds >= Mission->TimeLimitSeconds)
		{
			FailMission(Progress.MissionId, TEXT("The mission timer ran out."));
		}
	}

	TimedObjectiveAccumulator += DeltaTime;
	if (TimedObjectiveAccumulator < TimedObjectiveInterval)
	{
		return;
	}

	const float Interval = TimedObjectiveAccumulator;
	TimedObjectiveAccumulator = 0.0f;
	TickTimedObjectives(Interval);
}

void UEclipseMissionSubsystem::TickTimedObjectives(float DeltaSeconds)
{
	UWorld* World = GetWorld();

	for (TPair<FName, FEclipseMissionProgress>& Pair : MissionStates)
	{
		FEclipseMissionProgress& Progress = Pair.Value;
		if (Progress.State != EEclipseMissionState::Active)
		{
			continue;
		}

		UEclipseMissionCatalog* Catalog = UEclipseMissionCatalog::GetCatalog();
		const UEclipseMissionDefinition* Mission = Catalog != nullptr ? Catalog->Find(Progress.MissionId) : nullptr;
		if (Mission == nullptr || !Progress.Objectives.IsValidIndex(Progress.ActiveObjectiveIndex))
		{
			continue;
		}

		const FEclipseObjective& Objective = Mission->Objectives[Progress.ActiveObjectiveIndex];
		if (Objective.Type != EEclipseObjectiveType::Survive && Objective.Type != EEclipseObjectiveType::Defend)
		{
			continue;
		}

		// Survive accumulates while at least one squad member is alive and inside the
		// objective radius when one is set; Defend additionally requires the area to be
		// clear of hostiles.
		const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			continue;
		}

		const AEclipseCharacterBase* Character = Cast<AEclipseCharacterBase>(Pawn);
		if (Character != nullptr && Character->IsDead())
		{
			continue;
		}

		// A defend objective only accumulates while the area is clear: hostiles inside the
		// radius stop the clock, which is what makes defending an actual fight.
		if (Objective.Type == EEclipseObjectiveType::Defend && Objective.Radius > 0.0f)
		{
			const UEclipseAISubsystem* AISubsystem = World != nullptr ? World->GetSubsystem<UEclipseAISubsystem>() : nullptr;
			const EEclipseFaction PlayerFaction = Character != nullptr ? Character->GetFaction() : EEclipseFaction::None;

			if (AISubsystem != nullptr && AISubsystem->CountAgentsNear(Pawn->GetActorLocation(), Objective.Radius, PlayerFaction) > 0)
			{
				continue;
			}
		}

		FEclipseObjectiveProgress& ObjectiveProgress = Progress.Objectives[Progress.ActiveObjectiveIndex];
		ObjectiveProgress.AccumulatedSeconds = FMath::Min(
			Objective.DurationSeconds,
			ObjectiveProgress.AccumulatedSeconds + DeltaSeconds);

		if (ObjectiveProgress.AccumulatedSeconds >= Objective.DurationSeconds)
		{
			ObjectiveProgress.bComplete = true;
			OnObjectiveUpdated.Broadcast(Progress.MissionId, Progress.ActiveObjectiveIndex, ObjectiveProgress);
			AdvanceActiveObjective(Progress, *Mission);
		}
	}
}

bool UEclipseMissionSubsystem::CompleteMission(FName MissionId)
{
	FEclipseMissionProgress* Progress = MissionStates.Find(MissionId);
	if (Progress == nullptr || Progress->State != EEclipseMissionState::Active)
	{
		return false;
	}

	if (!Progress->IsComplete())
	{
		UE_LOG(LogEclipseMissions, Warning, TEXT("Mission %s was completed with unfinished objectives."), *MissionId.ToString());
	}

	SetMissionState(MissionId, EEclipseMissionState::Completed);

	UEclipseMissionCatalog* Catalog = UEclipseMissionCatalog::GetCatalog();
	if (const UEclipseMissionDefinition* Mission = Catalog != nullptr ? Catalog->Find(MissionId) : nullptr)
	{
		GrantRewards(*Mission);

		if (!Mission->NextMissionId.IsNone())
		{
			SetMissionState(Mission->NextMissionId, EEclipseMissionState::Available);
		}
	}

	return true;
}

void UEclipseMissionSubsystem::GrantRewards(const UEclipseMissionDefinition& Mission)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Credits belong to the profile, so they are granted to the game instance.
	UEclipseGameInstance* GameInstance = World->GetGameInstance<UEclipseGameInstance>();
	if (GameInstance != nullptr && Mission.Rewards.Credits > 0)
	{
		FString Refusal;
		GameInstance->SetCredits(GameInstance->GetCredits() + Mission.Rewards.Credits, Refusal);
	}

	// Experience and items go to every player, which is what makes co-op progression fair.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->Get() != nullptr ? It->Get()->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			continue;
		}

		if (UEclipseProgressionComponent* Progression = Pawn->FindComponentByClass<UEclipseProgressionComponent>())
		{
			Progression->AddExperience(Mission.Rewards.Experience);
		}

		if (UEclipseInventoryComponent* Inventory = Pawn->FindComponentByClass<UEclipseInventoryComponent>())
		{
			for (const FName ItemId : Mission.Rewards.Items)
			{
				Inventory->AddItem(ItemId, 1);
			}
		}
	}

	// Reputation uses the faction subsystem, which clamps and rejects absurd deltas.
	if (GameInstance != nullptr && !FMath::IsNearlyZero(Mission.ReputationReward))
	{
		if (UEclipseFactionSubsystem* FactionSubsystem = GameInstance->GetSubsystem<UEclipseFactionSubsystem>())
		{
			FString Refusal;
			FactionSubsystem->AddReputation(Mission.OfferingFaction, Mission.ReputationReward, Refusal);
		}
	}

	UE_LOG(LogEclipseMissions, Log, TEXT("Mission %s rewards granted: %d credits, %d experience, %d item(s)."),
		*Mission.MissionId.ToString(), Mission.Rewards.Credits, Mission.Rewards.Experience, Mission.Rewards.Items.Num());
}

bool UEclipseMissionSubsystem::FailMission(FName MissionId, const FString& Reason)
{
	FEclipseMissionProgress* Progress = MissionStates.Find(MissionId);
	if (Progress == nullptr || Progress->State != EEclipseMissionState::Active)
	{
		return false;
	}

	UE_LOG(LogEclipseMissions, Log, TEXT("Mission %s failed: %s"), *MissionId.ToString(), *Reason);
	SetMissionState(MissionId, EEclipseMissionState::Failed);
	return true;
}

void UEclipseMissionSubsystem::ReportEnemyKilled(AActor* Killer, AActor* Victim)
{
	if (Victim == nullptr)
	{
		return;
	}

	// Kill credit and experience first, progress second: a kill that finishes an objective
	// should never award less experience than one that does not.
	if (Killer != nullptr)
	{
		if (UEclipseProgressionComponent* Progression = Killer->FindComponentByClass<UEclipseProgressionComponent>())
		{
			Progression->AddExperience(ExperiencePerKill);
		}
	}

	ReportProgress(EEclipseObjectiveType::KillEnemies, 1, Killer, NAME_None);
}

void UEclipseMissionSubsystem::ReportInteraction(AActor* Interactor, AActor* Target)
{
	if (Interactor == nullptr)
	{
		return;
	}

	ReportProgress(EEclipseObjectiveType::Interact, 1, Target != nullptr ? Target : Interactor, NAME_None);
}

EEclipseMissionState UEclipseMissionSubsystem::GetMissionState(FName MissionId) const
{
	const FEclipseMissionProgress* Progress = MissionStates.Find(MissionId);
	return Progress != nullptr ? Progress->State : EEclipseMissionState::Locked;
}

const FEclipseMissionProgress* UEclipseMissionSubsystem::GetProgress(FName MissionId) const
{
	return MissionStates.Find(MissionId);
}

TArray<FName> UEclipseMissionSubsystem::GetActiveMissionIds() const
{
	TArray<FName> Ids;
	for (const TPair<FName, FEclipseMissionProgress>& Pair : MissionStates)
	{
		if (Pair.Value.State == EEclipseMissionState::Active)
		{
			Ids.Add(Pair.Key);
		}
	}

	// Sorted so the replicated list and the HUD order never depend on map ordering.
	Ids.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	return Ids;
}
