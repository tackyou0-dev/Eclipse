// PROJECT ECLIPSE - Mission definition implementation.
//
// Purpose
//   Catalogue lookup for missions.

#include "Missions/EclipseMissionDefinition.h"

#include "Core/EclipseLog.h"

UEclipseMissionDefinition* UEclipseMissionCatalog::Find(FName MissionId) const
{
	if (MissionId.IsNone())
	{
		return nullptr;
	}

	for (const TObjectPtr<UEclipseMissionDefinition>& Mission : Missions)
	{
		if (Mission != nullptr && Mission->MissionId == MissionId)
		{
			return Mission;
		}
	}

	return nullptr;
}

UEclipseMissionDefinition* UEclipseMissionCatalog::GetFirstMission() const
{
	for (const TObjectPtr<UEclipseMissionDefinition>& Mission : Missions)
	{
		if (Mission == nullptr)
		{
			continue;
		}

		const bool bIsFirst = Mission->MissionType == EEclipseMissionType::MainMission
			&& Mission->MinimumLevel <= 1
			&& Mission->MinimumReputation <= 0.0f;

		if (bIsFirst)
		{
			return Mission;
		}
	}

	return Missions.Num() > 0 ? Missions[0] : nullptr;
}

UEclipseMissionCatalog* UEclipseMissionCatalog::GetCatalog()
{
	static TWeakObjectPtr<UEclipseMissionCatalog> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	const FSoftObjectPath CatalogPath(TEXT("/Game/Eclipse/Data/Missions/DA_MissionCatalog.DA_MissionCatalog"));
	UEclipseMissionCatalog* Catalog = Cast<UEclipseMissionCatalog>(CatalogPath.TryLoad());
	if (Catalog == nullptr)
	{
		UE_LOG(LogEclipseMissions, Warning, TEXT("Mission catalogue could not be loaded from '%s'."), *CatalogPath.ToString());
		return nullptr;
	}

	Cached = Catalog;
	return Catalog;
}
