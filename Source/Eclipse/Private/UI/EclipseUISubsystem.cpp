// PROJECT ECLIPSE - UI subsystem implementation.
//
// Purpose
//   Menu lifecycle, prompts, notifications and craft progress. The rule the whole file
//   follows: state is tracked for every player, widgets only exist where there is a
//   viewport to draw them in.

#include "UI/EclipseUISubsystem.h"

#include "Crafting/EclipseCraftingSubsystem.h"
#include "Crafting/EclipseWorkstation.h"
#include "Core/EclipseLog.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UEclipseMenuWidget::NotifyOpened(APlayerController* Controller, AActor* InOwnerActor)
{
	OwningController = Controller;
	MenuOwnerActor = InOwnerActor;
	OnMenuOpened(Controller, InOwnerActor);
}

void UEclipseMenuWidget::NotifyClosed()
{
	OnMenuClosed();

	OwningController.Reset();
	MenuOwnerActor.Reset();
}

void UEclipseCraftingMenuWidget::SetCraftingContext(AEclipseWorkstation* InStation, const TArray<UEclipseRecipeDefinition*>& InRecipes)
{
	Station = InStation;

	KnownRecipes.Reset(InRecipes.Num());
	for (UEclipseRecipeDefinition* Recipe : InRecipes)
	{
		if (Recipe != nullptr)
		{
			KnownRecipes.Add(Recipe);
		}
	}

	LastProgress = -1.0f;
	OnCraftingContextChanged(KnownRecipes);
}

void UEclipseCraftingMenuWidget::RequestCraft(UEclipseRecipeDefinition* Recipe)
{
	AEclipseWorkstation* OpenStation = Station.Get();
	AActor* OwnerActor = GetMenuOwnerActor();

	if (OpenStation == nullptr || OwnerActor == nullptr || Recipe == nullptr)
	{
		UE_LOG(LogEclipsePresentation, Warning, TEXT("Craft requested with no station, owner or recipe."));
		return;
	}

	// Client intent, server decision: the station re-validates range, ingredients and
	// station before anything is consumed.
	OpenStation->ServerRequestCraft(OwnerActor, Recipe);
}

void UEclipseCraftingMenuWidget::RequestCancelCraft()
{
	if (AEclipseWorkstation* OpenStation = Station.Get())
	{
		if (AActor* OwnerActor = GetMenuOwnerActor())
		{
			OpenStation->ServerCancelCraft(OwnerActor);
		}
	}
}

void UEclipseCraftingMenuWidget::UpdateCraftProgress(float ProgressFraction, bool bCrafting)
{
	if (FMath::IsNearlyEqual(ProgressFraction, LastProgress) && bCrafting == bLastCrafting)
	{
		return;
	}

	LastProgress = ProgressFraction;
	bLastCrafting = bCrafting;
	OnCraftProgressUpdated(ProgressFraction, bCrafting);
}

void UEclipseUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PlayerEntries.Reset();
	CachedCrafting.Reset();
}

void UEclipseUISubsystem::Deinitialize()
{
	CloseAllMenus();
	PlayerEntries.Reset();
	CachedCrafting.Reset();

	Super::Deinitialize();
}

TStatId UEclipseUISubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEclipseUISubsystem, STATGROUP_Tickables);
}

UEclipseUISubsystem* UEclipseUISubsystem::Get(const UWorld* World)
{
	return World != nullptr ? World->GetSubsystem<UEclipseUISubsystem>() : nullptr;
}

APlayerController* UEclipseUISubsystem::ResolveController(const AActor* Actor)
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	if (const APawn* Pawn = Cast<const APawn>(Actor))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}

	if (const UActorComponent* Component = Cast<const UActorComponent>(Actor))
	{
		return ResolveController(Component->GetOwner());
	}

	return Cast<APlayerController>(const_cast<AActor*>(Actor));
}

bool UEclipseUISubsystem::HasLocalView(const APlayerController* Controller)
{
	// A dedicated server has no local controller, so nothing is ever created there; on a
	// listen host the host's own controller does have a local view.
	return Controller != nullptr && Controller->IsLocalController();
}

FEclipsePlayerUIEntry& UEclipseUISubsystem::FindOrAddEntry(APlayerController* Controller)
{
	for (FEclipsePlayerUIEntry& Entry : PlayerEntries)
	{
		if (Entry.Controller.Get() == Controller)
		{
			return Entry;
		}
	}

	FEclipsePlayerUIEntry& NewEntry = PlayerEntries.AddDefaulted_GetRef();
	NewEntry.Controller = Controller;
	return NewEntry;
}

