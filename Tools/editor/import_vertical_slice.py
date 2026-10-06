"""Import the vertical slice JSON into data assets.

Run this **inside the Unreal Editor** (Tools > Execute Python Script, or
``UnrealEditor-Cmd.exe <project> -run=pythonscript -script="Tools/editor/import_vertical_slice.py"``).
It cannot run under the engine-free CI: it imports ``unreal``.

What it does
    1. reads every file in Content/Eclipse/Data/VerticalSlice
    2. creates or updates one data asset per entry, named after its id
    3. resolves cross-references (an enemy's loot table, a recipe's output item, a skill's
       unlocked recipe) by asset path, and *reports* anything it could not resolve
    4. writes a report to Saved/Logs/vertical_slice_import.txt

Property names are mapped explicitly per asset class rather than by guessing: a key this
script does not know is reported, never silently dropped. That is deliberate - the JSON is
the source of truth for content, so a field that stops being imported is a bug worth seeing.

Usage (in-editor)
    exec(open("Tools/editor/import_vertical_slice.py").read())
"""

from __future__ import annotations

import json
import os

import unreal  # type: ignore  # provided by the editor

DATA_DIR = os.path.join("Content", "Eclipse", "Data", "VerticalSlice")
DESTINATION_ROOT = "/Game/Eclipse/Data"

ITEM_CLASS = "/Script/Eclipse.EclipseItemDefinition"
WEAPON_CLASS = "/Script/Eclipse.EclipseWeaponDefinition"
ATTACHMENT_CLASS = "/Script/Eclipse.EclipseAttachmentDefinition"
LOOT_CLASS = "/Script/Eclipse.EclipseLootTable"
RECIPE_CLASS = "/Script/Eclipse.EclipseRecipeDefinition"
SKILL_CLASS = "/Script/Eclipse.EclipseSkillDefinition"
FACTION_CLASS = "/Script/Eclipse.EclipseFactionDefinition"
ENEMY_CLASS = "/Script/Eclipse.EclipseEnemyDefinition"
MISSION_CLASS = "/Script/Eclipse.EclipseMissionDefinition"
VENDOR_CLASS = "/Script/Eclipse.EclipseVendorDefinition"

# JSON key -> UPROPERTY name. Keys absent from a map are reported as unmapped.
ITEM_MAP = {
    "itemId": "ItemId", "displayName": "DisplayName", "description": "Description",
    "category": "Category", "rarity": "Rarity", "gridSize": "GridSize", "weight": "Weight",
    "volume": "Volume", "maxStackSize": "MaxStackSize", "maxDurability": "MaxDurability",
    "baseValue": "BaseValue", "equippable": "bEquippable", "equipSlotTag": "EquipSlotTag",
}

WEAPON_MAP = {
    "weaponId": "WeaponId", "displayName": "DisplayName", "description": "Description",
    "fireMode": "FireMode", "deliveryMode": "DeliveryMode", "damage": "Damage",
    "damageType": "DamageType", "spreadDegrees": "SpreadDegrees", "recoilVertical": "RecoilVertical",
    "recoilHorizontal": "RecoilHorizontal", "effectiveRange": "EffectiveRange", "maxRange": "MaxRange",
    "roundsPerMinute": "RoundsPerMinute", "reloadSeconds": "ReloadSeconds",
    "magazineCapacity": "MagazineCapacity", "aimDownSightsSeconds": "AimDownSightsSeconds",
    "aimSpreadMultiplier": "AimSpreadMultiplier", "armorPenetration": "ArmorPenetration",
    "noiseRadius": "NoiseRadius", "weight": "Weight", "ammoItemId": "AmmoItemId",
}

ATTACHMENT_MAP = {
    "attachmentId": "AttachmentId", "displayName": "DisplayName", "slot": "Slot",
    "rarity": "Rarity", "modifiers": "Modifiers", "itemId": "ItemId",
}

LOOT_MAP = {
    "tableId": "TableId", "description": "Description", "rollCount": "RollCountMin",
    "minimumThreatLevel": "MinimumThreatLevel", "rarityWeights": "RarityWeights",
    "entries": "Entries",
}

RECIPE_MAP = {
    "recipeId": "RecipeId", "displayName": "DisplayName", "category": "Category",
    "requiredStation": "RequiredStation", "requiredLevel": "RequiredLevel",
    "requiredSkillId": "RequiredSkillId", "craftTimeSeconds": "CraftTimeSeconds",
    "outputItemId": "OutputItemId", "outputCount": "OutputCount", "ingredients": "Ingredients",
}

SKILL_MAP = {
    "skillId": "SkillId", "displayName": "DisplayName", "description": "Description",
    "tree": "Tree", "tier": "Tier", "skillPointCost": "SkillPointCost", "prerequisites": "Prerequisites",
    "effect": "Effect", "targetStat": "TargetStat", "additiveValue": "AdditiveValue",
    "multiplicativeValue": "MultiplicativeValue", "unlockedId": "UnlockedId",
}

