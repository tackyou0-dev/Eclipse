// PROJECT ECLIPSE - UI subsystem.
//
// Purpose
//   The UI layer's server-side and client-side halves meet here. The subsystem owns, per
//   player: the open menu, the interaction prompt, the notification queue and the crafting
//   progress readout. Widgets themselves are UMG classes referenced by soft class path, so
//   the presentation is authored in Blueprint while the state machine that decides what
//   should be on screen stays in C++.
//
// Authority
//   Menu *state* is tracked on the server, because the server is what decides whether a
//   player may craft, loot or trade. Widget *instances* are only ever created for a
//   locally controlled player controller: a dedicated server has no viewport, and a
//   client must not instantiate widgets for other players' pawns. Craft requests made
//   from a widget travel back through the workstation's server RPC, never through a
//   local mutation.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/EclipseTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "EclipseUISubsystem.generated.h"

class AEclipseWorkstation;
class APlayerController;
class UEclipseCraftingSubsystem;
class UEclipseRecipeDefinition;

/** One transient message shown in the notification area. */
USTRUCT(BlueprintType)
struct ECLIPSE_API FEclipseNotification
{
	GENERATED_BODY()

	/** Message text. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|UI")
	FText Text;

	/** Seconds the message stays on screen. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|UI")
	float DurationSeconds = 3.0f;

	/** Seconds left before it disappears. Driven by the subsystem. */
	UPROPERTY(BlueprintReadOnly, Category = "Eclipse|UI")
	float TimeRemaining = 0.0f;

	/** True while there is something to show. */
	bool IsActive() const
	{
		return !Text.IsEmpty() && TimeRemaining > 0.0f;
	}
};

/** Broadcast when a player's notification changes (including when it expires). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FEclipseNotificationChanged, APlayerController* /*Controller*/, const FEclipseNotification& /*Notification*/);

/**
 * Base class for every menu the UI subsystem opens.
 *
 * Blueprint subclasses get OnMenuOpened / OnMenuClosed with the owning controller and the
 * actor that opened the menu, which is everything a menu needs to bind its buttons.
 */
UCLASS(Abstract, BlueprintType)
class ECLIPSE_API UEclipseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called by the subsystem right after the widget is created. */
	void NotifyOpened(APlayerController* Controller, AActor* InOwnerActor);

	/** Called by the subsystem before the widget is removed. */
	void NotifyClosed();

	/** Controller this widget belongs to. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|UI")
	APlayerController* GetOwningController() const { return OwningController.Get(); }

	/** Actor that opened the menu: the interactor, not the workstation. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|UI")
	AActor* GetMenuOwnerActor() const { return MenuOwnerActor.Get(); }

	/** Blueprint hook: bind data and buttons here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Eclipse|UI", meta = (DisplayName = "On Menu Opened"))
	void OnMenuOpened(APlayerController* Controller, AActor* OwnerActor);

	/** Blueprint hook: unbind timers and delegates here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Eclipse|UI", meta = (DisplayName = "On Menu Closed"))
	void OnMenuClosed();

protected:
	/** Controller this widget was created for. */
	TWeakObjectPtr<APlayerController> OwningController;

	/** Actor that asked for the menu. */
	TWeakObjectPtr<AActor> MenuOwnerActor;
};

/**
 * Crafting menu base.
 *
 * The subsystem feeds it the station and the recipes the crafter can currently make, then
 * pushes craft progress every frame the craft is running. Crafting itself is a server
 * intent: RequestCraft calls the station's server RPC.
 */
UCLASS(Abstract, BlueprintType)
class ECLIPSE_API UEclipseCraftingMenuWidget : public UEclipseMenuWidget
{
	GENERATED_BODY()

public:
	/** Station and recipes this menu is showing. */
	void SetCraftingContext(AEclipseWorkstation* InStation, const TArray<UEclipseRecipeDefinition*>& InRecipes);

	/** Station the menu was opened at. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|UI")
	AEclipseWorkstation* GetStation() const { return Station.Get(); }

	/** Recipes the crafter can make, filtered by station, level and skill. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|UI")
	const TArray<UEclipseRecipeDefinition*>& GetKnownRecipes() const { return KnownRecipes; }

	/** Blueprint hook: rebuild the recipe list. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Eclipse|UI", meta = (DisplayName = "On Crafting Context Changed"))
	void OnCraftingContextChanged(const TArray<UEclipseRecipeDefinition*>& Recipes);

	/** Blueprint hook: progress bar and cancel button state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Eclipse|UI", meta = (DisplayName = "On Craft Progress"))
	void OnCraftProgressUpdated(float ProgressFraction, bool bCrafting);

	/** Ask the server to start this recipe at the open station. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|UI")
	void RequestCraft(UEclipseRecipeDefinition* Recipe);

	/** Ask the server to cancel the running craft. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|UI")
	void RequestCancelCraft();

	/** Called by the subsystem as progress changes. Only broadcasts when it changed. */
	void UpdateCraftProgress(float ProgressFraction, bool bCrafting);

protected:
	/** Station the menu was opened at. */
	TWeakObjectPtr<AEclipseWorkstation> Station;

	/** Recipes currently offered. */
	UPROPERTY()
	TArray<TObjectPtr<UEclipseRecipeDefinition>> KnownRecipes;

