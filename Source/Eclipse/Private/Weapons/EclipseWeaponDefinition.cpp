// PROJECT ECLIPSE - Weapon definition implementation.
//
// Purpose
//   Maps the generic EEclipseStat enum onto the weapon's named fields. This mapping is
//   the only place that knows which field backs which stat, so attachments and skills
//   never have to know about FEclipseWeaponStats.

#include "Weapons/EclipseWeaponDefinition.h"

float UEclipseWeaponDefinition::GetBaseStat(EEclipseStat Stat) const
{
	switch (Stat)
	{
	case EEclipseStat::Damage:					return Damage;
	case EEclipseStat::EffectiveRange:			return EffectiveRange;
	case EEclipseStat::MaxRange:				return MaxRange;
	case EEclipseStat::SpreadDegrees:			return SpreadDegrees;
	case EEclipseStat::RecoilVertical:			return RecoilVertical;
	case EEclipseStat::RecoilHorizontal:		return RecoilHorizontal;
	case EEclipseStat::RoundsPerMinute:			return RoundsPerMinute;
	case EEclipseStat::ReloadSeconds:			return ReloadSeconds;
	case EEclipseStat::MagazineCapacity:		return static_cast<float>(MagazineCapacity);
	case EEclipseStat::AimDownSightsSeconds:	return AimDownSightsSeconds;
	case EEclipseStat::ArmorPenetration:		return ArmorPenetration;
	case EEclipseStat::NoiseRadius:				return NoiseRadius;
	case EEclipseStat::Weight:					return Weight;
	case EEclipseStat::HeatPerShot:				return HeatPerShot;
	default:
		break;
	}

	// Stats that do not describe a weapon read as zero so that a caller which asks the
	// wrong question gets an obviously wrong answer instead of a plausible one.
	return 0.0f;
}
