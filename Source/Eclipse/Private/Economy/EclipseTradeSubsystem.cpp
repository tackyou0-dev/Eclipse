// PROJECT ECLIPSE - Trade subsystem implementation.
//
// Purpose
//   Pricing and the two transactions that move credits. Every refusal path fills in the
//   transaction record with a reason, because "the trade button did nothing" is the least
//   debuggable bug class in a game with an economy.

#include "Economy/EclipseTradeSubsystem.h"

#include "Core/EclipseDeveloperSettings.h"
#include "Core/EclipseGameInstance.h"
#include "Core/EclipseLog.h"
#include "Factions/EclipseFactionSubsystem.h"
#include "Inventory/EclipseInventoryComponent.h"
#include "Inventory/EclipseItemDefinition.h"

namespace
{
	/** Round half up, matching the reference model's floor(x + 0.5). */
	int64 RoundHalfUp(float Value)
	{
		return static_cast<int64>(FMath::FloorToDouble(static_cast<double>(Value) + 0.5));
	}

	/** Resolve an item definition, or null. */
	UEclipseItemDefinition* ResolveItem(FName ItemId)
	{
		UEclipseItemCatalog* Catalog = UEclipseItemCatalog::GetCatalog();
		return Catalog != nullptr ? Catalog->Find(ItemId) : nullptr;
	}
}

int64 UEclipseTradeSubsystem::GetBuyerCredits(AActor* Buyer) const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UEclipseGameInstance* GameInstance = World->GetGameInstance<UEclipseGameInstance>())
		{
			return GameInstance->GetCredits();
		}
	}
	return 0;
}

bool UEclipseTradeSubsystem::ApplyCredits(AActor* Buyer, int64 Delta, FString& OutRejectReason) const
{
	UWorld* World = GetWorld();
	UEclipseGameInstance* GameInstance = World != nullptr ? World->GetGameInstance<UEclipseGameInstance>() : nullptr;
	if (GameInstance == nullptr)
	{
		OutRejectReason = TEXT("No game instance to hold credits.");
		return false;
	}

	const int64 NewTotal = GameInstance->GetCredits() + Delta;
	return GameInstance->SetCredits(NewTotal, OutRejectReason);
}

FEclipsePriceQuote UEclipseTradeSubsystem::QuotePrice(FName ItemId, EEclipseFaction VendorFaction) const
{
	FEclipsePriceQuote Quote;

	const UEclipseItemDefinition* Definition = ResolveItem(ItemId);
	if (Definition == nullptr)
	{
		Quote.bRefused = true;
		return Quote;
	}

	// Quest items are not for sale, and nothing can be sold to a vendor that does not
	// trust the player.
	if (Definition->Category == EEclipseItemCategory::QuestItem)
	{
		Quote.bRefused = true;
		return Quote;
	}

	const UEclipseGameInstance* GameInstance = GetWorld() != nullptr ? GetWorld()->GetGameInstance<UEclipseGameInstance>() : nullptr;
	EEclipseReputationBand Band = EEclipseReputationBand::Neutral;

	if (const UGameInstance* GameInstanceBase = GameInstance)
	{
		if (const UEclipseFactionSubsystem* FactionSubsystem = GameInstanceBase->GetSubsystem<UEclipseFactionSubsystem>())
		{
			Band = FactionSubsystem->GetBand(VendorFaction);
		}
	}

	float BuyMultiplier = 1.25f;
	float SellMultiplier = 0.35f;
	if (const UGameInstance* GameInstanceBase = GameInstance)
	{
		if (const UEclipseFactionSubsystem* FactionSubsystem = GameInstanceBase->GetSubsystem<UEclipseFactionSubsystem>())
		{
			if (const UEclipseFactionDefinition* FactionDefinition = FactionSubsystem->GetDefinition(VendorFaction))
			{
				const FEclipseVendorPricing Pricing = FactionDefinition->GetPricing(Definition->Category);
				BuyMultiplier = Pricing.BuyPriceMultiplier;
				SellMultiplier = Pricing.SellPriceMultiplier;
			}
		}
	}

	const float BaseValue = static_cast<float>(Definition->BaseValue);
	Quote.BuyPrice = FMath::Max<int64>(1, RoundHalfUp(BaseValue * BuyMultiplier * UEclipseVendorDefinition::GetBandBuyMultiplier(Band)));
	Quote.SellPrice = FMath::Max<int64>(0, RoundHalfUp(BaseValue * SellMultiplier * UEclipseVendorDefinition::GetBandSellMultiplier(Band)));
	return Quote;
}

int32 UEclipseTradeSubsystem::GetStockCount(const UEclipseVendorDefinition* Vendor, FName ItemId) const
{
	if (Vendor == nullptr)
	{
		return 0;
	}

	if (const TArray<FEclipseVendorStockEntry>* Stock = VendorStock.Find(Vendor->VendorId))
	{
		if (const FEclipseVendorStockEntry* Entry = Stock->FindByPredicate([ItemId](const FEclipseVendorStockEntry& Candidate)
			{
				return Candidate.ItemId == ItemId;
			}))
		{
			return Entry->Count;
		}
		return 0;
	}

	// Not yet restocked this session: the authored counts are the answer until the first
	// transaction, which is what makes a fresh vendor usable immediately.
	if (const FEclipseVendorStockEntry* Authored = Vendor->FindStockEntry(ItemId))
	{
		return Authored->Count;
	}

	return 0;
}