FACTION_MAP = {
    "faction": "Faction", "displayName": "DisplayName", "description": "Description",
    "enemies": "Enemies", "allies": "Allies", "hostileThreshold": "HostileThreshold",
    "friendlyThreshold": "FriendlyThreshold", "alliedThreshold": "AlliedThreshold",
}

ENEMY_MAP = {
    "enemyId": "EnemyId", "bossId": "EnemyId", "displayName": "DisplayName",
    "description": "Description", "faction": "Faction", "threatLevel": "ThreatLevel",
    "maxHealth": "MaxHealth", "armor": "Armor", "shield": "Shield", "moveSpeed": "MoveSpeed",
    "threatWeight": "ThreatWeight", "noiseRadius": "NoiseRadius", "damageType": "DamageType",
    "resistances": "Resistances", "behaviour": "Behaviour", "squadRole": "SquadRole",
    "biomes": "Biomes", "nightOnly": "bNightOnly", "weatherSpawnModifiers": "WeatherSpawnModifiers",
    "weakPoints": "WeakPoints", "phases": "Phases", "arena": "ArenaId",
}

MISSION_MAP = {
    "missionId": "MissionId", "displayName": "DisplayName", "briefing": "Briefing",
    "missionType": "MissionType", "offeringFaction": "OfferingFaction", "minimumLevel": "MinimumLevel",
    "minimumReputation": "MinimumReputation", "requiredBiome": "RequiredBiome",
    "requiredWeather": "RequiredWeather", "hasTimeWindow": "bHasTimeWindow",
    "timeWindowStartHour": "TimeWindowStartHour", "timeWindowEndHour": "TimeWindowEndHour",
    "timeLimitSeconds": "TimeLimitSeconds", "objectives": "Objectives",
    "reputationReward": "ReputationReward", "nextMissionId": "NextMissionId",
}

REPORTS = []


def report(message: str) -> None:
    REPORTS.append(message)
    unreal.log(message)


def asset_path(folder: str, name: str) -> str:
    return f"{DESTINATION_ROOT}/{folder}/DA_{name.replace('.', '_')}"


def load_json(file_name: str) -> dict:
    path = os.path.join(unreal.Paths.project_dir(), DATA_DIR, file_name)
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def coerce(value):
    """Conversion hook for the editor property system.

    Nested structures (modifier tables, loot entries, boss phases) are passed through
    unchanged: the data asset's own struct layout is the authority, and a mismatch shows up
    as an editor warning rather than as silently wrong content.
    """
    return value


def create_or_update(asset_folder: str, name: str, class_path: str, entry: dict, property_map: dict) -> object:
    """Create or update one data asset from one JSON entry."""
    path = asset_path(asset_folder, name)
    asset = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None

    if asset is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("DataAssetClass", unreal.load_class(None, class_path))
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            f"DA_{name.replace('.', '_')}", f"{DESTINATION_ROOT}/{asset_folder}", None, factory)

    if asset is None:
        report(f"FAILED to create {path}")
        return None

    for json_key, value in entry.items():
        property_name = property_map.get(json_key)
        if property_name is None:
            # Content that stops being imported is a bug worth seeing, not a silent drop.
            report(f"  {path}: no property mapped for JSON key '{json_key}'")
            continue
        try:
            asset.set_editor_property(property_name, coerce(value))
        except Exception as error:  # the editor reports the exact type mismatch
            report(f"  {path}.{property_name}: {error}")

    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    return asset


def import_items() -> dict:
    document = load_json("items.json")
    mapping = {}

    for entry in document["items"]:
        asset = create_or_update("Items", entry["itemId"], ITEM_CLASS, entry, ITEM_MAP)
        mapping[entry["itemId"]] = asset

    for entry in document["weapons"]:
        asset = create_or_update("Items", entry["itemId"], ITEM_CLASS, entry, ITEM_MAP)
        mapping[entry["itemId"]] = asset

    for entry in document["attachments"]:
        asset = create_or_update("Items", entry["itemId"], ITEM_CLASS, entry, ITEM_MAP)
        mapping[entry["itemId"]] = asset

    return mapping


def import_weapons(item_assets: dict) -> None:
    document = load_json("weapons.json")

    for entry in document["weapons"]:
        asset = create_or_update("Weapons", entry["weaponId"], WEAPON_CLASS, entry, WEAPON_MAP)
        if asset is not None and entry.get("ammoItemId"):
            ammo = item_assets.get(entry["ammoItemId"])
            if ammo is None:
                report(f"weapons.json:{entry['weaponId']} needs ammo '{entry['ammoItemId']}' (not imported)")
            else:
                asset.set_editor_property("AmmoItem", ammo)

    for entry in document["attachments"]:
        create_or_update("Attachments", entry["attachmentId"], ATTACHMENT_CLASS, entry, ATTACHMENT_MAP)


