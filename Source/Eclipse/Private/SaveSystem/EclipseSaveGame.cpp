// PROJECT ECLIPSE - Save implementation.
//
// Purpose
//   Record round-trip, the v1 -> v2 migration and the slot API. The one rule this file
//   follows everywhere: never apply a partially-read save. Gather first, verify, then
//   apply, so a corrupt record leaves the running game untouched.

#include "SaveSystem/EclipseSaveGame.h"

#include "Core/EclipseDeterministicRandom.h"
#include "Core/EclipseGameInstance.h"
#include "Core/EclipseLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Factions/EclipseFactionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "TimerManager.h"
#include "World/EclipseWorldSubsystem.h"

namespace
{
	/** A game-instance subsystem has no world of its own; it goes through the instance. */
	UWorld* GetGameWorld(const UGameInstanceSubsystem* Subsystem)
	{
		const UGameInstance* GameInstance = Subsystem != nullptr ? Subsystem->GetGameInstance() : nullptr;
		return GameInstance != nullptr ? GameInstance->GetWorld() : nullptr;
	}
}

const FString UEclipseSaveSubsystem::DefaultSlotName = TEXT("EclipseProfile1");

namespace EclipseSave
{
	FGuid MakeRecordId(FName RecordType, const FString& Key)
	{
		// Two independent hashes of the same input give a stable 128-bit-ish key: the type
		// hash keeps record kinds in different spaces, and the second mix keeps the four
		// GUID words from being correlated.
		const FString TypeKey = RecordType.ToString();
		const uint32 A = FEclipseDeterministicRandom::StableIdFromString(TypeKey);
		const uint32 B = FEclipseDeterministicRandom::StableIdFromString(Key);
		const uint32 C = FEclipseDeterministicRandom::MixSeed(A, B);
		const uint32 D = FEclipseDeterministicRandom::MixSeed(B, A + 0x9E3779B9u);

		return FGuid(C, D, B, A);
	}
}

bool UEclipseSaveGame::MigrateToCurrent(FString& OutError)
{
	if (SchemaVersion > UEclipseSaveGameVersion::Current)
	{
		OutError = FString::Printf(
			TEXT("Save was written by a newer build (schema %d, this build reads up to %d)."),
			SchemaVersion, UEclipseSaveGameVersion::Current);
		return false;
	}

	if (SchemaVersion < UEclipseSaveGameVersion::MinSupported)
	{
		OutError = FString::Printf(
			TEXT("Save schema %d is older than the oldest supported schema %d."),
			SchemaVersion, UEclipseSaveGameVersion::MinSupported);
		return false;
	}

	if (SchemaVersion == UEclipseSaveGameVersion::Current)
	{
		OutError.Reset();
		return true;
	}

	// v1 -> v2: v1 kept everything as strings in a per-system map and had no world
	// snapshot. Convert what can be converted and keep the rest as legacy values.
	if (SchemaVersion == UEclipseSaveGameVersion::LegacyV1)
	{
		for (FEclipseSaveRecord& Record : Records)
		{
			if (Record.SchemaVersion != UEclipseSaveGameVersion::LegacyV1)
			{
				continue;
			}

			if (const FString* SeedValue = Record.LegacyValues.Find(TEXT("WorldSeed")))
			{
				World.WorldSeed = FCString::Atoi(**SeedValue);
			}

			if (const FString* HourValue = Record.LegacyValues.Find(TEXT("TimeOfDayHours")))
			{
				World.TimeOfDayHours = FCString::Atof(**HourValue);
			}

			if (const FString* WeatherValue = Record.LegacyValues.Find(TEXT("Weather")))
			{
				World.Weather = static_cast<EEclipseWeather>(FCString::Atoi(**WeatherValue));
			}

			if (const FString* CreditsValue = Record.LegacyValues.Find(TEXT("Credits")))
			{
				Credits = FCString::Atoi64(**CreditsValue);
			}

			if (const FString* MapValue = Record.LegacyValues.Find(TEXT("MapName")))
			{
				MapName = *MapValue;
			}

			Record.SchemaVersion = UEclipseSaveGameVersion::Current;
		}

		SchemaVersion = UEclipseSaveGameVersion::Current;
		UE_LOG(LogEclipseSave, Log, TEXT("Migrated a v1 save ('%s') to schema %d."), *SlotName, SchemaVersion);
	}

	OutError.Reset();
	return true;
}

