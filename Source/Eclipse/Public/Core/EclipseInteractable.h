// PROJECT ECLIPSE - Interaction interface.
//
// Purpose
//   The contract for anything a player can interact with: containers, doors, terminals,
//   extraction zones, vendors, mission objectives. The interaction system is the only
//   thing that calls it, so an implementer never has to guess who is asking.
//
// Authority
//   CanInteract and IsInteractionEnabled are queried on both sides for prompt display.
//   Interact only ever executes on the server; the client sends an intent through the
//   player controller and the server re-queries the interface before acting.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EclipseInteractable.generated.h"

/** Bare UInterface so IEclipseInteractable is visible to the reflection system. */
UINTERFACE(MinimalAPI, BlueprintType)
class UEclipseInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by every interactable actor or component.
 *
 * The default implementations live in EclipseInteractable.cpp: an implementer that
 * forgets a method still behaves predictably (interactable, instant, generic prompt)
 * and logs a warning rather than silently doing nothing.
 */
class ECLIPSE_API IEclipseInteractable
{
	GENERATED_BODY()

public:
	/** True when Interactor is allowed to interact right now. Default: yes. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Eclipse|Interaction")
	bool CanInteract(AActor* Interactor) const;
	virtual bool CanInteract_Implementation(AActor* Interactor) const;

	/** True when the interaction is offered at all (for example, not already looted). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Eclipse|Interaction")
	bool IsInteractionEnabled() const;
	virtual bool IsInteractionEnabled_Implementation() const;

	/** Prompt shown to the player. Default: a generic "Interact". */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Eclipse|Interaction")
	FText GetInteractionPrompt() const;
	virtual FText GetInteractionPrompt_Implementation() const;

	/** Seconds the interaction takes. Zero means instant. Default: instant. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Eclipse|Interaction")
	float GetInteractionDuration() const;
	virtual float GetInteractionDuration_Implementation() const;

	/**
	 * Perform the interaction. Server only in multiplayer. The default implementation
	 * logs a warning so a missing override is visible during play rather than silent.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Eclipse|Interaction")
	void Interact(AActor* Interactor);
	virtual void Interact_Implementation(AActor* Interactor);
};
