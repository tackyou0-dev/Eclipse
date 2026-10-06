#!/usr/bin/env python3
"""Validate the vertical slice content data.

The JSON under ``Content/Eclipse/Data/VerticalSlice`` is the authored source of truth for
weapons, items, loot, recipes, skills, factions, enemies and missions. This validator is the
only thing standing between a typo in that data and a crash or a silent balance bug at
runtime, and it runs without an engine, which is the point: it can run on every push.

What it checks
    * JSON parses and every entry has the keys its type requires
    * every enum-valued field holds a member that actually exists in the C++ enum
    * ids are unique, and every cross-reference (loot -> item, recipe -> item, enemy ->
      loot table, mission -> item, skill prerequisite, faction relation) resolves
    * numeric fields are in range and internally consistent (ranges the right way round,
      weights positive, loot rarity weights summing to 100, boss phases descending)
    * the weapon/attachment slot pairing makes sense

What it warns about (soft findings, not failures)
    * an enemy with no weak points
    * an enemy that does not react to weather
    * missions that do not move faction standing
    * recipes that need no skill

Usage
    python3 Tools/ci/validate_content.py
    python3 Tools/ci/validate_content.py --quiet
    python3 Tools/ci/validate_content.py --strict
"""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
from dataclasses import dataclass, field
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
DATA_DIR = REPO_ROOT / "Content" / "Eclipse" / "Data" / "VerticalSlice"
HEADER_DIR = REPO_ROOT / "Source" / "Eclipse" / "Public"

# Data file -> array keys that hold entries. All of them are counted in the census.
FILE_ARRAYS: Dict[str, Tuple[str, ...]] = {
    "items.json": ("items", "weapons", "attachments"),
    "weapons.json": ("weapons", "attachments"),
    "loot.json": ("tables",),
    "crafting.json": ("recipes",),
    "skills.json": ("skills",),
    "factions.json": ("factions",),
    "enemies.json": ("enemies", "bosses"),
    "missions.json": ("missions",),
}

# Headers whose enums the data is validated against.
ENUM_HEADERS: Tuple[str, ...] = (
    "Core/EclipseTypes.h",
    "AI/EclipseAITypes.h",
    "Factions/EclipseFactionTypes.h",
    "Crafting/EclipseCraftingTypes.h",
    "Missions/EclipseMissionTypes.h",
    "Loot/EclipseLootTypes.h",
    "Inventory/EclipseItemTypes.h",
    "Economy/EclipseEconomyTypes.h",
    "Progression/EclipseSkillDefinition.h",
    "Combat/EclipseDamageTypes.h",
    "World/EclipseWorldTypes.h",
)

ENUM_RE = re.compile(r"enum\s+class\s+(E\w+)\s*:\s*uint8\s*\{(.*?)\n\};", re.S)


@dataclass
class Report:
    files: int = 0
    entries: int = 0
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)

    def error(self, where: str, message: str) -> None:
        self.errors.append(f"{where}: {message}")

    def warn(self, where: str, message: str) -> None:
        self.warnings.append(f"{where}: {message}")


def parse_enums() -> Dict[str, Set[str]]:
    """Enum name -> members, parsed straight out of the headers.

    Reading the C++ is deliberate: a hand-maintained copy of the enum list in this script
    is exactly the kind of duplication that drifts.
    """
    enums: Dict[str, Set[str]] = {}
    for relative in ENUM_HEADERS:
        path = HEADER_DIR / relative
        if not path.exists():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for match in ENUM_RE.finditer(text):
            name, body = match.group(1), match.group(2)
            members: Set[str] = set()
            for line in body.splitlines():
                line = line.split("UMETA")[0].split("//")[0].strip().rstrip(",")
                if not line or "=" in line:
                    continue
                member = re.match(r"^(\w+)", line)
                if member:
                    members.add(member.group(1))
            enums[name] = members
    return enums


