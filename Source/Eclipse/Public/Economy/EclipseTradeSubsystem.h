// PROJECT ECLIPSE - Trade subsystem.
//
// Purpose
//   Buying and selling. Prices are computed on the server from the vendor's faction
//   pricing table and the player's standing, never sent by a client, and every credit
//   movement is validated against UEclipseDeveloperSettings::MaxSingleTransactionDelta
//   before it is applied.
//
// Authority
//   Server only. The client's trade window calls these functions through its player
//   controller and receives the resulting transaction record for the log.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Economy/EclipseEconomyTypes.h"
#include "Economy/EclipseVendorDefinition.h"
#include "Subsystems/WorldSubsystem.h"
#include "EclipseTradeSubsystem.generated.h"

class UEclipseInventoryComponent;

/**
 * Server-side trading.
 *
 * Stock is tracked per vendor instance in this subsystem rather than on the data asset, so
 * two quartermasters of the same faction do not share a shelf, and so a restock timer can
 * refill one without touching the asset.
 */
UCLASS()
class ECLIPSE_API UEclipseTradeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Price an item for a buyer, including the buyer's standing with the vendor's faction.
	 * Never returns a zero buy price: everything costs at least one credit.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Trade")
	FEclipsePriceQuote QuotePrice(FName ItemId, EEclipseFaction VendorFaction) const;

	/** How many of an item a vendor currently has in stock. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Trade")
	int32 GetStockCount(const UEclipseVendorDefinition* Vendor, FName ItemId) const;

	/**
	 * Buy items. Returns false and moves nothing when the buyer cannot afford them, the
	 * vendor is out of stock, or the transaction would exceed the credit ceiling.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Trade")
	bool BuyItem(AActor* Buyer, UEclipseVendorDefinition* Vendor, FName ItemId, int32 Count, FEclipseTransaction& OutTransaction);

	/** Sell items from the buyer's inventory. Returns false when they do not have them. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Trade")
	bool SellItem(AActor* Buyer, UEclipseVendorDefinition* Vendor, FName ItemId, int32 Count, FEclipseTransaction& OutTransaction);

	/** Repair an item's durability for credits. Price scales with the missing durability. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Trade")
	bool RepairItem(AActor* Buyer, UEclipseVendorDefinition* Vendor, FName ItemId, FEclipseTransaction& OutTransaction);

	/** Restock a vendor to its defined counts. Called at raid start and by the timer. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Trade")
	void RestockVendor(UEclipseVendorDefinition* Vendor);

	/** Fired after a transaction, accepted or not. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FEclipseTransactionRecorded, const FEclipseTransaction& /*Transaction*/);
	FEclipseTransactionRecorded OnTransactionRecorded;

	/** Cost in credits to fully repair one point of missing durability. */
	static constexpr float RepairCostPerDurabilityPoint = 1.5f;

protected:
	/** Live stock per vendor id. */
	UPROPERTY()
	TMap<FName, TArray<FEclipseVendorStockEntry>> VendorStock;

	/** Read the buyer's credit balance. */
	int64 GetBuyerCredits(AActor* Buyer) const;

	/** Apply a credit delta to the buyer, rejecting anything past the ceiling. */
	bool ApplyCredits(AActor* Buyer, int64 Delta, FString& OutRejectReason) const;

	/** Record and broadcast a transaction. */
	void Record(const FEclipseTransaction& Transaction);
};
