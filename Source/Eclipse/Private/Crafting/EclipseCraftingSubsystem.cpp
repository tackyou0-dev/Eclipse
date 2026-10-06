// PROJECT ECLIPSE - Crafting subsystem implementation.
//
// Purpose
//   Ingredient validation, timed crafts and refunds. Note the ordering: every check runs
//   before anything is consumed, and the refund path is the exact inverse of the consume
//   path, so a cancelled craft can never lose materials.

#include "Crafting/EclipseCraftingSubsystem.h"

#include "Crafting/EclipseWorkstation.h"
#include "Core/EclipseLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Inventory/EclipseInventoryComponent.h"
#include "Progression/EclipseProgressionComponent.h"
#include "TimerManager.h"

void UEclipseCraftingSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		for (const TPair<TWeakObjectPtr<AActor>, FTimerHandle>& Pair : CraftTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}

	CraftTimers.Reset();
	ActiveCrafts.Reset();

	Super::Deinitialize();
}

namespace
{
	/** Find the inventory a crafter works out of. */
	UEclipseInventoryComponent* GetCrafterInventory(AActor* Crafter)
	{
		return Crafter != nullptr ? Crafter->FindComponentByClass<UEclipseInventoryComponent>() : nullptr;
	}

	/** Find the progression state of a crafter, if the actor has any. */
	UEclipseProgressionComponent* GetCrafterProgression(AActor* Crafter)
	{
		return Crafter != nullptr ? Crafter->FindComponentByClass<UEclipseProgressionComponent>() : nullptr;
	}

	/** True when a workstation providing Module is within range of Crafter. */
	bool HasStationInRange(const AActor* Crafter, EEclipseBaseModule Module)
	{
		if (Module == EEclipseBaseModule::None)
		{
			return true;
		}

		UWorld* World = Crafter != nullptr ? Crafter->GetWorld() : nullptr;
		if (World == nullptr)
		{
			return false;
		}

		for (TActorIterator<AEclipseWorkstation> It(World); It; ++It)
		{
			const AEclipseWorkstation* Station = *It;
			if (Station != nullptr
				&& Station->ProvidesModule(Module)
				&& FVector::DistSquared(Station->GetActorLocation(), Crafter->GetActorLocation()) <= FMath::Square(AEclipseWorkstation::StationRadius))
			{
				return true;
			}
		}

		return false;
	}
}

TArray<FEclipseRecipeIngredient> UEclipseCraftingSubsystem::GetMissingIngredients(const UEclipseInventoryComponent* Inventory, UEclipseRecipeDefinition* Recipe) const
{
	TArray<FEclipseRecipeIngredient> Missing;

	if (Inventory == nullptr || Recipe == nullptr)
	{
		return Missing;
	}

	for (const FEclipseRecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		const int32 Carried = Inventory->CountItem(Ingredient.ItemId);
		if (Carried < Ingredient.Count)
		{
			Missing.Emplace(Ingredient.ItemId, Ingredient.Count - Carried);
		}
	}

	return Missing;
}

bool UEclipseCraftingSubsystem::CanCraft(AActor* Crafter, UEclipseRecipeDefinition* Recipe, EEclipseCraftRefusal& OutRefusal, TArray<FEclipseRecipeIngredient>& OutMissingIngredients) const
{
	OutRefusal = EEclipseCraftRefusal::None;
	OutMissingIngredients.Reset();

	if (Recipe == nullptr || !Recipe->IsValidRecipe())
	{
		OutRefusal = EEclipseCraftRefusal::UnknownRecipe;
		return false;
	}

	if (Crafter == nullptr)
	{
		OutRefusal = EEclipseCraftRefusal::UnknownRecipe;
		return false;
	}

	if (IsCrafting(Crafter))
	{
		OutRefusal = EEclipseCraftRefusal::Busy;
		return false;
	}

	UEclipseInventoryComponent* Inventory = GetCrafterInventory(Crafter);
	if (Inventory == nullptr)
	{
		OutRefusal = EEclipseCraftRefusal::InventoryFull;
		return false;
	}

	const UEclipseProgressionComponent* Progression = GetCrafterProgression(Crafter);
	if (Progression != nullptr)
	{
		if (Progression->GetLevel() < Recipe->RequiredLevel)
		{
			OutRefusal = EEclipseCraftRefusal::MissingLevel;
			return false;
		}

		if (!Recipe->RequiredSkillId.IsNone() && !Progression->KnowsSkill(Recipe->RequiredSkillId))
		{
			OutRefusal = EEclipseCraftRefusal::MissingSkill;
			return false;
		}

		if (!Progression->KnowsRecipe(Recipe->RecipeId))
		{
			OutRefusal = EEclipseCraftRefusal::MissingSkill;
			return false;
		}
	}

	if (!HasStationInRange(Crafter, Recipe->RequiredStation))
	{
		OutRefusal = EEclipseCraftRefusal::MissingStation;
		return false;
	}

	OutMissingIngredients = GetMissingIngredients(Inventory, Recipe);
	if (OutMissingIngredients.Num() > 0)
	{
		OutRefusal = EEclipseCraftRefusal::MissingIngredients;
		return false;
	}

	// Space for the output is checked with the ingredients still in the pack, which is the
	// conservative answer: a craft that cannot fit its result is refused up front.
	if (Inventory->GetAcceptableCount(Recipe->OutputItemId) < Recipe->OutputCount)
	{
		OutRefusal = EEclipseCraftRefusal::InventoryFull;
		return false;
	}

	return true;
}