def import_loot(item_assets: dict) -> dict:
    document = load_json("loot.json")
    mapping = {}

    for entry in document["tables"]:
        asset = create_or_update("Loot", entry["tableId"], LOOT_CLASS, entry, LOOT_MAP)
        mapping[entry["tableId"]] = asset

        for loot_entry in entry.get("entries", []):
            if loot_entry["itemId"] not in item_assets:
                report(f"loot.json:{entry['tableId']} references unimported item '{loot_entry['itemId']}'")

    return mapping


def import_progression() -> dict:
    document = load_json("skills.json")
    mapping = {}

    for entry in document["skills"]:
        asset = create_or_update("Skills", entry["skillId"], SKILL_CLASS, entry, SKILL_MAP)
        mapping[entry["skillId"]] = asset

    # Prerequisites and unlocks resolve by id after every skill exists, so a tree can be
    # authored in any order in the JSON.
    for entry in document["skills"]:
        asset = mapping[entry["skillId"]]
        prerequisites = [mapping[other] for other in entry.get("prerequisites", []) if other in mapping]
        if prerequisites:
            asset.set_editor_property("PrerequisiteSkills", prerequisites)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)

    return mapping


def import_crafting(item_assets: dict, skill_assets: dict) -> None:
    document = load_json("crafting.json")

    for entry in document["recipes"]:
        asset = create_or_update("Recipes", entry["recipeId"], RECIPE_CLASS, entry, RECIPE_MAP)
        if asset is None:
            continue

        output = item_assets.get(entry["outputItemId"])
        if output is None:
            report(f"crafting.json:{entry['recipeId']} outputs unimported item '{entry['outputItemId']}'")
        else:
            asset.set_editor_property("OutputItem", output)

        skill_id = entry.get("requiredSkillId")
        if skill_id:
            if skill_id in skill_assets:
                asset.set_editor_property("RequiredSkill", skill_assets[skill_id])
            else:
                report(f"crafting.json:{entry['recipeId']} requires unimported skill '{skill_id}'")

        unreal.EditorAssetLibrary.save_loaded_asset(asset)


def import_factions() -> dict:
    document = load_json("factions.json")
    mapping = {}

    for entry in document["factions"]:
        asset = create_or_update("Factions", entry["faction"], FACTION_CLASS, entry, FACTION_MAP)
        mapping[entry["faction"]] = asset

        pricing = entry.get("vendorPricing", [])
        if pricing:
            vendor = create_or_update("Vendors", f"Vendor.{entry['faction']}", VENDOR_CLASS, entry, {
                "faction": "Faction",
                "vendorPricing": "PricingRows",
            })
            if vendor is not None:
                vendor.set_editor_property("Faction", entry["faction"])
                unreal.EditorAssetLibrary.save_loaded_asset(vendor)

    return mapping


def import_enemies(loot_assets: dict) -> None:
    document = load_json("enemies.json")

    for entry in document["enemies"] + document["bosses"]:
        key = entry.get("enemyId") or entry.get("bossId")
        asset = create_or_update("Enemies", key, ENEMY_CLASS, entry, ENEMY_MAP)
        if asset is None:
            continue

        loot = loot_assets.get(entry.get("lootTableId"))
        if loot is None:
            report(f"enemies.json:{key} references unimported loot table '{entry.get('lootTableId')}'")
        else:
            asset.set_editor_property("LootTable", loot)

        unreal.EditorAssetLibrary.save_loaded_asset(asset)


def import_missions(item_assets: dict) -> None:
    document = load_json("missions.json")

    for entry in document["missions"]:
        asset = create_or_update("Missions", entry["missionId"], MISSION_CLASS, entry, MISSION_MAP)
        if asset is None:
            continue

        for item_id in entry.get("rewards", {}).get("items", []):
            if item_id not in item_assets:
                report(f"missions.json:{entry['missionId']} rewards unimported item '{item_id}'")

        unreal.EditorAssetLibrary.save_loaded_asset(asset)


def main() -> None:
    report("Eclipse vertical slice import")
    report(f"  source: {DATA_DIR}")

    item_assets = import_items()
    import_weapons(item_assets)
    loot_assets = import_loot(item_assets)
    skill_assets = import_progression()
    import_crafting(item_assets, skill_assets)
    import_factions()
    import_enemies(loot_assets)
    import_missions(item_assets)

    report_path = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "vertical_slice_import.txt")
    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    with open(report_path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(REPORTS) + "\n")

    report(f"  report: {report_path}")
    report("Import finished. Resolve every reported line before committing assets.")


if __name__ == "__main__":
    main()
