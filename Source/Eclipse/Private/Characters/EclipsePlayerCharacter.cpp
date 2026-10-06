// PROJECT ECLIPSE - Player character implementation.
//
// Purpose
//   Input, camera and interaction for the player's colonist. Interaction is worth
//   reading closely: the client only ever names a target, the server re-checks the range
//   and then drives the interface. That is the pattern every other client action follows.

#include "Characters/EclipsePlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Core/EclipseGameMode.h"
#include "Core/EclipseLog.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/EclipseInventoryComponent.h"
#include "Weapons/EclipseWeaponInstance.h"

AEclipsePlayerCharacter::AEclipsePlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;

	// Third-person boom: long enough to see the character, short enough to stay usable
	// indoors. The probe channel keeps the camera out of walls.
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 340.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 60.0f, 70.0f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeChannel = ECC_Camera;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = 90.0f;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->FieldOfView = 95.0f;
	FirstPersonCamera->SetActive(false);

	InventoryComponent = CreateDefaultSubobject<UEclipseInventoryComponent>(TEXT("InventoryComponent"));
	WeaponComponent = CreateDefaultSubobject<UEclipseWeaponComponent>(TEXT("WeaponComponent"));
	CombatComponent = CreateDefaultSubobject<UEclipseCombatComponent>(TEXT("CombatComponent"));
}

void AEclipsePlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponComponent != nullptr)
	{
		// The weapon asks the inventory for ammunition rather than reaching into it, which
		// keeps the weapon system usable by AI that have no inventory at all.
		WeaponComponent->OnReloadAmmoRequested.BindUObject(this, &AEclipsePlayerCharacter::RequestReloadAmmo);
	}

	ApplyDefaultLoadout();
}

void AEclipsePlayerCharacter::ApplyDefaultLoadout()
{
	if (WeaponComponent == nullptr)
	{
		return;
	}

	int32 EquippedIndex = 0;
	for (UEclipseWeaponDefinition* Definition : StartingWeapons)
	{
		if (Definition == nullptr)
		{
			continue;
		}

		const EEclipseEquipmentSlot Slot = EquippedIndex == 0
			? EEclipseEquipmentSlot::PrimaryWeapon
			: (EquippedIndex == 1 ? EEclipseEquipmentSlot::SecondaryWeapon : EEclipseEquipmentSlot::Sidearm);

		WeaponComponent->EquipWeapon(Definition, Slot);
		++EquippedIndex;
	}
}

void AEclipsePlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedInput == nullptr)
	{
		UE_LOG(LogEclipse, Warning, TEXT("%s has no Enhanced Input component; the character will not respond to input."), *GetName());
		return;
	}

	if (MoveAction != nullptr)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AEclipsePlayerCharacter::OnMove);
	}

	if (LookAction != nullptr)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AEclipsePlayerCharacter::OnLook);
	}

	if (JumpAction != nullptr)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AEclipsePlayerCharacter::OnJumpStarted);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &AEclipsePlayerCharacter::OnJumpStopped);
	}

	if (SprintAction != nullptr)
	{
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &AEclipsePlayerCharacter::OnSprintStarted);
		EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &AEclipsePlayerCharacter::OnSprintStopped);
	}

	if (CrouchAction != nullptr)
	{
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &AEclipsePlayerCharacter::OnCrouchToggled);
	}

	if (FireAction != nullptr)
	{
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &AEclipsePlayerCharacter::OnFireStarted);
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Completed, this, &AEclipsePlayerCharacter::OnFireStopped);
	}

	if (AimAction != nullptr)
	{
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Started, this, &AEclipsePlayerCharacter::OnAimStarted);
		EnhancedInput->BindAction(AimAction, ETriggerEvent::Completed, this, &AEclipsePlayerCharacter::OnAimStopped);
	}

	if (ReloadAction != nullptr)
	{
		EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &AEclipsePlayerCharacter::OnReload);
	}

	if (InteractAction != nullptr)
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AEclipsePlayerCharacter::OnInteract);
	}

	if (SwitchWeaponAction != nullptr)
	{
		EnhancedInput->BindAction(SwitchWeaponAction, ETriggerEvent::Started, this, &AEclipsePlayerCharacter::OnSwitchWeapon);
	}

	if (ToggleViewAction != nullptr)
	{
		EnhancedInput->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &AEclipsePlayerCharacter::OnToggleView);
	}
}