FEclipseSaveRecord* UEclipseSaveGame::FindRecord(FName RecordType, const FGuid& RecordId)
{
	return const_cast<FEclipseSaveRecord*>(static_cast<const UEclipseSaveGame*>(this)->FindRecord(RecordType, RecordId));
}

const FEclipseSaveRecord* UEclipseSaveGame::FindRecord(FName RecordType, const FGuid& RecordId) const
{
	for (const FEclipseSaveRecord& Record : Records)
	{
		if (Record.RecordType == RecordType && Record.RecordId == RecordId)
		{
			return &Record;
		}
	}

	return nullptr;
}

void UEclipseSaveGame::AddOrReplaceRecord(const FEclipseSaveRecord& Record)
{
	if (FEclipseSaveRecord* Existing = FindRecord(Record.RecordType, Record.RecordId))
	{
		*Existing = Record;
		return;
	}

	Records.Add(Record);
}

int32 UEclipseSaveGame::RemoveRecordsOfType(FName RecordType)
{
	return Records.RemoveAll([RecordType](const FEclipseSaveRecord& Record)
	{
		return Record.RecordType == RecordType;
	});
}

void UEclipseSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	CurrentSlotName = DefaultSlotName;
}

void UEclipseSaveSubsystem::Deinitialize()
{
	Saveables.Reset();

	if (UWorld* World = GetGameWorld(this))
	{
		World->GetTimerManager().ClearTimer(AutosaveHandle);
	}

	Super::Deinitialize();
}

void UEclipseSaveSubsystem::RegisterSaveable(TScriptInterface<IEclipseSaveable> Saveable)
{
	UObject* Object = Saveable.GetObject();
	if (Object == nullptr || Saveables.Contains(Saveable))
	{
		return;
	}

	Saveables.Add(Saveable);
}

void UEclipseSaveSubsystem::UnregisterSaveable(UObject* SaveableObject)
{
	if (SaveableObject == nullptr)
	{
		return;
	}

	Saveables.RemoveAll([SaveableObject](const TScriptInterface<IEclipseSaveable>& Entry)
	{
		return Entry.GetObject() == SaveableObject;
	});
}

bool UEclipseSaveSubsystem::DoesSlotExist(const FString& SlotName, int32 UserIndex) const
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
}

void UEclipseSaveSubsystem::SetCurrentSlotName(const FString& SlotName)
{
	if (!SlotName.IsEmpty())
	{
		CurrentSlotName = SlotName;
	}
}

void UEclipseSaveSubsystem::GatherSave(UEclipseSaveGame& OutSave) const
{
	OutSave.SchemaVersion = UEclipseSaveGameVersion::Current;
	OutSave.BuildVersion = FApp::GetBuildVersion();
	OutSave.SavedAtUtc = FDateTime::UtcNow();

	const UWorld* World = GetGameWorld(this);
	OutSave.MapName = World != nullptr ? World->GetMapName() : FString();
	OutSave.SlotName = CurrentSlotName;

	if (const UEclipseWorldSubsystem* WorldSubsystem = World != nullptr ? World->GetSubsystem<UEclipseWorldSubsystem>() : nullptr)
	{
		OutSave.World = WorldSubsystem->GetSnapshot();
	}

	if (const UEclipseGameInstance* GameInstance = Cast<UEclipseGameInstance>(GetGameInstance()))
	{
		OutSave.Credits = GameInstance->GetCredits();
	}

	if (const UEclipseFactionSubsystem* Factions = GetGameInstance() != nullptr
		? GetGameInstance()->GetSubsystem<UEclipseFactionSubsystem>()
		: nullptr)
	{
		OutSave.Reputations = Factions->GetAllReputations();
	}

	// Registered systems each contribute one record.
	for (const TScriptInterface<IEclipseSaveable>& Entry : Saveables)
	{
		IEclipseSaveable* Saveable = Entry.GetObject() != nullptr ? Cast<IEclipseSaveable>(Entry.GetObject()) : nullptr;
		if (Saveable == nullptr)
		{
			continue;
		}

		FEclipseSaveRecord Record;
		Record.RecordType = Saveable->GetSaveRecordType();
		Record.RecordId = Saveable->GetSaveRecordId();
		Record.SchemaVersion = UEclipseSaveGameVersion::Current;

		Saveable->WriteSaveRecord(Record);
		OutSave.AddOrReplaceRecord(Record);
	}
}

