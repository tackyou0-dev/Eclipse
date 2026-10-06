// PROJECT ECLIPSE - Damage value type implementation.
//
// Purpose
//   Resistance lookups with clamping. Clamping lives here rather than at the call site
//   so that a bad data entry can never produce immunity (0.0 is the floor, and a damage
//   type is only immune if the designer explicitly writes MinimumResistance).

#include "Combat/EclipseDamageTypes.h"

float FEclipseResistanceProfile::Get(EEclipseDamageType DamageType) const
{
	if (const float* Found = Resistances.Find(DamageType))
	{
		return FMath::Clamp(*Found, MinimumResistance, MaximumResistance);
	}

	// Unknown types take normal damage. This is deliberate: a designer who forgets an
	// entry gets a visible, tunable result rather than an invisible immunity.
	return 1.0f;
}

void FEclipseResistanceProfile::Set(EEclipseDamageType DamageType, float Multiplier)
{
	Resistances.Add(DamageType, FMath::Clamp(Multiplier, MinimumResistance, MaximumResistance));
}
