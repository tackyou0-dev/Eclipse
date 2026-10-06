// PROJECT ECLIPSE - Item definition and catalogue implementation.
//
// Purpose
//   Catalogue lookup. The catalogue itself is loaded from a soft reference so the module
//   does not have to know about /Game paths at compile time.

#include "Inventory/EclipseItemDefinition.h"

#include "Core/EclipseLog.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	/** Where the shipped catalogue lives. Set from the content build step. */
	const TCHAR* GItemCatalogPath = TEXT("/Game/Eclipse/Data/Items/DA_ItemCatalog.DA_ItemCatalog");

	/** Cached catalogue, so repeated lookups during a raid do not hit the asset manager. */
	TWeakObjectPtr<UEclipseItemCatalog> GCachedCatalog;
}

UEclipseItemDefinition* UEclipseItemCatalog::Find(FName ItemId) const
{
	if (ItemId.IsNone())
	{
		return nullptr;
	}

	for (const TObjectPtr<UEclipseItemDefinition>& Item : Items)
	{
		if (Item != nullptr && Item->ItemId == ItemId)
		{
			return Item;
		}
	}

	UE_LOG(LogEclipseItems, Warning, TEXT("Item id '%s' is not in the catalogue."), *ItemId.ToString());
	return nullptr;
}

bool UEclipseItemCatalog::Contains(FName ItemId) const
{
	for (const TObjectPtr<UEclipseItemDefinition>& Item : Items)
	{
		if (Item != nullptr && Item->ItemId == ItemId)
		{
			return true;
		}
	}
	return false;
}

TArray<FName> UEclipseItemCatalog::GetAllItemIds() const
{
	TArray<FName> Ids;
	Ids.Reserve(Items.Num());
	for (const TObjectPtr<UEclipseItemDefinition>& Item : Items)
	{
		if (Item != nullptr)
		{
			Ids.Add(Item->ItemId);
		}
	}
	return Ids;
}

UEclipseItemCatalog* UEclipseItemCatalog::GetCatalog()
{
	if (GCachedCatalog.IsValid())
	{
		return GCachedCatalog.Get();
	}

	const FSoftObjectPath CatalogPath(GItemCatalogPath);
	if (!CatalogPath.IsValid())
	{
		UE_LOG(LogEclipseItems, Error, TEXT("The item catalogue path is not valid."));
		return nullptr;
	}

	UObject* Loaded = CatalogPath.TryLoad();
	UEclipseItemCatalog* Catalog = Cast<UEclipseItemCatalog>(Loaded);
	if (Catalog == nullptr)
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("Item catalogue '%s' could not be loaded; item lookups will fail."), GItemCatalogPath);
		return nullptr;
	}

	GCachedCatalog = Catalog;
	return Catalog;
}
