// PROJECT ECLIPSE - Shared gameplay enumerations and small value types.
//
// Purpose
//   The vocabulary every system shares. Data assets, replicated state, save files, the
//   JSON content layer in Content/Eclipse/Data/VerticalSlice and the Python reference
//   model all key off these enumerations, so adding a value here is a cross-cutting
//   change: update the JSON content, the validator and the reference model in the same
//   commit.
//
// Rules
//   * Never reorder or remove a value that has shipped - save files and replication
//     store the numeric value, not the name.
//   * Append new values at the end of an enum and mirror them in the reference model.
//   * Anything a designer tunes belongs in a data asset or UEclipseDeveloperSettings,
//     not in a constant here.

#pragma once

#include "CoreMinimal.h"
#include "EclipseTypes.generated.h"

/** Damage schools. Armour and creatures resist these individually. */
UENUM(BlueprintType)
enum class EEclipseDamageType : uint8
{
	Physical		UMETA(DisplayName = "Physical"),
	Thermal			UMETA(DisplayName = "Thermal"),
	Chemical		UMETA(DisplayName = "Chemical"),
	Electrical		UMETA(DisplayName = "Electrical"),
	Kinetic			UMETA(DisplayName = "Kinetic"),
	Explosive		UMETA(DisplayName = "Explosive"),
	Radiation		UMETA(DisplayName = "Radiation"),
	Void			UMETA(DisplayName = "Void"),

	Count			UMETA(Hidden)
};

/** Rarity bands. Drive loot weighting, vendor pricing and UI colour. */
UENUM(BlueprintType)
enum class EEclipseRarity : uint8
{
	Common			UMETA(DisplayName = "Common"),
	Uncommon		UMETA(DisplayName = "Uncommon"),
	Rare			UMETA(DisplayName = "Rare"),
	Epic			UMETA(DisplayName = "Epic"),
	Legendary		UMETA(DisplayName = "Legendary"),
	Mythic			UMETA(DisplayName = "Mythic"),

	Count			UMETA(Hidden)
};

/** Inventory categories. Purely a classification; behaviour comes from the definition. */
UENUM(BlueprintType)
enum class EEclipseItemCategory : uint8
{
	Miscellaneous	UMETA(DisplayName = "Miscellaneous"),
	Ammunition		UMETA(DisplayName = "Ammunition"),
	Medical			UMETA(DisplayName = "Medical"),
	Consumable		UMETA(DisplayName = "Consumable"),
	Armor			UMETA(DisplayName = "Armor"),
	Weapon			UMETA(DisplayName = "Weapon"),
	Attachment		UMETA(DisplayName = "Attachment"),
	Resource		UMETA(DisplayName = "Resource"),
	Technology		UMETA(DisplayName = "Technology"),
	Artifact		UMETA(DisplayName = "Artifact"),
	QuestItem		UMETA(DisplayName = "Quest Item"),

	Count			UMETA(Hidden)
};

/** Equipment slots. Weapons can be holstered into the sidearm/secondary slots. */
UENUM(BlueprintType)
enum class EEclipseEquipmentSlot : uint8
{
	None			UMETA(DisplayName = "None"),
	PrimaryWeapon	UMETA(DisplayName = "Primary Weapon"),
	SecondaryWeapon	UMETA(DisplayName = "Secondary Weapon"),
	Sidearm			UMETA(DisplayName = "Sidearm"),
	Melee			UMETA(DisplayName = "Melee"),
	Head			UMETA(DisplayName = "Head"),
	Chest			UMETA(DisplayName = "Chest"),
	Legs			UMETA(DisplayName = "Legs"),
	Backpack		UMETA(DisplayName = "Backpack"),
	QuickSlotOne	UMETA(DisplayName = "Quick Slot 1"),
	QuickSlotTwo	UMETA(DisplayName = "Quick Slot 2"),
	QuickSlotThree	UMETA(DisplayName = "Quick Slot 3"),
	QuickSlotFour	UMETA(DisplayName = "Quick Slot 4"),

