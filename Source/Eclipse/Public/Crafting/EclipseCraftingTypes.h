// PROJECT ECLIPSE - Crafting value types.
//
// Purpose
//   Recipe inputs, craft requests and craft bookkeeping. The types are shared between the
//   recipe data asset, the crafting subsystem and the UI so that "what does this recipe
//   need" has exactly one representation.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "EclipseCraftingTypes.generated.h"

/** One ingredient of a recipe. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseRecipeIngredient
{
	GENERATED_BODY()

	/** Item definition id required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Crafting")
	FName ItemId;

	/** How many are required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Crafting", meta = (ClampMin = "1"))
	int32 Count = 1;

	FEclipseRecipeIngredient() = default;

	FEclipseRecipeIngredient(FName InItemId, int32 InCount)
		: ItemId(InItemId)
		, Count(FMath::Max(1, InCount))
	{
	}

	/** True when the ingredient is usable. */
	bool IsValidIngredient() const
	{
		return !ItemId.IsNone() && Count > 0;
	}
};

/** A craft in progress. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseActiveCraft
{
	GENERATED_BODY()

	/** Recipe being crafted. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Crafting")
	FName RecipeId;

	/** Crafter the result will be given to. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Crafting")
	TObjectPtr<AActor> Crafter = nullptr;

	/** Total duration in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Crafting")
	float DurationSeconds = 0.0f;

	/** World seconds at which the craft started. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Crafting")
	float StartWorldTimeSeconds = 0.0f;

	/** Progress in [0, 1] at the given world time. */
	float GetProgress(float NowSeconds) const
	{
		if (DurationSeconds <= 0.0f)
		{
			return 1.0f;
		}
		return FMath::Clamp((NowSeconds - StartWorldTimeSeconds) / DurationSeconds, 0.0f, 1.0f);
	}

	/** Seconds left at the given world time. */
	float GetRemaining(float NowSeconds) const
	{
		return FMath::Max(0.0f, DurationSeconds - (NowSeconds - StartWorldTimeSeconds));
	}
};

/** Why a craft was refused. */
UENUM(BlueprintType)
enum class EEclipseCraftRefusal : uint8
{
	None				UMETA(DisplayName = "None"),
	UnknownRecipe		UMETA(DisplayName = "Unknown Recipe"),
	MissingStation		UMETA(DisplayName = "Missing Station"),
	MissingLevel		UMETA(DisplayName = "Level Too Low"),
	MissingSkill		UMETA(DisplayName = "Skill Not Learned"),
	MissingIngredients	UMETA(DisplayName = "Missing Ingredients"),
	InventoryFull		UMETA(DisplayName = "Inventory Full"),
	Busy				UMETA(DisplayName = "Already Crafting"),

	Count				UMETA(Hidden)
};
