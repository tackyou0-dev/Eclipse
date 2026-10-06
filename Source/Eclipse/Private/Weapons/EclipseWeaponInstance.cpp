// PROJECT ECLIPSE - Runtime weapon instance implementation.
//
// Purpose
//   Attachment management and stat folding. The order - definition base, attachment
//   modifiers, owner stat modifiers - is part of the balance contract and is mirrored by
//   the reference model's WeaponStats.effective().

#include "Weapons/EclipseWeaponInstance.h"

#include "Core/EclipseLog.h"

namespace
{
	/** Stats a weapon can meaningfully report. Keeps the cache bounded and predictable. */
	const EEclipseStat GWeaponStats[] =
	{
		EEclipseStat::Damage,
		EEclipseStat::EffectiveRange,
		EEclipseStat::MaxRange,
		EEclipseStat::SpreadDegrees,
		EEclipseStat::RecoilVertical,
		EEclipseStat::RecoilHorizontal,
		EEclipseStat::RoundsPerMinute,
		EEclipseStat::ReloadSeconds,
		EEclipseStat::MagazineCapacity,
		EEclipseStat::AimDownSightsSeconds,
		EEclipseStat::ArmorPenetration,
		EEclipseStat::NoiseRadius,
		EEclipseStat::Weight,
		EEclipseStat::HeatPerShot
	};
}

void UEclipseWeaponInstance::InitializeFromDefinition(UEclipseWeaponDefinition* InDefinition)
{
	Definition = InDefinition;
	FittedAttachments.Reset();
	AmmoInMagazine = GetMagazineCapacity();
	InvalidateStatCache();
}

FName UEclipseWeaponInstance::GetWeaponId() const
{
	return Definition != nullptr ? Definition->WeaponId : NAME_None;
}

bool UEclipseWeaponInstance::AddAttachment(UEclipseAttachmentDefinition* Attachment, FName ItemId, FName& OutRejectReason)
{
	if (Definition == nullptr)
	{
		OutRejectReason = TEXT("Weapon has no definition.");
		return false;
	}

	if (Attachment == nullptr || !Attachment->IsValidDefinition())
	{
		OutRejectReason = TEXT("Attachment definition is invalid.");
		return false;
	}

	if (!Definition->AvailableSlots.Contains(Attachment->Slot))
	{
		OutRejectReason = FString::Printf(TEXT("%s does not accept a %s attachment."),
			*Definition->WeaponId.ToString(),
			*UEnum::GetDisplayValueAsText(Attachment->Slot).ToString());
		return false;
	}

	// Replacing is intentional: fitting a second suppressor should swap the first, and the
	// caller is responsible for returning the replaced item to the inventory.
	FittedAttachments.RemoveAll([Attachment](const FEclipseFittedAttachment& Fitted)
	{
		return Fitted.Definition == Attachment || (Fitted.Definition != nullptr && Fitted.Definition->Slot == Attachment->Slot);
	});

	FEclipseFittedAttachment& Fitted = FittedAttachments.AddDefaulted_GetRef();
	Fitted.Definition = Attachment;
	Fitted.ItemId = ItemId.IsNone() ? Attachment->ItemId : ItemId;

	InvalidateStatCache();
	OutRejectReason.Reset();
	return true;
}

FName UEclipseWeaponInstance::RemoveAttachment(EEclipseAttachmentSlot Slot)
{
	FName RemovedItemId = NAME_None;
	bool bRemoved = false;

	for (int32 Index = 0; Index < FittedAttachments.Num(); ++Index)
	{
		const FEclipseFittedAttachment& Fitted = FittedAttachments[Index];
		if (Fitted.Definition == nullptr || Fitted.Definition->Slot != Slot)
		{
			continue;
		}

		// Capture the item id before the entry goes away: it is what the caller hands back
		// to the inventory.
		RemovedItemId = Fitted.ItemId.IsNone() ? Fitted.Definition->ItemId : Fitted.ItemId;
		FittedAttachments.RemoveAt(Index);
		bRemoved = true;
		break;
	}

	if (!bRemoved)
	{
		return NAME_None;
	}

	InvalidateStatCache();

	if (RemovedItemId.IsNone())
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("Removed %s attachment from %s but it had no item id to return."),
			*UEnum::GetDisplayValueAsText(Slot).ToString(),
			Definition != nullptr ? *Definition->WeaponId.ToString() : TEXT("<no definition>"));
	}

	return RemovedItemId;
}

UEclipseAttachmentDefinition* UEclipseWeaponInstance::GetAttachment(EEclipseAttachmentSlot Slot) const
{
	for (const FEclipseFittedAttachment& Fitted : FittedAttachments)
	{
		if (Fitted.Definition != nullptr && Fitted.Definition->Slot == Slot)
		{
			return Fitted.Definition;
		}
	}
	return nullptr;
}

TArray<EEclipseAttachmentSlot> UEclipseWeaponInstance::GetAvailableSlots() const
{
	return Definition != nullptr ? Definition->AvailableSlots : TArray<EEclipseAttachmentSlot>();
}

