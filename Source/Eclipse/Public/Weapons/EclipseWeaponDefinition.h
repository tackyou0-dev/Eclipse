// PROJECT ECLIPSE - Weapon and attachment definitions.
//
// Purpose
//   The data half of the weapon system. A weapon is a data asset that describes how it
//   behaves; UEclipseWeaponInstance holds how it currently is. The vertical slice ships
//   AR-01, VX-9, M-12 and SR-77, mirrored from
//   Content/Eclipse/Data/VerticalSlice/weapons.json.
//
// Balance rule
//   No weapon numbers in code. If a balance value is needed that is not on this asset,
//   it belongs in UEclipseDeveloperSettings or in the weapon's data, never in a literal.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Engine/DataAsset.h"
#include "EclipseWeaponDefinition.generated.h"

/** Every tunable value of a weapon, after modifiers. Used by the stats panel and tests. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseWeaponStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float Damage = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float EffectiveRange = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float MaxRange = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float SpreadDegrees = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float RecoilVertical = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float RecoilHorizontal = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float RoundsPerMinute = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float ReloadSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") int32 MagazineCapacity = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float AimDownSightsSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float AimSpreadMultiplier = 1.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float ArmorPenetration = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float NoiseRadius = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float Weight = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") float HeatPerShot = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Weapon") int32 PelletCount = 1;
};

/**
 * Attachment definition.
 *
 * Modifiers are applied multiplicatively by default, which is why they are expressed as
 * multipliers around 1.0 rather than as deltas: a 0.35 noise multiplier from a suppressor
 * and a 1.1 from a compensator compose to 0.385, and neither can drive a stat negative.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseAttachmentDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Attachment id, for example "Muzzle.Suppressor". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Attachment")
	FName AttachmentId;

	/** Display name shown in the workbench. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Attachment")
	FText DisplayName;

	/** Slot this attachment occupies. A weapon accepts at most one of each. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Attachment")
	EEclipseAttachmentSlot Slot = EEclipseAttachmentSlot::None;

	/** Rarity, for loot weighting and UI colour. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Attachment")
	EEclipseRarity Rarity = EEclipseRarity::Common;

	/** Stat modifiers applied to the weapon while fitted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Attachment")
	TArray<FEclipseStatModifier> Modifiers;

	/** Item id handed back when the attachment is removed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Attachment")
	FName ItemId;

	/** True when this attachment is valid: it must have an id and a slot. */
	bool IsValidDefinition() const
	{
		return !AttachmentId.IsNone() && Slot != EEclipseAttachmentSlot::None;
	}
};

/**
 * Weapon definition.
 *
 * FireMode and DeliveryMode are orthogonal: an automatic shotgun and a single-shot
 * marksman rifle are both legal, and the combat component switches on the pair.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Weapon id, for example "AR-01". Matches weapons.json. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon")
	FName WeaponId;

	/** Display name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon")
	FText DisplayName;

	/** Designer-facing description. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (MultiLine = "true"))
	FText Description;

	/** Trigger behaviour. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon")
	EEclipseFireMode FireMode = EEclipseFireMode::Single;

	/** How the shot reaches the target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon")
	EEclipseDeliveryMode DeliveryMode = EEclipseDeliveryMode::Hitscan;

	/** Damage per projectile or pellet, before mitigation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0"))
	float Damage = 0.0f;

	/** Damage school. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon")
	EEclipseDamageType DamageType = EEclipseDamageType::Physical;

	/** Base spread in degrees before aim and movement modifiers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0"))
	float SpreadDegrees = 0.0f;

	/** Vertical recoil in degrees per shot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0"))
	float RecoilVertical = 0.0f;

	/** Horizontal recoil in degrees per shot, applied with a deterministic sign. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0"))
	float RecoilHorizontal = 0.0f;

	/** Range at which distance falloff starts, in centimetres. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0"))
	float EffectiveRange = 0.0f;

	/** Range at which distance falloff stops, in centimetres. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0"))
	float MaxRange = 0.0f;

	/** Cyclic rate of fire. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "1.0"))
	float RoundsPerMinute = 600.0f;

	/** Reload duration in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0"))
	float ReloadSeconds = 2.0f;

	/** Rounds per magazine. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "1"))
	int32 MagazineCapacity = 30;

	/** Seconds to raise the weapon to the aim pose. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0"))
	float AimDownSightsSeconds = 0.25f;

	/** Spread multiplier while aiming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0"))
	float AimSpreadMultiplier = 0.4f;

	/** Fraction of the target's armour this weapon ignores, in [0, 1]. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ArmorPenetration = 0.0f;

	/** Radius in centimetres at which the shot becomes audible to AI. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0"))
	float NoiseRadius = 0.0f;

	/** Carried weight in kilograms, for the inventory capacity rule. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0"))
	float Weight = 0.0f;

	/** Heat generated per shot. Drives the overheat lockout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "0.0"))
	float HeatPerShot = 0.0f;

	/** Pellets per shot. 1 for every weapon except shotguns. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "1"))
	int32 PelletCount = 1;

	/** Rounds fired per trigger pull when FireMode is Burst. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon", meta = (ClampMin = "1"))
	int32 BurstCount = 1;

	/** Rarity, for loot weighting and UI colour. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon")
	EEclipseRarity Rarity = EEclipseRarity::Common;

	/** Ammunition item consumed per shot. Must exist in the item catalogue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon")
	FName AmmoItemId;

	/** Slots this weapon accepts. Empty means no attachment support. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon")
	TArray<EEclipseAttachmentSlot> AvailableSlots;

	/** Socket the muzzle flash and tracer originate from. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Weapon")
	FName MuzzleSocketName;

	/** True when the definition is complete enough to be spawned and fired. */
	bool IsValidDefinition() const
	{
		return !WeaponId.IsNone()
			&& Damage > 0.0f
			&& MagazineCapacity > 0
			&& RoundsPerMinute > 0.0f
			&& MaxRange > 0.0f
			&& !AmmoItemId.IsNone();
	}

	/** Seconds between shots. */
	float GetShotIntervalSeconds() const
	{
		return RoundsPerMinute > 0.0f ? 60.0f / RoundsPerMinute : 0.0f;
	}

	/** True when the trigger can be held to keep firing. */
	bool IsAutomatic() const
	{
		return FireMode == EEclipseFireMode::Auto;
	}

	/** Base value of a stat from the definition alone. */
	float GetBaseStat(EEclipseStat Stat) const;
};
