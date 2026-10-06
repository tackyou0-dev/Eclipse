// PROJECT ECLIPSE - Biome volume implementation.
//
// Purpose
//   Containment tests and size reporting for overlap resolution.

#include "World/EclipseBiomeVolume.h"

#include "Components/BoxComponent.h"

AEclipseBiomeVolume::AEclipseBiomeVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	BoxComponent->SetBoxExtent(FVector(2000.0f, 2000.0f, 1000.0f));
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoxComponent->SetGenerateOverlapEvents(false);
	RootComponent = BoxComponent;
}

bool AEclipseBiomeVolume::ContainsLocation(const FVector& Location) const
{
	if (BoxComponent == nullptr)
	{
		return false;
	}

	// Transform the point into the volume's local space so a rotated box works, which is
	// what a designer expects when they rotate a ridge line.
	const FVector LocalLocation = BoxComponent->GetComponentTransform().InverseTransformPosition(Location);
	const FVector Extent = BoxComponent->GetScaledBoxExtent();

	return FMath::Abs(LocalLocation.X) <= Extent.X
		&& FMath::Abs(LocalLocation.Y) <= Extent.Y
		&& FMath::Abs(LocalLocation.Z) <= Extent.Z;
}

float AEclipseBiomeVolume::GetVolumeSize() const
{
	if (BoxComponent == nullptr)
	{
		return 0.0f;
	}

	const FVector Extent = BoxComponent->GetScaledBoxExtent();
	return 8.0f * Extent.X * Extent.Y * Extent.Z;
}

FVector AEclipseBiomeVolume::GetBoxExtent() const
{
	return BoxComponent != nullptr ? BoxComponent->GetScaledBoxExtent() : FVector::ZeroVector;
}
