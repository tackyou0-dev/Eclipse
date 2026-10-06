// PROJECT ECLIPSE - Progression component.
//
// Purpose
//   Experience, levels, skill points and the unlocks they buy. Purchasing a skill applies
//   its real effect immediately: a stat modifier goes onto the stat component under a
//   source id, an unlock goes into the unlocked sets that crafting and the weapon rack
//   read.
//
// Curve
//   Experience needed to reach the next level is XPBase + XPGrowth * (Level - 1), and the
//   reference model implements exactly that formula. Changing it here without changing
//   eclipse_reference_model.get_xp_required_for_level is a build break by contract.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Core/EclipseTypes.h"
#include "Progression/EclipseSkillDefinition.h"
#include "EclipseProgressionComponent.generated.h"

/** Fired when the character gains a level. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEclipseLevelChangedSignature, int32, NewLevel, int32, SkillPointsAvailable);

/** Fired when a skill is purchased. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEclipseSkillUnlockedSignature, FName, SkillId);

/**
 * Character progression.
 *
 * The component is the only writer of the skill state, which is what makes the save file
 * and the UI consistent: replaying the purchased skill list from a save reproduces the
 * exact same stat modifiers.
 */
UCLASS(ClassGroup = (Eclipse), meta = (BlueprintSpawnableComponent))
class ECLIPSE_API UEclipseProgressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEclipseProgressionComponent();

	/** UActorComponent: replicate progression to the owning client. */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------------------------
	// Experience and levels
	// ------------------------------------------------------------------

	/** Total experience earned. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	int32 GetExperience() const { return Experience; }

	/** Current character level, starting at 1. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	int32 GetLevel() const { return Level; }

	/** Experience required to advance from the current level. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	int32 GetExperienceForNextLevel() const;

	/** Progress through the current level, in [0, 1]. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	float GetLevelProgress() const;

	/** Experience remaining before the next level. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	int32 GetExperienceToNextLevel() const;

	/** Add experience, levelling up as many times as the total allows. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Progression")
	void AddExperience(int32 Amount);

	/** Set the total experience directly. Used when a save is applied. */
	void SetExperience(int32 NewExperience);

	// ------------------------------------------------------------------
	// Skill points
	// ------------------------------------------------------------------

	/** Unspent skill points. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	int32 GetSkillPoints() const { return SkillPoints; }

	/** Points already committed to skills. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	int32 GetSpentSkillPoints() const;

	/**
	 * Buy a skill. Validates cost, level and prerequisites, then applies the skill's effect.
	 * Returns false with a reason when it cannot be bought.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Progression")
	bool SpendSkillPoint(FName SkillId, FString& OutRejectReason);

	/** True when the skill is already purchased. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	bool KnowsSkill(FName SkillId) const;

	/** True when the skill could be bought right now. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Progression")
	bool CanLearnSkill(FName SkillId, FString& OutRejectReason) const;

	/** Every purchased skill id, in purchase order. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	const TArray<FName>& GetLearnedSkills() const { return LearnedSkills; }

	// ------------------------------------------------------------------
	// Unlocks
	// ------------------------------------------------------------------

	/** True when a recipe is available: either unlocked by a skill, or skill-free. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	bool KnowsRecipe(FName RecipeId) const;

	/** True when an ability or passive has been unlocked. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	bool KnowsAbility(FName AbilityId) const;

	/** True when a base module has been unlocked. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	bool KnowsBaseModule(EEclipseBaseModule Module) const;

	/** Re-apply every purchased skill's effect. Used after a load. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Progression")
	void ReapplyAllSkillEffects();

	/** Set the learned skills directly, then reapply them. Used when a save is applied. */
	void SetLearnedSkills(const TArray<FName>& Skills);

	/** Experience needed to advance from Level. Static so tools and tests can use it. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Progression")
	static int32 GetXpRequiredForLevel(int32 InLevel);

	/** Highest achievable level. */
	static constexpr int32 MaxLevel = 20;

	/** Experience needed for level 2. */
	static constexpr int32 XpBase = 400;

	/** Extra experience each level adds to the requirement. */
	static constexpr int32 XpGrowthPerLevel = 250;

	/** Skill points granted per level. */
	static constexpr int32 SkillPointsPerLevel = 1;

	/** Modifier source prefix for skill-granted stat modifiers. */
	static FName MakeSkillModifierSourceId(FName SkillId);

	/** Fired on level up. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|Progression")
	FEclipseLevelChangedSignature OnLevelChanged;

	/** Fired when a skill is purchased. */
	UPROPERTY(BlueprintAssignable, Category = "Eclipse|Progression")
	FEclipseSkillUnlockedSignature OnSkillUnlocked;

protected:
	/** Apply a purchased skill's effect to the character. */
	void ApplySkillEffect(const UEclipseSkillDefinition& Skill);

	/** Remove a skill's effect. Only used when a save is rolled back. */
	void RemoveSkillEffect(FName SkillId);

	/** Total experience consumed by the levels already passed. */
	int32 ExperienceAtLevelStart() const;

	/** Total experience earned. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Progression")
	int32 Experience = 0;

	/** Current level. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Progression")
	int32 Level = 1;

	/** Unspent skill points. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Progression")
	int32 SkillPoints = 0;

	/** Purchased skills, in purchase order. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Progression")
	TArray<FName> LearnedSkills;

	/** Recipes unlocked by purchased skills. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Progression")
	TArray<FName> UnlockedRecipes;

	/** Abilities and passives unlocked by purchased skills. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Progression")
	TArray<FName> UnlockedAbilities;

	/** Base modules unlocked by purchased skills. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Eclipse|Progression")
	TArray<EEclipseBaseModule> UnlockedModules;
};