void UEclipseWeaponInstance::SetOwnerStatComponent(UEclipseStatComponent* InStatComponent)
{
	if (OwnerStatComponent == InStatComponent)
	{
		return;
	}

	OwnerStatComponent = InStatComponent;
	InvalidateStatCache();
}

void UEclipseWeaponInstance::InvalidateStatCache()
{
	bCacheValid = false;
}

TArray<FEclipseStatModifier> UEclipseWeaponInstance::BuildAttachmentModifiers() const
{
	TArray<FEclipseStatModifier> Modifiers;
	for (const FEclipseFittedAttachment& Fitted : FittedAttachments)
	{
		if (Fitted.Definition != nullptr)
		{
			Modifiers.Append(Fitted.Definition->Modifiers);
		}
	}
	return Modifiers;
}

void UEclipseWeaponInstance::BuildStatCache() const
{
	CachedStats.Reset();

	if (Definition == nullptr)
	{
		bCacheValid = true;
		return;
	}

	for (const EEclipseStat Stat : GWeaponStats)
	{
		const float Base = Definition->GetBaseStat(Stat);

		FEclipseStatAggregate Aggregate;
		for (const FEclipseStatModifier& Modifier : BuildAttachmentModifiers())
		{
			if (Modifier.Stat == Stat)
			{
				Aggregate.Accumulate(Modifier);
			}
		}

		// Owner modifiers (skills, status effects) are folded in last so that a Marksman
		// skill and a compensator both apply, in a fixed and testable order.
		if (OwnerStatComponent != nullptr)
		{
			if (const FEclipseStatAggregate* OwnerAggregate = OwnerStatComponent->GetAggregate(Stat))
			{
				Aggregate.Additive += OwnerAggregate->Additive;
				Aggregate.Multiplicative *= OwnerAggregate->Multiplicative;
			}
		}

		CachedStats.Add(Stat, Aggregate.Apply(Base));
	}

	bCacheValid = true;
}

float UEclipseWeaponInstance::GetStat(EEclipseStat Stat) const
{
	if (Definition == nullptr)
	{
		return 0.0f;
	}

	if (!bCacheValid)
	{
		BuildStatCache();
	}

	if (const float* Found = CachedStats.Find(Stat))
	{
		return *Found;
	}

	return Definition->GetBaseStat(Stat);
}

FEclipseWeaponStats UEclipseWeaponInstance::GetEffectiveStats() const
{
	FEclipseWeaponStats Stats;

	if (Definition == nullptr)
	{
		return Stats;
	}

	Stats.Damage = GetStat(EEclipseStat::Damage);
	Stats.EffectiveRange = GetStat(EEclipseStat::EffectiveRange);
	Stats.MaxRange = GetStat(EEclipseStat::MaxRange);
	Stats.SpreadDegrees = GetStat(EEclipseStat::SpreadDegrees);
	Stats.RecoilVertical = GetStat(EEclipseStat::RecoilVertical);
	Stats.RecoilHorizontal = GetStat(EEclipseStat::RecoilHorizontal);
	Stats.RoundsPerMinute = GetStat(EEclipseStat::RoundsPerMinute);
	Stats.ReloadSeconds = GetStat(EEclipseStat::ReloadSeconds);
	Stats.MagazineCapacity = FMath::Max(1, FMath::RoundToInt(GetStat(EEclipseStat::MagazineCapacity)));
	Stats.AimDownSightsSeconds = GetStat(EEclipseStat::AimDownSightsSeconds);
	Stats.AimSpreadMultiplier = Definition->AimSpreadMultiplier;
	Stats.ArmorPenetration = GetStat(EEclipseStat::ArmorPenetration);
	Stats.NoiseRadius = GetStat(EEclipseStat::NoiseRadius);
	Stats.Weight = GetStat(EEclipseStat::Weight);
	Stats.HeatPerShot = GetStat(EEclipseStat::HeatPerShot);
	Stats.PelletCount = FMath::Max(1, Definition->PelletCount);

	return Stats;
}

int32 UEclipseWeaponInstance::GetMagazineCapacity() const
{
	return FMath::Max(1, FMath::RoundToInt(GetStat(EEclipseStat::MagazineCapacity)));
}

bool UEclipseWeaponInstance::ConsumeAmmo(int32 Count)
{
	if (Count <= 0)
	{
		return true;
	}

	if (AmmoInMagazine < Count)
	{
		return false;
	}

	AmmoInMagazine -= Count;
	return true;
}

int32 UEclipseWeaponInstance::LoadAmmo(int32 Count)
{
	if (Count <= 0)
	{
		return 0;
	}

	const int32 Capacity = GetMagazineCapacity();
	const int32 Loaded = FMath::Min(Count, Capacity - AmmoInMagazine);
	AmmoInMagazine += Loaded;
	return Loaded;
}

void UEclipseWeaponInstance::RefillMagazine()
{
	AmmoInMagazine = GetMagazineCapacity();
}

FName UEclipseWeaponInstance::GetAmmoItemId() const
{
	return Definition != nullptr ? Definition->AmmoItemId : NAME_None;
}
