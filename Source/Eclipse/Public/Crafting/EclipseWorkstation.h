// PROJECT ECLIPSE - Crafting workstation.
//
// Purpose
//   The physical station a recipe requires: a workshop bench, an armory rack, a
//   laboratory, a medical bay. Station gating is real gameplay - the relay module cannot
//   be built at a campfire - so the crafting subsystem looks for one of these in range
//   before it will start a craft.
//
// Interaction
//   Interacting opens the crafting menu with this station selected, which is how the
//   player gets from the world to the recipe list.

#pragma once

#include "Core/EclipseInteractable.h"
#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "GameFramework/Actor.h"
#include "EclipseWorkstation.generated.h"

class UEclipseRecipeDefinition;
class UStaticMeshComponent;

/**
 * A place where crafting happens.
 *
 * Level designers place these; a workstation provides exactly one base module so that the
 * required-station rule stays readable ("relay module needs the Laboratory").
 */
UCLASS()
class ECLIPSE_API AEclipseWorkstation : public AActor, public IEclipseInteractable
{
	GENERATED_BODY()

public:
	AEclipseWorkstation();

	/** Module this station provides. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Crafting")
	EEclipseBaseModule GetProvidedModule() const { return ProvidedModule; }

	/** True when this station satisfies a recipe requirement. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Crafting")
	bool ProvidesModule(EEclipseBaseModule Module) const;

	// ------------------------------------------------------------------
	// IEclipseInteractable
	// ------------------------------------------------------------------

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool IsInteractionEnabled_Implementation() const override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	// ------------------------------------------------------------------
	// Client intents
	// ------------------------------------------------------------------

	/**
	 * Ask the server to start a craft at this station. Clients never craft locally: the
	 * menu sends this intent and the server re-runs every check before anything moves.
	 */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Eclipse|Crafting")
	void ServerRequestCraft(AActor* Interactor, UEclipseRecipeDefinition* Recipe);

	/** Ask the server to cancel the craft this interactor has running. */
	UFUNCTION(Server, Reliable, WithValidation, BlueprintCallable, Category = "Eclipse|Crafting")
	void ServerCancelCraft(AActor* Interactor);

	/** Distance in centimetres within which the station counts as usable. */
	static constexpr float StationRadius = 600.0f;

	/** Mesh used for the station. Optional while the slice is grey-boxed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|Crafting")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

protected:
	/** Module this station provides. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|Crafting")
	EEclipseBaseModule ProvidedModule = EEclipseBaseModule::Workshop;
};
