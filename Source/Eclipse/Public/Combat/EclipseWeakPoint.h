// PROJECT ECLIPSE - Weak point interface.
//
// Purpose
//   Lets a damage dealer ask a target what a hit bone is worth without knowing anything
//   about that target. Enemies declare weak points in their definition (a drone's power
//   core, a brute's back plate, the Warden's exposed reactor) and this interface is how
//   the shooter finds out.
//
// Used by
//   UEclipseCombatComponent when resolving a hit, and AEclipseProjectile on impact.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EclipseWeakPoint.generated.h"

/** Bare UInterface so IEclipseWeakPointOwner is visible to the reflection system. */
UINTERFACE(MinimalAPI, BlueprintType)
class UEclipseWeakPointOwner : public UInterface
{
	GENERATED_BODY()
};

/** Implemented by anything whose limbs are worth different amounts of damage. */
class ECLIPSE_API IEclipseWeakPointOwner
{
	GENERATED_BODY()

public:
	/**
	 * Damage multiplier for a hit bone. Returns 1.0 for bones that are not weak points,
	 * so a caller never has to special-case a miss.
	 */
	virtual float GetWeakPointMultiplier(FName BoneName) const = 0;
};
