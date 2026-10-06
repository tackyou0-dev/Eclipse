// PROJECT ECLIPSE - Stat component implementation.
//
// Purpose
//   Modifier aggregation and stamina simulation. Aggregates are cached and rebuilt on
//   change rather than recomputed per query, because GetStat is called from the weapon
//   fire path and from movement every frame.

#include "Core/EclipseStatComponent.h"

#include "Core/EclipseLog.h"
#include "Net/UnrealNetwork.h"

UEclipseStatComponent::UEclipseStatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UEclipseStatComponent::BeginPlay()
{
	Super::BeginPlay();

	// Sensible defaults for a humanoid colonist. Characters override these in their
	// data-driven setup; a component with no configuration still behaves.
	SetBaseStat(EEclipseStat::MaxHealth, 100.0f);
	SetBaseStat(EEclipseStat::MaxShield, 0.0f);
	SetBaseStat(EEclipseStat::MaxStamina, 100.0f);
	SetBaseStat(EEclipseStat::Armor, 0.0f);
	SetBaseStat(EEclipseStat::MoveSpeed, 600.0f);
	SetBaseStat(EEclipseStat::SprintSpeed, 1.0f);
	SetBaseStat(EEclipseStat::CarryCapacity, 120.0f);
	SetBaseStat(EEclipseStat::LootYield, 1.0f);
	SetBaseStat(EEclipseStat::EnvironmentalResistance, 0.0f);
	SetBaseStat(EEclipseStat::VoidResistance, 0.0f);

	CurrentStamina = GetMaxStamina();

	const bool bHasStaminaPool = GetMaxStamina() > 0.0f;
	SetComponentTickEnabled(bHasStaminaPool);
}

void UEclipseStatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	TimeSinceStaminaSpend += DeltaTime;

	const float MaxStamina = GetMaxStamina();
	if (MaxStamina <= 0.0f || StaminaRegenPerSecond <= 0.0f)
	{
		return;
	}

	if (CurrentStamina >= MaxStamina || TimeSinceStaminaSpend < StaminaRegenDelay)
	{
		return;
	}

	// Only the server mutates stamina; clients receive it through replication.
	const AActor* Owner = GetOwner();
	if (Owner == nullptr || !Owner->HasAuthority())
	{
		return;
	}

	RestoreStamina(StaminaRegenPerSecond * DeltaTime);
}

void UEclipseStatComponent::SetBaseStat(EEclipseStat Stat, float Value)
{
	if (Stat == EEclipseStat::None || Stat == EEclipseStat::Count)
	{
		UE_LOG(LogEclipse, Warning, TEXT("SetBaseStat called with an invalid stat."));
		return;
	}

	BaseStats.Add(Stat, FMath::Max(0.0f, Value));

	if (Stat == EEclipseStat::MaxStamina)
	{
		CurrentStamina = FMath::Min(CurrentStamina, GetMaxStamina());
		SetComponentTickEnabled(GetMaxStamina() > 0.0f);
	}

	OnStatChanged.Broadcast(Stat, GetStat(Stat));
}

float UEclipseStatComponent::GetBaseStat(EEclipseStat Stat) const
{
	if (const float* Found = BaseStats.Find(Stat))
	{
		return *Found;
	}
	return 0.0f;
}

float UEclipseStatComponent::GetStat(EEclipseStat Stat) const
{
	const float Base = GetBaseStat(Stat);
	if (const FEclipseStatAggregate* Aggregate = Aggregates.Find(Stat))
	{
		return Aggregate->Apply(Base);
	}
	return Base;
}

const FEclipseStatAggregate* UEclipseStatComponent::GetAggregate(EEclipseStat Stat) const
{
	return Aggregates.Find(Stat);
}

void UEclipseStatComponent::AddModifierSource(FName SourceId, const TArray<FEclipseStatModifier>& Modifiers)
{
	if (SourceId.IsNone())
	{
		UE_LOG(LogEclipse, Warning, TEXT("AddModifierSource called with an empty source id."));
		return;
	}

	FEclipseModifierSource& Source = ActiveSources.FindOrAdd(SourceId);
	Source.SourceId = SourceId;
	Source.Modifiers = Modifiers;

	RebuildAggregates();
	BroadcastStatChanges(Modifiers);
}