void UEclipseTradeSubsystem::RestockVendor(UEclipseVendorDefinition* Vendor)
{
	if (Vendor == nullptr)
	{
		return;
	}

	VendorStock.Add(Vendor->VendorId, Vendor->Stock);
	UE_LOG(LogEclipseItems, Log, TEXT("Vendor %s restocked with %d lines."),
		*Vendor->VendorId.ToString(), Vendor->Stock.Num());
}

void UEclipseTradeSubsystem::Record(const FEclipseTransaction& Transaction)
{
	OnTransactionRecorded.Broadcast(Transaction);

	UE_LOG(LogEclipseItems, Log, TEXT("Transaction %s: %s x%d, %lld credits, accepted=%s%s."),
		*UEnum::GetValueAsString(Transaction.Kind),
		*Transaction.ItemId.ToString(),
		Transaction.Count,
		Transaction.CreditDelta,
		Transaction.bAccepted ? TEXT("yes") : TEXT("no"),
		Transaction.RefusalReason.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Transaction.RefusalReason));
}

bool UEclipseTradeSubsystem::BuyItem(AActor* Buyer, UEclipseVendorDefinition* Vendor, FName ItemId, int32 Count, FEclipseTransaction& OutTransaction)
{
	OutTransaction = FEclipseTransaction();
	OutTransaction.Kind = EEclipseTransactionKind::Buy;
	OutTransaction.ItemId = ItemId;
	OutTransaction.Count = Count;

	if (Buyer == nullptr || Vendor == nullptr || Count <= 0)
	{
		OutTransaction.RefusalReason = TEXT("Invalid trade request.");
		Record(OutTransaction);
		return false;
	}

	OutTransaction.Counterparty = Vendor->Faction;

	if (!Vendor->bSellsItems)
	{
		OutTransaction.RefusalReason = TEXT("This vendor does not sell.");
		Record(OutTransaction);
		return false;
	}

	UEclipseInventoryComponent* Inventory = Buyer->FindComponentByClass<UEclipseInventoryComponent>();
	if (Inventory == nullptr)
	{
		OutTransaction.RefusalReason = TEXT("Buyer has no inventory.");
		Record(OutTransaction);
		return false;
	}

	if (GetStockCount(Vendor, ItemId) < Count)
	{
		OutTransaction.RefusalReason = TEXT("Not enough stock.");
		Record(OutTransaction);
		return false;
	}

	const UEclipseItemDefinition* Definition = ResolveItem(ItemId);
	if (Definition == nullptr)
	{
		OutTransaction.RefusalReason = TEXT("Unknown item.");
		Record(OutTransaction);
		return false;
	}

	if (Inventory->GetAcceptableCount(ItemId) < Count)
	{
		OutTransaction.RefusalReason = TEXT("Not enough space.");
		Record(OutTransaction);
		return false;
	}

	const FEclipsePriceQuote Quote = QuotePrice(ItemId, Vendor->Faction);
	if (Quote.bRefused)
	{
		OutTransaction.RefusalReason = TEXT("This item is not for sale.");
		Record(OutTransaction);
		return false;
	}

	const int64 TotalCost = Quote.BuyPrice * Count;
	if (GetBuyerCredits(Buyer) < TotalCost)
	{
		OutTransaction.RefusalReason = TEXT("Not enough credits.");
		Record(OutTransaction);
		return false;
	}

	FString CreditRefusal;
	if (!ApplyCredits(Buyer, -TotalCost, CreditRefusal))
	{
		OutTransaction.RefusalReason = CreditRefusal;
		Record(OutTransaction);
		return false;
	}

	const int32 Accepted = Inventory->AddItem(ItemId, Count);
	if (Accepted < Count)
	{
		// The inventory shrank between the check and the add (another trade, a dropped
		// item). Refund the difference rather than charging for goods that were not given.
		const int64 Refund = Quote.BuyPrice * (Count - Accepted);
		FString RefundRefusal;
		ApplyCredits(Buyer, Refund, RefundRefusal);
	}

	OutTransaction.Count = Accepted;
	OutTransaction.CreditDelta = -(Quote.BuyPrice * Accepted);
	OutTransaction.bAccepted = Accepted > 0;

	if (TArray<FEclipseVendorStockEntry>* Stock = VendorStock.Find(Vendor->VendorId))
	{
		for (FEclipseVendorStockEntry& Entry : *Stock)
		{
			if (Entry.ItemId == ItemId)
			{
				Entry.Count = FMath::Max(0, Entry.Count - Accepted);
				break;
			}
		}
	}

	Record(OutTransaction);
	return OutTransaction.bAccepted;
}

