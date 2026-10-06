// PROJECT ECLIPSE - Economy value types.
//
// Purpose
//   Pricing and transaction records. Every credit that moves in the game is described by
//   an FEclipseTransaction, and every transaction is validated against the same ceiling
//   and written to the analytics stream.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "EclipseEconomyTypes.generated.h"

/**
 * Vendor pricing for one item category.
 *
 * Multipliers are applied to the item's base value: buy prices use BuyPriceMultiplier,
 * sell prices use SellPriceMultiplier. Both are authored per faction, which is what makes
 * the Iron Wolves expensive for ammunition and the Colonists cheap for it.
 */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseVendorPricing
{
	GENERATED_BODY()

	/** Item category this row applies to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Economy")
	EEclipseItemCategory Category = EEclipseItemCategory::Miscellaneous;

	/** Multiplier applied when the player buys. Above 1.0 is a markup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Economy", meta = (ClampMin = "0.0"))
	float BuyPriceMultiplier = 1.0f;

	/** Multiplier applied when the player sells. Below 1.0 is a trade-in loss. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eclipse|Economy", meta = (ClampMin = "0.0"))
	float SellPriceMultiplier = 0.5f;
};

/** A record of credits moving. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseTransaction
{
	GENERATED_BODY()

	/** What kind of transaction this was. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Economy")
	EEclipseTransactionKind Kind = EEclipseTransactionKind::Buy;

	/** Item involved, or None for credit-only transactions such as rewards. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Economy")
	FName ItemId;

	/** Units moved. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Economy")
	int32 Count = 0;

	/** Credits moved. Positive is a payment to the player, negative is a payment from them. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Economy")
	int64 CreditDelta = 0;

	/** Vendor or faction on the other side of the trade. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Economy")
	EEclipseFaction Counterparty = EEclipseFaction::None;

	/** True when the transaction was accepted. Refusals are recorded too. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Economy")
	bool bAccepted = false;

	/** Reason a transaction was refused, for the log and for analytics. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Economy")
	FString RefusalReason;
};

/** Result of pricing an item. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipsePriceQuote
{
	GENERATED_BODY()

	/** What the player pays to acquire one unit. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Economy")
	int64 BuyPrice = 0;

	/** What the player receives for selling one unit. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Economy")
	int64 SellPrice = 0;

	/** True when the vendor refuses to trade this item at all. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|Economy")
	bool bRefused = false;
};