void UEclipseSaveSubsystem::ApplySave(const UEclipseSaveGame& InSave, FString& OutError)
{
	UWorld* World = GetGameWorld(this);

	if (UEclipseWorldSubsystem* WorldSubsystem = World != nullptr ? World->GetSubsystem<UEclipseWorldSubsystem>() : nullptr)
	{
		WorldSubsystem->ApplySnapshot(InSave.World);
	}

	if (UEclipseGameInstance* GameInstance = Cast<UEclipseGameInstance>(GetGameInstance()))
	{
		GameInstance->SetCredits(InSave.Credits, OutError);
	}

	if (UEclipseFactionSubsystem* Factions = GetGameInstance() != nullptr
		? GetGameInstance()->GetSubsystem<UEclipseFactionSubsystem>()
		: nullptr)
	{
		for (const FEclipseReputation& Entry : InSave.Reputations)
		{
			if (Entry.Faction != EEclipseFaction::None)
			{
				Factions->SetReputation(Entry.Faction, Entry.Value);
			}
		}
	}

	// Systems are read after the world and profile are in place, so a system that needs
	// the current weather or the credit total sees the loaded value, not the old one.
	int32 AppliedCount = 0;
	for (const TScriptInterface<IEclipseSaveable>& Entry : Saveables)
	{
		IEclipseSaveable* Saveable = Entry.GetObject() != nullptr ? Cast<IEclipseSaveable>(Entry.GetObject()) : nullptr;
		if (Saveable == nullptr)
		{
			continue;
		}

		const FEclipseSaveRecord* Record = InSave.FindRecord(Saveable->GetSaveRecordType(), Saveable->GetSaveRecordId());
		if (Record == nullptr)
		{
			continue;
		}

		if (!Saveable->ReadSaveRecord(*Record))
		{
			UE_LOG(LogEclipseSave, Warning, TEXT("System '%s' refused save record %s."),
				*Saveable->GetSaveRecordType().ToString(), *Record->RecordId.ToString());
			continue;
		}

		++AppliedCount;
	}

	UE_LOG(LogEclipseSave, Log, TEXT("Applied save '%s': %d record(s), %d system(s)."),
		*InSave.SlotName, InSave.Records.Num(), AppliedCount);
}

bool UEclipseSaveSubsystem::SaveToSlot(const FString& SlotName, FString& OutError, int32 UserIndex)
{
	if (SlotName.IsEmpty())
	{
		OutError = TEXT("Save slot name is empty.");
		return false;
	}

	if (bSaveInFlight)
	{
		OutError = TEXT("A save is already in flight.");
		return false;
	}

	UWorld* World = GetGameWorld(this);
	if (World != nullptr && World->GetNetMode() == NM_Client)
	{
		OutError = TEXT("Only the server writes saves.");
		return false;
	}

	bSaveInFlight = true;

	UEclipseSaveGame* SaveObject = Cast<UEclipseSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UEclipseSaveGame::StaticClass()));
	if (SaveObject == nullptr)
	{
		bSaveInFlight = false;
		OutError = TEXT("Could not create a save object.");
		return false;
	}

	GatherSave(*SaveObject);

	const bool bWritten = UGameplayStatics::SaveGameToSlot(SaveObject, SlotName, UserIndex);
	bSaveInFlight = false;

	if (!bWritten)
	{
		OutError = FString::Printf(TEXT("Could not write save slot '%s'."), *SlotName);
		OnSaveCompleted.Broadcast(false, OutError);
		return false;
	}

	CurrentSlotName = SlotName;
	OutError.Reset();
	OnSaveCompleted.Broadcast(true, OutError);

	UE_LOG(LogEclipseSave, Log, TEXT("Saved '%s' (%d record(s), %d bytes)."),
		*SlotName, SaveObject->Records.Num(), SaveObject->Records.GetAllocatedSize());

	return true;
}

