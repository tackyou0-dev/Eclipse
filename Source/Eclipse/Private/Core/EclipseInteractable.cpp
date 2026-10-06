// PROJECT ECLIPSE - Interaction interface default implementations.
//
// Purpose
//   Real defaults rather than empty bodies: the fallbacks keep the interaction system
//   functional for a partially implemented actor, and the warning in Interact makes the
//   missing override obvious in the log instead of looking like a broken input bind.

#include "Core/EclipseInteractable.h"

#include "Core/EclipseLog.h"
#include "GameFramework/Actor.h"

bool IEclipseInteractable::CanInteract_Implementation(AActor* Interactor) const
{
	// Interaction is allowed for anyone unless an implementer says otherwise. The
	// interaction system performs its own range, line-of-sight and state checks before
	// this is ever consulted.
	return IsInteractionEnabled_Implementation();
}

bool IEclipseInteractable::IsInteractionEnabled_Implementation() const
{
	return true;
}

FText IEclipseInteractable::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("Eclipse", "GenericInteractPrompt", "Interact");
}

float IEclipseInteractable::GetInteractionDuration_Implementation() const
{
	return 0.0f;
}

void IEclipseInteractable::Interact_Implementation(AActor* Interactor)
{
	const UObject* Self = _getUObject();
	UE_LOG(LogEclipse, Warning,
		TEXT("%s implements IEclipseInteractable but does not override Interact; the interaction was ignored."),
		Self != nullptr ? *Self->GetName() : TEXT("<unknown>"));
}
