// PROJECT ECLIPSE - Biome volume.
//
// Purpose
//   A named region of the map: the frozen plateau, the toxic marsh, the orbital crash zone.
//   Biomes drive weather, spawn tables and the HUD compass label, so they have to be
//   authored in the level rather than inferred.
//
// Overlaps
//   Volumes may overlap. The subsystem resolves an overlap by taking the smallest volume
//   containing the point, which is what lets a designer place a small "underground complex"
//   box inside a larger "desert expanse" without a conflict.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "GameFramework/Actor.h"
#include "EclipseBiomeVolume.generated.h"

class UBoxComponent;

/**
 * A box region tagged with a biome.
 *
 * Also carries an optional shelter flag: a volume marked sheltered stops weather damage
 * for anything inside it, which is how interiors work without a separate system.
 */
UCLASS()
class ECLIPSE_API AEclipseBiomeVolume : public AActor
{
	GENERATED_BODY()

public:
	AEclipseBiomeVolume();

	/** Biome this volume declares. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	EEclipseBiome GetBiome() const { return Biome; }

	/** True when the volume shelters actors from weather damage. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	bool IsSheltered() const { return bSheltered; }

	/** True when a world position is inside the volume. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	bool ContainsLocation(const FVector& Location) const;

	/** Volume of the box in cubic centimetres, used to resolve overlaps. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	float GetVolumeSize() const;

	/** Box extents in centimetres. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|World")
	FVector GetBoxExtent() const;

	/** Box component defining the region. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Eclipse|World")
	TObjectPtr<UBoxComponent> BoxComponent;

protected:
	/** Biome declared by this volume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|World")
	EEclipseBiome Biome = EEclipseBiome::None;

	/** True when the volume shelters its contents from the weather. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eclipse|World")
	bool bSheltered = false;
};
