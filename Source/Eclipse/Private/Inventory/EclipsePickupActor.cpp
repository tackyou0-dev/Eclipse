// PROJECT ECLIPSE - Pickup actor implementation.
//
// Purpose
//   Handing items to an inventory. The server does the moving; the client only asks.

#include "Inventory/EclipsePickupActor.h"

#include "Components/StaticMeshComponent.h"
#include "Core/EclipseLog.h"
#include "Engine/World.h"
#include "Inventory/EclipseInventoryComponent.h"
#include "Inventory/EclipseItemDefinition.h"
#include "Net/UnrealNetwork.h"

AEclipsePickupActor::AEclipsePickupActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	MeshComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	MeshComponent->SetSimulatePhysics(false);
	RootComponent = MeshComponent;
}

void AEclipsePickupActor::InitializePickup(FName InItemId, int32 InCount)
{
	ItemId = InItemId;
	Count = FMath::Max(1, InCount);
	bLooted = false;
}

bool AEclipsePickupActor::CanInteract_Implementation(AActor* Interactor) const
{
	return IsInteractionEnabled_Implementation() && Interactor != nullptr;
}

bool AEclipsePickupActor::IsInteractionEnabled_Implementation() const
{
	return !bLooted && !ItemId.IsNone() && Count > 0;
}

FText AEclipsePickupActor::GetInteractionPrompt_Implementation() const
{
	if (const UEclipseItemCatalog* Catalog = UEclipseItemCatalog::GetCatalog())
	{
		if (const UEclipseItemDefinition* Definition = Catalog->Find(ItemId))
		{
			if (Count > 1)
			{
				return FText::Format(NSLOCTEXT("Eclipse", "PickupPromptCounted", "Take {0} x{1}"),
					Definition->DisplayName, FText::AsNumber(Count));
			}
			return FText::Format(NSLOCTEXT("Eclipse", "PickupPrompt", "Take {0}"), Definition->DisplayName);
		}
	}

	return NSLOCTEXT("Eclipse", "PickupPromptGeneric", "Take item");
}

void AEclipsePickupActor::Interact_Implementation(AActor* Interactor)
{
	if (!HasAuthority())
	{
		UE_LOG(LogEclipseSecurity, Warning, TEXT("Pickup interaction reached a client; ignoring."));
		return;
	}

	if (!IsInteractionEnabled_Implementation() || Interactor == nullptr)
	{
		return;
	}

	UEclipseInventoryComponent* Inventory = Interactor->FindComponentByClass<UEclipseInventoryComponent>();
	if (Inventory == nullptr)
	{
		UE_LOG(LogEclipseItems, Verbose, TEXT("%s has no inventory; pickup refused."), *Interactor->GetName());
		return;
	}

	const int32 Accepted = Inventory->AddItem(ItemId, Count);
	if (Accepted <= 0)
	{
		return;
	}

	Count -= Accepted;
	if (Count <= 0)
	{
		bLooted = true;
		Count = 0;

		if (LootedLifetimeSeconds > 0.0f)
		{
			SetLifeSpan(LootedLifetimeSeconds);
		}
		else
		{
			Destroy();
		}
	}
}

void AEclipsePickupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEclipsePickupActor, ItemId);
	DOREPLIFETIME(AEclipsePickupActor, Count);
	DOREPLIFETIME(AEclipsePickupActor, bLooted);
}