	Count			UMETA(Hidden)
};

/** Weapon attachment slots. Attachments declare exactly one slot. */
UENUM(BlueprintType)
enum class EEclipseAttachmentSlot : uint8
{
	None			UMETA(DisplayName = "None"),
	Barrel			UMETA(DisplayName = "Barrel"),
	Magazine		UMETA(DisplayName = "Magazine"),
	Optic			UMETA(DisplayName = "Optic"),
	Muzzle			UMETA(DisplayName = "Muzzle"),
	Stock			UMETA(DisplayName = "Stock"),
	Grip			UMETA(DisplayName = "Grip"),
	Underbarrel		UMETA(DisplayName = "Underbarrel"),
	PowerCore		UMETA(DisplayName = "Power Core"),

	Count			UMETA(Hidden)
};

/** Trigger behaviour. Burst and Charge read BurstCount / ChargeSeconds. */
UENUM(BlueprintType)
enum class EEclipseFireMode : uint8
{
	Single			UMETA(DisplayName = "Single"),
	Burst			UMETA(DisplayName = "Burst"),
	Auto			UMETA(DisplayName = "Automatic"),
	Charge			UMETA(DisplayName = "Charge"),

	Count			UMETA(Hidden)
};

/** How a shot is delivered. Projectile weapons spawn AEclipseProjectile. */
UENUM(BlueprintType)
enum class EEclipseDeliveryMode : uint8
{
	Hitscan			UMETA(DisplayName = "Hitscan"),
	Projectile		UMETA(DisplayName = "Projectile"),
	Beam			UMETA(DisplayName = "Beam"),
	Explosive		UMETA(DisplayName = "Explosive"),

	Count			UMETA(Hidden)
};

/** Player movement states the character and the camera both care about. */
UENUM(BlueprintType)
enum class EEclipseMovementState : uint8
{
	Idle			UMETA(DisplayName = "Idle"),
	Walking			UMETA(DisplayName = "Walking"),
	Sprinting		UMETA(DisplayName = "Sprinting"),
	Crouching		UMETA(DisplayName = "Crouching"),
	Prone			UMETA(DisplayName = "Prone"),
	Climbing		UMETA(DisplayName = "Climbing"),
	Swimming		UMETA(DisplayName = "Swimming"),
	Falling			UMETA(DisplayName = "Falling"),

	Count			UMETA(Hidden)
};

/** Named stats. Skills, attachments and status effects all modify these. */
UENUM(BlueprintType)
enum class EEclipseStat : uint8
{
	None					UMETA(DisplayName = "None"),
	MaxHealth				UMETA(DisplayName = "Max Health"),
	MaxShield				UMETA(DisplayName = "Max Shield"),
	MaxStamina				UMETA(DisplayName = "Max Stamina"),
	Armor					UMETA(DisplayName = "Armor"),
	ArmorPenetration		UMETA(DisplayName = "Armor Penetration"),
	Damage					UMETA(DisplayName = "Damage"),
	EffectiveRange			UMETA(DisplayName = "Effective Range"),
	MaxRange				UMETA(DisplayName = "Max Range"),
	SpreadDegrees			UMETA(DisplayName = "Spread"),
	RecoilVertical			UMETA(DisplayName = "Vertical Recoil"),
	RecoilHorizontal		UMETA(DisplayName = "Horizontal Recoil"),
	RoundsPerMinute			UMETA(DisplayName = "Rounds Per Minute"),
	ReloadSeconds			UMETA(DisplayName = "Reload Time"),
	MagazineCapacity		UMETA(DisplayName = "Magazine Capacity"),
	AimDownSightsSeconds	UMETA(DisplayName = "Aim Down Sights Time"),
	HeatPerShot				UMETA(DisplayName = "Heat Per Shot"),
	Weight					UMETA(DisplayName = "Weight"),
	NoiseRadius				UMETA(DisplayName = "Noise Radius"),
	CarryCapacity			UMETA(DisplayName = "Carry Capacity"),
	MoveSpeed				UMETA(DisplayName = "Move Speed"),
	SprintSpeed				UMETA(DisplayName = "Sprint Speed"),
	LootYield				UMETA(DisplayName = "Loot Yield"),
	EnvironmentalResistance	UMETA(DisplayName = "Environmental Resistance"),
	VoidResistance			UMETA(DisplayName = "Void Resistance"),

