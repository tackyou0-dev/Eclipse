// PROJECT ECLIPSE - Recipe definition and catalogue.
//
// Purpose
//   Recipes are data. Ten ship with the vertical slice: medkits, stim shots, 5.56mm and
//   .338 ammunition, power cells, a suppressor, a 4x scope, a composite vest, a hazmat
//   suit and a relay module.
//
// Station gating
//   RequiredStation is an EEclipseBaseModule, so a recipe that needs the laboratory cannot
//   be crafted at the workshop. Skills unlock recipes through
//   EEclipseSkillEffectType::UnlockRecipe.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Crafting/EclipseCraftingTypes.h"
#include "Engine/DataAsset.h"
#include "EclipseRecipeDefinition.generated.h"

/** Item categories a recipe can belong to. Drives the crafting menu tabs. */
UENUM(BlueprintType)
enum class EEclipseRecipeCategory : uint8
{
	Medical			UMETA(DisplayName = "Medical"),
	Ammunition		UMETA(DisplayName = "Ammunition"),
	Weapon			UMETA(DisplayName = "Weapon"),
	Armor			UMETA(DisplayName = "Armor"),
	Technology		UMETA(DisplayName = "Technology"),
	Resource		UMETA(DisplayName = "Resource"),
	Base			UMETA(DisplayName = "Base"),

	Count			UMETA(Hidden)
};

/**
 * A craftable recipe.
 *
 * Ingredients are consumed when the craft starts, not when it finishes, so a cancelled
 * craft returns what it took rather than leaving the player short.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseRecipeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable id, for example "Recipe.Medkit". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Crafting")
	FName RecipeId;

	/** Display name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Crafting")
	FText DisplayName;

	/** Menu category. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Crafting")
	EEclipseRecipeCategory Category = EEclipseRecipeCategory::Technology;

	/** Station required to craft. None means "craftable anywhere". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Crafting")
	EEclipseBaseModule RequiredStation = EEclipseBaseModule::None;

	/** Character level required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Crafting", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;

	/** Skill that must be learned, or None. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Crafting")
	FName RequiredSkillId;

	/** Seconds the craft takes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Crafting", meta = (ClampMin = "0.0"))
	float CraftTimeSeconds = 1.0f;

	/** Item produced. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Crafting")
	FName OutputItemId;

	/** How many are produced per craft. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Crafting", meta = (ClampMin = "1"))
	int32 OutputCount = 1;

	/** What the craft consumes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Crafting")
	TArray<FEclipseRecipeIngredient> Ingredients;

	/** True when the recipe is complete enough to be offered to a player. */
	bool IsValidRecipe() const
	{
		if (RecipeId.IsNone() || OutputItemId.IsNone() || OutputCount < 1 || CraftTimeSeconds < 0.0f || Ingredients.Num() == 0)
		{
			return false;
		}

		for (const FEclipseRecipeIngredient& Ingredient : Ingredients)
		{
			if (!Ingredient.IsValidIngredient())
			{
				return false;
			}
		}

		return true;
	}

	/** Total number of ingredient units consumed. */
	int32 GetTotalIngredientUnits() const;
};

/**
 * Every recipe in the game, so the crafting UI and the content validator share one list.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseRecipeCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** All recipes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Catalog")
	TArray<TObjectPtr<UEclipseRecipeDefinition>> Recipes;

	/** Find a recipe by id, or null. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Catalog")
	UEclipseRecipeDefinition* Find(FName RecipeId) const;

	/** Every recipe that produces an item, for the "where do I get this" tooltip. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Catalog")
	TArray<UEclipseRecipeDefinition*> FindByOutput(FName ItemId) const;

	/** True when the id exists. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Catalog")
	bool Contains(FName RecipeId) const;

	/** Static accessor for the shipped catalogue. */
	static UEclipseRecipeCatalog* GetCatalog();
};
