// PROJECT ECLIPSE - Vehicle implementation.
//
// Purpose
//   Seating, fuel and damage for the rover. Occupants are attached to seat sockets and
//   the driver possesses the vehicle, which is the standard UE split: the vehicle is the
//   pawn while driving, and the character is re-possessed on the way out.

#include "Vehicles/EclipseVehiclePawn.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Characters/EclipseCharacterBase.h"
#include "Combat/EclipseHealthComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/EclipseLog.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/EclipseInventoryComponent.h"

AEclipseVehiclePawn::AEclipseVehiclePawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	// The mesh is the root: a Chaos wheeled vehicle drives its skeletal mesh through the
	// movement component, so the mesh has to be the component the physics moves.
	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	RootComponent = MeshComponent;

	VehicleMovement = CreateDefaultSubobject<UChaosWheeledVehicleMovementComponent>(TEXT("VehicleMovement"));
	VehicleMovement->SetUpdatedComponent(MeshComponent);
	VehicleMovement->SetUseAutomaticGears(true);

	HealthComponent = CreateDefaultSubobject<UEclipseHealthComponent>(TEXT("HealthComponent"));
	CargoInventory = CreateDefaultSubobject<UEclipseInventoryComponent>(TEXT("CargoInventory"));
}

void AEclipseVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	FuelLitres = FMath::Clamp(FuelLitres, 0.0f, MaxFuelLitres);
}

bool AEclipseVehiclePawn::HasFreeSeat() const
{
	return GetOccupantCount() < FMath::Max(1, SeatCount);
}

int32 AEclipseVehiclePawn::GetOccupantCount() const
{
	int32 Count = Driver.IsValid() ? 1 : 0;
	for (const TWeakObjectPtr<APawn>& Passenger : Passengers)
	{
		if (Passenger.IsValid())
		{
			++Count;
		}
	}

	return Count;
}

bool AEclipseVehiclePawn::IsDriver(const AActor* Occupant) const
{
	return Occupant != nullptr && Driver.Get() == Occupant;
}

bool AEclipseVehiclePawn::IsOccupant(const AActor* Occupant) const
{
	if (Occupant == nullptr)
	{
		return false;
	}

	if (Driver.Get() == Occupant)
	{
		return true;
	}

	return Passengers.ContainsByPredicate([Occupant](const TWeakObjectPtr<APawn>& Entry)
	{
		return Entry.Get() == Occupant;
	});
}

FName AEclipseVehiclePawn::GetSeatSocketName(int32 SeatIndex)
{
	// Seats are sockets named Seat_0 .. Seat_N on the vehicle mesh; a mesh without them
	// still works, the occupant just sits at the vehicle's origin.
	return FName(*FString::Printf(TEXT("Seat_%d"), FMath::Max(0, SeatIndex)));
}

void AEclipseVehiclePawn::AttachOccupant(APawn* Occupant, EEclipseSeatType SeatType)
{
	if (Occupant == nullptr)
	{
		return;
	}

	const int32 SeatIndex = SeatType == EEclipseSeatType::Driver ? 0 : 1 + Passengers.Num();
	const FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, true);

	Occupant->AttachToComponent(MeshComponent, Rules, GetSeatSocketName(SeatIndex));
	Occupant->SetActorEnableCollision(false);

	// A passenger must not keep walking around inside the rover, and a driver's character
	// is inert while the vehicle is possessed.
	if (AController* Controller = Occupant->GetController())
	{
		if (SeatType == EEclipseSeatType::Driver)
		{
			Controller->Possess(this);
		}
		else
		{
			Occupant->DisableInput(Controller);
		}
	}
}

void AEclipseVehiclePawn::DetachOccupant(APawn* Occupant)
{
	if (Occupant == nullptr)
	{
		return;
	}

	Occupant->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Occupant->SetActorEnableCollision(true);

	// Step out beside the vehicle rather than inside its collision.
	const FVector ExitOffset = GetActorRotation().RotateVector(FVector(0.0f, 180.0f, 60.0f));
	Occupant->SetActorLocation(GetActorLocation() + ExitOffset, false, nullptr, ETeleportType::TeleportPhysics);
}