	Count					UMETA(Hidden)
};

/** Biomes of ECLIPSE-7. Each biome owns its own spawn, weather and resource rules. */
UENUM(BlueprintType)
enum class EEclipseBiome : uint8
{
	None				UMETA(DisplayName = "None"),
	AbandonedMegacity	UMETA(DisplayName = "Abandoned Megacity"),
	TerraformingForest	UMETA(DisplayName = "Terraforming Forest"),
	FrozenPlateau		UMETA(DisplayName = "Frozen Plateau"),
	VolcanicRegion		UMETA(DisplayName = "Volcanic Region"),
	ToxicMarsh			UMETA(DisplayName = "Toxic Marsh"),
	OrbitalCrashZone	UMETA(DisplayName = "Orbital Crash Zone"),
	UndergroundComplex	UMETA(DisplayName = "Underground Complex"),
	DesertExpanse		UMETA(DisplayName = "Desert Expanse"),

	Count				UMETA(Hidden)
};

/** Weather states. The world subsystem publishes exactly one profile at a time. */
UENUM(BlueprintType)
enum class EEclipseWeather : uint8
{
	Clear					UMETA(DisplayName = "Clear"),
	Rain					UMETA(DisplayName = "Rain"),
	Storm					UMETA(DisplayName = "Storm"),
	Sandstorm				UMETA(DisplayName = "Sandstorm"),
	Blizzard				UMETA(DisplayName = "Blizzard"),
	ToxicStorm				UMETA(DisplayName = "Toxic Storm"),
	ElectromagneticStorm	UMETA(DisplayName = "Electromagnetic Storm"),

	Count					UMETA(Hidden)
};

/** Coarse time-of-day bands derived from the world clock. */
UENUM(BlueprintType)
enum class EEclipseTimeOfDay : uint8
{
	Dawn			UMETA(DisplayName = "Dawn"),
	Day				UMETA(DisplayName = "Day"),
	Dusk			UMETA(DisplayName = "Dusk"),
	Night			UMETA(DisplayName = "Night"),

	Count			UMETA(Hidden)
};

/** Factions. Neutral is used by unaligned security systems such as WARDEN PRIME. */
UENUM(BlueprintType)
enum class EEclipseFaction : uint8
{
	None				UMETA(DisplayName = "None"),
	HelixCorporation	UMETA(DisplayName = "Helix Corporation"),
	FreeColonists		UMETA(DisplayName = "Free Colonists"),
	IronWolves			UMETA(DisplayName = "Iron Wolves"),
	Ascendants			UMETA(DisplayName = "Ascendants"),
	Wildlife			UMETA(DisplayName = "Wildlife"),
	Neutral				UMETA(DisplayName = "Neutral"),

	Count				UMETA(Hidden)
};

/** Base modules the player can build. Skill unlocks gate most of them. */
UENUM(BlueprintType)
enum class EEclipseBaseModule : uint8
{
	None			UMETA(DisplayName = "None"),
	CommandCenter	UMETA(DisplayName = "Command Center"),
	Workshop		UMETA(DisplayName = "Workshop"),
	Armory			UMETA(DisplayName = "Armory"),
	Laboratory		UMETA(DisplayName = "Laboratory"),
	MedicalBay		UMETA(DisplayName = "Medical Bay"),
	Barracks		UMETA(DisplayName = "Barracks"),
	PowerPlant		UMETA(DisplayName = "Power Plant"),
	StorageVault	UMETA(DisplayName = "Storage Vault"),
	LandingPad		UMETA(DisplayName = "Landing Pad"),

	Count			UMETA(Hidden)
};