const FEclipsePlayerUIEntry* UEclipseUISubsystem::FindEntry(const APlayerController* Controller) const
{
	for (const FEclipsePlayerUIEntry& Entry : PlayerEntries)
	{
		if (Entry.Controller.Get() == Controller)
		{
			return &Entry;
		}
	}

	return nullptr;
}

bool UEclipseUISubsystem::OpenCraftingMenu(AActor* Interactor, AEclipseWorkstation* Workstation)
{
	if (Interactor == nullptr || Workstation == nullptr)
	{
		return false;
	}

	APlayerController* Controller = ResolveController(Interactor);
	if (Controller == nullptr)
	{
		UE_LOG(LogEclipsePresentation, Warning, TEXT("Cannot open the crafting menu: no player controller behind %s."), *Interactor->GetName());
		return false;
	}

	FEclipsePlayerUIEntry& Entry = FindOrAddEntry(Controller);

	// Re-opening at another station replaces the menu rather than stacking two of them.
	DestroyMenu(Entry);
	Entry.OpenStation = Workstation;

	UEclipseMenuWidget* Widget = CreateMenuWidget(Controller, CraftingMenuWidgetClass, Interactor);
	Entry.MenuWidget = Widget;

	if (UEclipseCraftingMenuWidget* CraftingWidget = Cast<UEclipseCraftingMenuWidget>(Widget))
	{
		UEclipseCraftingSubsystem* Crafting = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UEclipseCraftingSubsystem>() : nullptr;
		CachedCrafting = Crafting;

		if (Crafting != nullptr)
		{
			CraftingWidget->SetCraftingContext(Workstation, Crafting->GetKnownRecipes(Interactor));
			CraftingWidget->UpdateCraftProgress(
				Crafting->IsCrafting(Interactor) ? Crafting->GetCraftProgress(Interactor) : 0.0f,
				Crafting->IsCrafting(Interactor));
		}
	}

	UE_LOG(LogEclipsePresentation, Verbose, TEXT("Crafting menu opened for %s at %s."),
		*Controller->GetName(), *Workstation->GetName());

	// The menu is open as soon as it exists for a player with a view; on a dedicated
	// server there is nothing to draw, which is why this returns HasLocalView.
	return HasLocalView(Controller);
}

bool UEclipseUISubsystem::CloseMenu(AActor* Interactor)
{
	APlayerController* Controller = ResolveController(Interactor);
	const FEclipsePlayerUIEntry* Found = FindEntry(Controller);
	if (Controller == nullptr || Found == nullptr || !Found->HasMenu())
	{
		return false;
	}

	for (FEclipsePlayerUIEntry& Entry : PlayerEntries)
	{
		if (Entry.Controller.Get() == Controller)
		{
			DestroyMenu(Entry);
			return true;
		}
	}

	return false;
}

bool UEclipseUISubsystem::IsMenuOpen(const AActor* Interactor) const
{
	const FEclipsePlayerUIEntry* Entry = FindEntry(ResolveController(Interactor));
	return Entry != nullptr && Entry->HasMenu();
}

AEclipseWorkstation* UEclipseUISubsystem::GetOpenStation(const AActor* Interactor) const
{
	const FEclipsePlayerUIEntry* Entry = FindEntry(ResolveController(Interactor));
	return Entry != nullptr ? Entry->OpenStation.Get() : nullptr;
}

void UEclipseUISubsystem::CloseAllMenus()
{
	for (FEclipsePlayerUIEntry& Entry : PlayerEntries)
	{
		DestroyMenu(Entry);
	}
}

UEclipseMenuWidget* UEclipseUISubsystem::CreateMenuWidget(APlayerController* Controller, TSoftClassPtr<UEclipseMenuWidget> WidgetClass, AActor* OwnerActor)
{
	if (!HasLocalView(Controller) || WidgetClass.IsNull())
	{
		return nullptr;
	}

	UClass* LoadedClass = WidgetClass.LoadSynchronous();
	if (LoadedClass == nullptr)
	{
		UE_LOG(LogEclipsePresentation, Warning, TEXT("Menu widget class '%s' failed to load."), *WidgetClass.ToString());
		return nullptr;
	}

	UEclipseMenuWidget* Widget = CreateWidget<UEclipseMenuWidget>(Controller, LoadedClass);
	if (Widget == nullptr)
	{
		return nullptr;
	}

	Widget->NotifyOpened(Controller, OwnerActor);
	Widget->AddToViewport();
	return Widget;
}

void UEclipseUISubsystem::DestroyMenu(FEclipsePlayerUIEntry& Entry)
{
	if (UEclipseMenuWidget* Widget = Entry.MenuWidget.Get())
	{
		Widget->NotifyClosed();
		Widget->RemoveFromParent();
	}

	Entry.MenuWidget = nullptr;
	Entry.OpenStation = nullptr;
}

