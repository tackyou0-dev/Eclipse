// PROJECT ECLIPSE - Log categories.
//
// Purpose
//   One category per subsystem group. Logging with a category (rather than LogTemp) is
//   enforced by review and by Tools/ci/lint_cpp.py, because it is the only way to filter
//   a multiplayer shakedown log down to the system under investigation.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/** General gameplay: core flow, game mode, game state. Defined in Eclipse.cpp. */
DECLARE_LOG_CATEGORY_EXTERN(LogEclipse, Log, All);

/** AI, squads, spawn director, navigation. */
DECLARE_LOG_CATEGORY_EXTERN(LogEclipseAI, Log, All);

/** Damage, weapons, projectiles, status effects. */
DECLARE_LOG_CATEGORY_EXTERN(LogEclipseCombat, Log, All);

/** Inventory, loot, crafting and economy transactions. */
DECLARE_LOG_CATEGORY_EXTERN(LogEclipseItems, Log, All);

/** World simulation: time of day, weather, biomes, procedural generation. */
DECLARE_LOG_CATEGORY_EXTERN(LogEclipseWorld, Log, All);

/** Missions, objectives and progression. */
DECLARE_LOG_CATEGORY_EXTERN(LogEclipseMissions, Log, All);

/** Networking, replication and session management. */
DECLARE_LOG_CATEGORY_EXTERN(LogEclipseNet, Log, All);

/** Server-side validation and anti-cheat. */
DECLARE_LOG_CATEGORY_EXTERN(LogEclipseSecurity, Log, All);

/** Save and load pipeline. */
DECLARE_LOG_CATEGORY_EXTERN(LogEclipseSave, Log, All);

/** UI, HUD, audio and analytics. */
DECLARE_LOG_CATEGORY_EXTERN(LogEclipsePresentation, Log, All);

/** Content validation and tooling (editor and CI). */
DECLARE_LOG_CATEGORY_EXTERN(LogEclipseTools, Log, All);