/** Skill trees. Skill points are spent inside a tree; tiers gate prerequisites. */
UENUM(BlueprintType)
enum class EEclipseSkillTree : uint8
{
	Combat			UMETA(DisplayName = "Combat"),
	Survival		UMETA(DisplayName = "Survival"),
	Engineering		UMETA(DisplayName = "Engineering"),
	Technology		UMETA(DisplayName = "Technology"),
	Exploration		UMETA(DisplayName = "Exploration"),

	Count			UMETA(Hidden)
};

/** What a skill actually does when purchased. No skill is cosmetic. */
UENUM(BlueprintType)
enum class EEclipseSkillEffectType : uint8
{
	StatModifier		UMETA(DisplayName = "Stat Modifier"),
	UnlockRecipe		UMETA(DisplayName = "Unlock Recipe"),
	UnlockAbility		UMETA(DisplayName = "Unlock Ability"),
	UnlockBaseModule	UMETA(DisplayName = "Unlock Base Module"),
	PassiveAbility		UMETA(DisplayName = "Passive Ability"),

	Count				UMETA(Hidden)
};

/** Mission classifications. Search/Sabotage/etc. drive objective generation rules. */
UENUM(BlueprintType)
enum class EEclipseMissionType : uint8
{
	MainMission		UMETA(DisplayName = "Main Mission"),
	SideMission		UMETA(DisplayName = "Side Mission"),
	FactionMission	UMETA(DisplayName = "Faction Mission"),
	DynamicEvent	UMETA(DisplayName = "Dynamic Event"),
	Search			UMETA(DisplayName = "Search"),
	Sabotage		UMETA(DisplayName = "Sabotage"),
	Assault			UMETA(DisplayName = "Assault"),
	Defense			UMETA(DisplayName = "Defense"),
	Escort			UMETA(DisplayName = "Escort"),
	Extraction		UMETA(DisplayName = "Extraction"),
	Exploration		UMETA(DisplayName = "Exploration"),

	Count			UMETA(Hidden)
};

/** Objective kinds. Each has a real handler in UEclipseMissionSubsystem. */
UENUM(BlueprintType)
enum class EEclipseObjectiveType : uint8
{
	KillEnemies		UMETA(DisplayName = "Kill Enemies"),
	ReachLocation	UMETA(DisplayName = "Reach Location"),
	CollectItems	UMETA(DisplayName = "Collect Items"),
	Interact		UMETA(DisplayName = "Interact"),
	Survive			UMETA(DisplayName = "Survive"),
	Escort			UMETA(DisplayName = "Escort"),
	Defend			UMETA(DisplayName = "Defend"),
	Hack			UMETA(DisplayName = "Hack"),
	Extract			UMETA(DisplayName = "Extract"),
	Scan			UMETA(DisplayName = "Scan"),

	Count			UMETA(Hidden)
};

/** Mission lifecycle. Only one state is authoritative and it lives on the server. */
UENUM(BlueprintType)
enum class EEclipseMissionState : uint8
{
	Locked			UMETA(DisplayName = "Locked"),
	Available		UMETA(DisplayName = "Available"),
	Offered			UMETA(DisplayName = "Offered"),
	Active			UMETA(DisplayName = "Active"),
	Completed		UMETA(DisplayName = "Completed"),
	Failed			UMETA(DisplayName = "Failed"),
	Expired			UMETA(DisplayName = "Expired"),

	Count			UMETA(Hidden)
};

/** Enemy behaviour archetypes handled by AEclipseAIController. */
UENUM(BlueprintType)
enum class EEclipseEnemyBehaviour : uint8
{
	Patrol			UMETA(DisplayName = "Patrol"),
	Guard			UMETA(DisplayName = "Guard"),
	Hunt			UMETA(DisplayName = "Hunt"),
	Ambush			UMETA(DisplayName = "Ambush"),
	Assault			UMETA(DisplayName = "Assault"),
	Squad			UMETA(DisplayName = "Squad"),
	Support			UMETA(DisplayName = "Support"),
	Flee			UMETA(DisplayName = "Flee"),

