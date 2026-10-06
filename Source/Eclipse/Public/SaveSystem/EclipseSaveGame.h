// PROJECT ECLIPSE - Save file and save service.
//
// Purpose
//   One save file format for the whole game, and one service that fills it. A save is a
//   header (schema version, build, map, world snapshot, profile totals) plus a list of
//   typed records: each system that wants to persist registers itself as an
//   IEclipseSaveable and writes one record, keyed by a deterministic id.
//
// Schema history
//   v1 stored a flat FString map per system. It is still readable: MigrateToCurrent
//   converts v1 records (and v1 profile totals) into v2 records, so a save from the first
//   playtest still loads. A save written by a newer build is refused rather than
//   half-read, which is the only safe option.
//
// Authority
//   Saving is a server action in multiplayer: a client asking to save is asking the host
//   to write the raid's state, not its own copy of it.
//
// The record type and IEclipseSaveable live together in this header on purpose: the
// interface's contract is "write exactly one FEclipseSaveRecord", and splitting them
// across files invites the two to drift.

#pragma once

#include "Core/EclipseTypes.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Factions/EclipseFactionTypes.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "World/EclipseWorldTypes.h"
#include "EclipseSaveGame.generated.h"

/** Delegate fired when a save finishes (successfully or not). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FEclipseSaveCompleted, bool /*bSuccess*/, const FString& /*Error*/);

/** Delegate fired when a load finishes. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FEclipseLoadCompleted, bool /*bSuccess*/, const FString& /*Error*/);

/**
 * One system's slice of a save.
 *
 * Payload is a serialised struct written with FMemoryWriter; LegacyValues carries the
 * v1 string map for records that predate the byte payload.
 */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseSaveRecord
{
	GENERATED_BODY()

	/** Record kind, for example "World", "Player", "Inventory". */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	FName RecordType;

	/** Deterministic id of the system instance this record belongs to. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	FGuid RecordId;

	/** Schema version the record was written with. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	int32 SchemaVersion = 1;

	/** Serialised payload. Empty for legacy records. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	TArray<uint8> Payload;

	/** v1 key/value pairs, kept so an old save can be migrated instead of discarded. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	TMap<FString, FString> LegacyValues;

	/** True when there is anything to read. */
	bool HasData() const
	{
		return Payload.Num() > 0 || LegacyValues.Num() > 0;
	}
};

/** Helper used by saveables to pack and unpack their payload. */
namespace EclipseSave
{
	/** Deterministic record id from a type and a key. */
	ECLIPSE_API FGuid MakeRecordId(FName RecordType, const FString& Key);

	/** Serialise a struct into a record's payload. */
	template <typename T>
	void WriteStruct(FEclipseSaveRecord& Record, const T& Value)
	{
		Record.Payload.Reset();

		FMemoryWriter Writer(Record.Payload, true);
		T ValueCopy = Value;
		Writer << ValueCopy;
	}

	/** Deserialise a record's payload. Returns false for an empty or malformed payload. */
	template <typename T>
	bool ReadStruct(const FEclipseSaveRecord& Record, T& OutValue)
	{
		if (Record.Payload.Num() == 0)
		{
			return false;
		}

		FMemoryReader Reader(Record.Payload, true);
		Reader << OutValue;
		return !Reader.IsError();
	}
}

/**
 * Interface implemented by anything that persists itself.
 *
 * The save service owns the file; saveables own their own serialisation, which is what
 * keeps the format from becoming a god struct that every system edits.
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UEclipseSaveable : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by a system that writes one record into the save. */
class ECLIPSE_API IEclipseSaveable
{
	GENERATED_BODY()

public:
	/** Record type this system writes. Must be stable across builds. */
	virtual FName GetSaveRecordType() const = 0;

	/** Record id of this instance. Use EclipseSave::MakeRecordId for a stable id. */
	virtual FGuid GetSaveRecordId() const = 0;

	/** Write this system's state into a record. */
	virtual void WriteSaveRecord(FEclipseSaveRecord& Record) const = 0;

	/** Restore this system's state from a record. Return false when the record is unusable. */
	virtual bool ReadSaveRecord(const FEclipseSaveRecord& Record) = 0;
};

