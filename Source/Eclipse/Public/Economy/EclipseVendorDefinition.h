// PROJECT ECLIPSE - Vendor definition.
//
// Purpose
//   A trader: which faction they belong to, what they stock, and how often they restock.
//   Prices come from the faction's pricing table plus the player's standing band, so a
//   vendor never carries a second copy of the balance numbers.
//
// Pricing contract (mirrored by the reference model)
//   buy  = floor(base * factionBuyMultiplier * bandBuyMultiplier + 0.5)
//   sell = floor(base * factionSellMultiplier * bandSellMultiplier + 0.5)
//   Band multipliers: hostile 1.25/0.75, neutral 1.0/1.0, friendly 0.92/1.08, allied 0.85/1.15.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Engine/DataAsset.h"
#include "Factions/EclipseFactionTypes.h"
#include "EclipseVendorDefinition.generated.h"

/** One line of a vendor's stock. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseVendorStockEntry
{
	GENERATED_BODY()

	/** Item definition id stocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Vendor")
	FName ItemId;

	/** How many are on the shelf. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Vendor", meta = (ClampMin = "0"))
	int32 Count = 0;

	/** Seconds until this line restocks. Zero means it never does. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Vendor", meta = (ClampMin = "0.0"))
	float RestockSeconds = 0.0f;

	/** True when the line is usable. */
	bool IsValidEntry() const
	{
		return !ItemId.IsNone() && Count >= 0;
	}
};

/**
 * A trader the player can buy from and sell to.
 *
 * Vendors are placed in the level and interactable; the trade subsystem owns the actual
 * transaction so that a client cannot invent a price.
 */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseVendorDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable id, for example "Vendor.ColonistQuartermaster". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vendor")
	FName VendorId;

	/** Display name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vendor")
	FText DisplayName;

	/** Faction the vendor belongs to. Drives pricing and standing checks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vendor")
	EEclipseFaction Faction = EEclipseFaction::FreeColonists;

	/** True when the player can buy from this vendor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vendor")
	bool bSellsItems = true;

	/** True when the player can sell to this vendor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vendor")
	bool bBuysItems = true;

	/**
	 * Lowest standing band the vendor will trade with. A hostile faction vendor simply
	 * refuses to open the trade window.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vendor")
	EEclipseReputationBand MinimumBand = EEclipseReputationBand::Neutral;

	/** What the vendor stocks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Vendor")
	TArray<FEclipseVendorStockEntry> Stock;

	/** Buy-price multiplier for the player's standing band. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vendor")
	static float GetBandBuyMultiplier(EEclipseReputationBand Band);

	/** Sell-price multiplier for the player's standing band. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vendor")
	static float GetBandSellMultiplier(EEclipseReputationBand Band);

	/** True when this vendor will trade at all with the given standing. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Vendor")
	bool WillTradeAt(EEclipseReputationBand Band) const;

	/** Stock entry for an item, or null. */
	const FEclipseVendorStockEntry* FindStockEntry(FName ItemId) const;
};
