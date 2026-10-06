// PROJECT ECLIPSE - Vendor definition implementation.
//
// Purpose
//   Band pricing and stock lookup. The band multipliers are constants rather than data
//   because they are a rule of the economy, not a per-vendor flavour choice; a vendor that
//   wants different rates gets them from its faction's category multipliers.

#include "Economy/EclipseVendorDefinition.h"

float UEclipseVendorDefinition::GetBandBuyMultiplier(EEclipseReputationBand Band)
{
	switch (Band)
	{
	case EEclipseReputationBand::Hostile:	return 1.25f;
	case EEclipseReputationBand::Neutral:	return 1.0f;
	case EEclipseReputationBand::Friendly:	return 0.92f;
	case EEclipseReputationBand::Allied:	return 0.85f;
	default:								break;
	}
	return 1.0f;
}

float UEclipseVendorDefinition::GetBandSellMultiplier(EEclipseReputationBand Band)
{
	switch (Band)
	{
	case EEclipseReputationBand::Hostile:	return 0.75f;
	case EEclipseReputationBand::Neutral:	return 1.0f;
	case EEclipseReputationBand::Friendly:	return 1.08f;
	case EEclipseReputationBand::Allied:	return 1.15f;
	default:								break;
	}
	return 1.0f;
}

bool UEclipseVendorDefinition::WillTradeAt(EEclipseReputationBand Band) const
{
	return static_cast<uint8>(Band) >= static_cast<uint8>(MinimumBand);
}

const FEclipseVendorStockEntry* UEclipseVendorDefinition::FindStockEntry(FName ItemId) const
{
	return Stock.FindByPredicate([ItemId](const FEclipseVendorStockEntry& Entry)
	{
		return Entry.ItemId == ItemId;
	});
}