void AEclipsePlayerCharacter::OnMove(const FInputActionValue& Value)
{
	if (IsDead() || Controller == nullptr)
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	if (Axis.IsNearlyZero())
	{
		return;
	}

	const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right, Axis.X);
}

void AEclipsePlayerCharacter::OnLook(const FInputActionValue& Value)
{
	if (Controller == nullptr)
	{
		return;
	}

	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(-Axis.Y);
}

void AEclipsePlayerCharacter::OnJumpStarted()
{
	if (!IsDead())
	{
		Jump();
	}
}

void AEclipsePlayerCharacter::OnJumpStopped()
{
	StopJumping();
}

void AEclipsePlayerCharacter::OnSprintStarted()
{
	if (IsDead())
	{
		return;
	}

	if (UEclipseCharacterMovementComponent* Movement = GetEclipseMovement())
	{
		Movement->TryStartSprint();
	}
}

void AEclipsePlayerCharacter::OnSprintStopped()
{
	if (UEclipseCharacterMovementComponent* Movement = GetEclipseMovement())
	{
		Movement->StopSprint();
	}
}

void AEclipsePlayerCharacter::OnCrouchToggled()
{
	if (IsDead())
	{
		return;
	}

	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AEclipsePlayerCharacter::OnFireStarted()
{
	if (!IsDead() && CombatComponent != nullptr)
	{
		CombatComponent->StartFire();
	}
}

void AEclipsePlayerCharacter::OnFireStopped()
{
	if (CombatComponent != nullptr)
	{
		CombatComponent->StopFire();
	}
}

void AEclipsePlayerCharacter::OnAimStarted()
{
	if (CombatComponent != nullptr && !IsDead())
	{
		CombatComponent->SetAiming(true);
	}
}

void AEclipsePlayerCharacter::OnAimStopped()
{
	if (CombatComponent != nullptr)
	{
		CombatComponent->SetAiming(false);
	}
}

void AEclipsePlayerCharacter::OnReload()
{
	if (CombatComponent != nullptr && !IsDead())
	{
		CombatComponent->RequestReload();
	}
}

void AEclipsePlayerCharacter::OnInteract()
{
	TryInteract();
}

void AEclipsePlayerCharacter::OnSwitchWeapon()
{
	if (WeaponComponent != nullptr)
	{
		WeaponComponent->CycleActiveSlot();
	}
}

void AEclipsePlayerCharacter::OnToggleView()
{
	bFirstPersonView = !bFirstPersonView;

	if (FollowCamera != nullptr)
	{
		FollowCamera->SetActive(!bFirstPersonView);
	}

	if (FirstPersonCamera != nullptr)
	{
		FirstPersonCamera->SetActive(bFirstPersonView);
	}

	// First person hides the character mesh from its own camera so the head does not clip.
	SetOwnerNoSee(bFirstPersonView);
}

int32 AEclipsePlayerCharacter::RequestReloadAmmo(int32 RequestedRounds)
{
	if (InventoryComponent == nullptr || RequestedRounds <= 0)
	{
		return 0;
	}

	const UEclipseWeaponInstance* Weapon = WeaponComponent != nullptr ? WeaponComponent->GetActiveWeapon() : nullptr;
	if (Weapon == nullptr)
	{
		return 0;
	}

	const FName AmmoItemId = Weapon->GetAmmoItemId();
	if (AmmoItemId.IsNone())
	{
		return 0;
	}

	const int32 Available = InventoryComponent->CountItem(AmmoItemId);
	const int32 ToLoad = FMath::Min(RequestedRounds, Available);
	if (ToLoad <= 0)
	{
		return 0;
	}

	return InventoryComponent->RemoveItem(AmmoItemId, ToLoad) ? ToLoad : 0;
}

AActor* AEclipsePlayerCharacter::FindInteractableTarget() const
{
	UWorld* World = GetWorld();
	if (World == nullptr || IsDead())
	{
		return nullptr;
	}

	const FVector Start = GetPawnViewLocation();
	const FVector End = Start + GetViewRotation().Vector() * InteractionReachCentimetres;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EclipseInteraction), false, this);
	World->OverlapMultiByChannel(Overlaps, End, FQuat::Identity, ECC_WorldDynamic, FCollisionShape::MakeSphere(InteractionReachCentimetres * 0.5f), Params);

	AActor* Best = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (Candidate == nullptr || Candidate == this)
		{
			continue;
		}

		if (!Candidate->Implements<UEclipseInteractable>() || !IEclipseInteractable::Execute_IsInteractionEnabled(Candidate))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared(Start, Candidate->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Best = Candidate;
		}
	}

	return Best;
}

