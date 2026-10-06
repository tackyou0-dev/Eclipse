# Vertical Slice Content Data

The content for **THE FALLEN BASIN** is authored here as JSON that mirrors the C++ data
asset schemas. This is the single source of truth for the slice's balance; it is validated
in CI by `Tools/ci/validate_content.py` on any machine, so content errors are caught long
before anyone opens the editor.

## Why JSON and not just .uasset

Binary `.uasset` files cannot be diffed, reviewed or validated without the engine. This
JSON layer gives the team reviewable, version-controlled balance that CI can check against
the actual enums parsed from `Source/Eclipse/Core/EclipseTypes.h`. A small importer (to be
added in Phase 18) converts these into data assets in `/Game/Eclipse/Data/`.

## Files

| File | Mirrors | Contents |
|------|---------|----------|
| `weapons.json` | `UEclipseWeaponDefinition`, `FEclipseWeaponAttachment` | AR-01, VX-9, M-12, SR-77 and 13 attachments. |
| `items.json` | `UEclipseItemDefinition` | Ammo, medical, consumables, armour, resources, technology, artifacts, quest items, weapon and attachment items. |
| `loot.json` | `UEclipseLootTable` | Nine tables, one per enemy tier, supply cache, military crate, relic vault and the boss. |
| `missions.json` | `UEclipseMissionDefinition` | First Contact, Lost Convoy, Black Signal, Broken Relay, The Warden. |
| `enemies.json` | Enemy + boss definitions | Six enemies and the WARDEN PRIME four-phase boss. |
| `crafting.json` | `UEclipseRecipeDefinition` | Ten recipes across the workshop, armory, laboratory and medical bay. |
| `skills.json` | `UEclipseSkillDefinition` | Seventeen skills across the five trees, with real behavioural effects. |
| `factions.json` | `UEclipseFactionDefinition` | The four named factions plus wildlife, with pricing and thresholds. |

## Validating

```bash
python3 Tools/ci/validate_content.py
```

The validator checks:

- Every enum value matches the real `EEclipse*` enum in the headers.
- Cross-references resolve: weapon ammo, loot entries, recipe ingredients and outputs,
  mission rewards and objectives, enemy loot tables, skill prerequisites and unlocked
  recipes/modules, boss loot tables.
- Numeric ranges: positive weights, valid count ranges, ascending faction thresholds,
  descending boss phase thresholds, valid mission time windows.
- Skill prerequisites form no cycle.

Faction asymmetry (a predator lists everyone as an enemy but nobody lists the predator)
is reported as a warning, not an error — the C++ subsystem resolves hostility
symmetrically, so a one-sided declaration is intentional for wildlife.

## Authoring rules

- Ids are stable and dotted (`Ammo.556`, `Weapon.AR01`, `Mission.TheWarden`). Changing an
  id breaks saves and references.
- Never invent an enum value. Check `Source/Eclipse/Core/EclipseTypes.h` first; the
  validator will reject it.
- Every item an enemy, recipe or mission references must exist in `items.json`.
- Keep balance here, not in code. If a value needs to be tuned per session, it belongs in
  a data asset, not a constant.
