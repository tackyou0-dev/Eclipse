// PROJECT ECLIPSE - Weapon component implementation.
//
// Purpose
//   Weapon rack, reload timing and heat venting. Nothing here applies damage; the combat
//   component owns the shot and this component owns what the shot costs.

#include "Weapons/EclipseWeaponComponent.h"

#include "Core/EclipseLog.h"
#include "GameFramework/Actor.h"

UEclipseWeaponComponent::UEclipseWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UEclipseWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	for (const EEclipseEquipmentSlot Slot : DefaultSlots)
	{
		if (Slot != EEclipseEquipmentSlot::None && !EquippedWeapons.Contains(Slot))
		{
			EquippedWeapons.Add(Slot, nullptr);
		}
	}

	if (ActiveSlot == EEclipseEquipmentSlot::None && DefaultSlots.Num() > 0)
	{
		ActiveSlot = DefaultSlots[0];
	}
}

void UEclipseWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	// Reload countdown.
	if (bReloading)
	{
		ReloadTimeRemaining -= DeltaTime;
		if (ReloadTimeRemaining <= 0.0f)
		{
			bReloading = false;
			ReloadTimeRemaining = 0.0f;

			if (UEclipseWeaponInstance* Weapon = GetActiveWeapon())
			{
				const int32 Loaded = Weapon->LoadAmmo(Weapon->GetMagazineCapacity());
				UE_LOG(LogEclipseCombat, Verbose, TEXT("%s reloaded %d rounds."), *Weapon->GetWeaponId().ToString(), Loaded);
			}
		}
	}

	// Heat venting. Heat only falls while the trigger is released, which is what makes
	// sustained fire feel like a resource rather than a delay.
	if (Heat > 0.0f)
	{
		Heat = FMath::Max(0.0f, Heat - HeatVentPerSecond * DeltaTime);
	}

	if (bOverheated && Heat <= OverheatReleaseThreshold)
	{
		bOverheated = false;
		UE_LOG(LogEclipseCombat, Verbose, TEXT("%s cooled down and can fire again."),
			GetOwner() != nullptr ? *GetOwner()->GetName() : TEXT("<component>"));
	}
}

UEclipseWeaponInstance* UEclipseWeaponComponent::EquipWeapon(UEclipseWeaponDefinition* Definition, EEclipseEquipmentSlot Slot)
{
	if (Definition == nullptr || Slot == EEclipseEquipmentSlot::None)
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("EquipWeapon called without a definition or slot."));
		return nullptr;
	}

	if (!Definition->IsValidDefinition())
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("EquipWeapon refused %s: the definition is incomplete."),
			*Definition->WeaponId.ToString());
		return nullptr;
	}

	UEclipseWeaponInstance* Instance = NewObject<UEclipseWeaponInstance>(this);
	Instance->InitializeFromDefinition(Definition);

	if (const AActor* Owner = GetOwner())
	{
		Instance->SetOwnerStatComponent(Owner->FindComponentByClass<UEclipseStatComponent>());
	}

	EquippedWeapons.Add(Slot, Instance);

	if (ActiveSlot == EEclipseEquipmentSlot::None)
	{
		ActiveSlot = Slot;
		OnActiveWeaponChanged.Broadcast(Instance, ActiveSlot);
	}

	return Instance;
}

UEclipseWeaponInstance* UEclipseWeaponComponent::UnequipSlot(EEclipseEquipmentSlot Slot)
{
	TObjectPtr<UEclipseWeaponInstance>* Found = EquippedWeapons.Find(Slot);
	if (Found == nullptr || *Found == nullptr)
	{
		return nullptr;
	}

	UEclipseWeaponInstance* Removed = *Found;
	EquippedWeapons.Remove(Slot);

	if (ActiveSlot == Slot)
	{
		CancelReload();
		ActiveSlot = EEclipseEquipmentSlot::None;

		// Fall back to any remaining weapon so the character is never left holding a
		// pointer to something that no longer exists.
		for (const TPair<EEclipseEquipmentSlot, TObjectPtr<UEclipseWeaponInstance>>& Pair : EquippedWeapons)
		{
			if (Pair.Value != nullptr)
			{
				ActiveSlot = Pair.Key;
				break;
			}
		}

		OnActiveWeaponChanged.Broadcast(GetActiveWeapon(), ActiveSlot);
	}

	return Removed;
}

UEclipseWeaponInstance* UEclipseWeaponComponent::GetWeaponInSlot(EEclipseEquipmentSlot Slot) const
{
	if (const TObjectPtr<UEclipseWeaponInstance>* Found = EquippedWeapons.Find(Slot))
	{
		return *Found;
	}
	return nullptr;
}

UEclipseWeaponInstance* UEclipseWeaponComponent::GetActiveWeapon() const
{
	return GetWeaponInSlot(ActiveSlot);
}

bool UEclipseWeaponComponent::SetActiveSlot(EEclipseEquipmentSlot Slot)
{
	if (Slot == ActiveSlot)
	{
		return true;
	}

	UEclipseWeaponInstance* Weapon = GetWeaponInSlot(Slot);
	if (Weapon == nullptr)
	{
		return false;
	}

	CancelReload();
	ActiveSlot = Slot;
	OnActiveWeaponChanged.Broadcast(Weapon, ActiveSlot);
	return true;
}