bool AEclipsePlayerCharacter::TryInteract()
{
	AActor* Target = FindInteractableTarget();
	if (Target == nullptr)
	{
		return false;
	}

	if (!IEclipseInteractable::Execute_CanInteract(Target, this))
	{
		return false;
	}

	// The client names a target; the server re-validates the range before acting, so a
	// modified client cannot loot a crate from across the map.
	if (HasAuthority())
	{
		IEclipseInteractable::Execute_Interact(Target, this);
	}
	else
	{
		ServerInteract(Target);
	}

	return true;
}

void AEclipsePlayerCharacter::ServerInteract_Implementation(AActor* Target)
{
	if (Target == nullptr || IsDead())
	{
		return;
	}

	const float DistanceSquared = FVector::DistSquared(GetPawnViewLocation(), Target->GetActorLocation());
	const float MaxDistanceSquared = FMath::Square(InteractionReachCentimetres * 2.0f);
	if (DistanceSquared > MaxDistanceSquared)
	{
		UE_LOG(LogEclipseSecurity, Warning, TEXT("%s tried to interact with %s from %.0f cm away; refused."),
			*GetName(), *Target->GetName(), FMath::Sqrt(DistanceSquared));
		return;
	}

	if (!Target->Implements<UEclipseInteractable>() || !IEclipseInteractable::Execute_IsInteractionEnabled(Target))
	{
		return;
	}

	if (!IEclipseInteractable::Execute_CanInteract(Target, this))
	{
		return;
	}

	IEclipseInteractable::Execute_Interact(Target, this);
}

bool AEclipsePlayerCharacter::ServerInteract_Validate(AActor* Target)
{
	return IsValid(Target);
}

bool AEclipsePlayerCharacter::CanInteract_Implementation(AActor* Interactor) const
{
	return IsInteractionEnabled_Implementation() && Interactor != this;
}

bool AEclipsePlayerCharacter::IsInteractionEnabled_Implementation() const
{
	// The only interaction a player character offers is being revived while down.
	return IsDead();
}

FText AEclipsePlayerCharacter::GetInteractionPrompt_Implementation() const
{
	return FText::Format(NSLOCTEXT("Eclipse", "RevivePrompt", "Revive {0}"), FText::FromString(GetName()));
}

float AEclipsePlayerCharacter::GetInteractionDuration_Implementation() const
{
	return ReviveDurationSeconds;
}

void AEclipsePlayerCharacter::Interact_Implementation(AActor* Interactor)
{
	if (!IsDead())
	{
		return;
	}

	const AEclipsePlayerCharacter* RevivingPlayer = Cast<AEclipsePlayerCharacter>(Interactor);
	if (RevivingPlayer == nullptr)
	{
		UE_LOG(LogEclipse, Verbose, TEXT("%s cannot be revived by %s."),
			*GetName(), Interactor != nullptr ? *Interactor->GetName() : TEXT("<null>"));
		return;
	}

	ReviveCharacter(ReviveHealthFraction);
}

void AEclipsePlayerCharacter::HandleDeath(AActor* Killer, EEclipseDamageType KillingDamageType)
{
	Super::HandleDeath(Killer, KillingDamageType);

	if (CombatComponent != nullptr)
	{
		CombatComponent->StopFire();
	}

	// Downed players are revived rather than respawned, so the death camera stays on the
	// body and the squad has time to reach it.
	if (FollowCamera != nullptr)
	{
		FollowCamera->SetActive(true);
	}

	if (AEclipseGameMode* GameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<AEclipseGameMode>() : nullptr)
	{
		GameMode->NotifyPlayerKilled(Cast<APlayerController>(GetController()));
	}
}

void AEclipsePlayerCharacter::ReviveCharacter(float HealthFraction)
{
	Super::ReviveCharacter(HealthFraction);

	if (Controller != nullptr)
	{
		Controller->SetIgnoreMoveInput(false);
		Controller->SetIgnoreLookInput(false);
	}
}
