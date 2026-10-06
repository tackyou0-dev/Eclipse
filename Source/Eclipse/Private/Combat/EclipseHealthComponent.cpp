// PROJECT ECLIPSE - Health component implementation.
//
// Purpose
//   Applies the damage pipeline to the pools. The arithmetic itself lives in
//   UEclipseDamageLibrary::ComputeFinalDamage so that the reference model can test it;
//   this file only decides what happens to the number once it exists.

#include "Combat/EclipseHealthComponent.h"

#include "Combat/EclipseDamageLibrary.h"
#include "Core/EclipseLog.h"
#include "Core/EclipseStatComponent.h"
#include "GameFramework/Actor.h"

UEclipseHealthComponent::UEclipseHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UEclipseHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = ResolveMaxHealth();
	CurrentShield = FMath::Min(CurrentShield, ConfiguredMaxShield);
	if (CurrentShield <= 0.0f)
	{
		CurrentShield = ConfiguredMaxShield;
	}

	RefreshTickState();
}

void UEclipseHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceDamage += DeltaTime;

	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	if (ShieldRegenPerSecond <= 0.0f || TimeSinceDamage < ShieldRegenDelay)
	{
		return;
	}

	if (CurrentShield >= ConfiguredMaxShield)
	{
		return;
	}

	RestoreShield(ShieldRegenPerSecond * DeltaTime);
}

void UEclipseHealthComponent::RefreshTickState()
{
	const bool bShouldTick = ShieldRegenPerSecond > 0.0f && ConfiguredMaxShield > 0.0f;
	SetComponentTickEnabled(bShouldTick);
}

float UEclipseHealthComponent::ResolveMaxHealth() const
{
	const AActor* Owner = GetOwner();
	if (Owner != nullptr)
	{
		if (const UEclipseStatComponent* StatComponent = Owner->FindComponentByClass<UEclipseStatComponent>())
		{
			const float StatHealth = StatComponent->GetStat(EEclipseStat::MaxHealth);
			if (StatHealth > 0.0f)
			{
				return StatHealth;
			}
		}
	}
	return ConfiguredMaxHealth;
}

float UEclipseHealthComponent::GetMaximumHealth() const
{
	return ResolveMaxHealth();
}

float UEclipseHealthComponent::GetCurrentHealth() const
{
	return CurrentHealth;
}

bool UEclipseHealthComponent::IsDead() const
{
	return CurrentHealth <= 0.0f;
}

bool UEclipseHealthComponent::IsDamageable() const
{
	return !bInvulnerable && !IsDead();
}

