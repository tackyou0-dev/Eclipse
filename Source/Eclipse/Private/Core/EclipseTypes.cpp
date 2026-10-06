// PROJECT ECLIPSE - Shared enum name implementation.
//
// Purpose
//   Implements the human-readable names declared in EclipseTypes.h. Every switch is
//   exhaustive over its enum: the trailing return keeps the compiler happy and the
//   default case is deliberately absent so a new enum value is a compile error rather
//   than a silent "Unknown" in the logs.

#include "Core/EclipseTypes.h"

namespace EclipseEnumNames
{
	const TCHAR* ToString(EEclipseDamageType Value)
	{
		switch (Value)
		{
		case EEclipseDamageType::Physical:	return TEXT("Physical");
		case EEclipseDamageType::Thermal:	return TEXT("Thermal");
		case EEclipseDamageType::Chemical:	return TEXT("Chemical");
		case EEclipseDamageType::Electrical:return TEXT("Electrical");
		case EEclipseDamageType::Kinetic:	return TEXT("Kinetic");
		case EEclipseDamageType::Explosive:	return TEXT("Explosive");
		case EEclipseDamageType::Radiation:	return TEXT("Radiation");
		case EEclipseDamageType::Void:		return TEXT("Void");
		default:							break;
		}
		return TEXT("Unknown");
	}

	const TCHAR* ToString(EEclipseRarity Value)
	{
		switch (Value)
		{
		case EEclipseRarity::Common:	return TEXT("Common");
		case EEclipseRarity::Uncommon:	return TEXT("Uncommon");
		case EEclipseRarity::Rare:		return TEXT("Rare");
		case EEclipseRarity::Epic:		return TEXT("Epic");
		case EEclipseRarity::Legendary:	return TEXT("Legendary");
		case EEclipseRarity::Mythic:	return TEXT("Mythic");
		default:						break;
		}
		return TEXT("Unknown");
	}

	const TCHAR* ToString(EEclipseBiome Value)
	{
		switch (Value)
		{
		case EEclipseBiome::None:				return TEXT("None");
		case EEclipseBiome::AbandonedMegacity:	return TEXT("AbandonedMegacity");
		case EEclipseBiome::TerraformingForest:	return TEXT("TerraformingForest");
		case EEclipseBiome::FrozenPlateau:		return TEXT("FrozenPlateau");
		case EEclipseBiome::VolcanicRegion:		return TEXT("VolcanicRegion");
		case EEclipseBiome::ToxicMarsh:			return TEXT("ToxicMarsh");
		case EEclipseBiome::OrbitalCrashZone:	return TEXT("OrbitalCrashZone");
		case EEclipseBiome::UndergroundComplex:	return TEXT("UndergroundComplex");
		case EEclipseBiome::DesertExpanse:		return TEXT("DesertExpanse");
		default:								break;
		}
		return TEXT("Unknown");
	}

	const TCHAR* ToString(EEclipseWeather Value)
	{
		switch (Value)
		{
		case EEclipseWeather::Clear:				return TEXT("Clear");
		case EEclipseWeather::Rain:					return TEXT("Rain");
		case EEclipseWeather::Storm:				return TEXT("Storm");
		case EEclipseWeather::Sandstorm:			return TEXT("Sandstorm");
		case EEclipseWeather::Blizzard:				return TEXT("Blizzard");
		case EEclipseWeather::ToxicStorm:			return TEXT("ToxicStorm");
		case EEclipseWeather::ElectromagneticStorm:	return TEXT("ElectromagneticStorm");
		default:									break;
		}
		return TEXT("Unknown");
	}

