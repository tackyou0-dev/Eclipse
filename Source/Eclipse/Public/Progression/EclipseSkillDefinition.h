// PROJECT ECLIPSE - Skill definition and catalogue.
//
// Purpose
//   Skills are data and every one of them does something: a stat modifier, a recipe, an
//   ability or a base module. There is no cosmetic skill in the tree.
//
// Prerequisites
//   Prerequisites are skill ids, enforced by UEclipseProgressionComponent and checked for
//   cycles by Tools/ci/validate_content.py. Tier is a UI grouping, not a gate: the gate is
//   the prerequisite chain plus the character level.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Engine/DataAsset.h"
#include "EclipseSkillDefinition.generated.h"

/** One node in a skill tree. */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseSkillDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Stable id, for example "Skill.RecoilControl". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill")
	FName SkillId;

	/** Display name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill")
	FText DisplayName;

	/** What the skill does, in the player's words. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill", meta = (MultiLine = "true"))
	FText Description;

	/** Tree this skill belongs to. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill")
	EEclipseSkillTree Tree = EEclipseSkillTree::Combat;

	/** Column inside the tree, used by the UI layout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill", meta = (ClampMin = "0"))
	int32 Tier = 0;

	/** Skill points this costs. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill", meta = (ClampMin = "1"))
	int32 SkillPointCost = 1;

	/** Character level required before the skill can be bought. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;

	/** Skills that must be learned first. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill")
	TArray<FName> Prerequisites;

	/** What purchasing the skill does. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill")
	EEclipseSkillEffectType Effect = EEclipseSkillEffectType::StatModifier;

	/** Stat changed when Effect is StatModifier or PassiveAbility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill")
	EEclipseStat TargetStat = EEclipseStat::None;

	/** Flat amount added before multipliers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill")
	float AdditiveValue = 0.0f;

	/** Multiplier applied after the additive term. 1.0 means no change. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill")
	float MultiplicativeValue = 1.0f;

	/** Recipe id, ability id or module name unlocked by the skill. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Skill")
	FName UnlockedId;

	/** True when the skill is complete enough to be offered. */
	bool IsValidSkill() const
	{
		if (SkillId.IsNone() || SkillPointCost < 1 || RequiredLevel < 1)
		{
			return false;
		}

		if (Effect == EEclipseSkillEffectType::StatModifier && TargetStat == EEclipseStat::None)
		{
			return false;
		}

		if (Effect == EEclipseSkillEffectType::PassiveAbility && UnlockedId.IsNone())
		{
			// A passive still has to name the ability it grants, even when it also modifies
			// a stat.
			return false;
		}

		if (Effect != EEclipseSkillEffectType::StatModifier && Effect != EEclipseSkillEffectType::PassiveAbility && UnlockedId.IsNone())
		{
			return false;
		}

		return true;
	}

	/** The stat modifier this skill grants, if any. */
	bool BuildStatModifier(FEclipseStatModifier& OutModifier) const
	{
		if (TargetStat == EEclipseStat::None)
		{
			return false;
		}

		OutModifier = FEclipseStatModifier(TargetStat, AdditiveValue, MultiplicativeValue);
		return true;
	}
};

/** Every skill in the game. */
UCLASS(BlueprintType)
class ECLIPSE_API UEclipseSkillCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** All skills. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Catalog")
	TArray<TObjectPtr<UEclipseSkillDefinition>> Skills;

	/** Find a skill by id, or null. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Catalog")
	UEclipseSkillDefinition* Find(FName SkillId) const;

	/** Every skill in a tree, in tier order. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Catalog")
	TArray<UEclipseSkillDefinition*> GetTree(EEclipseSkillTree Tree) const;

	/** Static accessor for the shipped catalogue. */
	static UEclipseSkillCatalog* GetCatalog();
};
