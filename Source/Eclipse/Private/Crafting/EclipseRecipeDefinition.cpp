// PROJECT ECLIPSE - Recipe definition and catalogue implementation.
//
// Purpose
//   Ingredient accounting and catalogue lookup.

#include "Crafting/EclipseRecipeDefinition.h"

#include "Core/EclipseLog.h"

int32 UEclipseRecipeDefinition::GetTotalIngredientUnits() const
{
	int32 Total = 0;
	for (const FEclipseRecipeIngredient& Ingredient : Ingredients)
	{
		Total += Ingredient.Count;
	}
	return Total;
}

UEclipseRecipeDefinition* UEclipseRecipeCatalog::Find(FName RecipeId) const
{
	if (RecipeId.IsNone())
	{
		return nullptr;
	}

	for (const TObjectPtr<UEclipseRecipeDefinition>& Recipe : Recipes)
	{
		if (Recipe != nullptr && Recipe->RecipeId == RecipeId)
		{
			return Recipe;
		}
	}

	return nullptr;
}

bool UEclipseRecipeCatalog::Contains(FName RecipeId) const
{
	return Find(RecipeId) != nullptr;
}

TArray<UEclipseRecipeDefinition*> UEclipseRecipeCatalog::FindByOutput(FName ItemId) const
{
	TArray<UEclipseRecipeDefinition*> Matches;
	for (const TObjectPtr<UEclipseRecipeDefinition>& Recipe : Recipes)
	{
		if (Recipe != nullptr && Recipe->OutputItemId == ItemId)
		{
			Matches.Add(Recipe);
		}
	}
	return Matches;
}

UEclipseRecipeCatalog* UEclipseRecipeCatalog::GetCatalog()
{
	static TWeakObjectPtr<UEclipseRecipeCatalog> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	const FSoftObjectPath CatalogPath(TEXT("/Game/Eclipse/Data/Recipes/DA_RecipeCatalog.DA_RecipeCatalog"));
	UObject* Loaded = CatalogPath.TryLoad();
	UEclipseRecipeCatalog* Catalog = Cast<UEclipseRecipeCatalog>(Loaded);
	if (Catalog == nullptr)
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("Recipe catalogue could not be loaded from '%s'."), *CatalogPath.ToString());
		return nullptr;
	}

	Cached = Catalog;
	return Catalog;
}