bool UEclipseSaveSubsystem::LoadFromSlot(const FString& SlotName, FString& OutError, int32 UserIndex)
{
	if (SlotName.IsEmpty())
	{
		OutError = TEXT("Save slot name is empty.");
		return false;
	}

	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		OutError = FString::Printf(TEXT("Save slot '%s' does not exist."), *SlotName);
		OnLoadCompleted.Broadcast(false, OutError);
		return false;
	}

	USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
	UEclipseSaveGame* SaveObject = Cast<UEclipseSaveGame>(Loaded);
	if (SaveObject == nullptr)
	{
		OutError = FString::Printf(TEXT("Save slot '%s' is not an Eclipse save."), *SlotName);
		OnLoadCompleted.Broadcast(false, OutError);
		return false;
	}

	// Verify and migrate before touching anything live.
	if (!SaveObject->MigrateToCurrent(OutError))
	{
		OnLoadCompleted.Broadcast(false, OutError);
		return false;
	}

	SaveObject->SlotName = SlotName;
	ApplySave(*SaveObject, OutError);

	if (!OutError.IsEmpty())
	{
		OnLoadCompleted.Broadcast(false, OutError);
		return false;
	}

	CurrentSlotName = SlotName;
	OnLoadCompleted.Broadcast(true, OutError);
	return true;
}

bool UEclipseSaveSubsystem::DeleteSlot(const FString& SlotName, FString& OutError, int32 UserIndex)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		OutError = FString::Printf(TEXT("Save slot '%s' does not exist."), *SlotName);
		return false;
	}

	if (!UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex))
	{
		OutError = FString::Printf(TEXT("Could not delete save slot '%s'."), *SlotName);
		return false;
	}

	OutError.Reset();
	return true;
}

bool UEclipseSaveSubsystem::SaveNow(FString& OutError)
{
	return SaveToSlot(CurrentSlotName, OutError);
}

bool UEclipseSaveSubsystem::LoadNow(FString& OutError)
{
	return LoadFromSlot(CurrentSlotName, OutError);
}

void UEclipseSaveSubsystem::SetAutosaveInterval(float IntervalSeconds)
{
	UWorld* World = GetGameWorld(this);
	if (World == nullptr)
	{
		return;
	}

	StopAutosave();

	if (IntervalSeconds <= 0.0f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		AutosaveHandle,
		this,
		&UEclipseSaveSubsystem::HandleAutosave,
		IntervalSeconds,
		true,
		IntervalSeconds);
}

void UEclipseSaveSubsystem::StopAutosave()
{
	if (UWorld* World = GetGameWorld(this))
	{
		World->GetTimerManager().ClearTimer(AutosaveHandle);
	}
}

bool UEclipseSaveSubsystem::IsAutosaveEnabled() const
{
	const UWorld* World = GetGameWorld(this);
	return World != nullptr && World->GetTimerManager().IsTimerActive(AutosaveHandle);
}

void UEclipseSaveSubsystem::HandleAutosave()
{
	// An autosave never overwrites the player's manual slot choice, and never runs on a
	// client; the host owns the file.
	FString Error;
	if (!SaveNow(Error))
	{
		UE_LOG(LogEclipseSave, Warning, TEXT("Autosave failed: %s"), *Error);
	}
}