	/** Last progress broadcast, so the widget is not updated every frame for nothing. */
	float LastProgress = -1.0f;

	/** Last crafting flag broadcast. */
	bool bLastCrafting = false;
};

/**
 * Per-player UI state.
 */
USTRUCT()
struct FEclipsePlayerUIEntry
{
	GENERATED_BODY()

	/** Controller this entry belongs to. */
	UPROPERTY()
	TWeakObjectPtr<APlayerController> Controller;

	/** Menu currently open, or null. */
	UPROPERTY()
	TWeakObjectPtr<UEclipseMenuWidget> MenuWidget;

	/** Station the open menu belongs to. */
	UPROPERTY()
	TWeakObjectPtr<AEclipseWorkstation> OpenStation;

	/** Prompt currently shown for the interaction under the crosshair. */
	UPROPERTY()
	FText InteractionPrompt;

	/** Current notification, including its remaining time. */
	UPROPERTY()
	FEclipseNotification Notification;

	/** True when this player currently has a menu open. */
	bool HasMenu() const { return MenuWidget.IsValid(); }
};

/**
 * World-scoped UI service: menu state, prompts, notifications and craft progress.
 */
UCLASS()
class ECLIPSE_API UEclipseUISubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** UTickableWorldSubsystem. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** UTickableWorldSubsystem. */
	virtual void Deinitialize() override;

	/** UTickableWorldSubsystem: notifications, craft progress and dead-entry pruning. */
	virtual void Tick(float DeltaTime) override;

	/** FTickableGameObject. */
	virtual TStatId GetStatId() const override;

	/** Convenience accessor; null outside a game world. */
	static UEclipseUISubsystem* Get(const UWorld* World);

	// ------------------------------------------------------------------
	// Menus
	// ------------------------------------------------------------------

	/**
	 * Open the crafting menu for an interactor at a station. Returns false when there is
	 * no local view to draw into (dedicated server) or the controller cannot be resolved.
	 */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|UI")
	bool OpenCraftingMenu(AActor* Interactor, AEclipseWorkstation* Workstation);

	/** Close whatever menu the interactor has open. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|UI")
	bool CloseMenu(AActor* Interactor);

	/** True when the interactor has a menu open. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|UI")
	bool IsMenuOpen(const AActor* Interactor) const;

	/** Station behind the interactor's open menu, or null. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|UI")
	AEclipseWorkstation* GetOpenStation(const AActor* Interactor) const;

	/** Close every menu. Called when the raid ends. */
	void CloseAllMenus();

	// ------------------------------------------------------------------
	// Prompts and notifications
	// ------------------------------------------------------------------

	/** Show (or clear, with an empty text) the interaction prompt for a player. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|UI")
	void SetInteractionPrompt(AActor* Interactor, const FText& Prompt);

	/** Prompt currently shown, or empty. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|UI")
	FText GetInteractionPrompt(const AActor* Interactor) const;

	/** Show a transient message to one player. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|UI")
	void ShowNotification(AActor* Interactor, const FText& Text, float DurationSeconds = 3.0f);

	/** Show a transient message to every player. Used for weather and raid events. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|UI")
	void BroadcastNotification(const FText& Text, float DurationSeconds = 3.0f);

	/** Notification currently shown for a player, or an inactive one. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|UI")
	FEclipseNotification GetNotification(const AActor* Interactor) const;

	/** Fired whenever a player's notification changes. */
	FEclipseNotificationChanged OnNotificationChanged;

	// ------------------------------------------------------------------
	// Widget classes
	// ------------------------------------------------------------------

	/** Widget created by OpenCraftingMenu. Set on a Blueprint subclass of this subsystem. */
	UPROPERTY(EditDefaultsOnly, Category = "Eclipse|UI")
	TSoftClassPtr<UEclipseCraftingMenuWidget> CraftingMenuWidgetClass;

protected:
	/** Find or create the entry for a controller. */
	FEclipsePlayerUIEntry& FindOrAddEntry(APlayerController* Controller);

	/** Find the entry for a controller, or null. */
	const FEclipsePlayerUIEntry* FindEntry(const APlayerController* Controller) const;

	/** Resolve the controller behind an actor (pawn, controller or component owner). */
	static APlayerController* ResolveController(const AActor* Actor);

	/** True when the controller has a local viewport to draw into. */
	static bool HasLocalView(const APlayerController* Controller);

	/** Create and register a menu widget, replacing any menu already open. */
	UEclipseMenuWidget* CreateMenuWidget(APlayerController* Controller, TSoftClassPtr<UEclipseMenuWidget> WidgetClass, AActor* OwnerActor);

	/** Remove a player's menu widget. */
	void DestroyMenu(FEclipsePlayerUIEntry& Entry);

	/** Push craft progress into the open crafting menu, if any. */
	void UpdateCraftingMenus();

	/** Drop entries whose controller has gone away. */
	void PruneEntries();

	/** Per-controller UI state. */
	UPROPERTY()
	TArray<FEclipsePlayerUIEntry> PlayerEntries;

	/** Cached crafting subsystem, resolved on first use. */
	TWeakObjectPtr<UEclipseCraftingSubsystem> CachedCrafting;
};
