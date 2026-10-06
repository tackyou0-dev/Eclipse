// PROJECT ECLIPSE - Crafting workstation implementation.
//
// Purpose
//   Station reporting and the interaction that opens the crafting menu.

#include "Crafting/EclipseWorkstation.h"

#include "Crafting/EclipseCraftingSubsystem.h"
#include "Crafting/EclipseRecipeDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Core/EclipseLog.h"
#include "Engine/World.h"
#include "UI/EclipseUISubsystem.h"

AEclipseWorkstation::AEclipseWorkstation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	RootComponent = MeshComponent;
}

bool AEclipseWorkstation::ProvidesModule(EEclipseBaseModule Module) const
{
	return Module != EEclipseBaseModule::None && ProvidedModule == Module;
}

bool AEclipseWorkstation::CanInteract_Implementation(AActor* Interactor) const
{
	return Interactor != nullptr && IsInteractionEnabled_Implementation();
}

bool AEclipseWorkstation::IsInteractionEnabled_Implementation() const
{
	// A station is always usable once built; a destroyed station would be a different actor.
	return ProvidedModule != EEclipseBaseModule::None;
}

FText AEclipseWorkstation::GetInteractionPrompt_Implementation() const
{
	const FString ModuleName = StaticEnum<EEclipseBaseModule>() != nullptr
		? StaticEnum<EEclipseBaseModule>()->GetNameStringByValue(static_cast<int64>(ProvidedModule))
		: TEXT("Station");

	return FText::Format(NSLOCTEXT("Eclipse", "UseStationPrompt", "Use {0}"), FText::FromString(ModuleName));
}

void AEclipseWorkstation::Interact_Implementation(AActor* Interactor)
{
	if (Interactor == nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	UEclipseUISubsystem* UISubsystem = World != nullptr ? World->GetSubsystem<UEclipseUISubsystem>() : nullptr;
	if (UISubsystem == nullptr)
	{
		UE_LOG(LogEclipsePresentation, Warning, TEXT("No UI subsystem: cannot open the crafting menu."));
		return;
	}

	UISubsystem->OpenCraftingMenu(Interactor, this);
}

bool AEclipseWorkstation::ServerRequestCraft_Validate(AActor* Interactor, UEclipseRecipeDefinition* Recipe)
{
	// A malformed intent is rejected before it reaches gameplay code.
	return Interactor != nullptr && Recipe != nullptr && Recipe->RecipeId != NAME_None;
}

void AEclipseWorkstation::ServerRequestCraft_Implementation(AActor* Interactor, UEclipseRecipeDefinition* Recipe)
{
	UWorld* World = GetWorld();
	if (World == nullptr || Interactor == nullptr || Recipe == nullptr)
	{
		return;
	}

	// Range is re-checked on the server: the client's menu being open is not proof that
	// the player is anywhere near the station.
	const float AllowedRadius = StationRadius * 1.5f;
	if (FVector::DistSquared(Interactor->GetActorLocation(), GetActorLocation()) > FMath::Square(AllowedRadius))
	{
		UE_LOG(LogEclipseItems, Warning, TEXT("%s tried to craft at %s from out of range."),
			*Interactor->GetName(), *GetName());
		return;
	}

	UEclipseCraftingSubsystem* Crafting = World->GetSubsystem<UEclipseCraftingSubsystem>();
	if (Crafting == nullptr)
	{
		return;
	}

	EEclipseCraftRefusal Refusal = EEclipseCraftRefusal::None;
	TArray<FEclipseRecipeIngredient> Missing;
	if (!Crafting->CanCraft(Interactor, Recipe, Refusal, Missing))
	{
		UE_LOG(LogEclipseItems, Log, TEXT("Craft refused for %s (reason code %d)."), *Interactor->GetName(), static_cast<int32>(Refusal));
		return;
	}

	Crafting->BeginCraft(Interactor, Recipe);
}

bool AEclipseWorkstation::ServerCancelCraft_Validate(AActor* Interactor)
{
	return Interactor != nullptr;
}

void AEclipseWorkstation::ServerCancelCraft_Implementation(AActor* Interactor)
{
	if (UWorld* World = GetWorld())
	{
		if (UEclipseCraftingSubsystem* Crafting = World->GetSubsystem<UEclipseCraftingSubsystem>())
		{
			Crafting->CancelCraft(Interactor);
		}
	}
}