	Count			UMETA(Hidden)
};

/** Role inside a squad. The squad coordinator assigns these. */
UENUM(BlueprintType)
enum class EEclipseSquadRole : uint8
{
	Leader			UMETA(DisplayName = "Leader"),
	Assault			UMETA(DisplayName = "Assault"),
	Support			UMETA(DisplayName = "Support"),
	Sniper			UMETA(DisplayName = "Sniper"),
	Flanker			UMETA(DisplayName = "Flanker"),
	Medic			UMETA(DisplayName = "Medic"),

	Count			UMETA(Hidden)
};

/**
 * AI simulation tier. Tier assignment is what keeps 64 agents affordable: only agents
 * inside UEclipseDeveloperSettings::AIHighFrequencyRadius tick at full rate.
 */
UENUM(BlueprintType)
enum class EEclipseAITier : uint8
{
	Dormant			UMETA(DisplayName = "Dormant"),
	LowFrequency	UMETA(DisplayName = "Low Frequency"),
	HighFrequency	UMETA(DisplayName = "High Frequency"),
	Hero			UMETA(DisplayName = "Hero"),
	Boss			UMETA(DisplayName = "Boss"),

	Count			UMETA(Hidden)
};

/** Money-moving operations. Every one of them is logged and validated. */
UENUM(BlueprintType)
enum class EEclipseTransactionKind : uint8
{
	Buy				UMETA(DisplayName = "Buy"),
	Sell			UMETA(DisplayName = "Sell"),
	Repair			UMETA(DisplayName = "Repair"),
	Craft			UMETA(DisplayName = "Craft"),
	Reward			UMETA(DisplayName = "Reward"),

	Count			UMETA(Hidden)
};

/** Why a raid ended. Drives the post-raid summary and analytics. */
UENUM(BlueprintType)
enum class EEclipseRaidOutcome : uint8
{
	InProgress		UMETA(DisplayName = "In Progress"),
	Extracted		UMETA(DisplayName = "Extracted"),
	KilledInAction	UMETA(DisplayName = "Killed In Action"),
	TimedOut		UMETA(DisplayName = "Timed Out"),
	BossDefeated	UMETA(DisplayName = "Boss Defeated"),

	Count			UMETA(Hidden)
};

/**
 * Save schema versions.
 *
 * MinSupported is the oldest file this build will load. Migrations exist for every
 * version between MinSupported and Current; a save written by a newer build is refused
 * rather than half-read.
 */
namespace UEclipseSaveGameVersion
{
	/** Oldest schema this build can migrate from. */
	constexpr int32 MinSupported = 1;

	/** Schema written by this build. */
	constexpr int32 Current = 2;

	/** Schema the v1 -> v2 migration upgrades from. */
	constexpr int32 LegacyV1 = 1;
}

/**
 * A single stat modification.
 *
 * Modifiers are always applied as (Base + Additive) * Multiplicative, summed per stat.
 * This ordering is the contract the reference model tests: additive bonuses first, then
 * multiplicative stacking, so two 0.5 multipliers give 0.25x and never 1.0x.
 */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseStatModifier
{
	GENERATED_BODY()

	/** Which stat this touches. None disables the modifier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Stats")
	EEclipseStat Stat = EEclipseStat::None;

	/** Flat amount added before multipliers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Stats")
	float Additive = 0.0f;

	/** Multiplicative factor applied after Additive. 1.0 means no change. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Stats")
	float Multiplicative = 1.0f;

	FEclipseStatModifier() = default;

	FEclipseStatModifier(EEclipseStat InStat, float InAdditive, float InMultiplicative)
		: Stat(InStat)
		, Additive(InAdditive)
		, Multiplicative(InMultiplicative)
	{
	}

	/** Apply this modifier to a base value. */
	float Apply(float BaseValue) const
	{
		return (BaseValue + Additive) * Multiplicative;
	}

	/** True when the modifier cannot change anything. */
	bool IsIdentity() const
	{
		return Stat == EEclipseStat::None || (FMath::IsNearlyZero(Additive) && FMath::IsNearlyEqual(Multiplicative, 1.0f));
	}
};