bool UEclipseCraftingSubsystem::BeginCraft(AActor* Crafter, UEclipseRecipeDefinition* Recipe)
{
	UWorld* World = GetWorld();
	if (World == nullptr || Crafter == nullptr || Recipe == nullptr)
	{
		return false;
	}

	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogEclipseSecurity, Warning, TEXT("BeginCraft refused on a client."));
		return false;
	}

	EEclipseCraftRefusal Refusal = EEclipseCraftRefusal::None;
	TArray<FEclipseRecipeIngredient> Missing;
	if (!CanCraft(Crafter, Recipe, Refusal, Missing))
	{
		UE_LOG(LogEclipseItems, Verbose, TEXT("%s cannot craft %s: refusal %d."),
			*Crafter->GetName(), *Recipe->RecipeId.ToString(), static_cast<int32>(Refusal));
		OnCraftFailed.Broadcast(Crafter, Recipe->RecipeId, Refusal);
		return false;
	}

	UEclipseInventoryComponent* Inventory = GetCrafterInventory(Crafter);
	if (Inventory == nullptr)
	{
		return false;
	}

	// Consume everything up front. If any single removal fails the craft is rolled back,
	// so the inventory can never be left half-consumed.
	TArray<FEclipseRecipeIngredient> Consumed;
	for (const FEclipseRecipeIngredient& Ingredient : Recipe->Ingredients)
	{
		if (!Inventory->RemoveItem(Ingredient.ItemId, Ingredient.Count))
		{
			for (const FEclipseRecipeIngredient& Refund : Consumed)
			{
				Inventory->AddItem(Refund.ItemId, Refund.Count);
			}

			UE_LOG(LogEclipseItems, Warning, TEXT("Craft of %s rolled back: could not consume %s."),
				*Recipe->RecipeId.ToString(), *Ingredient.ItemId.ToString());
			OnCraftFailed.Broadcast(Crafter, Recipe->RecipeId, EEclipseCraftRefusal::MissingIngredients);
			return false;
		}

		Consumed.Add(Ingredient);
	}

	FEclipseActiveCraft& Craft = ActiveCrafts.FindOrAdd(Crafter);
	Craft.RecipeId = Recipe->RecipeId;
	Craft.Crafter = Crafter;
	Craft.DurationSeconds = FMath::Max(0.0f, Recipe->CraftTimeSeconds);
	Craft.StartWorldTimeSeconds = World->GetTimeSeconds();

	FTimerHandle& Handle = CraftTimers.FindOrAdd(Crafter);
	FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UEclipseCraftingSubsystem::CompleteCraft, Crafter);

	if (Craft.DurationSeconds <= 0.0f)
	{
		// Instant recipes still go through the same completion path so that logging,
		// analytics and the output grant exist in exactly one place.
		CompleteCraft(Crafter);
	}
	else
	{
		World->GetTimerManager().SetTimer(Handle, Delegate, Craft.DurationSeconds, false);
	}

	UE_LOG(LogEclipseItems, Log, TEXT("%s started crafting %s (%.1fs)."),
		*Crafter->GetName(), *Recipe->RecipeId.ToString(), Craft.DurationSeconds);

	return true;
}