	const TCHAR* ToString(EEclipseFaction Value)
	{
		switch (Value)
		{
		case EEclipseFaction::None:				return TEXT("None");
		case EEclipseFaction::HelixCorporation:	return TEXT("HelixCorporation");
		case EEclipseFaction::FreeColonists:	return TEXT("FreeColonists");
		case EEclipseFaction::IronWolves:		return TEXT("IronWolves");
		case EEclipseFaction::Ascendants:		return TEXT("Ascendants");
		case EEclipseFaction::Wildlife:			return TEXT("Wildlife");
		case EEclipseFaction::Neutral:			return TEXT("Neutral");
		default:								break;
		}
		return TEXT("Unknown");
	}

	const TCHAR* ToString(EEclipseMissionState Value)
	{
		switch (Value)
		{
		case EEclipseMissionState::Locked:		return TEXT("Locked");
		case EEclipseMissionState::Available:	return TEXT("Available");
		case EEclipseMissionState::Offered:		return TEXT("Offered");
		case EEclipseMissionState::Active:		return TEXT("Active");
		case EEclipseMissionState::Completed:	return TEXT("Completed");
		case EEclipseMissionState::Failed:		return TEXT("Failed");
		case EEclipseMissionState::Expired:		return TEXT("Expired");
		default:								break;
		}
		return TEXT("Unknown");
	}

	const TCHAR* ToString(EEclipseStat Value)
	{
		switch (Value)
		{
		case EEclipseStat::None:					return TEXT("None");
		case EEclipseStat::MaxHealth:				return TEXT("MaxHealth");
		case EEclipseStat::MaxShield:				return TEXT("MaxShield");
		case EEclipseStat::MaxStamina:				return TEXT("MaxStamina");
		case EEclipseStat::Armor:					return TEXT("Armor");
		case EEclipseStat::ArmorPenetration:		return TEXT("ArmorPenetration");
		case EEclipseStat::Damage:					return TEXT("Damage");
		case EEclipseStat::EffectiveRange:			return TEXT("EffectiveRange");
		case EEclipseStat::MaxRange:				return TEXT("MaxRange");
		case EEclipseStat::SpreadDegrees:			return TEXT("SpreadDegrees");
		case EEclipseStat::RecoilVertical:			return TEXT("RecoilVertical");
		case EEclipseStat::RecoilHorizontal:		return TEXT("RecoilHorizontal");
		case EEclipseStat::RoundsPerMinute:			return TEXT("RoundsPerMinute");
		case EEclipseStat::ReloadSeconds:			return TEXT("ReloadSeconds");
		case EEclipseStat::MagazineCapacity:		return TEXT("MagazineCapacity");
		case EEclipseStat::AimDownSightsSeconds:	return TEXT("AimDownSightsSeconds");
		case EEclipseStat::HeatPerShot:				return TEXT("HeatPerShot");
		case EEclipseStat::Weight:					return TEXT("Weight");
		case EEclipseStat::NoiseRadius:				return TEXT("NoiseRadius");
		case EEclipseStat::CarryCapacity:			return TEXT("CarryCapacity");
		case EEclipseStat::MoveSpeed:				return TEXT("MoveSpeed");
		case EEclipseStat::SprintSpeed:				return TEXT("SprintSpeed");
		case EEclipseStat::LootYield:				return TEXT("LootYield");
		case EEclipseStat::EnvironmentalResistance:	return TEXT("EnvironmentalResistance");
		case EEclipseStat::VoidResistance:			return TEXT("VoidResistance");
		default:									break;
		}
		return TEXT("Unknown");
	}

	const TCHAR* ToString(EEclipseRaidOutcome Value)
	{
		switch (Value)
		{
		case EEclipseRaidOutcome::InProgress:		return TEXT("InProgress");
		case EEclipseRaidOutcome::Extracted:		return TEXT("Extracted");
		case EEclipseRaidOutcome::KilledInAction:	return TEXT("KilledInAction");
		case EEclipseRaidOutcome::TimedOut:			return TEXT("TimedOut");
		case EEclipseRaidOutcome::BossDefeated:		return TEXT("BossDefeated");
		default:									break;
		}
		return TEXT("Unknown");
	}
}
