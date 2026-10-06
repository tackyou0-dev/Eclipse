// PROJECT ECLIPSE - Progression component implementation.
//
// Purpose
//   The experience curve, skill purchase validation and effect application.

#include "Progression/EclipseProgressionComponent.h"

#include "Core/EclipseLog.h"
#include "Core/EclipseStatComponent.h"
#include "Crafting/EclipseRecipeDefinition.h"
#include "Net/UnrealNetwork.h"

UEclipseProgressionComponent::UEclipseProgressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEclipseProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEclipseProgressionComponent, Experience);
	DOREPLIFETIME(UEclipseProgressionComponent, Level);
	DOREPLIFETIME(UEclipseProgressionComponent, SkillPoints);
	DOREPLIFETIME(UEclipseProgressionComponent, LearnedSkills);
	DOREPLIFETIME(UEclipseProgressionComponent, UnlockedRecipes);
	DOREPLIFETIME(UEclipseProgressionComponent, UnlockedAbilities);
	DOREPLIFETIME(UEclipseProgressionComponent, UnlockedModules);
}

int32 UEclipseProgressionComponent::GetXpRequiredForLevel(int32 InLevel)
{
	if (InLevel < 1 || InLevel >= MaxLevel)
	{
		// Below level 1 there is nothing to earn; at the cap the requirement is irrelevant
		// and returning the last requirement keeps the UI maths finite.
		return XpBase + XpGrowthPerLevel * FMath::Max(0, MaxLevel - 2);
	}

	return XpBase + XpGrowthPerLevel * (InLevel - 1);
}

int32 UEclipseProgressionComponent::GetExperienceForNextLevel() const
{
	return GetXpRequiredForLevel(Level);
}

int32 UEclipseProgressionComponent::GetExperienceToNextLevel() const
{
	if (Level >= MaxLevel)
	{
		return 0;
	}

	// The requirement is measured against the experience earned since the current level
	// started, so the HUD bar and the level-up check agree.
	const int32 SpentOnLevels = Experience - ExperienceAtLevelStart();
	return FMath::Max(0, GetExperienceForNextLevel() - SpentOnLevels);
}

float UEclipseProgressionComponent::GetLevelProgress() const
{
	if (Level >= MaxLevel)
	{
		return 1.0f;
	}

	const int32 Required = GetExperienceForNextLevel();
	if (Required <= 0)
	{
		return 1.0f;
	}

	const int32 EarnedThisLevel = Experience - ExperienceAtLevelStart();
	return FMath::Clamp(static_cast<float>(EarnedThisLevel) / static_cast<float>(Required), 0.0f, 1.0f);
}

void UEclipseProgressionComponent::AddExperience(int32 Amount)
{
	if (Amount <= 0 || !HasAuthority())
	{
		return;
	}

	Experience += Amount;

	int32 LevelsGained = 0;
	while (Level < MaxLevel)
	{
		const int32 Required = GetXpRequiredForLevel(Level);
		if (Experience - ExperienceAtLevelStart() < Required)
		{
			break;
		}

		++Level;
		++LevelsGained;
		SkillPoints += SkillPointsPerLevel;
	}

	if (LevelsGained > 0)
	{
		UE_LOG(LogEclipseMissions, Log, TEXT("Level up: now level %d with %d skill point(s)."), Level, SkillPoints);
		OnLevelChanged.Broadcast(Level, SkillPoints);
	}
}

void UEclipseProgressionComponent::SetExperience(int32 NewExperience)
{
	if (!HasAuthority())
	{
		return;
	}

	Experience = FMath::Max(0, NewExperience);

	// Recompute the level from scratch: a save can be older than the current curve, and
	// the level must match the curve, not the level that was stored with the old one.
	Level = 1;
	SkillPoints = 0;

	int32 Remaining = Experience;
	while (Level < MaxLevel)
	{
		const int32 Required = GetXpRequiredForLevel(Level);
		if (Remaining < Required)
		{
			break;
		}

		Remaining -= Required;
		++Level;
		SkillPoints += SkillPointsPerLevel;
	}

	// Points already spent are not re-granted: the learned skill list is authoritative.
	SkillPoints = FMath::Max(0, SkillPoints - GetSpentSkillPoints());

	OnLevelChanged.Broadcast(Level, SkillPoints);
}