bool UEclipseTradeSubsystem::SellItem(AActor* Buyer, UEclipseVendorDefinition* Vendor, FName ItemId, int32 Count, FEclipseTransaction& OutTransaction)
{
	OutTransaction = FEclipseTransaction();
	OutTransaction.Kind = EEclipseTransactionKind::Sell;
	OutTransaction.ItemId = ItemId;
	OutTransaction.Count = Count;

	if (Buyer == nullptr || Vendor == nullptr || Count <= 0)
	{
		OutTransaction.RefusalReason = TEXT("Invalid trade request.");
		Record(OutTransaction);
		return false;
	}

	OutTransaction.Counterparty = Vendor->Faction;

	if (!Vendor->bBuysItems)
	{
		OutTransaction.RefusalReason = TEXT("This vendor does not buy.");
		Record(OutTransaction);
		return false;
	}

	UEclipseInventoryComponent* Inventory = Buyer->FindComponentByClass<UEclipseInventoryComponent>();
	if (Inventory == nullptr || Inventory->CountItem(ItemId) < Count)
	{
		OutTransaction.RefusalReason = TEXT("Buyer does not have that many.");
		Record(OutTransaction);
		return false;
	}

	const FEclipsePriceQuote Quote = QuotePrice(ItemId, Vendor->Faction);
	if (Quote.bRefused)
	{
		OutTransaction.RefusalReason = TEXT("This item cannot be sold.");
		Record(OutTransaction);
		return false;
	}

	const int64 TotalPayment = Quote.SellPrice * Count;
	FString CreditRefusal;
	if (!ApplyCredits(Buyer, TotalPayment, CreditRefusal))
	{
		OutTransaction.RefusalReason = CreditRefusal;
		Record(OutTransaction);
		return false;
	}

	if (!Inventory->RemoveItem(ItemId, Count))
	{
		// Rolling the payment back keeps the buyer whole.
		FString RefundRefusal;
		ApplyCredits(Buyer, -TotalPayment, RefundRefusal);
		OutTransaction.RefusalReason = TEXT("Items could not be removed.");
		Record(OutTransaction);
		return false;
	}

	OutTransaction.CreditDelta = TotalPayment;
	OutTransaction.bAccepted = true;

	if (TArray<FEclipseVendorStockEntry>* Stock = VendorStock.Find(Vendor->VendorId))
	{
		for (FEclipseVendorStockEntry& Entry : *Stock)
		{
			if (Entry.ItemId == ItemId)
			{
				Entry.Count += Count;
				break;
			}
		}
	}

	Record(OutTransaction);
	return true;
}

bool UEclipseTradeSubsystem::RepairItem(AActor* Buyer, UEclipseVendorDefinition* Vendor, FName ItemId, FEclipseTransaction& OutTransaction)
{
	OutTransaction = FEclipseTransaction();
	OutTransaction.Kind = EEclipseTransactionKind::Repair;
	OutTransaction.ItemId = ItemId;
	OutTransaction.Count = 1;

	if (Buyer == nullptr || Vendor == nullptr)
	{
		OutTransaction.RefusalReason = TEXT("Invalid repair request.");
		Record(OutTransaction);
		return false;
	}

	OutTransaction.Counterparty = Vendor->Faction;

	UEclipseInventoryComponent* Inventory = Buyer->FindComponentByClass<UEclipseInventoryComponent>();
	const UEclipseItemDefinition* Definition = ResolveItem(ItemId);
	if (Inventory == nullptr || Definition == nullptr || Definition->MaxDurability <= 0.0f)
	{
		OutTransaction.RefusalReason = TEXT("Item cannot be repaired.");
		Record(OutTransaction);
		return false;
	}

	const FEclipseItemStack* Stack = Inventory->FindStack(ItemId);
	if (Stack == nullptr)
	{
		OutTransaction.RefusalReason = TEXT("Item is not carried.");
		Record(OutTransaction);
		return false;
	}

	const float Missing = Definition->MaxDurability - Stack->Durability;
	if (Missing <= 0.0f)
	{
		OutTransaction.RefusalReason = TEXT("Item is already at full durability.");
		Record(OutTransaction);
		return false;
	}

	const int64 Cost = FMath::Max<int64>(1, RoundHalfUp(Missing * RepairCostPerDurabilityPoint));
	if (GetBuyerCredits(Buyer) < Cost)
	{
		OutTransaction.RefusalReason = TEXT("Not enough credits.");
		Record(OutTransaction);
		return false;
	}

	FString CreditRefusal;
	if (!ApplyCredits(Buyer, -Cost, CreditRefusal))
	{
		OutTransaction.RefusalReason = CreditRefusal;
		Record(OutTransaction);
		return false;
	}

	// Durability is restored by repairing the inverse of the missing amount, which the
	// inventory APIs already model as damage.
	Inventory->DamageItemDurability(ItemId, -Missing);

	OutTransaction.CreditDelta = -Cost;
	OutTransaction.bAccepted = true;
	Record(OutTransaction);
	return true;
}
