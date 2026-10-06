# PROJECT ECLIPSE

A third- and first-person sci-fi survival action RPG built in Unreal Engine 5.6 / C++20.
The vertical slice is **THE FALLEN BASIN**: a four-player co-op raid on the terraforming
world ECLIPSE-7, where a failed colony, a buried orbital defence intelligence and a
planetary storm season are all trying to kill you.

This repository is the game, not a prototype of the game: every system in it is the system
that ships in the slice. There are no stubbed functions, no placeholder systems and no
"coming soon" panels.

---

## The slice in one paragraph

You drop into the Fallen Basin with a rifle, a medkit and no support. The world runs a real
clock and a real weather system; storms change what can see what, and hazardous weather will
kill you if you are caught in the open. You loot, craft, trade with whoever is still alive,
and work a five-mission chain that ends in the Warden's arena. You can bring three friends,
and the raid is server-authoritative, so what happens on the host's machine is what happens.

## Repository layout

| Path | What lives there |
| --- | --- |
| `Source/Eclipse/Public`, `Private` | The single runtime module, one folder per feature |
| `Content/Eclipse/Data/VerticalSlice` | The authored slice data: items, weapons, loot, crafting, skills, factions, enemies, missions |
| `Tests/Reference` | The engine-free reference model and its tests (mirrors the C++ arithmetic) |
| `Tools/ci` | The three CI gates: C++ lint, engine-free tests, content validation |
| `Tools/editor` | The in-editor importer that turns the slice JSON into data assets |
| `scripts` | Agent brief generator, deployment manifest, release script |
| `.agents/agents` | The ten agent role briefs this project is built with |
| `Documentation` | Decision records and the long-form agent rules |

## Feature folders

`Core` (types, settings, game framework, save, security) · `Characters` · `Combat` ·
`Weapons` · `Inventory` · `Loot` · `Crafting` · `Economy` · `Factions` · `Progression` ·
`AI` · `Missions` · `World` · `Vehicles` · `Multiplayer` · `UI` · `Audio` · `Analytics` ·
`Tools` · `Tests`

## Building

Requires Unreal Engine 5.6 with the C++ toolchain for your platform.

```bash
# Generate project files, then build the editor target
UnrealBuildTool EclipseEditor Win64 Development -project="$PWD/Eclipse.uproject"
```

See `BUILD.md` for the full build matrix (editor, game, dedicated server) and the packaging
steps.

## The three gates

CI proves what can be proven without an engine. Run all three before every commit:

```bash
python3 Tools/ci/lint_cpp.py
python3 -m unittest discover -s Tests/Reference -p "test_*.py"
python3 Tools/ci/validate_content.py --quiet
```

Expected output:

```text
lint_cpp: 66 headers, 57 sources, 0 errors, 0 warnings.
Ran 33 tests in ...  OK
107 content entries across 8 data files, 0 errors, 4 warnings.
```

The four content warnings are deliberate: they flag soft data gaps (an enemy without weak
points, an enemy that ignores the weather, missions that do not move faction standing,
recipes that need no skill) that are acceptable to ship but worth seeing.

The engine-side automation tests (`Eclipse.Core.*`, `Eclipse.Combat.*`, `Eclipse.World.*`)
run in the editor and assert the same arithmetic against the C++ implementation.

## How the project is run

The work is organised as ten specialist agents, each with a written brief in
`.agents/agents`. `AGENTS.md` is the contract they all follow; `Documentation/AGENTS.md`
carries the long-form version, and `Documentation/DECISIONS.md` records why the architecture
looks the way it does.

```bash
python3 scripts/generate_agents.py      # regenerate the ten role files
python3 scripts/deploy_subagents.py     # verify the briefs and write the manifest
```

## Documentation map

| Document | Contents |
| --- | --- |
| `AGENTS.md` | The rules every agent follows, and the definition of done |
| `ARCHITECTURE.md` | Module layout, system contracts, authority and determinism |
| `GAMEPLAY.md` | The slice's loop, systems and tuning numbers |
| `AI.md` | Tiering, squads, spawning, the Warden |
| `NETWORKING.md` | Replication model, session flow, validation |
| `BUILD.md` | Building, packaging, dedicated server |
| `TESTING.md` | Test layers and the divergence policy |
| `PERFORMANCE.md` | Budgets and the systems that defend them |
| `PUBLISHING.md` | Release checklist and the credential requirements |
| `ROADMAP.md` | What ships in the slice, and what comes after |

## Licence

Apache 2.0. See `LICENSE`.