int32 UEclipseProgressionComponent::GetSpentSkillPoints() const
{
	const UEclipseSkillCatalog* Catalog = UEclipseSkillCatalog::GetCatalog();
	if (Catalog == nullptr)
	{
		return LearnedSkills.Num();
	}

	int32 Spent = 0;
	for (const FName SkillId : LearnedSkills)
	{
		if (const UEclipseSkillDefinition* Skill = Catalog->Find(SkillId))
		{
			Spent += Skill->SkillPointCost;
		}
		else
		{
			// A skill that no longer exists still cost its point; counting one keeps the
			// budget balanced after a content change.
			Spent += 1;
		}
	}

	return Spent;
}

bool UEclipseProgressionComponent::KnowsSkill(FName SkillId) const
{
	return LearnedSkills.Contains(SkillId);
}

bool UEclipseProgressionComponent::CanLearnSkill(FName SkillId, FString& OutRejectReason) const
{
	const UEclipseSkillCatalog* Catalog = UEclipseSkillCatalog::GetCatalog();
	if (Catalog == nullptr)
	{
		OutRejectReason = TEXT("Skill catalogue is unavailable.");
		return false;
	}

	const UEclipseSkillDefinition* Skill = Catalog->Find(SkillId);
	if (Skill == nullptr || !Skill->IsValidSkill())
	{
		OutRejectReason = FString::Printf(TEXT("Unknown skill '%s'."), *SkillId.ToString());
		return false;
	}

	if (KnowsSkill(SkillId))
	{
		OutRejectReason = TEXT("Skill is already learned.");
		return false;
	}

	if (Level < Skill->RequiredLevel)
	{
		OutRejectReason = FString::Printf(TEXT("Requires level %d."), Skill->RequiredLevel);
		return false;
	}

	if (SkillPoints < Skill->SkillPointCost)
	{
		OutRejectReason = TEXT("Not enough skill points.");
		return false;
	}

	for (const FName Prerequisite : Skill->Prerequisites)
	{
		if (!KnowsSkill(Prerequisite))
		{
			OutRejectReason = FString::Printf(TEXT("Requires %s first."), *Prerequisite.ToString());
			return false;
		}
	}

	OutRejectReason.Reset();
	return true;
}

bool UEclipseProgressionComponent::SpendSkillPoint(FName SkillId, FString& OutRejectReason)
{
	if (!HasAuthority())
	{
		OutRejectReason = TEXT("Progression is server-authoritative.");
		return false;
	}

	if (!CanLearnSkill(SkillId, OutRejectReason))
	{
		return false;
	}

	const UEclipseSkillCatalog* Catalog = UEclipseSkillCatalog::GetCatalog();
	const UEclipseSkillDefinition* Skill = Catalog->Find(SkillId);
	if (Skill == nullptr)
	{
		return false;
	}

	SkillPoints -= Skill->SkillPointCost;
	LearnedSkills.Add(SkillId);

	ApplySkillEffect(*Skill);

	UE_LOG(LogEclipseMissions, Log, TEXT("Learned %s for %d point(s)."), *SkillId.ToString(), Skill->SkillPointCost);
	OnSkillUnlocked.Broadcast(SkillId);
	return true;
}