void UEclipseUISubsystem::SetInteractionPrompt(AActor* Interactor, const FText& Prompt)
{
	APlayerController* Controller = ResolveController(Interactor);
	if (Controller == nullptr)
	{
		return;
	}

	FEclipsePlayerUIEntry& Entry = FindOrAddEntry(Controller);
	Entry.InteractionPrompt = Prompt;
}

FText UEclipseUISubsystem::GetInteractionPrompt(const AActor* Interactor) const
{
	const FEclipsePlayerUIEntry* Entry = FindEntry(ResolveController(Interactor));
	return Entry != nullptr ? Entry->InteractionPrompt : FText::GetEmpty();
}

void UEclipseUISubsystem::ShowNotification(AActor* Interactor, const FText& Text, float DurationSeconds)
{
	APlayerController* Controller = ResolveController(Interactor);
	if (Controller == nullptr)
	{
		return;
	}

	FEclipsePlayerUIEntry& Entry = FindOrAddEntry(Controller);
	Entry.Notification.Text = Text;
	Entry.Notification.DurationSeconds = FMath::Max(0.5f, DurationSeconds);
	Entry.Notification.TimeRemaining = Entry.Notification.DurationSeconds;

	OnNotificationChanged.Broadcast(Controller, Entry.Notification);
}

void UEclipseUISubsystem::BroadcastNotification(const FText& Text, float DurationSeconds)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (Controller == nullptr)
		{
			continue;
		}

		FEclipsePlayerUIEntry& Entry = FindOrAddEntry(Controller);
		Entry.Notification.Text = Text;
		Entry.Notification.DurationSeconds = FMath::Max(0.5f, DurationSeconds);
		Entry.Notification.TimeRemaining = Entry.Notification.DurationSeconds;

		OnNotificationChanged.Broadcast(Controller, Entry.Notification);
	}
}

FEclipseNotification UEclipseUISubsystem::GetNotification(const AActor* Interactor) const
{
	const FEclipsePlayerUIEntry* Entry = FindEntry(ResolveController(Interactor));
	return Entry != nullptr ? Entry->Notification : FEclipseNotification();
}

void UEclipseUISubsystem::Tick(float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	for (FEclipsePlayerUIEntry& Entry : PlayerEntries)
	{
		if (!Entry.Notification.IsActive())
		{
			continue;
		}

		Entry.Notification.TimeRemaining = FMath::Max(0.0f, Entry.Notification.TimeRemaining - DeltaTime);

		if (!Entry.Notification.IsActive())
		{
			Entry.Notification.Text = FText::GetEmpty();
			Entry.Notification.TimeRemaining = 0.0f;
			OnNotificationChanged.Broadcast(Entry.Controller.Get(), Entry.Notification);
		}
	}

	UpdateCraftingMenus();
	PruneEntries();
}

void UEclipseUISubsystem::UpdateCraftingMenus()
{
	UWorld* World = GetWorld();
	UEclipseCraftingSubsystem* Crafting = CachedCrafting.Get();
	if (Crafting == nullptr && World != nullptr)
	{
		Crafting = World->GetSubsystem<UEclipseCraftingSubsystem>();
		CachedCrafting = Crafting;
	}

	if (Crafting == nullptr)
	{
		return;
	}

	for (FEclipsePlayerUIEntry& Entry : PlayerEntries)
	{
		UEclipseCraftingMenuWidget* Menu = Cast<UEclipseCraftingMenuWidget>(Entry.MenuWidget.Get());
		APlayerController* Controller = Entry.Controller.Get();
		if (Menu == nullptr || Controller == nullptr)
		{
			continue;
		}

		AActor* Crafter = Controller->GetPawn();
		if (Crafter == nullptr)
		{
			continue;
		}

		const bool bCrafting = Crafting->IsCrafting(Crafter);
		Menu->UpdateCraftProgress(bCrafting ? Crafting->GetCraftProgress(Crafter) : 0.0f, bCrafting);
	}
}

void UEclipseUISubsystem::PruneEntries()
{
	// A controller that has been destroyed leaves its widget behind unless it is dropped
	// here; the widget's own weak pointer is the signal.
	PlayerEntries.RemoveAll([](const FEclipsePlayerUIEntry& Entry)
	{
		return !Entry.Controller.IsValid();
	});

	for (FEclipsePlayerUIEntry& Entry : PlayerEntries)
	{
		if (!Entry.MenuWidget.IsValid())
		{
			// The widget was removed by something other than the subsystem (a level
			// transition, a Blueprint call): forget the menu so state matches the screen.
			Entry.MenuWidget = nullptr;
			Entry.OpenStation = nullptr;
		}
	}
}
