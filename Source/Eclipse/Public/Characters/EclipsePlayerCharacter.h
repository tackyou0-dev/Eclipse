// PROJECT ECLIPSE - Player character.
//
// Purpose
//   The colonist: camera rig, Enhanced Input bindings, interaction, and the revive loop
//   that makes four-player co-op work. Everything else is inherited or componentised.
//
// Interaction
//   The player character implements IEclipseInteractable so that a downed squad mate is
//   a valid interaction target: the revive prompt, the channel time and the actual
//   revive all use the same code path as looting a crate.

#pragma once

#include "Characters/EclipseCharacterBase.h"
#include "Combat/EclipseCombatComponent.h"
#include "Core/EclipseInteractable.h"
#include "CoreMinimal.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Weapons/EclipseWeaponComponent.h"
#include "EclipsePlayerCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UEclipseInventoryComponent;

/**
 * The player's colonist.
 *
 * View: third person by default with a first-person camera available on a toggle. Both
 * cameras are components on the character so possession never has to rebuild a rig.
 */
UCLASS()
class ECLIPSE_API AEclipsePlayerCharacter : public AEclipseCharacterBase, public IEclipseInteractable
{
	GENERATED_BODY()

public:
	AEclipsePlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** AActor: apply the loadout and bind the reload delegate. */
	virtual void BeginPlay() override;

	/** APawn: Enhanced Input bindings. */
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** ACharacter: notify the game mode when a player dies. */
	virtual void HandleDeath(AActor* Killer, EEclipseDamageType KillingDamageType) override;

	/** ACharacter: re-enable input and the camera when revived. */
	virtual void ReviveCharacter(float HealthFraction) override;

	/** IEclipseTeamAgent: this is a player. */
	virtual bool IsPlayerAgent() const override { return true; }

	// ------------------------------------------------------------------
	// IEclipseInteractable - a downed colonist can be revived
	// ------------------------------------------------------------------

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool IsInteractionEnabled_Implementation() const override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual float GetInteractionDuration_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	// ------------------------------------------------------------------
	// Interaction
	// ------------------------------------------------------------------

	/** Actor the player would interact with right now, or null. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Interaction")
	AActor* FindInteractableTarget() const;

	/** Begin interacting with the best target in reach. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Interaction")
	bool TryInteract();

	/** Fired when the interaction target changes, for the prompt widget. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FEclipseInteractionTargetChanged, AActor* /*NewTarget*/);
	FEclipseInteractionTargetChanged OnInteractionTargetChanged;

	/** Maximum distance in centimetres an interaction can reach. */
	static constexpr float InteractionReachCentimetres = 250.0f;

	/** Fraction of maximum health a revive restores. */
	static constexpr float ReviveHealthFraction = 0.4f;

	/** Seconds a revive takes to channel. */
	static constexpr float ReviveDurationSeconds = 3.0f;

	// ------------------------------------------------------------------
	// Components
	// ------------------------------------------------------------------

	/** Inventory: items, equipment and ammunition. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Character")
	UEclipseInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

	/** Weapon rack. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Character")
	UEclipseWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	/** Firing, spread and recoil. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Character")
	UEclipseCombatComponent* GetCombatComponent() const { return CombatComponent; }

	/** Third-person boom. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Character")
	USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** First-person camera, used when the view is toggled. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Character")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

protected:
	// ------------------------------------------------------------------
	// Input handlers
	// ------------------------------------------------------------------

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpStarted();
	void OnJumpStopped();
	void OnSprintStarted();
	void OnSprintStopped();
	void OnCrouchToggled();
	void OnFireStarted();
	void OnFireStopped();
	void OnAimStarted();
	void OnAimStopped();
	void OnReload();
	void OnInteract();
	void OnSwitchWeapon();
	void OnToggleView();

	/** Pull ammunition out of the inventory for a reload. */
	int32 RequestReloadAmmo(int32 RequestedRounds);

	/** Apply the starting loadout. Overridden by Blueprint for scripted starts. */
	virtual void ApplyDefaultLoadout();

	/** Server RPC: perform an interaction after the server has validated the range. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerInteract(AActor* Target);

	/** Input mapping context, registered by the player controller on possession. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input") TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input") TObjectPtr<UInputAction> LookAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input") TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input") TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input") TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input") TObjectPtr<UInputAction> FireAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input") TObjectPtr<UInputAction> AimAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input") TObjectPtr<UInputAction> ReloadAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input") TObjectPtr<UInputAction> InteractAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input") TObjectPtr<UInputAction> SwitchWeaponAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input") TObjectPtr<UInputAction> ToggleViewAction;

	/** Weapons the character starts with. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Loadout")
	TArray<TObjectPtr<UEclipseWeaponDefinition>> StartingWeapons;

	/** Inventory component. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Character")
	TObjectPtr<UEclipseInventoryComponent> InventoryComponent;

	/** Weapon rack. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Character")
	TObjectPtr<UEclipseWeaponComponent> WeaponComponent;

	/** Firing, spread and recoil. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Character")
	TObjectPtr<UEclipseCombatComponent> CombatComponent;

	/** Third-person boom. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Character")
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Third-person camera. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Character")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** First-person camera. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Character")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

private:
	/** Current interaction target, cached so the prompt only updates on change. */
	UPROPERTY()
	TObjectPtr<AActor> CurrentInteractionTarget = nullptr;

	/** True while the first-person camera is active. */
	bool bFirstPersonView = false;
};
