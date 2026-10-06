// PROJECT ECLIPSE - Base character.
//
// Purpose
//   Everything a humanoid in PROJECT ECLIPSE shares: stats, health, faction membership,
//   death and revival. The player character and every enemy derive from it, so dying,
//   being revived and belonging to a faction work identically for both.
//
// Composition
//   AEclipseCharacterBase is deliberately thin. Behaviour lives in components
//   (UEclipseStatComponent, UEclipseHealthComponent, UEclipseCharacterMovementComponent)
//   and in the derived classes. The base class owns the wiring, not the rules.

#pragma once

#include "Characters/EclipseCharacterMovementComponent.h"
#include "Combat/EclipseHealthComponent.h"
#include "Core/EclipseStatComponent.h"
#include "Core/EclipseTeamAgent.h"
#include "Core/EclipseTypes.h"
#include "GameFramework/Character.h"
#include "EclipseCharacterBase.generated.h"

/** Fired when this character dies. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FEclipseCharacterDeath, AEclipseCharacterBase* /*Character*/, AActor* /*Killer*/);

/** Fired when this character is revived. */
DECLARE_MULTICAST_DELEGATE_OneParam(FEclipseCharacterRevived, AEclipseCharacterBase* /*Character*/);

/**
 * Shared base for player and AI humanoids.
 *
 * Faction and squad id are replicated so that both the server and the clients agree on
 * who is hostile to whom; the hostility *rule* lives in the faction subsystem.
 */
UCLASS(Abstract)
class ECLIPSE_API AEclipseCharacterBase : public ACharacter, public IEclipseTeamAgent
{
	GENERATED_BODY()

public:
	AEclipseCharacterBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** AActor: resolve components and apply the initial stats. */
	virtual void BeginPlay() override;

	// ------------------------------------------------------------------
	// IEclipseTeamAgent
	// ------------------------------------------------------------------

	virtual EEclipseFaction GetFaction() const override { return Faction; }
	virtual FGuid GetSquadId() const override { return SquadId; }
	virtual bool IsAlive() const override;
	virtual bool IsPlayerAgent() const override { return false; }

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	/** Stat aggregation and stamina. Never null. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Character")
	UEclipseStatComponent* GetStatComponent() const { return StatComponent; }

	/** Health, shield, armour and death. Never null. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Character")
	UEclipseHealthComponent* GetHealthComponent() const { return HealthComponent; }

	/** Sprinting, speed resolution and fall damage. Never null. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Character")
	UEclipseCharacterMovementComponent* GetEclipseMovement() const;

	/** Movement state derived from velocity, stance and sprinting. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Character")
	EEclipseMovementState GetMovementState() const;

	// ------------------------------------------------------------------
	// Faction
	// ------------------------------------------------------------------

	/** Assign the faction. Server only; replicates to clients. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Character")
	void SetFaction(EEclipseFaction NewFaction);

	/** Assign the squad id. Server only. */
	void SetSquadId(const FGuid& NewSquadId);

	// ------------------------------------------------------------------
	// Life cycle
	// ------------------------------------------------------------------

	/** True while the character can act. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Character")
	bool IsDead() const;

	/** Bring the character back at a fraction of maximum health. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Character")
	virtual void ReviveCharacter(float HealthFraction);

	/** Server-side death handling. Disables movement and notifies listeners. */
	virtual void HandleDeath(AActor* Killer, EEclipseDamageType KillingDamageType);

	/** Fired on both server and clients when the character dies. */
	FEclipseCharacterDeath OnCharacterDeath;

	/** Fired when the character is revived. */
	FEclipseCharacterRevived OnCharacterRevived;

	/** UActorComponent: replicate faction, squad and movement state. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	/** Apply the faction, squad and components. Called from BeginPlay before stats. */
	virtual void InitializeCharacter();

	/** Recompute walk and sprint speeds from the current stats. */
	void RefreshMovementSpeed();

	/** Called when a stat changes, so speeds stay in step with the stats. */
	UFUNCTION()
	void HandleStatChanged(EEclipseStat Stat, float NewValue);

	/** Called when the health component reports death. */
	UFUNCTION()
	void HandleHealthDepleted(AActor* Killer, EEclipseDamageType KillingDamageType);

	/** Stat aggregation and stamina pool. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Character")
	TObjectPtr<UEclipseStatComponent> StatComponent;

	/** Health, shield, armour and death. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Character")
	TObjectPtr<UEclipseHealthComponent> HealthComponent;

	/** Faction this character belongs to. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Eclipse|Character")
	EEclipseFaction Faction = EEclipseFaction::Neutral;

	/** Squad id, or an invalid GUID when not in a squad. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Character")
	FGuid SquadId;

	/** True between death and revival. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Character")
	bool bDead = false;

	/** True while a revive is being channelled on this character. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Character")
	bool bBeingRevived = false;
};