/**
 * The save file.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/** Schema version this file was written with. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	int32 SchemaVersion = UEclipseSaveGameVersion::Current;

	/** Version string of the build that wrote the file, for diagnostics only. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	FString BuildVersion;

	/** UTC time the file was written. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	FDateTime SavedAtUtc;

	/** Map the save was taken in. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	FString MapName;

	/** Slot this file was loaded from or written to. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	FString SlotName;

	/** World clock and weather at the moment of saving. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	FEclipseWorldSnapshot World;

	/** Profile credits. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	int64 Credits = 0;

	/** Reputation per faction. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	TArray<FEclipseReputation> Reputations;

	/** Missions the profile has finished, for the hub's mission board. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	TArray<FName> CompletedMissionIds;

	/** Typed records written by saveable systems. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Save")
	TArray<FEclipseSaveRecord> Records;

	/** True when this build can read the file. */
	bool IsCompatibleWithThisBuild() const
	{
		return SchemaVersion >= UEclipseSaveGameVersion::MinSupported && SchemaVersion <= UEclipseSaveGameVersion::Current;
	}

	/** Upgrade the file in place. Returns false (with a reason) when it cannot be read. */
	bool MigrateToCurrent(FString& OutError);

	/** Find a record, or null. */
	FEclipseSaveRecord* FindRecord(FName RecordType, const FGuid& RecordId);

	/** Find a record, or null. */
	const FEclipseSaveRecord* FindRecord(FName RecordType, const FGuid& RecordId) const;

	/** Add a record, replacing any existing record with the same type and id. */
	void AddOrReplaceRecord(const FEclipseSaveRecord& Record);

	/** Remove every record of a type. */
	int32 RemoveRecordsOfType(FName RecordType);
};

/**
 * Game-instance-scoped save service.
 *
 * Owns the slot list, the autosave timer and the record round-trip. The world subsystem
 * and the profile are gathered here because they are always present; everything else
 * registers itself.
 */
UCLASS()
class ECLIPSE_API UEclipseSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** Slot a fresh profile is written to. */
	static const FString DefaultSlotName;

	/** UGameInstanceSubsystem. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** UGameInstanceSubsystem: flushes an autosave if one is pending. */
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	// Registration
	// ------------------------------------------------------------------

	/** Register a system that wants to persist. Duplicate objects are ignored. */
	void RegisterSaveable(TScriptInterface<IEclipseSaveable> Saveable);

	/** Stop persisting a system. Called before it is destroyed. */
	void UnregisterSaveable(UObject* SaveableObject);

	/** How many systems are registered. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Save")
	int32 GetRegisteredSaveableCount() const { return Saveables.Num(); }

	// ------------------------------------------------------------------
	// Slots
	// ------------------------------------------------------------------

	/** True when a slot exists on disk. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Save")
	bool DoesSlotExist(const FString& SlotName, int32 UserIndex = 0) const;

	/** Save the current state to a slot. Server only. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Save")
	bool SaveToSlot(const FString& SlotName, FString& OutError, int32 UserIndex = 0);

	/** Load a slot and apply it. Server only. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Save")
	bool LoadFromSlot(const FString& SlotName, FString& OutError, int32 UserIndex = 0);

	/** Delete a slot. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Save")
	bool DeleteSlot(const FString& SlotName, FString& OutError, int32 UserIndex = 0);

	/** Save to the current slot (DefaultSlotName until one is chosen). */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Save")
	bool SaveNow(FString& OutError);

	/** Load the current slot. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Save")
	bool LoadNow(FString& OutError);

	/** Slot name used by SaveNow/LoadNow. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Save")
	const FString& GetCurrentSlotName() const { return CurrentSlotName; }

	/** Choose the slot SaveNow writes to. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Save")
	void SetCurrentSlotName(const FString& SlotName);

	// ------------------------------------------------------------------
	// Autosave
	// ------------------------------------------------------------------

	/** Start periodic autosaving. Zero or negative disables it. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Save")
	void SetAutosaveInterval(float IntervalSeconds);

	/** Stop autosaving (for example during a boss fight, where a hitch matters). */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Save")
	void StopAutosave();

	/** True while an autosave timer is running. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Save")
	bool IsAutosaveEnabled() const;

	/** Fired after every save attempt. */
	FEclipseSaveCompleted OnSaveCompleted;

	/** Fired after every load attempt. */
	FEclipseLoadCompleted OnLoadCompleted;

	/** Default seconds between autosaves. */
	static constexpr float DefaultAutosaveInterval = 180.0f;

protected:
	/** Fill a save object from the live world and the registered saveables. */
	void GatherSave(UEclipseSaveGame& OutSave) const;

	/** Apply a save object to the live world and the registered saveables. */
	void ApplySave(const UEclipseSaveGame& InSave, FString& OutError);

	/** Autosave timer callback. */
	void HandleAutosave();

	/** Registered saveables. */
	UPROPERTY()
	TArray<TScriptInterface<IEclipseSaveable>> Saveables;

	/** Slot SaveNow/LoadNow use. */
	FString CurrentSlotName;

	/** Autosave timer. */
	FTimerHandle AutosaveHandle;

	/** True while a save is being written, so an autosave cannot stack on a manual one. */
	bool bSaveInFlight = false;
};
