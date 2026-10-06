# BUILD

## Requirements

| Tool | Version | Notes |
| --- | --- | --- |
| Unreal Engine | **5.6** | `Eclipse.uproject` sets `EngineAssociation: 5.6` |
| C++ standard | **C++20** | set in `Source/Eclipse/Eclipse.Build.cs` |
| Visual Studio | 2022, 17.8+ | Desktop C++ workload + Game development with C++ |
| Windows SDK | 10.0.22621+ | |
| Python | 3.10+ | for the three CI gates and the agent scripts |

Linux and macOS builds use the stock engine toolchain; nothing in the module is
platform-specific.

## Plugins

Enabled in `Eclipse.uproject`: Enhanced Input, Gameplay Abilities, Gameplay Tags, Chaos
Vehicles, Niagara, Metasound, PCG, Mass Entity, Common UI, Online Subsystem + Steam/EOS.

## Building

### Editor (the everyday target)

```bash
"$UE_ROOT/Engine/Build/BatchFiles/Windows/GenerateProjectFiles.bat" -project="$PWD/Eclipse.uproject" -game -engine
"$UE_ROOT/Engine/Build/BatchFiles/Windows/Build.bat" EclipseEditor Win64 Development -project="$PWD/Eclipse.uproject" -waitmutex
```

### Standalone game

```bash
"$UE_ROOT/Engine/Build/BatchFiles/Windows/Build.bat" Eclipse Win64 Development -project="$PWD/Eclipse.uproject"
```

### Dedicated server

```bash
"$UE_ROOT/Engine/Build/BatchFiles/Windows/Build.bat" EclipseServer Win64 Development -project="$PWD/Eclipse.uproject"
```

The dedicated server target exists from day one: it is what keeps the authority model
honest. If a system cannot run on a headless server, it is either display-only or wrong.

### Packaging

```bash
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun -project="$PWD/Eclipse.uproject" \
  -noP4 -platform=Win64 -clientconfig=Development -serverconfig=Development \
  -cook -build -stage -pak -archive -archivedirectory=Build/Windows
```

The vertical slice is cook-clean: every asset referenced by code is under
`/Game/Eclipse`, and `Content/Eclipse/Data/VerticalSlice` is imported through
`Tools/editor/import_vertical_slice.py` before the first cook.

## Importing the slice data

The authored content lives as JSON in `Content/Eclipse/Data/VerticalSlice` so it can be
reviewed, diffed and validated in CI. The editor script turns it into data assets:

```bash
"$UE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$PWD/Eclipse.uproject" \
  -run=pythonscript -script="Tools/editor/import_vertical_slice.py"
```

The importer resolves cross-references by asset path and writes a report to
`Saved/Logs/vertical_slice_import.txt`. Any unresolved reference is an error to fix before
committing assets; the report is the checklist.

## The three gates

Run these before every commit and before every packaging run:

```bash
python3 Tools/ci/lint_cpp.py
python3 -m unittest discover -s Tests/Reference -p "test_*.py"
python3 Tools/ci/validate_content.py --quiet
```

| Gate | Fails the build when |
| --- | --- |
| `lint_cpp.py` | a header lacks `#pragma once`, the `.generated.h` include is missing or not last, braces are unbalanced, a forbidden pattern (`LogTemp`, `TODO`, `using namespace`, `iostream`, screen debug, `system(`) appears, or a header has no matching source |
| `Tests/Reference` | any mirrored formula diverges from the reference model |
| `validate_content.py` | an id is duplicated, a cross-reference dangles, an enum value does not exist in C++, or a data file contradicts itself (errors); warnings flag soft gaps and can be promoted with `--strict` |

`scripts/publish.sh` runs all three, regenerates the agent briefs, verifies the manifest, and
exports the review bundle.

## Continuous integration

`.github/workflows/ci.yml` runs the three gates on every push and pull request, generates the
agent briefs, and asserts that exactly ten role files exist. It installs nothing: every gate
is standard-library Python, so the job is reproducible on any runner.

## Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| `Cannot open include file: 'Eclipse.generated.h'` | UHT has not run | generate project files, build from the IDE |
| `Unresolved external` on a subsystem | header exists, source missing | `lint_cpp.py` catches this before the compiler does |
| Import reports unresolved item ids | content JSON references a missing row | fix the JSON, re-run the validator, re-import |
| Data asset values look default | importer could not set a property | read the import report; the property name changed |
| Cook fails on a missing map | `DefaultMap` points outside the project | check `UEclipseDeveloperSettings::DefaultMap` |
