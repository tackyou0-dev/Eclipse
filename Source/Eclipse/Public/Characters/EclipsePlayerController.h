// PROJECT ECLIPSE - Player controller.
//
// Purpose
//   Input plumbing and the player's view of death and revival. The controller owns the
//   Enhanced Input mapping context (which belongs to the local player, not to the pawn)
//   and the respawn request.
//
// Why respawn lives here
//   A downed player is revived by a squad mate through the character's interaction
//   interface. Respawn is the fallback for a solo player or a squad that cannot reach the
//   body, so it is a controller decision, not a character one.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EclipsePlayerController.generated.h"

class UInputMappingContext;

/**
 * Player controller for PROJECT ECLIPSE.
 *
 * The mapping context is added on possess and removed on unpossess so that a spectator or
 * a menu never has gameplay input bound.
 */
UCLASS()
class ECLIPSE_API AEclipsePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AEclipsePlayerController();

	/** AActor: add the mapping context for the local player. */
	virtual void BeginPlay() override;

	/** AActor: remove the mapping context again. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** APlayerController: bind the input context to the new pawn. */
	virtual void OnPossess(APawn* InPawn) override;

	/** Ask the server to respawn this player at a player start. */
	UFUNCTION(BlueprintCallable, Category = "Eclipse|Player")
	void RequestRespawn();

	/** True when this player is waiting to respawn. */
	UFUNCTION(BlueprintPure, Category = "Eclipse|Player")
	bool IsAwaitingRespawn() const { return bAwaitingRespawn; }

	/** Fraction of maximum health a respawn grants. */
	static constexpr float RespawnHealthFraction = 0.5f;

protected:
	/** Server RPC behind RequestRespawn. */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRequestRespawn();

	/** Add the gameplay mapping context to the local player. */
	void AddGameplayMappingContext();

	/** Remove the gameplay mapping context. */
	void RemoveGameplayMappingContext();

	/** Mapping context registered while a raid character is possessed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input")
	TObjectPtr<UInputMappingContext> GameplayMappingContext;

	/** Priority of the gameplay mapping context. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eclipse|Input")
	int32 GameplayMappingPriority = 0;

private:
	/** True between death and respawn. */
	bool bAwaitingRespawn = false;
};