/** Aggregated modifiers for one stat: all adders summed, all multipliers multiplied. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseStatAggregate
{
	GENERATED_BODY()

	/** Sum of every additive contribution. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Stats")
	float Additive = 0.0f;

	/** Product of every multiplicative contribution. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Stats")
	float Multiplicative = 1.0f;

	/** Add one modifier into the aggregate. */
	void Accumulate(const FEclipseStatModifier& Modifier)
	{
		Additive += Modifier.Additive;
		Multiplicative *= Modifier.Multiplicative;
	}

	/** Apply the aggregate to a base value. */
	float Apply(float BaseValue) const
	{
		return (BaseValue + Additive) * Multiplicative;
	}

	/** True when applying the aggregate is a no-op. */
	bool IsIdentity() const
	{
		return FMath::IsNearlyZero(Additive) && FMath::IsNearlyEqual(Multiplicative, 1.0f);
	}
};

/** A credit or item reward granted on mission completion. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseMissionReward
{
	GENERATED_BODY()

	/** Credits paid out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Mission")
	int32 Credits = 0;

	/** Experience granted to every participant. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Mission")
	int32 Experience = 0;

	/** Item definition ids granted, in inventory order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Mission")
	TArray<FName> Items;

	/** True when there is nothing to grant. */
	bool IsEmpty() const
	{
		return Credits <= 0 && Experience <= 0 && Items.Num() == 0;
	}
};

/** Time-of-day helpers shared by the world subsystem, UI and the reference model. */
namespace EclipseTimeOfDay
{
	/** Hours in one in-game day. Fixed: the calendar uses 24-hour days everywhere. */
	constexpr float HoursPerDay = 24.0f;

	/** Normalise an hour value into [0, 24). Handles negative input. */
	FORCEINLINE float WrapHour(float Hours)
	{
		float Wrapped = FMath::Fmod(Hours, HoursPerDay);
		if (Wrapped < 0.0f)
		{
			Wrapped += HoursPerDay;
		}
		return Wrapped;
	}

	/**
	 * Classify an hour into a time-of-day band.
	 * Dawn 05:00-08:00, Day 08:00-18:00, Dusk 18:00-21:00, Night 21:00-05:00.
	 */
	FORCEINLINE EEclipseTimeOfDay FromHour(float Hours)
	{
		const float Hour = WrapHour(Hours);
		if (Hour >= 21.0f || Hour < 5.0f)
		{
			return EEclipseTimeOfDay::Night;
		}
		if (Hour < 8.0f)
		{
			return EEclipseTimeOfDay::Dawn;
		}
		if (Hour < 18.0f)
		{
			return EEclipseTimeOfDay::Day;
		}
		return EEclipseTimeOfDay::Dusk;
	}
}

/** Fired when the published weather changes. Declared in Core because both the world
 * subsystem (which publishes) and the game state (which replicates) depend on it. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEclipseWeatherChangedSignature, EEclipseWeather, NewWeather);

/** Human-readable names for logs, analytics payloads and test failure messages. */
namespace EclipseEnumNames
{
	ECLIPSE_API const TCHAR* ToString(EEclipseDamageType Value);
	ECLIPSE_API const TCHAR* ToString(EEclipseRarity Value);
	ECLIPSE_API const TCHAR* ToString(EEclipseBiome Value);
	ECLIPSE_API const TCHAR* ToString(EEclipseWeather Value);
	ECLIPSE_API const TCHAR* ToString(EEclipseFaction Value);
	ECLIPSE_API const TCHAR* ToString(EEclipseMissionState Value);
	ECLIPSE_API const TCHAR* ToString(EEclipseStat Value);
	ECLIPSE_API const TCHAR* ToString(EEclipseRaidOutcome Value);
}
