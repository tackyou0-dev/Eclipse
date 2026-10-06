// PROJECT ECLIPSE - Vehicle pawn.
//
// Purpose
//   The rover: the only way to move heavy salvage and the only thing in the slice that
//   burns fuel. It is a real wheeled Chaos vehicle (the movement component is created in
//   the constructor) with three gameplay systems attached: fuel, cargo and health.
//
// Authority
//   Vehicles are server-authoritative like everything else that moves. Entry, exit, fuel
//   and damage are decided on the server; the driver's input reaches the server through
//   the normal possession path, and a client that is not the driver cannot drive.
//
// Interaction
//   Interacting with a rover seats the interactor: as driver if the seat is free, as a
//   passenger otherwise. An empty-handed interactor with a fuel can also refuel by
//   interacting while carrying one, which is how the slice's salvage run is meant to be
//   played.

#pragma once

#include "Core/EclipseInteractable.h"
#include "Core/EclipseTypes.h"
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "EclipseVehiclePawn.generated.h"

class UChaosWheeledVehicleMovementComponent;
class UEclipseHealthComponent;
class UEclipseInventoryComponent;
class USkeletalMeshComponent;

/** Kinds of seat a vehicle has. */
UENUM(BlueprintType)
enum class EEclipseSeatType : uint8
{
	Driver				UMETA(DisplayName = "Driver"),
	Passenger			UMETA(DisplayName = "Passenger"),

	Count				UMETA(Hidden)
};

/**
 * A drivable rover.
 *
 * Occupants are attached to the vehicle for the duration of the ride; the driver's
 * controller possesses the vehicle itself, which is what lets the engine's own input and
 * camera paths work without a parallel control scheme.
 */
UCLASS()
class ECLIPSE_API AEclipseVehiclePawn : public APawn, public IEclipseInteractable
{
	GENERATED_BODY()

public:
	AEclipseVehiclePawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** APawn: fuel burn and engine cut-out. */
	virtual void Tick(float DeltaSeconds) override;

	/** APawn. */
	virtual void BeginPlay() override;

	// ------------------------------------------------------------------
	// Seating
	// ------------------------------------------------------------------

	/** True when the vehicle has a free seat. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vehicle")
	bool HasFreeSeat() const;

	/** True when this actor is currently the driver. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vehicle")
	bool IsDriver(const AActor* Occupant) const;

	/** True when this actor is somewhere in the vehicle. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vehicle")
	bool IsOccupant(const AActor* Occupant) const;

	/**
	 * Seat an occupant. The first occupant becomes the driver; later ones take passenger
	 * seats while they are free. Server only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Vehicle")
	bool EnterVehicle(APawn* Occupant);

	/** Remove an occupant and put them back on the ground next to the vehicle. Server only. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Vehicle")
	bool ExitVehicle(APawn* Occupant);

	/** Driver, or null. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vehicle")
	APawn* GetDriver() const { return Driver.Get(); }

	/** Number of seats occupied. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vehicle")
	int32 GetOccupantCount() const;

	// ------------------------------------------------------------------
	// Driving
	// ------------------------------------------------------------------

	/** Throttle in [0, 1]. Zero when the tank is dry or nobody is driving. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Vehicle")
	void SetThrottleInput(float Value);

	/** Steering in [-1, 1]. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Vehicle")
	void SetSteeringInput(float Value);

	/** Handbrake. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Vehicle")
	void SetHandbrakeInput(bool bEngaged);

	/** True when the engine is running (fuel remains and a driver is seated). */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vehicle")
	bool IsEngineRunning() const;

	// ------------------------------------------------------------------
	// Fuel
	// ------------------------------------------------------------------

	/** Litres in the tank. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vehicle")
	float GetFuelLitres() const { return FuelLitres; }

	/** Fuel fraction in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vehicle")
	float GetFuelFraction() const;

	/** Add fuel. Returns how much was actually taken. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Vehicle")
	float Refuel(float Litres);

	// ------------------------------------------------------------------
	// Presentation and systems
	// ------------------------------------------------------------------

	/** Health component. Vehicles take damage through the standard pipeline. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vehicle")
	UEclipseHealthComponent* GetHealthComponent() const { return HealthComponent; }

	/** Shared cargo inventory. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vehicle")
	UEclipseInventoryComponent* GetCargoInventory() const { return CargoInventory; }

	// ------------------------------------------------------------------
	// IEclipseInteractable
	// ------------------------------------------------------------------

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool IsInteractionEnabled_Implementation() const override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	/** Seats the vehicle supports, including the driver. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vehicle", meta = (ClampMin = "1", ClampMax = "6"))
	int32 SeatCount = 3;

	/** Tank capacity in litres. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vehicle", meta = (ClampMin = "1.0"))
	float MaxFuelLitres = 45.0f;

	/** Litres burned per second at full throttle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vehicle", meta = (ClampMin = "0.0"))
	float FuelBurnPerSecondAtFullThrottle = 0.35f;

	/** Litres burned per second while the engine idles with a driver aboard. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vehicle", meta = (ClampMin = "0.0"))
	float FuelBurnPerSecondIdle = 0.02f;

	/** Item id consumed to refuel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vehicle")
	FName FuelItemId = TEXT("Item.FuelCell");

	/** Litres one fuel item adds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vehicle", meta = (ClampMin = "1.0"))
	float RefuelAmountPerItem = 15.0f;

	/** Vehicle mesh. A Chaos wheeled vehicle needs a skeletal mesh with a physics asset. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Vehicle")
	TObjectPtr<USkeletalMeshComponent> MeshComponent;

	/** Chaos wheeled movement. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Vehicle")
	TObjectPtr<UChaosWheeledVehicleMovementComponent> VehicleMovement;

	/** Health. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Vehicle")
	TObjectPtr<UEclipseHealthComponent> HealthComponent;

	/** Cargo hold. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Vehicle")
	TObjectPtr<UEclipseInventoryComponent> CargoInventory;

protected:
	/** Attach an occupant to their seat socket. */
	void AttachOccupant(APawn* Occupant, EEclipseSeatType SeatType);

	/** Detach an occupant and drop them beside the vehicle. */
	void DetachOccupant(APawn* Occupant);

	/** Socket name for a seat index. */
	static FName GetSeatSocketName(int32 SeatIndex);

	/** Current driver. */
	UPROPERTY()
	TWeakObjectPtr<APawn> Driver;

	/** Passengers, in seat order. */
	UPROPERTY()
	TArray<TWeakObjectPtr<APawn>> Passengers;

	/** Fuel in the tank. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Vehicle")
	float FuelLitres = 30.0f;

	/** Last throttle command, kept so fuel burn is proportional to actual demand. */
	float LastThrottleInput = 0.0f;
};
