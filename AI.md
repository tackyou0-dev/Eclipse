# AI

The enemies in THE FALLEN BASIN are scavengers, security drones and one buried intelligence.
They are cheap to run, they coordinate in small groups, and they use the world (noise,
sight, weather, cover) rather than reading the player's position out of the air.

## The tiering rule

Every player has a distance to every agent. Instead of a full tick for all 64 agents, the
`UEclipseAISubsystem` refreshes a tier map every `AILowFrequencyTickInterval` (0.5 s):

| Distance to nearest player | Tier | AI controller tick |
| --- | --- | --- |
| ≤ `HighFrequencyRange` (4000 cm) | `High` | 0.1 s |
| ≤ `LowFrequencyRange` (16000 cm) | `Low` | 1.0 s |
| beyond | `Dormant` | 0 s (movement and brain suspended) |

`MaxActiveAIAgents` (64) caps what can be spawned in the first place; a spawner that would
exceed the cap holds its spawn request until a slot frees. Dormant agents keep their health
and position, and are woken by the subsystem, never by their own timer.

The multiplier is not a tuning knob in a vacuum: `HighFrequencyTickInterval` (0.1),
`LowFrequencyTickInterval` (1.0) and the two range constants are exposed in
`UEclipseDeveloperSettings` so a level can trade fidelity for frame time.

## Perception

`AEclipseAIController` sees what a player sees:

- **Sight** - a cone (`PeripheralVisionHalfAngleDegrees`, 75°) up to `SightRadius`
  (6000 cm), with a raycast to the target's chest and to the target's head. If the chest is
  blocked but the head is not, the enemy shoots at what it can see.
- **Hearing** - `ReportNoise(Location, Radius, Loudness)` from gunfire, sprinting,
  explosions and vehicles. Events are stored for `NoiseMemorySeconds` (6 s) in
  `FEclipseNoiseEvent` and pruned by the subsystem, so a noise the whole squad heard is
  investigated once, not N times.
- **Threat** - `ThreatScoreFor(Player)` is
  `500` for the player who made the noise, `100` for any other player,
  `- 0.01 * distance_cm`, `+ 150` if the player is the squad's focus. The highest score
  wins; ties break by the lower player id, which keeps the choice deterministic across
  server ticks.

Weather modifies all three: fog cuts sight radius, rain doubles the effective noise radius,
and a `VoidCreature` is only ever spawned at night (`bNightOnly`).

## Squads

`UEclipseSquadCoordinator` gives up to `MaxSquadMembers` (6) agents a shared blackboard:

- one `SquadFocus` target at a time, elected from the highest threat score;
- role assignment from the enemy definition's `SquadRole`: `Assault` closes, `Support`
  holds at range, `Flanker` tries to reach a side arc, `Sniper` refuses to engage below
  8000 cm;
- `LeashRadius` (6000 cm) from the squad anchor: an agent pulled past it disengages and
  walks back. Leashing is what stops a raid from turning into a train of 40 enemies.

Squad membership is by spawner pool, not by proximity: enemies spawned from the same
encounter coordinate, wandering wildlife does not.

## Spawning

`AEclipseEnemySpawner` owns everything: the pool, the respawn time, the per-biome and
per-weather weights, and the threat budget.

- **Budget** - a spawner spends `ThreatWeight` points against `ThreatBudget`. A boss costs
  its own weight; a pack of four costs four times a scavenger's.
- **Biome** - the spawner volume must match the enemy's `Biomes`.
- **Weather** - `WeatherSpawnModifiers` multiply the spawn weight; a `0.0` multiplier means
  "never" and is how the storms stay quiet.
- **Players** - a spawner only activates within `ActivationRadius` of a live player, and
  despawns nothing that is in combat: retreat is allowed, teleporting away is not.

`UEclipseAISubsystem::RequestSpawn` is the only way an agent enters the world. It checks the
global cap, the spawner's budget and the deterministic seed; the spawn location comes from
`FEclipseDeterministicRandom` seeded by `MixSeed(WorldSeed, SpawnerId)`, so a reloaded raid
finds the same scavengers in the same places.

## The Warden

`WardenPrime` is a scripted boss with four phases, each a real change in behaviour rather
than a stat multiplier:

| Phase | Health | Behaviour |
| --- | --- | --- |
| 1 | 100 % - 75 % | Charge, slam, single-target beams |
| 2 | 75 % - 50 % | Adds a rotating sweep; arena turrets activate |
| 3 | 50 % - 25 % | Shielded windows: damage is refused unless a weak point is hit |
| 4 | 25 % - 0 % | Storm-empowered: faster, `ArenaId` doors lock, enrage |

Phases advance on health thresholds, are replicated as `FEclipseBossState` on the game
state, and are announced through `OnBossPhaseChanged`. Weak points are `UEclipseWeakPoint`
components with their own damage multipliers; the phase-3 shield is implemented as a
resistance entry on the body, so the damage pipeline stays the only way damage is applied.

## Tuning checklist

Before changing any AI number:

1. Does the change alter what a player can observe? If not, it can wait.
2. Is the number in `UEclipseDeveloperSettings` or an enemy definition? If it is in a `Tick`,
   it is a bug.
3. Does the tick budget still hold at 64 agents, 4 players, one boss?
4. Does `Tests/Reference/test_reference_model.py` still pass, and does the C++ still match?