bool UEclipseWeaponComponent::CycleActiveSlot()
{
	if (EquippedWeapons.Num() <= 1)
	{
		return false;
	}

	TArray<EEclipseEquipmentSlot> Slots;
	for (const TPair<EEclipseEquipmentSlot, TObjectPtr<UEclipseWeaponInstance>>& Pair : EquippedWeapons)
	{
		if (Pair.Value != nullptr)
		{
			Slots.Add(Pair.Key);
		}
	}

	if (Slots.Num() <= 1)
	{
		return false;
	}

	Slots.Sort([](const EEclipseEquipmentSlot& Left, const EEclipseEquipmentSlot& Right)
	{
		return static_cast<uint8>(Left) < static_cast<uint8>(Right);
	});

	const int32 CurrentIndex = Slots.IndexOfByKey(ActiveSlot);
	const int32 NextIndex = (CurrentIndex + 1) % Slots.Num();
	return SetActiveSlot(Slots[NextIndex]);
}

bool UEclipseWeaponComponent::ConsumeAmmo(int32 Count)
{
	UEclipseWeaponInstance* Weapon = GetActiveWeapon();
	return Weapon != nullptr && Weapon->ConsumeAmmo(Count);
}

int32 UEclipseWeaponComponent::GetAmmoInMagazine() const
{
	const UEclipseWeaponInstance* Weapon = GetActiveWeapon();
	return Weapon != nullptr ? Weapon->GetAmmoInMagazine() : 0;
}

int32 UEclipseWeaponComponent::GetMagazineCapacity() const
{
	const UEclipseWeaponInstance* Weapon = GetActiveWeapon();
	return Weapon != nullptr ? Weapon->GetMagazineCapacity() : 0;
}

bool UEclipseWeaponComponent::StartReload()
{
	UEclipseWeaponInstance* Weapon = GetActiveWeapon();
	if (Weapon == nullptr || bReloading)
	{
		return false;
	}

	if (!Weapon->NeedsReload())
	{
		return false;
	}

	// Ammunition comes from the inventory, reached through the owner's delegate. A weapon
	// with no handler (an AI with infinite ammo, for example) reloads for free.
	int32 Supplied = Weapon->GetMagazineCapacity();
	if (OnReloadAmmoRequested.IsBound())
	{
		Supplied = OnReloadAmmoRequested.Execute(Weapon->GetMagazineCapacity() - Weapon->GetAmmoInMagazine());
	}

	if (Supplied <= 0)
	{
		UE_LOG(LogEclipseItems, Verbose, TEXT("Reload refused: no %s available."), *Weapon->GetAmmoItemId().ToString());
		return false;
	}

	// Rounds are taken now and loaded when the timer finishes, so a cancelled reload can
	// return them (the character's inventory owns that refund).
	PendingReloadRounds = Supplied;
	bReloading = true;
	ReloadDuration = FMath::Max(0.05f, Weapon->GetStat(EEclipseStat::ReloadSeconds));
	ReloadTimeRemaining = ReloadDuration;
	return true;
}

void UEclipseWeaponComponent::CancelReload()
{
	bReloading = false;
	ReloadTimeRemaining = 0.0f;
	PendingReloadRounds = 0;
}

float UEclipseWeaponComponent::GetReloadProgress() const
{
	if (!bReloading || ReloadDuration <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(1.0f - (ReloadTimeRemaining / ReloadDuration), 0.0f, 1.0f);
}

void UEclipseWeaponComponent::AddHeat(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	Heat = FMath::Min(Heat + Amount, OverheatThreshold);
	if (Heat >= OverheatThreshold && !bOverheated)
	{
		bOverheated = true;
		UE_LOG(LogEclipseCombat, Log, TEXT("%s overheated."), GetOwner() != nullptr ? *GetOwner()->GetName() : TEXT("<component>"));
	}
}

void UEclipseWeaponComponent::VentHeat(float Amount)
{
	Heat = FMath::Max(0.0f, Heat - FMath::Max(0.0f, Amount));
	if (bOverheated && Heat <= OverheatReleaseThreshold)
	{
		bOverheated = false;
	}
}

bool UEclipseWeaponComponent::AddAttachment(EEclipseEquipmentSlot WeaponSlot, UEclipseAttachmentDefinition* Attachment, FName ItemId, FName& OutRejectReason)
{
	UEclipseWeaponInstance* Weapon = GetWeaponInSlot(WeaponSlot);
	if (Weapon == nullptr)
	{
		OutRejectReason = TEXT("No weapon in that slot.");
		return false;
	}

	return Weapon->AddAttachment(Attachment, ItemId, OutRejectReason);
}

FName UEclipseWeaponComponent::RemoveAttachment(EEclipseEquipmentSlot WeaponSlot, EEclipseAttachmentSlot AttachmentSlot)
{
	UEclipseWeaponInstance* Weapon = GetWeaponInSlot(WeaponSlot);
	return Weapon != nullptr ? Weapon->RemoveAttachment(AttachmentSlot) : NAME_None;
}

float UEclipseWeaponComponent::GetStat(EEclipseStat Stat) const
{
	const UEclipseWeaponInstance* Weapon = GetActiveWeapon();
	return Weapon != nullptr ? Weapon->GetStat(Stat) : 0.0f;
}

void UEclipseWeaponComponent::RefreshWeaponStats()
{
	for (const TPair<EEclipseEquipmentSlot, TObjectPtr<UEclipseWeaponInstance>>& Pair : EquippedWeapons)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->InvalidateStatCache();
		}
	}
}
