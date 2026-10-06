// PROJECT ECLIPSE - Damageable interface.
//
// Purpose
//   The receiving half of the damage contract. UEclipseDamageLibrary talks to this
//   interface only, so anything with a health pool - characters, vehicles, destructible
//   props, the boss - is damaged the same way.
//
// Implementers
//   UEclipseHealthComponent is the standard implementation and is what an actor should
//   attach. A type that needs custom damage behaviour (a shield generator that redirects
//   damage, for example) implements this itself and is still compatible with every
//   damage source in the game.

#pragma once

#include "Combat/EclipseDamageTypes.h"
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EclipseDamageable.generated.h"

/** Bare UInterface so IEclipseDamageable is visible to the reflection system. */
UINTERFACE(MinimalAPI, BlueprintType)
class UEclipseDamageable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by anything that can take damage and die.
 *
 * Pure C++ virtuals: damage accounting is a server-side system, not Blueprint
 * composition. Blueprint-side presentation reacts to the health component's delegates
 * instead.
 */
class ECLIPSE_API IEclipseDamageable
{
	GENERATED_BODY()

public:
	/**
	 * Apply a damage context. Implementations must be idempotent per call and must not
	 * be invoked directly by gameplay code; go through UEclipseDamageLibrary::ApplyDamage
	 * so the pipeline is identical everywhere.
	 */
	virtual FEclipseDamageResult ApplyEclipseDamage(const FEclipseDamageContext& Context) = 0;

	/** True while the owner can still be damaged (alive, not phase-invulnerable). */
	virtual bool IsDamageable() const = 0;

	/** True when health has reached zero. */
	virtual bool IsDead() const = 0;

	/** Current health, for HUDs and for the damage result. */
	virtual float GetCurrentHealth() const = 0;

	/** Current maximum health, after stat modifiers. */
	virtual float GetMaximumHealth() const = 0;
};
