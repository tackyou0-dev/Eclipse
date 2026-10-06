# ARCHITECTURE

PROJECT ECLIPSE is one runtime module (`Eclipse`) with one folder per feature. The shape is
deliberate: while the vertical slice is being built, a plugin boundary would add build
friction without buying isolation, and a "core" module that everything depends on would
become the place where unrelated systems meet.

## Module layout

```text
Source/Eclipse
  Eclipse.Build.cs          one runtime module, C++20, UE 5.6
  Public/<Feature>/         the module's API (what other features may include)
  Private/<Feature>/        implementation
```

| Folder | Responsibility | Depends on (features) |
| --- | --- | --- |
| `Core` | Types, developer settings, game framework, save, security | - |
| `Characters` | Pawn classes, movement, player controller | Core, Combat, Weapons, Inventory, Progression |
| `Combat` | Damage pipeline, health, weak points, projectiles | Core |
| `Weapons` | Weapon/attachment definitions, firing, reload, recoil | Core, Combat, Inventory |
| `Inventory` | Items, stacks, equipment, pickups | Core |
| `Loot` | Loot tables, deterministic rolling, world drops | Core, Inventory |
| `Crafting` | Recipes, stations, craft sessions | Core, Inventory, Progression |
| `Economy` | Vendors, pricing, transactions | Core, Inventory, Factions |
| `Factions` | Reputation, relations, standings | Core |
| `Progression` | XP curve, skill trees, stat modifiers | Core |
| `AI` | Enemy definitions, spawning, squads, controller | Core, Combat, World |
| `Missions` | Mission chain, objectives, rewards | Core, Factions, World |
| `World` | Clock, weather, biomes, environmental damage | Core |
| `Vehicles` | The rover: fuel, cargo, seats | Core, Inventory, Combat |
| `Multiplayer` | Session hosting/joining/travel | Core |
| `Security` | Trust, rate limits, movement validation | Core |
| `UI` | Menu state, prompts, notifications, craft progress | Core, Crafting |
| `Audio` | Audio state by weather/hour, one-shots | Core, World |
| `Analytics` | Telemetry buffer, raid summary, provider interface | Core |
| `SaveSystem` | Save file, records, migrations, autosave | Core, World, Factions |
| `Tools` | Console commands for content validation | Core |
| `Tests` | In-engine automation tests | everything they assert |

Dependencies only ever point *down* that table. A feature that needs something from above it
receives it through a delegate or an interface, not by including the header.

## The five contracts

Everything a player does crosses one of these, and each has exactly one entry point. Two
paths to the same state is how a co-op game ends up disagreeing with itself.

1. **Damage** - `UEclipseDamageLibrary::ApplyDamage`. Everything that hurts anything goes
   through it: bullets, explosions, weather, fall damage, the void. It computes
   `Base * Falloff * WeakPoint * Critical * (1 - ArmorMitigation) * Resistance`, applies
   shields first, then health, and returns a `FEclipseDamageResult`.
2. **Interaction** - `IEclipseInteractable`. Containers, doors, terminals, vendors,
   workstations, downed players. `CanInteract` and the prompt are queried on both sides for
   the HUD; `Interact` only ever runs on the server.
3. **Targeting** - `IEclipseTeamAgent`. `GetFaction()` is the whole API: it is what makes
   "is this a valid target" one question with one answer.
4. **Saving** - `IEclipseSaveable`. A system writes exactly one `FEclipseSaveRecord` and
   reads exactly one back. The save service owns the file; systems own their own bytes.
5. **Telemetry** - `IEclipseAnalyticsProvider`. The analytic subsystem buffers events and
   ships them; a build with no provider still fills the raid summary the results screen
   reads.

## Authority model

| State | Owner | Replication |
| --- | --- | --- |
| World clock, weather | `UEclipseWorldSubsystem` (server) | `AEclipseGameState` properties + RepNotify |
| Player health, inventory, equipment | Server components | Replicated with RepNotify where UI cares |
| Missions, objectives | `UEclipseMissionSubsystem` (server) | Active mission ids on the game state |
| Boss phase | `AEclipseEnemyCharacter` (server) | `FEclipseBossState` on the game state |
| Menus, prompts | Both (display only) | Client-local, driven by server state |
| Currency, reputation | `UEclipseGameInstance` / `UEclipseFactionSubsystem` (server) | Replicated totals |

Clients send **intents** through server RPCs (`ServerRequestCraft`, `ServerCancelCraft`,
`ServerInteract`, `ServerRequestRespawn`). Every server handler re-validates: range,
ownership, cost, rate, and plausibility. `UEclipseSecuritySubsystem` keeps a trust score per
player and rate-limits actions; `UEclipseCharacterMovementComponent::ServerCheckClientError`
routes the engine's own movement correction hook through the same service.

## Determinism

Determinism is a feature, not a testing convenience: the same raid seed must produce the
same basin, the same containers and the same weather.

- `FEclipseDeterministicRandom` is an LCG (1664525/1013904223) followed by a three-round
  xorshift-multiply finaliser. Seed 0 is remapped to `0x9E3779B9`.
- Anything per-object uses `MixSeed(WorldSeed, StableId)` where `StableId` comes from
  `StableIdFromString` (FNV-1a). Container contents, spawner selection and placement all
  follow this rule.
- Algorithms that iterate a `TMap` are wrong. Rarity rolls, weather selection and loot
  entries iterate enum order or an explicitly sorted array.
- `Tests/Reference/eclipse_reference_model.py` reimplements the arithmetic in Python; the
  tests in that folder prove the contract holds without an engine, and the in-engine
  automation tests prove the C++ matches it.

## State and lifecycle

```text
GameInstance            outlives levels: profile, credits, raid lifecycle, session
  FactionSubsystem      reputation, relations
  SaveSubsystem         slots, autosave, record round-trip
  SecuritySubsystem     trust, rate limits
  AnalyticsSubsystem    telemetry buffer, raid summary
  SessionSubsystem      hosting, joining, travel

GameMode (per level)    admits players, applies raid settings, ends the raid
GameState (per level)   replicated world/weather/mission/boss snapshot
WorldSubsystem          clock, weather, biomes, environmental damage
```

Subsystems are used instead of singletons or manager actors because they are created,
ticked and destroyed with the thing they belong to. There is no global `GetGameManager()`
anywhere in the codebase.

## Tick budget

Everything that ticks has a reason and a frequency:

| System | Frequency | Why |
| --- | --- | --- |
| AI controller | 0.1 s near players, 1.0 s at range, 0 s dormant | distance tiering |
| AI subsystem | 0.5 s | tier refresh + noise pruning |
| World subsystem | every frame (cheap) | clock, weather transition, damage accumulator |
| Audio subsystem | 0.5 s | state resolution only |
| UI subsystem | every frame | notification expiry, craft progress |
| Health component | self-disabling | shield regen stops when full |

See `PERFORMANCE.md` for the budgets these exist to protect.

## Extension points

- A new enemy: add a `UEclipseEnemyDefinition` asset and put it in a spawner's pool. No code.
- A new weapon: a `UEclipseWeaponDefinition` plus an item row; the pipeline handles the rest.
- A new objective type: add the enum value, handle it in `TickTimedObjectives` or
  `ReportProgress`, and add the JSON row.
- A new damage school: add the enum value, a resistance entry, and the name mapping in
  `EclipseEnumNames`. The pipeline needs no change.

If an extension needs a change in three unrelated folders, the contract is wrong: fix the
contract first.
