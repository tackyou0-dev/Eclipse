// PROJECT ECLIPSE - Runtime weapon instance.
//
// Purpose
//   The stateful half of a weapon: which attachments are fitted, how much ammunition is
//   in the magazine, and what the effective stats are once attachments and the owner's
//   stat modifiers have been folded in.
//
// Why not on the component
//   A character carries up to three weapons. Keeping the per-weapon state in its own
//   object makes "switch weapon" a pointer swap and makes saving a weapon a matter of
//   serialising one object.

#pragma once

#include "Core/EclipseStatComponent.h"
#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "UObject/Object.h"
#include "Weapons/EclipseWeaponDefinition.h"
#include "EclipseWeaponInstance.generated.h"

/** One fitted attachment, kept together with the definition that produced it. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseFittedAttachment
{
	GENERATED_BODY()

	/** Data asset describing the attachment. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon")
	TObjectPtr<UEclipseAttachmentDefinition> Definition = nullptr;

	/** Item definition id, so removing the attachment can hand the item back. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon")
	FName ItemId;
};

/**
 * A weapon the character is carrying.
 *
 * Stats are cached lazily: GetStat recomputes from the definition plus every fitted
 * attachment plus the owner's modifier sources, and the cache is invalidated whenever an
 * attachment changes or the owner's stat component reports a change.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseWeaponInstance : public UObject
{
	GENERATED_BODY()

public:
	/** Point this instance at a weapon definition. Clears attachments and refills the magazine. */
	void InitializeFromDefinition(UEclipseWeaponDefinition* InDefinition);

	/** Weapon definition, never null after initialisation. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	UEclipseWeaponDefinition* GetDefinition() const { return Definition; }

	/** Weapon id, for example "AR-01". */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	FName GetWeaponId() const;

	// ------------------------------------------------------------------
	// Attachments
	// ------------------------------------------------------------------

	/**
	 * Fit an attachment. Replaces any attachment already in that slot.
	 * Returns false when the weapon does not accept the slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weapon")
	bool AddAttachment(UEclipseAttachmentDefinition* Attachment, FName ItemId, FName& OutRejectReason);

	/** Remove whatever is fitted in a slot. Returns the removed item id, or None. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weapon")
	FName RemoveAttachment(EEclipseAttachmentSlot Slot);

	/** Fitted attachment in a slot, or null. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	UEclipseAttachmentDefinition* GetAttachment(EEclipseAttachmentSlot Slot) const;

	/** Every fitted attachment. */
	const TArray<FEclipseFittedAttachment>& GetAttachments() const { return FittedAttachments; }

	/** All slots this weapon accepts, from the definition. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	TArray<EEclipseAttachmentSlot> GetAvailableSlots() const;

	// ------------------------------------------------------------------
	// Stats
	// ------------------------------------------------------------------

	/**
	 * Effective value of a stat: definition base, then attachments, then the owner's
	 * stat modifiers, applied as (Base + Additive) * Multiplicative.
	 */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	float GetStat(EEclipseStat Stat) const;

	/** Every value at once, for the weapon stats panel. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	FEclipseWeaponStats GetEffectiveStats() const;

	/** Stat modifiers contributed by the fitted attachments. */
	TArray<FEclipseStatModifier> BuildAttachmentModifiers() const;

	/** Attach the owning character's stat component so its modifiers are included. */
	void SetOwnerStatComponent(UEclipseStatComponent* InStatComponent);

	/** Recompute the cached stats on the next query. */
	void InvalidateStatCache();

	// ------------------------------------------------------------------
	// Ammunition
	// ------------------------------------------------------------------

	/** Rounds currently in the magazine. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	int32 GetAmmoInMagazine() const { return AmmoInMagazine; }

	/** Magazine size after attachments. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	int32 GetMagazineCapacity() const;

	/** True when the magazine is empty. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	bool IsEmpty() const { return AmmoInMagazine <= 0; }

	/** True when the magazine is not full, so a reload would do something. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	bool NeedsReload() const { return AmmoInMagazine < GetMagazineCapacity(); }

	/** Consume rounds. Returns false and consumes nothing when there are too few. */
	bool ConsumeAmmo(int32 Count);

	/** Load rounds into the magazine, clamped to capacity. Returns how many were loaded. */
	int32 LoadAmmo(int32 Count);

	/** Fill the magazine to capacity. Used on spawn and when restocking in the hub. */
	void RefillMagazine();

	/** Item id of the ammunition this weapon eats. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	FName GetAmmoItemId() const;

private:
	/** Rebuild CachedStats from the definition, attachments and owner modifiers. */
	void BuildStatCache() const;

	/** Definition this instance was created from. */
	UPROPERTY()
	TObjectPtr<UEclipseWeaponDefinition> Definition = nullptr;

	/** Fitted attachments, at most one per slot. */
	UPROPERTY()
	TArray<FEclipseFittedAttachment> FittedAttachments;

	/** Rounds in the magazine. */
	UPROPERTY()
	int32 AmmoInMagazine = 0;

	/** Stat source used to include the owner's modifiers in GetStat. */
	UPROPERTY()
	TObjectPtr<UEclipseStatComponent> OwnerStatComponent = nullptr;

	/** Cached effective stats. Rebuilt when invalidated. */
	mutable TMap<EEclipseStat, float> CachedStats;

	/** True when CachedStats reflects the current attachments and owner modifiers. */
	mutable bool bCacheValid = false;
};