float UEclipseHealthComponent::GetHealthFraction() const
{
	const float MaxHealth = ResolveMaxHealth();
	return MaxHealth > 0.0f ? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

float UEclipseHealthComponent::GetShieldFraction() const
{
	return ConfiguredMaxShield > 0.0f ? FMath::Clamp(CurrentShield / ConfiguredMaxShield, 0.0f, 1.0f) : 0.0f;
}

float UEclipseHealthComponent::GetArmor() const
{
	const AActor* Owner = GetOwner();
	if (Owner != nullptr)
	{
		if (const UEclipseStatComponent* StatComponent = Owner->FindComponentByClass<UEclipseStatComponent>())
		{
			const float StatArmor = StatComponent->GetStat(EEclipseStat::Armor);
			if (StatArmor > 0.0f)
			{
				return StatArmor;
			}
		}
	}
	return ConfiguredArmor;
}

void UEclipseHealthComponent::InitializeHealth(float InMaxHealth, float InMaxShield, float InArmor, const FEclipseResistanceProfile& InResistances)
{
	ConfiguredMaxHealth = FMath::Max(1.0f, InMaxHealth);
	ConfiguredMaxShield = FMath::Max(0.0f, InMaxShield);
	ConfiguredArmor = FMath::Max(0.0f, InArmor);
	Resistances = InResistances;

	CurrentHealth = ConfiguredMaxHealth;
	CurrentShield = ConfiguredMaxShield;
	bDeathBroadcast = false;

	RefreshTickState();
	OnHealthChanged.Broadcast(CurrentHealth, ConfiguredMaxHealth, 0.0f, nullptr);
	OnShieldChanged.Broadcast(CurrentShield, ConfiguredMaxShield);
}

void UEclipseHealthComponent::SetMaxHealth(float InMaxHealth)
{
	ConfiguredMaxHealth = FMath::Max(1.0f, InMaxHealth);
	CurrentHealth = FMath::Min(CurrentHealth, ConfiguredMaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, ConfiguredMaxHealth, 0.0f, nullptr);
}

void UEclipseHealthComponent::SetArmor(float InArmor)
{
	ConfiguredArmor = FMath::Max(0.0f, InArmor);
}

void UEclipseHealthComponent::SetResistances(const FEclipseResistanceProfile& InResistances)
{
	Resistances = InResistances;
}

void UEclipseHealthComponent::SetShieldRegeneration(float PointsPerSecond, float DelaySeconds)
{
	ShieldRegenPerSecond = FMath::Max(0.0f, PointsPerSecond);
	ShieldRegenDelay = FMath::Max(0.0f, DelaySeconds);
	RefreshTickState();
}

void UEclipseHealthComponent::SetInvulnerable(bool bInInvulnerable)
{
	bInvulnerable = bInInvulnerable;
}

FEclipseDamageResult UEclipseHealthComponent::ApplyEclipseDamage(const FEclipseDamageContext& Context)
{
	FEclipseDamageResult Result;

	if (!Context.IsValid() || !IsDamageable())
	{
		Result.HealthRemaining = CurrentHealth;
		Result.ShieldRemaining = CurrentShield;
		return Result;
	}

	Result.bApplied = true;
	Result.RawDamage = Context.BaseDamage;
	Result.ResistanceMultiplier = UEclipseDamageLibrary::ComputeResistanceMultiplier(Resistances, Context.DamageType);

	const float Armor = Context.bIgnoreArmor ? 0.0f : GetArmor();
	float ArmorMitigation = 0.0f;
	Result.MitigatedDamage = UEclipseDamageLibrary::ComputeFinalDamage(Context, Armor, Resistances, ArmorMitigation);
	Result.ArmorMitigation = ArmorMitigation;

	float Remaining = Result.MitigatedDamage;

	// Shields absorb first unless the damage type bypasses them (Void, EMP).
	if (!Context.bIgnoreShields && CurrentShield > 0.0f && Remaining > 0.0f)
	{
		const float Absorbed = FMath::Min(CurrentShield, Remaining);
		CurrentShield -= Absorbed;
		Remaining -= Absorbed;
		Result.ShieldDamage = Absorbed;
	}

	if (Remaining > 0.0f)
	{
		const float Applied = FMath::Min(CurrentHealth, Remaining);
		CurrentHealth -= Applied;
		Result.HealthDamage = Applied;
	}

	TimeSinceDamage = 0.0f;

	Result.HealthRemaining = CurrentHealth;
	Result.ShieldRemaining = CurrentShield;
	Result.bKilled = CurrentHealth <= 0.0f;

	OnHealthChanged.Broadcast(CurrentHealth, ResolveMaxHealth(), -Result.HealthDamage, Context.Instigator);
	if (Result.ShieldDamage > 0.0f)
	{
		OnShieldChanged.Broadcast(CurrentShield, ConfiguredMaxShield);
	}

	if (Result.bKilled && !bDeathBroadcast)
	{
		bDeathBroadcast = true;
		SetComponentTickEnabled(false);

		UE_LOG(LogEclipseCombat, Log, TEXT("%s died to %s (%s, %.1f damage)."),
			*GetOwner()->GetName(),
			Context.Instigator != nullptr ? *Context.Instigator->GetName() : TEXT("the world"),
			EclipseEnumNames::ToString(Context.DamageType),
			Result.MitigatedDamage);

		OnDeath.Broadcast(Context.Instigator, Context.DamageType);
	}

	return Result;
}

float UEclipseHealthComponent::Heal(float Amount)
{
	if (Amount <= 0.0f || IsDead())
	{
		return 0.0f;
	}

	const float MaxHealth = ResolveMaxHealth();
	const float Restored = FMath::Min(Amount, MaxHealth - CurrentHealth);
	if (Restored <= 0.0f)
	{
		return 0.0f;
	}

	CurrentHealth += Restored;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, Restored, nullptr);
	return Restored;
}

float UEclipseHealthComponent::RestoreShield(float Amount)
{
	if (Amount <= 0.0f || IsDead())
	{
		return 0.0f;
	}

	const float Restored = FMath::Min(Amount, ConfiguredMaxShield - CurrentShield);
	if (Restored <= 0.0f)
	{
		return 0.0f;
	}

	CurrentShield += Restored;
	OnShieldChanged.Broadcast(CurrentShield, ConfiguredMaxShield);
	return Restored;
}

void UEclipseHealthComponent::SetShield(float NewShield)
{
	CurrentShield = FMath::Clamp(NewShield, 0.0f, ConfiguredMaxShield);
	OnShieldChanged.Broadcast(CurrentShield, ConfiguredMaxShield);
}

void UEclipseHealthComponent::SetHealth(float NewHealth)
{
	const float MaxHealth = ResolveMaxHealth();
	CurrentHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	bDeathBroadcast = CurrentHealth <= 0.0f;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, 0.0f, nullptr);
}

void UEclipseHealthComponent::Kill(AActor* Killer)
{
	if (IsDead())
	{
		return;
	}

	FEclipseDamageContext Context;
	Context.BaseDamage = CurrentHealth;
	Context.DamageType = EEclipseDamageType::Physical;
	Context.Instigator = Killer;
	Context.DamageSourceId = TEXT("System.Kill");
	Context.bIgnoreArmor = true;
	Context.bIgnoreShields = true;
	Context.HitLocation = GetOwner() != nullptr ? GetOwner()->GetActorLocation() : FVector::ZeroVector;

	ApplyEclipseDamage(Context);
}

void UEclipseHealthComponent::Revive(float HealthFraction)
{
	const float MaxHealth = ResolveMaxHealth();
	CurrentHealth = FMath::Clamp(MaxHealth * FMath::Clamp(HealthFraction, 0.0f, 1.0f), 0.0f, MaxHealth);
	CurrentShield = ConfiguredMaxShield;
	TimeSinceDamage = 0.0f;
	bDeathBroadcast = false;

	RefreshTickState();

	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth, 0.0f, nullptr);
	OnShieldChanged.Broadcast(CurrentShield, ConfiguredMaxShield);
	OnRevived.Broadcast();
}
