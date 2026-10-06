// PROJECT ECLIPSE - Pickup actor.
//
// Purpose
//   An item in the world that a player can pick up: dropped loot, quest items, supply
//   crates handed out by missions.
//
// Interaction
//   Implements IEclipseInteractable, so picking something up is the same code path as
//   opening a door. Pickup is server-authoritative: the client asks, the server moves the
//   items into the inventory and destroys the actor.

#pragma once

#include "Core/EclipseInteractable.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EclipsePickupActor.generated.h"

class UStaticMeshComponent;

/**
 * A pickup in the world.
 *
 * Stack contents live on the actor rather than in a spawned inventory component, because a
 * pickup holds exactly one stack until someone takes it.
 */
UCLASS()
class ECLIPSE_API AEclipsePickupActor : public AActor, public IEclipseInteractable
{
	GENERATED_BODY()

public:
	AEclipsePickupActor();

	/** Configure the pickup. Called immediately after spawning and when loading a save. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Pickup")
	void InitializePickup(FName InItemId, int32 InCount);

	/** Item this pickup holds. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Pickup")
	FName GetItemId() const { return ItemId; }

	/** How many are held. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Pickup")
	int32 GetCount() const { return Count; }

	/** True once the contents have been taken. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Pickup")
	bool IsLooted() const { return bLooted; }

	// ------------------------------------------------------------------
	// IEclipseInteractable
	// ------------------------------------------------------------------

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool IsInteractionEnabled_Implementation() const override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	/** Mesh used for the pickup. Optional: a greybox pickup can be meshless. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Pickup")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

protected:
	/** Item held by this pickup. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Eclipse|Pickup")
	FName ItemId;

	/** Number of items held. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Eclipse|Pickup")
	int32 Count = 1;

	/** True once taken. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Pickup")
	bool bLooted = false;

	/** Lifetime in seconds after being looted. Zero destroys immediately. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|Pickup", meta = (ClampMin = "0.0"))
	float LootedLifetimeSeconds = 0.5f;

	/** UActorComponent: replicate the contents. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
