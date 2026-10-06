// PROJECT ECLIPSE - Skill definition implementation.
//
// Purpose
//   Catalogue lookup and tree queries.

#include "Progression/EclipseSkillDefinition.h"

#include "Core/EclipseLog.h"

UEclipseSkillDefinition* UEclipseSkillCatalog::Find(FName SkillId) const
{
	if (SkillId.IsNone())
	{
		return nullptr;
	}

	for (const TObjectPtr<UEclipseSkillDefinition>& Skill : Skills)
	{
		if (Skill != nullptr && Skill->SkillId == SkillId)
		{
			return Skill;
		}
	}

	return nullptr;
}

TArray<UEclipseSkillDefinition*> UEclipseSkillCatalog::GetTree(EEclipseSkillTree Tree) const
{
	TArray<UEclipseSkillDefinition*> Result;
	for (const TObjectPtr<UEclipseSkillDefinition>& Skill : Skills)
	{
		if (Skill != nullptr && Skill->Tree == Tree)
		{
			Result.Add(Skill);
		}
	}

	// Tier then id: a stable order means the UI never reshuffles between openings.
	Result.Sort([](const UEclipseSkillDefinition& Left, const UEclipseSkillDefinition& Right)
	{
		if (Left.Tier != Right.Tier)
		{
			return Left.Tier < Right.Tier;
		}
		return Left.SkillId.LexicalLess(Right.SkillId);
	});

	return Result;
}

UEclipseSkillCatalog* UEclipseSkillCatalog::GetCatalog()
{
	static TWeakObjectPtr<UEclipseSkillCatalog> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	const FSoftObjectPath CatalogPath(TEXT("/Game/Eclipse/Data/Skills/DA_SkillCatalog.DA_SkillCatalog"));
	UEclipseSkillCatalog* Catalog = Cast<UEclipseSkillCatalog>(CatalogPath.TryLoad());
	if (Catalog == nullptr)
	{
		UE_LOG(LogEclipseMissions, Warning, TEXT("Skill catalogue could not be loaded from '%s'."), *CatalogPath.ToString());
		return nullptr;
	}

	Cached = Catalog;
	return Catalog;
}