void UEclipseCraftingSubsystem::CompleteCraft(AActor* Crafter)
{
	FEclipseActiveCraft* Craft = ActiveCrafts.Find(Crafter);
	if (Craft == nullptr)
	{
		return;
	}

	const FName RecipeId = Craft->RecipeId;
	ActiveCrafts.Remove(Crafter);
	CraftTimers.Remove(Crafter);

	UEclipseRecipeCatalog* Catalog = UEclipseRecipeCatalog::GetCatalog();
	UEclipseRecipeDefinition* Recipe = Catalog != nullptr ? Catalog->Find(RecipeId) : nullptr;
	if (Recipe == nullptr)
	{
		UE_LOG(LogEclipseItems, Error, TEXT("Craft of '%s' completed but the recipe no longer exists."), *RecipeId.ToString());
		return;
	}

	UEclipseInventoryComponent* Inventory = GetCrafterInventory(Crafter);
	if (Inventory == nullptr)
	{
		return;
	}

	const int32 Accepted = Inventory->AddItem(Recipe->OutputItemId, Recipe->OutputCount);
	if (Accepted < Recipe->OutputCount)
	{
		// The pack filled up mid-craft. The remainder is refunded as ingredients rather
		// than vanishing, which is the only outcome a player would call fair.
		const int32 Missing = Recipe->OutputCount - Accepted;
		UE_LOG(LogEclipseItems, Warning, TEXT("Craft of %s overflowed the inventory; refunding %d unit(s) of ingredients."),
			*RecipeId.ToString(), Missing);

		for (const FEclipseRecipeIngredient& Ingredient : Recipe->Ingredients)
		{
			const int32 Refund = FMath::Max(1, FMath::RoundToInt(Ingredient.Count * (static_cast<float>(Missing) / static_cast<float>(Recipe->OutputCount))));
			Inventory->AddItem(Ingredient.ItemId, Refund);
		}
	}

	OnCraftCompleted.Broadcast(Crafter, RecipeId);
}

bool UEclipseCraftingSubsystem::CancelCraft(AActor* Crafter)
{
	const FEclipseActiveCraft* Craft = ActiveCrafts.Find(Crafter);
	if (Craft == nullptr)
	{
		return false;
	}

	const FName RecipeId = Craft->RecipeId;

	if (UWorld* World = GetWorld())
	{
		if (FTimerHandle* Handle = CraftTimers.Find(Crafter))
		{
			World->GetTimerManager().ClearTimer(*Handle);
		}
	}

	ActiveCrafts.Remove(Crafter);
	CraftTimers.Remove(Crafter);

	// Return the ingredients: cancelling must never cost the player materials.
	UEclipseRecipeCatalog* Catalog = UEclipseRecipeCatalog::GetCatalog();
	UEclipseRecipeDefinition* Recipe = Catalog != nullptr ? Catalog->Find(RecipeId) : nullptr;
	if (Recipe != nullptr)
	{
		if (UEclipseInventoryComponent* Inventory = GetCrafterInventory(Crafter))
		{
			for (const FEclipseRecipeIngredient& Ingredient : Recipe->Ingredients)
			{
				Inventory->AddItem(Ingredient.ItemId, Ingredient.Count);
			}
		}
	}

	OnCraftFailed.Broadcast(Crafter, RecipeId, EEclipseCraftRefusal::None);
	return true;
}

bool UEclipseCraftingSubsystem::IsCrafting(AActor* Crafter) const
{
	return Crafter != nullptr && ActiveCrafts.Contains(Crafter);
}

float UEclipseCraftingSubsystem::GetCraftProgress(AActor* Crafter) const
{
	const UWorld* World = GetWorld();
	const FEclipseActiveCraft* Craft = Crafter != nullptr ? ActiveCrafts.Find(Crafter) : nullptr;
	if (Craft == nullptr || World == nullptr)
	{
		return 0.0f;
	}

	// Progress is derived from the world clock rather than from the timer handle, so it
	// keeps working for instant crafts and after a level transition.
	return Craft->GetProgress(World->GetTimeSeconds());
}

TArray<UEclipseRecipeDefinition*> UEclipseCraftingSubsystem::GetKnownRecipes(AActor* Crafter) const
{
	TArray<UEclipseRecipeDefinition*> Known;

	UEclipseRecipeCatalog* Catalog = UEclipseRecipeCatalog::GetCatalog();
	if (Catalog == nullptr)
	{
		return Known;
	}

	const UEclipseProgressionComponent* Progression = GetCrafterProgression(Crafter);

	for (const TObjectPtr<UEclipseRecipeDefinition>& Recipe : Catalog->Recipes)
	{
		if (Recipe == nullptr)
		{
			continue;
		}

		if (Progression != nullptr)
		{
			if (Progression->GetLevel() < Recipe->RequiredLevel || !Progression->KnowsRecipe(Recipe->RecipeId))
			{
				continue;
			}
		}

		Known.Add(Recipe);
	}

	return Known;
}
