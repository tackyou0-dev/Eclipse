// PROJECT ECLIPSE - Weapon component.
//
// Purpose
//   The character's weapon rack: which weapons are equipped, which one is active, and
//   the shared state that is not per-weapon - reload timing, heat, and the attachment
//   workbench operations.
//
// Authority
//   Equipping, reloading and attachment changes are server-authoritative. Clients call
//   the public API, which forwards to the server through the owning character's
//   Server RPCs; the resulting state replicates on the weapon instances.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Weapons/EclipseWeaponInstance.h"
#include "EclipseWeaponComponent.generated.h"

/**
 * Called when a reload needs ammunition. The character that owns the weapon implements
 * this and pulls rounds from the inventory, which keeps the weapon system independent of
 * the inventory system.
 *
 * Return the number of rounds actually supplied.
 */
DECLARE_DELEGATE_RetVal_OneParam(int32, FEclipseReloadAmmoRequest, int32 /*RequestedRounds*/);

/** Fired when the active weapon changes. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FEclipseActiveWeaponChanged, UEclipseWeaponInstance* /*Weapon*/, EEclipseEquipmentSlot /*Slot*/);

/**
 * Equipped weapons, reload state and heat for one character.
 *
 * Heat is modelled per character rather than per weapon because the vertical slice only
 * has one heat source at a time (the overcharge core), and a per-weapon heat pool would
 * be dead weight until more energy weapons exist.
 */
UCLASS(ClassGroup = (Eclipse), meta = (BlueprintSpawnableComponent))
class ECLIPSE_API UEclipseWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEclipseWeaponComponent();

	/** UActorComponent: pick a starting weapon slot and start ticking. */
	virtual void BeginPlay() override;

	/** UActorComponent: reload countdown and heat venting. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	// Equipping
	// ------------------------------------------------------------------

	/** Create and equip a weapon in a slot. Replaces whatever was there. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weapon")
	UEclipseWeaponInstance* EquipWeapon(UEclipseWeaponDefinition* Definition, EEclipseEquipmentSlot Slot);

	/** Remove a weapon from a slot and return it so the inventory can take it back. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weapon")
	UEclipseWeaponInstance* UnequipSlot(EEclipseEquipmentSlot Slot);

	/** Weapon in a slot, or null. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	UEclipseWeaponInstance* GetWeaponInSlot(EEclipseEquipmentSlot Slot) const;

	/** The weapon the player is currently holding. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	UEclipseWeaponInstance* GetActiveWeapon() const;

	/** Switch to a slot. Returns false when the slot is empty. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weapon")
	bool SetActiveSlot(EEclipseEquipmentSlot Slot);

	/** Current slot. None when the character is unarmed. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	EEclipseEquipmentSlot GetActiveSlot() const { return ActiveSlot; }

	/** Cycle to the next non-empty slot, skipping the current one. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weapon")
	bool CycleActiveSlot();

	// ------------------------------------------------------------------
	// Ammunition and reload
	// ------------------------------------------------------------------

	/** Consume rounds from the active weapon. */
	bool ConsumeAmmo(int32 Count);

	/** Rounds in the active weapon's magazine. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	int32 GetAmmoInMagazine() const;

	/** Magazine capacity of the active weapon. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	int32 GetMagazineCapacity() const;

	/**
	 * Begin a reload. Requests ammunition through OnReloadAmmoRequested and starts the
	 * weapon's reload timer. Returns false when there is nothing to do or no ammunition
	 * was supplied.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weapon")
	bool StartReload();

	/** Cancel an in-progress reload (weapon switch, death). */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weapon")
	void CancelReload();

	/** True while a reload is in progress. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	bool IsReloading() const { return bReloading; }

	/** Reload progress in [0, 1], for the HUD ring. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	float GetReloadProgress() const;

	// ------------------------------------------------------------------
	// Heat
	// ------------------------------------------------------------------

	/** Current heat. Firing is refused at or above the overheat threshold. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	float GetHeat() const { return Heat; }

	/** Add heat from a shot. */
	void AddHeat(float Amount);

	/** Remove heat (venting, coolant, water). */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weapon")
	void VentHeat(float Amount);

	/** True while overheated. Clears once heat falls below the release threshold. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	bool IsOverheated() const { return bOverheated; }

	// ------------------------------------------------------------------
	// Attachments
	// ------------------------------------------------------------------

	/** Fit an attachment to a weapon in a slot. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weapon")
	bool AddAttachment(EEclipseEquipmentSlot WeaponSlot, UEclipseAttachmentDefinition* Attachment, FName ItemId, FName& OutRejectReason);

	/** Detach from a weapon in a slot, returning the item id that was removed. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weapon")
	FName RemoveAttachment(EEclipseEquipmentSlot WeaponSlot, EEclipseAttachmentSlot AttachmentSlot);

	/** Effective stat of the active weapon. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Weapon")
	float GetStat(EEclipseStat Stat) const;

	/** Called by the owner when its modifier sources change, so weapon stats refresh. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Weapon")
	void RefreshWeaponStats();

	/** Heat generated past which firing is refused. */
	static constexpr float OverheatThreshold = 100.0f;

	/** Heat at which the weapon is usable again after overheating. */
	static constexpr float OverheatReleaseThreshold = 40.0f;

	/** Heat vented per second while not firing. */
	static constexpr float HeatVentPerSecond = 12.0f;

	/** Ammunition request handler, implemented by the owning character. */
	FEclipseReloadAmmoRequest OnReloadAmmoRequested;

	/** Fired when the active weapon changes. */
	FEclipseActiveWeaponChanged OnActiveWeaponChanged;

protected:
	/** Slots the character should start with. Filled from the character's loadout. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Weapon")
	TArray<EEclipseEquipmentSlot> DefaultSlots;

private:
	/** Equipped weapons by slot. */
	UPROPERTY()
	TMap<EEclipseEquipmentSlot, TObjectPtr<UEclipseWeaponInstance>> EquippedWeapons;

	/** Slot the character is holding. */
	UPROPERTY()
	EEclipseEquipmentSlot ActiveSlot = EEclipseEquipmentSlot::None;

	/** True while a reload is running. */
	bool bReloading = false;

	/** Seconds remaining on the reload. */
	float ReloadTimeRemaining = 0.0f;

	/** Duration of the current reload, for progress reporting. */
	float ReloadDuration = 0.0f;

	/** Heat accumulated by firing. */
	float Heat = 0.0f;

	/** True while overheated. */
	bool bOverheated = false;

	/** Rounds handed over for the current reload, loaded when the timer finishes. */
	int32 PendingReloadRounds = 0;
};
