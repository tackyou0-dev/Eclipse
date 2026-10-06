// PROJECT ECLIPSE - Faction definition implementation.
//
// Purpose
//   Band resolution and pricing lookup.

#include "Factions/EclipseFactionDefinition.h"

#include "Core/EclipseLog.h"

float UEclipseFactionDefinition::ClampReputation(float Reputation)
{
	return FMath::Clamp(Reputation, MinimumReputation, MaximumReputation);
}

EEclipseReputationBand UEclipseFactionDefinition::GetBand(float Reputation) const
{
	const float Clamped = ClampReputation(Reputation);

	if (Clamped <= HostileThreshold)
	{
		return EEclipseReputationBand::Hostile;
	}

	if (Clamped >= AlliedThreshold)
	{
		return EEclipseReputationBand::Allied;
	}

	if (Clamped >= FriendlyThreshold)
	{
		return EEclipseReputationBand::Friendly;
	}

	return EEclipseReputationBand::Neutral;
}

FEclipseVendorPricing UEclipseFactionDefinition::GetPricing(EEclipseItemCategory Category) const
{
	for (const FEclipseVendorPricing& Pricing : VendorPricing)
	{
		if (Pricing.Category == Category)
		{
			return Pricing;
		}
	}

	// A faction with no row for a category still trades, at a deliberately poor rate: the
	// player can always dump loot, but the good rates come from the right vendor.
	FEclipseVendorPricing Default;
	Default.Category = Category;
	Default.BuyPriceMultiplier = 1.25f;
	Default.SellPriceMultiplier = 0.35f;
	return Default;
}

bool UEclipseFactionDefinition::DeclaresEnemy(EEclipseFaction Other) const
{
	return Enemies.Contains(Other);
}

bool UEclipseFactionDefinition::DeclaresAlly(EEclipseFaction Other) const
{
	return Allies.Contains(Other);
}

UEclipseFactionDefinition* UEclipseFactionCatalog::Find(EEclipseFaction Faction) const
{
	for (const TObjectPtr<UEclipseFactionDefinition>& Definition : Factions)
	{
		if (Definition != nullptr && Definition->Faction == Faction)
		{
			return Definition;
		}
	}
	return nullptr;
}

UEclipseFactionCatalog* UEclipseFactionCatalog::GetCatalog()
{
	static TWeakObjectPtr<UEclipseFactionCatalog> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	const FSoftObjectPath CatalogPath(TEXT("/Game/Eclipse/Data/Factions/DA_FactionCatalog.DA_FactionCatalog"));
	UEclipseFactionCatalog* Catalog = Cast<UEclipseFactionCatalog>(CatalogPath.TryLoad());
	if (Catalog == nullptr)
	{
		UE_LOG(LogEclipseTools, Warning, TEXT("Faction catalogue could not be loaded from '%s'."), *CatalogPath.ToString());
		return nullptr;
	}

	Cached = Catalog;
	return Catalog;
}
