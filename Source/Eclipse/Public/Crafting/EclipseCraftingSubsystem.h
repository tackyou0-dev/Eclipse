// PROJECT ECLIPSE - Crafting subsystem.
//
// Purpose
//   Timed crafting with real ingredient accounting. The subsystem owns one craft per
//   crafter at a time, consumes ingredients when the craft starts, and returns them if the
//   craft is cancelled.
//
// Authority
//   Server only. A client asks to craft through its player controller; the server checks
//   station, level, skill and ingredients before anything is consumed.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Crafting/EclipseCraftingTypes.h"
#include "Crafting/EclipseRecipeDefinition.h"
#include "Subsystems/WorldSubsystem.h"
#include "EclipseCraftingSubsystem.generated.h"

class UEclipseInventoryComponent;
class UEclipseProgressionComponent;

/** Fired when a craft completes. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FEclipseCraftCompleted, AActor* /*Crafter*/, FName /*RecipeId*/);

/** Fired when a craft is cancelled or refused. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FEclipseCraftFailed, AActor* /*Crafter*/, FName /*RecipeId*/, EEclipseCraftRefusal /*Reason*/);

/**
 * World-scoped crafting service.
 *
 * Timers are used rather than a tick loop because a craft is a single duration and the
 * server may be simulating dozens of them at once across a squad.
 */
UCLASS()
class ECLIPSE_API UEclipseCraftingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** UWorldSubsystem: clear timers when the world goes away. */
	virtual void Deinitialize() override;

	/**
	 * Check whether a craft is possible right now, without changing anything.
	 * OutReason is filled with the first blocking condition.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Crafting")
	bool CanCraft(AActor* Crafter, UEclipseRecipeDefinition* Recipe, EEclipseCraftRefusal& OutRefusal, TArray<FEclipseRecipeIngredient>& OutMissingIngredients) const;

	/**
	 * Start a craft. Ingredients are taken immediately; the output is granted when the
	 * timer finishes. Returns false and consumes nothing when CanCraft fails.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Crafting")
	bool BeginCraft(AActor* Crafter, UEclipseRecipeDefinition* Recipe);

	/** Cancel a craft, returning its ingredients. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Crafting")
	bool CancelCraft(AActor* Crafter);

	/** True while this crafter is busy. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Crafting")
	bool IsCrafting(AActor* Crafter) const;

	/** Progress of a craft in [0, 1], or zero when the crafter is idle. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Crafting")
	float GetCraftProgress(AActor* Crafter) const;

	/** Every recipe the crafter currently qualifies for by level and skill. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Crafting")
	TArray<UEclipseRecipeDefinition*> GetKnownRecipes(AActor* Crafter) const;

	/** Ingredients the crafter is short of. Empty when the craft is possible. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Crafting")
	TArray<FEclipseRecipeIngredient> GetMissingIngredients(const UEclipseInventoryComponent* Inventory, UEclipseRecipeDefinition* Recipe) const;

	/** Craft completed. */
	FEclipseCraftCompleted OnCraftCompleted;

	/** Craft refused or cancelled. */
	FEclipseCraftFailed OnCraftFailed;

protected:
	/** Finish a craft: hand over the output and clear the timer. */
	void CompleteCraft(AActor* Crafter);

	/** Ingredients consumed by an in-flight craft, keyed by crafter, for cancellation. */
	UPROPERTY()
	TMap<TObjectPtr<AActor>, FEclipseActiveCraft> ActiveCrafts;

	/** Timer handles per crafter, kept out of the replicated state. */
	TMap<TWeakObjectPtr<AActor>, FTimerHandle> CraftTimers;
};