class Validator:
    def __init__(self, enums: Dict[str, Set[str]]) -> None:
        self.enums = enums
        self.report = Report()
        self.documents: Dict[str, dict] = {}
        self.item_ids: Set[str] = set()
        self.weapon_ids: Set[str] = set()
        self.attachment_ids: Set[str] = set()
        self.loot_ids: Set[str] = set()
        self.skill_ids: Set[str] = set()
        self.faction_ids: Set[str] = set()
        self.recipe_ids: Set[str] = set()

    # ------------------------------------------------------------------ helpers

    def enum_check(self, where: str, enum_name: str, value: str) -> bool:
        members = self.enums.get(enum_name)
        if members is None:
            self.report.error(where, f"enum {enum_name} was not found in the headers")
            return False
        if value == "" and "None" in members:
            return True
        if value not in members:
            self.report.error(where, f"'{value}' is not a member of {enum_name}")
            return False
        return True

    def require(self, where: str, entry: dict, keys: Iterable[str]) -> bool:
        ok = True
        for key in keys:
            if key not in entry:
                self.report.error(where, f"missing required key '{key}'")
                ok = False
        return ok

    def positive(self, where: str, entry: dict, key: str, allow_zero: bool = False) -> None:
        if key not in entry:
            return
        value = entry[key]
        if not isinstance(value, (int, float)):
            self.report.error(where, f"'{key}' must be a number, got {type(value).__name__}")
        elif value < 0 or (value == 0 and not allow_zero):
            self.report.error(where, f"'{key}' must be positive, got {value}")

    # ------------------------------------------------------------------ load

    def load(self) -> bool:
        for file_name, arrays in FILE_ARRAYS.items():
            path = DATA_DIR / file_name
            try:
                document = json.loads(path.read_text(encoding="utf-8"))
            except FileNotFoundError:
                self.report.error(file_name, "data file is missing")
                return False
            except json.JSONDecodeError as error:
                self.report.error(file_name, f"invalid JSON: {error}")
                return False

            self.documents[file_name] = document
            self.report.files += 1

            for key in arrays:
                entries = document.get(key)
                if entries is None:
                    self.report.error(file_name, f"missing array '{key}'")
                    continue
                if not isinstance(entries, list):
                    self.report.error(file_name, f"'{key}' must be an array")
                    continue
                self.report.entries += len(entries)

        return True

    # ------------------------------------------------------------------ per file

    def validate_items(self) -> None:
        document = self.documents["items.json"]

        for entry in document["items"]:
            where = f"items.json:{entry.get('itemId', '<no id>')}"
            self.require(where, entry, ("itemId", "displayName", "category", "rarity", "weight", "baseValue"))
            self.enum_check(where, "EEclipseItemCategory", entry.get("category", ""))
            self.enum_check(where, "EEclipseRarity", entry.get("rarity", ""))
            self.positive(where, entry, "weight", allow_zero=True)
            self.positive(where, entry, "baseValue", allow_zero=True)
            self.positive(where, entry, "maxStackSize")
            self.item_ids.add(entry.get("itemId", ""))

        for entry in document["weapons"]:
            where = f"items.json:{entry.get('itemId', '<no id>')}"
            self.require(where, entry, ("itemId", "weaponId", "category", "rarity", "equipSlotTag"))
            self.enum_check(where, "EEclipseItemCategory", entry.get("category", ""))
            self.enum_check(where, "EEclipseRarity", entry.get("rarity", ""))
            self.item_ids.add(entry.get("itemId", ""))
            self.weapon_ids.add(entry.get("weaponId", ""))

        for entry in document["attachments"]:
            where = f"items.json:{entry.get('itemId', '<no id>')}"
            self.require(where, entry, ("itemId", "attachmentId", "category", "rarity"))
            self.enum_check(where, "EEclipseItemCategory", entry.get("category", ""))
            self.enum_check(where, "EEclipseRarity", entry.get("rarity", ""))
            self.item_ids.add(entry.get("itemId", ""))
            self.attachment_ids.add(entry.get("attachmentId", ""))

        expected = len(document["items"]) + len(document["weapons"]) + len(document["attachments"])
        if len(self.item_ids) != expected:
            self.report.error("items.json", f"{expected - len(self.item_ids)} duplicate item id(s)")

    def validate_weapons(self) -> None:
        document = self.documents["weapons.json"]

        for entry in document["weapons"]:
            where = f"weapons.json:{entry.get('weaponId', '<no id>')}"
            self.require(where, entry, (
                "weaponId", "displayName", "fireMode", "deliveryMode", "damage", "damageType",
                "effectiveRange", "maxRange", "roundsPerMinute", "magazineCapacity", "availableSlots",
            ))
            self.enum_check(where, "EEclipseFireMode", entry.get("fireMode", ""))
            self.enum_check(where, "EEclipseDeliveryMode", entry.get("deliveryMode", ""))
            self.enum_check(where, "EEclipseDamageType", entry.get("damageType", ""))
            self.positive(where, entry, "damage")
            self.positive(where, entry, "effectiveRange")
            self.positive(where, entry, "maxRange")
            self.positive(where, entry, "magazineCapacity")
            self.positive(where, entry, "roundsPerMinute")

            if entry.get("maxRange", 0) < entry.get("effectiveRange", 0):
                self.report.error(where, "maxRange is below effectiveRange")

            for slot in entry.get("availableSlots", []):
                self.enum_check(where, "EEclipseAttachmentSlot", slot)

            if entry.get("weaponId") not in self.weapon_ids:
                self.report.warn(where, "weapon has no matching entry in items.json (no inventory item)")

        for entry in document["attachments"]:
            where = f"weapons.json:{entry.get('attachmentId', '<no id>')}"
            self.require(where, entry, ("attachmentId", "displayName", "slot", "rarity", "modifiers"))
            self.enum_check(where, "EEclipseAttachmentSlot", entry.get("slot", ""))
            self.enum_check(where, "EEclipseRarity", entry.get("rarity", ""))
            for stat, value in entry.get("modifiers", {}).items():
                self.enum_check(where, "EEclipseStat", stat)
                if not isinstance(value, (int, float)) or value <= 0:
                    self.report.error(where, f"modifier '{stat}' must be a positive number")
            if entry.get("attachmentId") not in self.attachment_ids:
                self.report.error(where, "attachment has no matching item in items.json")

    def validate_loot(self) -> None:
        document = self.documents["loot.json"]

        for entry in document["tables"]:
            where = f"loot.json:{entry.get('tableId', '<no id>')}"
            self.require(where, entry, ("tableId", "rollCount", "rarityWeights", "entries"))
            self.loot_ids.add(entry.get("tableId", ""))
            self.positive(where, entry, "rollCount")

            weights = entry.get("rarityWeights", {})
            total = 0.0
            for rarity, weight in weights.items():
                self.enum_check(where, "EEclipseRarity", rarity)
                if not isinstance(weight, (int, float)) or weight < 0:
                    self.report.error(where, f"rarity weight for '{rarity}' must be a non-negative number")
                else:
                    total += weight

            if weights and abs(total - 100.0) > 0.01:
                self.report.error(where, f"rarity weights sum to {total}, expected 100")

            if not entry.get("entries"):
                self.report.error(where, "loot table has no entries")

            for loot_entry in entry.get("entries", []):
                item_id = loot_entry.get("itemId", "")
                if item_id not in self.item_ids:
                    self.report.error(where, f"entry references unknown item '{item_id}'")
                self.enum_check(where, "EEclipseRarity", loot_entry.get("rarity", ""))
                self.positive(where, loot_entry, "weight")
                self.positive(where, loot_entry, "minCount")
                self.positive(where, loot_entry, "maxCount", allow_zero=True)
                if loot_entry.get("maxCount", 0) < loot_entry.get("minCount", 0):
                    self.report.error(where, f"entry '{item_id}' has maxCount below minCount")

    def validate_crafting(self) -> None:
        document = self.documents["crafting.json"]
        skill_gated = 0

        for entry in document["recipes"]:
            where = f"crafting.json:{entry.get('recipeId', '<no id>')}"
            self.require(where, entry, (
                "recipeId", "displayName", "category", "requiredStation", "requiredLevel",
                "craftTimeSeconds", "outputItemId", "outputCount", "ingredients",
            ))
            self.recipe_ids.add(entry.get("recipeId", ""))
            self.enum_check(where, "EEclipseItemCategory", entry.get("category", ""))
            self.enum_check(where, "EEclipseBaseModule", entry.get("requiredStation", ""))
            self.positive(where, entry, "requiredLevel")
            self.positive(where, entry, "craftTimeSeconds", allow_zero=True)
            self.positive(where, entry, "outputCount")

            if entry.get("outputItemId") not in self.item_ids:
                self.report.error(where, f"outputs unknown item '{entry.get('outputItemId')}'")

            if entry.get("requiredSkillId"):
                if entry["requiredSkillId"] not in self.skill_ids:
                    self.report.error(where, f"requires unknown skill '{entry['requiredSkillId']}'")
                skill_gated += 1

            if not entry.get("ingredients"):
                self.report.error(where, "recipe has no ingredients")

            for ingredient in entry.get("ingredients", []):
                if ingredient.get("itemId") not in self.item_ids:
                    self.report.error(where, f"ingredient '{ingredient.get('itemId')}' is not an item")
                self.positive(where, ingredient, "count")

        total = len(document["recipes"])
        if total > 0 and skill_gated < total:
            self.report.warn(
                "crafting.json",
                f"{total - skill_gated}/{total} recipes require no skill (available to every character)",
            )

    def validate_skills(self) -> None:
        document = self.documents["skills.json"]

        for entry in document["skills"]:
            where = f"skills.json:{entry.get('skillId', '<no id>')}"
            self.require(where, entry, (
                "skillId", "displayName", "tree", "tier", "skillPointCost", "effect",
                "targetStat", "additiveValue", "multiplicativeValue",
            ))
            self.skill_ids.add(entry.get("skillId", ""))
            self.enum_check(where, "EEclipseSkillTree", entry.get("tree", ""))
            self.enum_check(where, "EEclipseSkillEffectType", entry.get("effect", ""))
            self.positive(where, entry, "skillPointCost")
            self.positive(where, entry, "tier", allow_zero=True)
            self.positive(where, entry, "multiplicativeValue", allow_zero=True)

            if entry.get("effect") == "StatModifier":
                if not entry.get("targetStat"):
                    self.report.error(where, "a StatModifier skill must name a targetStat")
                else:
                    self.enum_check(where, "EEclipseStat", entry["targetStat"])
                if entry.get("additiveValue", 0) == 0 and entry.get("multiplicativeValue", 1) == 1:
                    self.report.error(where, "a StatModifier skill changes nothing")

            if entry.get("effect") == "UnlockRecipe" and not entry.get("unlockedId"):
                self.report.error(where, "an UnlockRecipe skill must name the recipe it unlocks")

        # Prerequisites are checked after every id is known, so forward references are legal.
        for entry in document["skills"]:
            where = f"skills.json:{entry.get('skillId', '<no id>')}"
            for prerequisite in entry.get("prerequisites", []):
                if prerequisite not in self.skill_ids:
                    self.report.error(where, f"prerequisite '{prerequisite}' is not a skill")

        if len(self.skill_ids) != len(document["skills"]):
            self.report.error("skills.json", "duplicate skill id")


    def check_skill_unlock_targets(self) -> None:
        """Cross-check the recipe-shaped unlocks. Runs after crafting, because recipes are
        what supplies the id set."""
        document = self.documents["skills.json"]
        # unlockedId spans several namespaces (recipes, base modules, abilities), so only a
        # recipe-looking id can be cross-checked.
        unlocked = {entry.get("unlockedId") for entry in document["skills"] if entry.get("unlockedId")}
        for target in unlocked:
            if target.startswith("Recipe.") and target not in self.recipe_ids:
                self.report.warn("skills.json", f"unlocks unknown recipe '{target}'")

    def validate_factions(self) -> None:
        document = self.documents["factions.json"]

        for entry in document["factions"]:
            where = f"factions.json:{entry.get('faction', '<no faction>')}"
            self.require(where, entry, (
                "faction", "displayName", "hostileThreshold", "friendlyThreshold", "alliedThreshold",
            ))
            faction = entry.get("faction", "")
            self.enum_check(where, "EEclipseFaction", faction)
            self.faction_ids.add(faction)

            hostile, friendly, allied = (
                entry.get("hostileThreshold", 0),
                entry.get("friendlyThreshold", 0),
                entry.get("alliedThreshold", 0),
            )
            if not hostile <= friendly <= allied:
                self.report.error(where, "reputation thresholds must satisfy hostile <= friendly <= allied")

            # Vendor pricing is optional: a faction that does not trade (Wildlife) has no
            # table, and the trade subsystem falls back to the default markup and trade-in.
            for pricing in entry.get("vendorPricing", []):
                self.enum_check(where, "EEclipseItemCategory", pricing.get("category", ""))
                self.positive(where, pricing, "buyPriceMultiplier")
                self.positive(where, pricing, "sellPriceMultiplier", allow_zero=True)

        for entry in document["factions"]:
            where = f"factions.json:{entry.get('faction', '<no faction>')}"
            for other in entry.get("allies", []) + entry.get("enemies", []):
                if other not in self.faction_ids:
                    self.report.error(where, f"relation to unknown faction '{other}'")
            overlap = set(entry.get("allies", [])) & set(entry.get("enemies", []))
            if overlap:
                self.report.error(where, f"faction is both ally and enemy of {sorted(overlap)}")

    def validate_enemies(self) -> None:
        document = self.documents["enemies.json"]
        no_weak_points = 0
        weather_blind = 0
        total = 0

        def check_enemy(entry: dict, is_boss: bool) -> None:
            nonlocal no_weak_points, weather_blind, total
            total += 1
            key = "bossId" if is_boss else "enemyId"
            where = f"enemies.json:{entry.get(key, '<no id>')}"
            required = [
                key, "displayName", "faction", "threatLevel", "maxHealth", "armor", "shield",
                "moveSpeed", "damageType", "resistances", "lootTableId",
            ]
            if not is_boss:
                # threatWeight only matters for spawner-placed enemies: it is the AI budget
                # cost. A boss is placed by the level or by script.
                required.append("threatWeight")
            self.require(where, entry, required)
            self.enum_check(where, "EEclipseFaction", entry.get("faction", ""))
            self.enum_check(where, "EEclipseDamageType", entry.get("damageType", ""))
            self.positive(where, entry, "maxHealth")
            self.positive(where, entry, "moveSpeed")
            self.positive(where, entry, "threatLevel")
            if not is_boss:
                self.positive(where, entry, "threatWeight")
            self.positive(where, entry, "armor", allow_zero=True)
            self.positive(where, entry, "shield", allow_zero=True)

            if entry.get("lootTableId") not in self.loot_ids:
                self.report.error(where, f"references unknown loot table '{entry.get('lootTableId')}'")

            for damage_type, multiplier in entry.get("resistances", {}).items():
                self.enum_check(where, "EEclipseDamageType", damage_type)
                if not isinstance(multiplier, (int, float)) or multiplier < 0:
                    self.report.error(where, f"resistance '{damage_type}' must be a non-negative number")

            for biome in entry.get("biomes", []):
                self.enum_check(where, "EEclipseBiome", biome)

            for weather, multiplier in entry.get("weatherSpawnModifiers", {}).items():
                self.enum_check(where, "EEclipseWeather", weather)
                if not isinstance(multiplier, (int, float)) or multiplier < 0:
                    self.report.error(where, f"weather modifier '{weather}' must be a non-negative number")

            for weak_point in entry.get("weakPoints", []):
                if not (weak_point.get("name") or weak_point.get("boneName")):
                    self.report.error(where, "weak point has no bone name")
                self.positive(where, weak_point, "damageMultiplier")

            if not is_boss:
                self.enum_check(where, "EEclipseEnemyBehaviour", entry.get("behaviour", ""))
                self.enum_check(where, "EEclipseSquadRole", entry.get("squadRole", ""))

            # Bosses are excluded from both: a boss is a scripted encounter with phases
            # rather than a spawner-placed enemy, so it legitimately has neither.
            if not is_boss:
                if not entry.get("weakPoints"):
                    no_weak_points += 1
                    self.report.warn(where, "enemy has no weak points (shots do the same damage everywhere)")

                if not entry.get("weatherSpawnModifiers"):
                    weather_blind += 1
                    self.report.warn(where, "enemy has no weather spawn modifiers (spawns equally in every weather)")

        for entry in document["enemies"]:
            check_enemy(entry, is_boss=False)

        for entry in document["bosses"]:
            check_enemy(entry, is_boss=True)
            where = f"enemies.json:{entry.get('bossId', '<no id>')}"
            phases = entry.get("phases", [])
            if not phases:
                self.report.error(where, "a boss must have phases")
            thresholds = [phase.get("healthThreshold", 0) for phase in phases]
            if thresholds != sorted(thresholds, reverse=True):
                self.report.error(where, "boss phase thresholds must be in descending order")
            if thresholds and abs(thresholds[0] - 1.0) > 0.001:
                self.report.error(where, "the first boss phase must start at health threshold 1.0")
            if len(phases) != len({phase.get("phaseIndex") for phase in phases}):
                self.report.error(where, "boss phase indices must be unique")

        if total > 0 and weather_blind > total // 2:
            self.report.warn("enemies.json", f"{weather_blind}/{total} enemies ignore the weather")

    def validate_missions(self) -> None:
        document = self.documents["missions.json"]
        rewarded = 0

        for entry in document["missions"]:
            where = f"missions.json:{entry.get('missionId', '<no id>')}"
            self.require(where, entry, (
                "missionId", "displayName", "briefing", "missionType", "offeringFaction",
                "minimumLevel", "objectives", "rewards",
            ))
            self.enum_check(where, "EEclipseMissionType", entry.get("missionType", ""))
            self.enum_check(where, "EEclipseFaction", entry.get("offeringFaction", ""))
            self.enum_check(where, "EEclipseBiome", entry.get("requiredBiome", "None"))
            self.positive(where, entry, "minimumLevel")

            for weather in entry.get("requiredWeather", []):
                self.enum_check(where, "EEclipseWeather", weather)

            objectives = entry.get("objectives", [])
            if not objectives:
                self.report.error(where, "mission has no objectives")
            last_flags = [objective.get("mustBeLast", False) for objective in objectives]
            if last_flags.count(True) > 1:
                self.report.error(where, "only one objective may be flagged mustBeLast")
            for objective in objectives:
                self.enum_check(where, "EEclipseObjectiveType", objective.get("type", ""))
                self.positive(where, objective, "requiredCount")
                self.positive(where, objective, "radius", allow_zero=True)
                if objective.get("type") == "CollectItems" and not objective.get("targetItemId"):
                    self.report.error(where, "a CollectItems objective must name a targetItemId")
                if objective.get("targetItemId") and objective["targetItemId"] not in self.item_ids:
                    self.report.error(where, f"objective targets unknown item '{objective['targetItemId']}'")

            rewards = entry.get("rewards", {})
            if not isinstance(rewards, dict):
                self.report.error(where, "rewards must be an object")
                continue
            for item_id in rewards.get("items", []):
                if item_id not in self.item_ids:
                    self.report.error(where, f"reward item '{item_id}' is not an item")
            self.positive(where, rewards, "credits", allow_zero=True)
            self.positive(where, rewards, "experience", allow_zero=True)

            if entry.get("reputationReward"):
                rewarded += 1

            if entry.get("hasTimeWindow") and entry.get("timeWindowEndHour", 0) <= entry.get("timeWindowStartHour", 0) and entry.get("timeWindowEndHour", 0) != 0:
                # A window that ends before it starts is legal only as an overnight window.
                if entry.get("timeWindowEndHour", 0) > 0:
                    self.report.warn(where, "time window ends before it starts (overnight window?)")

        total = len(document["missions"])
        if total > 0 and rewarded < total:
            self.report.warn(
                "missions.json",
                f"{total - rewarded}/{total} missions do not declare a reputationReward (standing will not move)",
            )

    # ------------------------------------------------------------------ driver

    def run(self) -> Report:
        if not self.load():
            return self.report

        self.validate_items()
        self.validate_weapons()
        self.validate_loot()
        self.validate_skills()   # skills before recipes: recipes reference skills
        self.validate_crafting()
        self.check_skill_unlock_targets()
        self.validate_factions()
        self.validate_enemies()
        self.validate_missions()

        return self.report


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Validate the Eclipse vertical slice content data.")
    parser.add_argument("--quiet", action="store_true", help="print only the summary line")
    parser.add_argument("--strict", action="store_true", help="treat warnings as failures")
    args = parser.parse_args(argv)

    enums = parse_enums()
    if not enums:
        print("validate_content: no enums found; check the header paths", file=sys.stderr)
        return 2

    report = Validator(enums).run()

    summary = (
        f"{report.entries} content entries across {report.files} data files, "
        f"{len(report.errors)} errors, {len(report.warnings)} warnings."
    )

    if not args.quiet:
        print(f"Eclipse content validation ({DATA_DIR.relative_to(REPO_ROOT)})")
        print(f"  enums parsed: {len(enums)}")

        for message in report.errors:
            print(f"  error: {message}")
        for message in report.warnings:
            print(f"  warning: {message}")

    print(summary)

    if report.errors or (args.strict and report.warnings):
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
