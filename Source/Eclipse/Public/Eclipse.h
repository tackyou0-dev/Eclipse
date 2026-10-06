// PROJECT ECLIPSE - Eclipse module public header.
//
// Purpose
//   Declares the module's exported helpers. Gameplay code should not need to include
//   this header; it exists so tools and tests can query build-identifying information
//   without pulling in the whole module.

#pragma once

#include "CoreMinimal.h"
#include "Core/EclipseLog.h"
#include "Modules/ModuleManager.h"

/**
 * Module-wide helpers for PROJECT ECLIPSE.
 *
 * The module is a single runtime module named "Eclipse". Feature folders live directly
 * underneath Source/Eclipse: Core, Characters, Combat, Weapons, Inventory, Loot,
 * Crafting, Economy, Factions, Progression, AI, Missions, World, Vehicles, Multiplayer,
 * Security, SaveSystem, UI, Audio, Analytics, Tools and Tests.
 */
namespace EclipseModule
{
	/** Human-readable project version, mirrored from the project settings. */
	ECLIPSE_API const TCHAR* GetProjectVersion();

	/** The save schema version this build writes. Must match UEclipseSaveGame. */
	ECLIPSE_API int32 GetSaveSchemaVersion();
}