bool AEclipseVehiclePawn::EnterVehicle(APawn* Occupant)
{
	UWorld* World = GetWorld();
	if (World == nullptr || Occupant == nullptr || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	if (IsOccupant(Occupant) || !HasFreeSeat())
	{
		return false;
	}

	if (HealthComponent != nullptr && HealthComponent->IsDead())
	{
		// A wreck is not a seat.
		return false;
	}

	if (!Driver.IsValid())
	{
		Driver = Occupant;
		AttachOccupant(Occupant, EEclipseSeatType::Driver);

		UE_LOG(LogEclipse, Log, TEXT("%s took the driver's seat of %s."), *Occupant->GetName(), *GetName());
		return true;
	}

	Passengers.Add(Occupant);
	AttachOccupant(Occupant, EEclipseSeatType::Passenger);

	UE_LOG(LogEclipse, Log, TEXT("%s boarded %s as a passenger."), *Occupant->GetName(), *GetName());
	return true;
}

bool AEclipseVehiclePawn::ExitVehicle(APawn* Occupant)
{
	UWorld* World = GetWorld();
	if (World == nullptr || Occupant == nullptr || World->GetNetMode() == NM_Client)
	{
		return false;
	}

	if (!IsOccupant(Occupant))
	{
		return false;
	}

	if (Driver.Get() == Occupant)
	{
		AController* Controller = GetController();

		// Unpossess first: the vehicle must not keep taking input from a driver who is
		// about to be standing outside it.
		if (Controller != nullptr)
		{
			Controller->UnPossess();
		}

		DetachOccupant(Occupant);
		Driver.Reset();

		if (Controller != nullptr)
		{
			Controller->Possess(Occupant);
		}

		SetThrottleInput(0.0f);
		SetSteeringInput(0.0f);
		SetHandbrakeInput(true);

		UE_LOG(LogEclipse, Log, TEXT("%s left the driver's seat of %s."), *Occupant->GetName(), *GetName());
		return true;
	}

	Passengers.RemoveAll([Occupant](const TWeakObjectPtr<APawn>& Entry)
	{
		return Entry.Get() == Occupant;
	});

	DetachOccupant(Occupant);

	if (AController* Controller = Occupant->GetController())
	{
		Occupant->EnableInput(Controller);
	}

	UE_LOG(LogEclipse, Log, TEXT("%s left %s."), *Occupant->GetName(), *GetName());
	return true;
}

void AEclipseVehiclePawn::SetThrottleInput(float Value)
{
	float Resolved = FMath::Clamp(Value, 0.0f, 1.0f);

	// The engine cannot pull what it does not have fuel for, and nobody drives an empty
	// rover from the passenger seat.
	if (!IsEngineRunning() || VehicleMovement == nullptr)
	{
		Resolved = 0.0f;
	}

	LastThrottleInput = Resolved;
	VehicleMovement->SetThrottleInput(Resolved);
}

void AEclipseVehiclePawn::SetSteeringInput(float Value)
{
	const float Resolved = IsEngineRunning() ? FMath::Clamp(Value, -1.0f, 1.0f) : 0.0f;

	if (VehicleMovement != nullptr)
	{
		VehicleMovement->SetSteeringInput(Resolved);
	}
}

void AEclipseVehiclePawn::SetHandbrakeInput(bool bEngaged)
{
	if (VehicleMovement != nullptr)
	{
		VehicleMovement->SetHandbrakeInput(bEngaged);
	}
}

bool AEclipseVehiclePawn::IsEngineRunning() const
{
	return FuelLitres > 0.0f && Driver.IsValid() && (HealthComponent == nullptr || !HealthComponent->IsDead());
}

float AEclipseVehiclePawn::GetFuelFraction() const
{
	return MaxFuelLitres > 0.0f ? FMath::Clamp(FuelLitres / MaxFuelLitres, 0.0f, 1.0f) : 0.0f;
}

float AEclipseVehiclePawn::Refuel(float Litres)
{
	if (Litres <= 0.0f)
	{
		return 0.0f;
	}

	const float Before = FuelLitres;
	FuelLitres = FMath::Clamp(FuelLitres + Litres, 0.0f, MaxFuelLitres);
	return FuelLitres - Before;
}

void AEclipseVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client || DeltaSeconds <= 0.0f)
	{
		return;
	}

	if (FuelLitres <= 0.0f)
	{
		// Dry tank: make sure the physics is not still being asked for power.
		if (LastThrottleInput > 0.0f && VehicleMovement != nullptr)
		{
			LastThrottleInput = 0.0f;
			VehicleMovement->SetThrottleInput(0.0f);
		}

		return;
	}

	const bool bIdling = !Driver.IsValid() || LastThrottleInput <= KINDA_SMALL_NUMBER;
	const float BurnRate = bIdling ? FuelBurnPerSecondIdle : FuelBurnPerSecondAtFullThrottle * LastThrottleInput;
	FuelLitres = FMath::Max(0.0f, FuelLitres - BurnRate * DeltaSeconds);
}

bool AEclipseVehiclePawn::CanInteract_Implementation(AActor* Interactor) const
{
	return Interactor != nullptr && IsInteractionEnabled_Implementation();
}

bool AEclipseVehiclePawn::IsInteractionEnabled_Implementation() const
{
	return HealthComponent == nullptr || !HealthComponent->IsDead();
}

FText AEclipseVehiclePawn::GetInteractionPrompt_Implementation() const
{
	if (FuelLitres >= MaxFuelLitres - KINDA_SMALL_NUMBER)
	{
		return NSLOCTEXT("Eclipse", "VehicleDrivePrompt", "Drive");
	}

	return HasFreeSeat()
		? NSLOCTEXT("Eclipse", "VehicleDriveOrRefuelPrompt", "Drive (or refuel with a fuel cell)")
		: NSLOCTEXT("Eclipse", "VehicleRefuelPrompt", "Refuel");
}

void AEclipseVehiclePawn::Interact_Implementation(AActor* Interactor)
{
	if (Interactor == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_Client)
	{
		// The interaction system reruns this on the server; a client that got here directly
		// must not seat itself.
		return;
	}

	// Refuelling wins over boarding when the player is carrying a fuel cell and the tank
	// has room: standing at a rover holding fuel means you want to pour it in.
	if (FuelLitres < MaxFuelLitres - KINDA_SMALL_NUMBER)
	{
		if (UEclipseInventoryComponent* Inventory = Interactor->FindComponentByClass<UEclipseInventoryComponent>())
		{
			if (Inventory->CountItem(FuelItemId) > 0 && Inventory->RemoveItem(FuelItemId, 1))
			{
				const float Added = Refuel(RefuelAmountPerItem);
				UE_LOG(LogEclipse, Log, TEXT("%s refuelled %s by %.1f litres."), *Interactor->GetName(), *GetName(), Added);
				return;
			}
		}
	}

	APawn* Occupant = Cast<APawn>(Interactor);
	if (Occupant == nullptr)
	{
		Occupant = Interactor->GetInstigator();
	}

	if (Occupant != nullptr)
	{
		EnterVehicle(Occupant);
	}
}