void UEclipseProgressionComponent::ApplySkillEffect(const UEclipseSkillDefinition& Skill)
{
	switch (Skill.Effect)
	{
	case EEclipseSkillEffectType::StatModifier:
	case EEclipseSkillEffectType::PassiveAbility:
	{
		// A passive ability can carry a stat modifier as well, which is why both cases
		// share this branch.
		FEclipseStatModifier Modifier;
		if (Skill.BuildStatModifier(Modifier))
		{
			if (UEclipseStatComponent* StatComponent = GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UEclipseStatComponent>() : nullptr)
			{
				TArray<FEclipseStatModifier> Modifiers;
				Modifiers.Add(Modifier);
				StatComponent->AddModifierSource(MakeSkillModifierSourceId(Skill.SkillId), Modifiers);
			}
		}

		if (Skill.Effect == EEclipseSkillEffectType::PassiveAbility && !Skill.UnlockedId.IsNone())
		{
			UnlockedAbilities.AddUnique(Skill.UnlockedId);
		}
		break;
	}

	case EEclipseSkillEffectType::UnlockRecipe:
		UnlockedRecipes.AddUnique(Skill.UnlockedId);
		break;

	case EEclipseSkillEffectType::UnlockAbility:
		UnlockedAbilities.AddUnique(Skill.UnlockedId);
		break;

	case EEclipseSkillEffectType::UnlockBaseModule:
		if (StaticEnum<EEclipseBaseModule>() != nullptr)
		{
			const int64 ModuleValue = StaticEnum<EEclipseBaseModule>()->GetValueByNameString(Skill.UnlockedId.ToString());
			if (ModuleValue != INDEX_NONE)
			{
				UnlockedModules.AddUnique(static_cast<EEclipseBaseModule>(ModuleValue));
			}
			else
			{
				UE_LOG(LogEclipseMissions, Warning, TEXT("Skill %s unlocks unknown base module '%s'."),
					*Skill.SkillId.ToString(), *Skill.UnlockedId.ToString());
			}
		}
		break;

	default:
		break;
	}
}

void UEclipseProgressionComponent::RemoveSkillEffect(FName SkillId)
{
	if (UEclipseStatComponent* StatComponent = GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UEclipseStatComponent>() : nullptr)
	{
		StatComponent->RemoveModifierSource(MakeSkillModifierSourceId(SkillId));
	}
}

void UEclipseProgressionComponent::ReapplyAllSkillEffects()
{
	if (UEclipseStatComponent* StatComponent = GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UEclipseStatComponent>() : nullptr)
	{
		// Clear every skill source first so that the reapply cannot double up after a load.
		for (const FName SkillId : LearnedSkills)
		{
			StatComponent->RemoveModifierSource(MakeSkillModifierSourceId(SkillId));
		}
	}

	UnlockedRecipes.Reset();
	UnlockedAbilities.Reset();
	UnlockedModules.Reset();

	const UEclipseSkillCatalog* Catalog = UEclipseSkillCatalog::GetCatalog();
	if (Catalog == nullptr)
	{
		return;
	}

	for (const FName SkillId : LearnedSkills)
	{
		if (const UEclipseSkillDefinition* Skill = Catalog->Find(SkillId))
		{
			ApplySkillEffect(*Skill);
		}
	}
}

void UEclipseProgressionComponent::SetLearnedSkills(const TArray<FName>& Skills)
{
	if (!HasAuthority())
	{
		return;
	}

	LearnedSkills = Skills;
	ReapplyAllSkillEffects();
}

bool UEclipseProgressionComponent::KnowsRecipe(FName RecipeId) const
{
	if (RecipeId.IsNone())
	{
		return false;
	}

	if (UnlockedRecipes.Contains(RecipeId))
	{
		return true;
	}

	// Recipes with no skill requirement are known to everyone, which is what lets a fresh
	// colonist craft a medkit without spending a point.
	if (const UEclipseRecipeCatalog* Catalog = UEclipseRecipeCatalog::GetCatalog())
	{
		if (const UEclipseRecipeDefinition* Recipe = Catalog->Find(RecipeId))
		{
			return Recipe->RequiredSkillId.IsNone();
		}
	}

	return false;
}

bool UEclipseProgressionComponent::KnowsAbility(FName AbilityId) const
{
	return UnlockedAbilities.Contains(AbilityId);
}

bool UEclipseProgressionComponent::KnowsBaseModule(EEclipseBaseModule Module) const
{
	// Workshop and Armory are baseline: the slice gives every colonist a bench to work at.
	if (Module == EEclipseBaseModule::None || Module == EEclipseBaseModule::Workshop || Module == EEclipseBaseModule::Armory)
	{
		return true;
	}

	return UnlockedModules.Contains(Module);
}

int32 UEclipseProgressionComponent::ExperienceAtLevelStart() const
{
	int32 Total = 0;
	for (int32 Index = 1; Index < Level; ++Index)
	{
		Total += GetXpRequiredForLevel(Index);
	}
	return Total;
}

FName UEclipseProgressionComponent::MakeSkillModifierSourceId(FName SkillId)
{
	return FName(*FString::Printf(TEXT("Skill.%s"), *SkillId.ToString()));
}