bool UEclipseStatComponent::RemoveModifierSource(FName SourceId)
{
	if (ActiveSources.Remove(SourceId) <= 0)
	{
		return false;
	}

	RebuildAggregates();

	// Broadcast the whole stat table: removing a source can change anything it touched and
	// the set is small enough that a full refresh is cheaper than tracking it.
	for (const TPair<EEclipseStat, float>& Pair : BaseStats)
	{
		OnStatChanged.Broadcast(Pair.Key, GetStat(Pair.Key));
	}

	return true;
}

bool UEclipseStatComponent::HasModifierSource(FName SourceId) const
{
	return ActiveSources.Contains(SourceId);
}

void UEclipseStatComponent::ClearModifierSources()
{
	ActiveSources.Reset();
	Aggregates.Reset();

	for (const TPair<EEclipseStat, float>& Pair : BaseStats)
	{
		OnStatChanged.Broadcast(Pair.Key, GetStat(Pair.Key));
	}
}

TArray<FName> UEclipseStatComponent::GetModifierSourceIds() const
{
	TArray<FName> Ids;
	ActiveSources.GetKeys(Ids);
	return Ids;
}

void UEclipseStatComponent::RebuildAggregates()
{
	Aggregates.Reset();

	for (const TPair<FName, FEclipseModifierSource>& SourcePair : ActiveSources)
	{
		for (const FEclipseStatModifier& Modifier : SourcePair.Value.Modifiers)
		{
			if (Modifier.Stat == EEclipseStat::None || Modifier.Stat == EEclipseStat::Count)
			{
				continue;
			}

			FEclipseStatAggregate& Aggregate = Aggregates.FindOrAdd(Modifier.Stat);
			Aggregate.Accumulate(Modifier);
		}
	}
}

void UEclipseStatComponent::BroadcastStatChanges(const TArray<FEclipseStatModifier>& Modifiers)
{
	for (const FEclipseStatModifier& Modifier : Modifiers)
	{
		if (Modifier.Stat == EEclipseStat::None || Modifier.Stat == EEclipseStat::Count)
		{
			continue;
		}
		OnStatChanged.Broadcast(Modifier.Stat, GetStat(Modifier.Stat));
	}
}

float UEclipseStatComponent::GetMaxStamina() const
{
	return GetStat(EEclipseStat::MaxStamina);
}

float UEclipseStatComponent::GetStaminaFraction() const
{
	const float MaxStamina = GetMaxStamina();
	if (MaxStamina <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(CurrentStamina / MaxStamina, 0.0f, 1.0f);
}

bool UEclipseStatComponent::ConsumeStamina(float Amount)
{
	if (Amount <= 0.0f)
	{
		return true;
	}

	if (CurrentStamina < Amount)
	{
		return false;
	}

	SetStamina(CurrentStamina - Amount);
	return true;
}

void UEclipseStatComponent::RestoreStamina(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	SetStamina(CurrentStamina + Amount);
}

bool UEclipseStatComponent::ConsumeSprintStamina(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.0f)
	{
		return true;
	}

	const float Cost = SprintStaminaPerSecond * DeltaSeconds;
	if (CurrentStamina <= 0.0f)
	{
		OnStaminaDepleted.Broadcast();
		return false;
	}

	// Sprinting is allowed to run the pool down to zero; the last frame simply costs what
	// is left. Returning false on the same frame stops the sprint cleanly.
	const bool bHasEnough = CurrentStamina >= Cost;
	SetStamina(CurrentStamina - Cost);

	if (!bHasEnough)
	{
		OnStaminaDepleted.Broadcast();
		return false;
	}

	return true;
}

void UEclipseStatComponent::RefillStamina()
{
	CurrentStamina = GetMaxStamina();
	TimeSinceStaminaSpend = 0.0f;
	OnStatChanged.Broadcast(EEclipseStat::MaxStamina, GetMaxStamina());
}

void UEclipseStatComponent::SetStamina(float NewStamina)
{
	const float Clamped = FMath::Clamp(NewStamina, 0.0f, FMath::Max(GetMaxStamina(), 1.0f));
	if (FMath::IsNearlyEqual(Clamped, CurrentStamina, 0.01f))
	{
		return;
	}

	CurrentStamina = Clamped;
	TimeSinceStaminaSpend = 0.0f;

	if (CurrentStamina <= 0.0f)
	{
		OnStaminaDepleted.Broadcast();
	}
}

void UEclipseStatComponent::OnRep_CurrentStamina()
{
	OnStatChanged.Broadcast(EEclipseStat::MaxStamina, GetMaxStamina());
}

void UEclipseStatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEclipseStatComponent, CurrentStamina);
}
