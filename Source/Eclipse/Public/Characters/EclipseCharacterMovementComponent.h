// PROJECT ECLIPSE - Character movement component.
//
// Purpose
//   Movement tuning for PROJECT ECLIPSE: speeds come from the stat component rather than
//   from literals on the character blueprint, sprinting is a stamina cost rather than a
//   flag, and fall damage goes through the same damage pipeline as everything else.
//
// Why a subclass
//   GetMaxSpeed is the single place the engine asks "how fast may this pawn move". By
//   overriding it, a +15% sprinter skill, a heavy armour penalty and a slow effect all
//   work without a single line of movement code changing.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EclipseCharacterMovementComponent.generated.h"

/** Forward declaration: the stat component lives in Core. */
class UEclipseStatComponent;

/**
 * Movement component used by every Eclipse character.
 *
 * Sprinting is server-authoritative in the sense that the server drains stamina and
 * refuses the sprint when the pool is empty; the client predicts the speed change.
 */
UCLASS()
class ECLIPSE_API UEclipseCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UEclipseCharacterMovementComponent();

	/** UCharacterMovementComponent: speed from stats, sprint multiplier applied. */
	virtual float GetMaxSpeed() const override;

	/** UCharacterMovementComponent: fall damage routed through the damage library. */
	virtual bool HandleFallDamage(float FallDistance, float DamageMultiplier, const FHitResult& Hit) override;

	/**
	 * UCharacterMovementComponent: the engine's own hook for a client move the server does
	 * not believe. Super's checks run first; the security subsystem then applies the
	 * gameplay-side plausibility test (a player cannot cross the Basin between two moves).
	 */
	virtual bool ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode) override;

	/** UActorComponent: stamina drain while sprinting. */
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Ask to sprint. Fails when the stamina pool cannot pay for it, when the character is
	 * aiming, or when it is not moving on the ground.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Movement")
	bool TryStartSprint();

	/** Stop sprinting. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Movement")
	void StopSprint();

	/** True while the character is sprinting with stamina still flowing. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Movement")
	bool IsSprinting() const { return bSprinting; }

	/** Movement state derived from velocity, stance and sprinting. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Movement")
	EEclipseMovementState GetMovementState() const;

	/** Speed at which the character walks, from EEclipseStat::MoveSpeed. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Movement")
	float GetWalkSpeed() const;

	/** Severity of fall damage per centimetre fallen beyond the safe distance. */
	static constexpr float FallDamagePerCentimetre = 0.12f;

	/** Distance in centimetres the character may fall unharmed. */
	static constexpr float SafeFallDistance = 650.0f;

	/** Damage school applied by a fall. */
	static constexpr EEclipseDamageType FallDamageType = EEclipseDamageType::Kinetic;

protected:
	/** Stat component of the owning character, resolved lazily. */
	UEclipseStatComponent* GetStatComponent() const;

	/** True while the character is sprinting. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Movement")
	bool bSprinting = false;

	/** Minimum speed ratio for sprinting to be meaningful. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumSprintSpeedRatio = 0.35f;
};
