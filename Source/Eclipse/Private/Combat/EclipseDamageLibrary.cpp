// PROJECT ECLIPSE - Damage pipeline implementation.
//
// Purpose
//   The arithmetic described in EclipseDamageLibrary.h. Keep this file and
//   Tests/Reference/eclipse_reference_model.py in step; the reference tests exist
//   precisely to catch a divergence between the two.

#include "Combat/EclipseDamageLibrary.h"

#include "Combat/EclipseDamageable.h"
#include "Components/ActorComponent.h"
#include "Core/EclipseLog.h"
#include "Core/EclipseTeamAgent.h"
#include "GameFramework/Actor.h"

float UEclipseDamageLibrary::ComputeArmorMitigation(float Armor, float ArmorPenetration)
{
	const float EffectiveArmor = FMath::Max(0.0f, Armor * (1.0f - FMath::Clamp(ArmorPenetration, 0.0f, 1.0f)));
	if (EffectiveArmor <= 0.0f)
	{
		return 0.0f;
	}

	// Diminishing returns: mitigation = A / (A + K). Doubling armour never doubles the
	// protection, which keeps late-slice enemies from becoming unkillable.
	const float Mitigation = EffectiveArmor / (EffectiveArmor + ArmorMitigationConstant);
	return FMath::Clamp(Mitigation, 0.0f, MaxArmorMitigation);
}

float UEclipseDamageLibrary::ComputeResistanceMultiplier(const FEclipseResistanceProfile& Profile, EEclipseDamageType DamageType)
{
	return Profile.Get(DamageType);
}

float UEclipseDamageLibrary::ComputeFinalDamage(const FEclipseDamageContext& Context, float Armor, const FEclipseResistanceProfile& Profile, float& OutArmorMitigation)
{
	if (!Context.IsValid())
	{
		OutArmorMitigation = 0.0f;
		return 0.0f;
	}

	OutArmorMitigation = Context.bIgnoreArmor ? 0.0f : ComputeArmorMitigation(Armor, Context.ArmorPenetration);

	const float Falloff = FMath::Clamp(Context.DistanceFalloff, 0.0f, 1.0f);
	const float WeakPoint = FMath::Max(0.0f, Context.WeakPointMultiplier);
	const float Critical = FMath::Max(0.0f, Context.CriticalMultiplier);
	const float Resistance = ComputeResistanceMultiplier(Profile, Context.DamageType);

	return Context.BaseDamage * Falloff * WeakPoint * Critical * (1.0f - OutArmorMitigation) * Resistance;
}

float UEclipseDamageLibrary::ComputeDistanceFalloff(float Distance, float EffectiveRange, float MaxRange)
{
	if (Distance <= EffectiveRange || EffectiveRange <= 0.0f)
	{
		return 1.0f;
	}

	if (Distance >= MaxRange || MaxRange <= EffectiveRange)
	{
		return MinimumRangeFalloff;
	}

	const float Alpha = (Distance - EffectiveRange) / (MaxRange - EffectiveRange);
	return FMath::Lerp(1.0f, MinimumRangeFalloff, Alpha);
}

TScriptInterface<IEclipseDamageable> UEclipseDamageLibrary::FindDamageable(AActor* Target)
{
	if (Target == nullptr)
	{
		return TScriptInterface<IEclipseDamageable>();
	}

	if (Target->Implements<UEclipseDamageable>())
	{
		return TScriptInterface<IEclipseDamageable>(Target);
	}

	// Actors commonly put the health component on a child component (for example a
	// vehicle), so fall back to the component list before giving up.
	TInlineComponentArray<UActorComponent*> Components(Target);
	for (UActorComponent* Component : Components)
	{
		if (Component != nullptr && Component->Implements<UEclipseDamageable>())
		{
			return TScriptInterface<IEclipseDamageable>(Component);
		}
	}

	return TScriptInterface<IEclipseDamageable>();
}

FEclipseDamageResult UEclipseDamageLibrary::ApplyDamage(AActor* Instigator, AActor* Target, const FEclipseDamageContext& Context)
{
	FEclipseDamageResult Result;

	if (!Context.IsValid())
	{
		return Result;
	}

	TScriptInterface<IEclipseDamageable> Damageable = FindDamageable(Target);
	if (!Damageable.GetInterface() || !Damageable->IsDamageable())
	{
		UE_LOG(LogEclipseCombat, Verbose, TEXT("Damage to %s ignored: no damageable target."),
			Target != nullptr ? *Target->GetName() : TEXT("<null>"));
		return Result;
	}

	// Damage always uses the instigator carried by the context; the parameter exists so
	// that Blueprint callers have a self-documenting pin, and is only a fallback.
	FEclipseDamageContext EffectiveContext = Context;
	if (EffectiveContext.Instigator == nullptr)
	{
		EffectiveContext.Instigator = Instigator;
	}

	return Damageable->ApplyEclipseDamage(EffectiveContext);
}

TArray<FEclipseDamageResult> UEclipseDamageLibrary::ApplyRadialDamage(AActor* Instigator, const FVector& Origin, float Radius, const FEclipseDamageContext& Context, const TArray<AActor*>& IgnoreActors)
{
	TArray<FEclipseDamageResult> Results;
	if (Radius <= 0.0f)
	{
		return Results;
	}

	if (UWorld* World = Instigator != nullptr ? Instigator->GetWorld() : nullptr)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(EclipseRadialDamage), false, Instigator);
		Params.AddIgnoredActors(IgnoreActors);

		World->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(Radius), Params);

		TSet<AActor*> Processed;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* OverlappedActor = Overlap.GetActor();
			if (OverlappedActor == nullptr || Processed.Contains(OverlappedActor) || IgnoreActors.Contains(OverlappedActor))
			{
				continue;
			}
			Processed.Add(OverlappedActor);

			const float Distance = FVector::Dist(Origin, OverlappedActor->GetActorLocation());
			FEclipseDamageContext Scaled = Context;
			Scaled.DistanceFalloff = FMath::Clamp(1.0f - (Distance / Radius), MinimumRangeFalloff, 1.0f);
			Scaled.HitLocation = OverlappedActor->GetActorLocation();
			Scaled.ShotDirection = (OverlappedActor->GetActorLocation() - Origin).GetSafeNormal();

			Results.Add(ApplyDamage(Instigator, OverlappedActor, Scaled));
		}
	}

	return Results;
}
