"""PROJECT ECLIPSE reference model (engine-free).

This module is a second implementation of the game's arithmetic, written in plain Python so
it can be tested on a machine with no Unreal Engine. It exists for one reason: the C++
systems and this file must agree, and the way to guarantee that is to assert the same
numbers on both sides.

This file is a mirror, not a reimplementation with its own ideas. When a formula changes in
``Source/Eclipse``, change it here in the same commit:

* ``FEclipseDeterministicRandom``        -> :class:`DeterministicRandom`
* the stat aggregation contract          -> :func:`aggregate_stat`
* ``UEclipseDamageLibrary``              -> :func:`compute_damage`
* ``UEclipseLootTable::Roll``            -> :func:`roll_loot`
* ``UEclipseProgressionComponent``       -> :func:`xp_required_for_level`
* ``UEclipseTradeSubsystem`` pricing     -> :func:`quote_prices`
* ``EclipseAI`` tiering and scoring      -> :func:`tier_for_distance`, :func:`score_target`
* ``UEclipseWeatherTable::PickWeather``  -> :func:`pick_weather`

The enum tuples below mirror the *order* of the C++ enums, because several algorithms
iterate an enum rather than a map and therefore depend on that order.
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

MODEL_VERSION = "1.0"

# --------------------------------------------------------------------------------------
# Enum orders (must match Source/Eclipse/Public/Core/EclipseTypes.h)
# --------------------------------------------------------------------------------------

DAMAGE_TYPES: Tuple[str, ...] = (
    "Physical", "Thermal", "Chemical", "Electrical", "Kinetic", "Explosive", "Radiation", "Void",
)

RARITIES: Tuple[str, ...] = ("Common", "Uncommon", "Rare", "Epic", "Legendary", "Mythic")

ITEM_CATEGORIES: Tuple[str, ...] = (
    "Miscellaneous", "Ammunition", "Medical", "Consumable", "Armor", "Weapon",
    "Attachment", "Resource", "Technology", "Artifact", "QuestItem",
)

EQUIPMENT_SLOTS: Tuple[str, ...] = (
    "None", "PrimaryWeapon", "SecondaryWeapon", "Sidearm", "Melee", "Head", "Chest", "Legs",
    "Backpack", "QuickSlotOne", "QuickSlotTwo", "QuickSlotThree", "QuickSlotFour",
)

ATTACHMENT_SLOTS: Tuple[str, ...] = (
    "None", "Barrel", "Magazine", "Optic", "Muzzle", "Stock", "Grip", "Underbarrel", "PowerCore",
)

FIRE_MODES: Tuple[str, ...] = ("Single", "Burst", "Auto", "Charge")
DELIVERY_MODES: Tuple[str, ...] = ("Hitscan", "Projectile", "Beam", "Explosive")

MOVEMENT_STATES: Tuple[str, ...] = (
    "Idle", "Walking", "Sprinting", "Crouching", "Prone", "Climbing", "Swimming", "Falling",
)

STATS: Tuple[str, ...] = (
    "None", "MaxHealth", "MaxShield", "MaxStamina", "Armor", "ArmorPenetration", "Damage",
    "EffectiveRange", "MaxRange", "SpreadDegrees", "RecoilVertical", "RecoilHorizontal",
    "RoundsPerMinute", "ReloadSeconds", "MagazineCapacity", "AimDownSightsSeconds",
    "HeatPerShot", "Weight", "NoiseRadius", "CarryCapacity", "MoveSpeed", "SprintSpeed",
    "LootYield", "EnvironmentalResistance", "VoidResistance",
)

BIOMES: Tuple[str, ...] = (
    "None", "AbandonedMegacity", "TerraformingForest", "FrozenPlateau", "VolcanicRegion",
    "ToxicMarsh", "OrbitalCrashZone", "UndergroundComplex", "DesertExpanse",
)

WEATHER: Tuple[str, ...] = (
    "Clear", "Rain", "Storm", "Sandstorm", "Blizzard", "ToxicStorm", "ElectromagneticStorm",
)

TIME_BANDS: Tuple[str, ...] = ("Dawn", "Day", "Dusk", "Night")

FACTIONS: Tuple[str, ...] = (
    "None", "HelixCorporation", "FreeColonists", "IronWolves", "Ascendants", "Wildlife", "Neutral",
)

REPUTATION_BANDS: Tuple[str, ...] = ("Hostile", "Neutral", "Friendly", "Allied")

AI_TIERS: Tuple[str, ...] = ("Dormant", "LowFrequency", "HighFrequency", "Hero", "Boss")

SQUAD_ROLES: Tuple[str, ...] = ("Leader", "Assault", "Support", "Sniper", "Flanker", "Medic")

MISSION_TYPES: Tuple[str, ...] = (
    "MainMission", "SideMission", "FactionMission", "DynamicEvent", "Search", "Sabotage",
    "Assault", "Defense", "Escort", "Extraction", "Exploration",
)

OBJECTIVE_TYPES: Tuple[str, ...] = (
    "KillEnemies", "ReachLocation", "CollectItems", "Interact", "Survive", "Escort", "Defend",
    "Hack", "Extract", "Scan",
)

MISSION_STATES: Tuple[str, ...] = (
    "Locked", "Available", "Offered", "Active", "Completed", "Failed", "Expired",
)

RAID_OUTCOMES: Tuple[str, ...] = (
    "InProgress", "Extracted", "KilledInAction", "TimedOut", "BossDefeated",
)

CRAFT_REFUSALS: Tuple[str, ...] = (
    "None", "UnknownRecipe", "MissingStation", "MissingLevel", "MissingSkill",
    "MissingIngredients", "InventoryFull", "Busy",
)


def enum_index(enum_order: Sequence[str], value: str) -> int:
    """Index of a name inside an enum order tuple. Raises KeyError like a bad cast would."""
    return enum_order.index(value)


# --------------------------------------------------------------------------------------
# Deterministic random (mirrors FEclipseDeterministicRandom)
# --------------------------------------------------------------------------------------

GOLDEN_RATIO_32 = 0x9E3779B9
FNV_OFFSET_BASIS_32 = 2166136261
FNV_PRIME_32 = 16777619
UINT32_MASK = 0xFFFFFFFF


class DeterministicRandom:
    """LCG followed by a three-round xorshift-multiply finaliser.

    The weak LCG alone would be trivially predictable from a handful of outputs; the
    finaliser is what makes the stream usable for anything a player could observe.
    """

    def __init__(self, seed: int = GOLDEN_RATIO_32) -> None:
        self.reset(seed)

    def reset(self, seed: int) -> None:
        """Zero is remapped: a zero state would make the LCG's first output a constant."""
        self.state = GOLDEN_RATIO_32 if seed == 0 else seed & UINT32_MASK

    def next_uint32(self) -> int:
        self.state = (self.state * 1664525 + 1013904223) & UINT32_MASK
        value = self.state
        value ^= value >> 16
        value = (value * 0x7FEB352D) & UINT32_MASK
        value ^= value >> 15
        value = (value * 0x846CA68B) & UINT32_MASK
        value ^= value >> 16
        return value & UINT32_MASK

    def next_float(self) -> float:
        """Uniform in [0, 1)."""
        return self.next_uint32() / 4294967296.0

    def range(self, minimum: float, maximum: float) -> float:
        """Uniform in [min, max)."""
        if maximum < minimum:
            raise ValueError("range called with maximum < minimum")
        return minimum + (maximum - minimum) * self.next_float()

    def range_int(self, minimum: int, maximum_inclusive: int) -> int:
        """Uniform integer in [min, max]."""
        span = (maximum_inclusive - minimum) + 1
        if span <= 0:
            raise ValueError("range_int called with maximum < minimum")
        return minimum + (self.next_uint32() % span)

    def chance(self, probability: float) -> bool:
        """True with probability p, clamped to [0, 1]."""
        return self.next_float() < min(1.0, max(0.0, probability))

    @staticmethod
    def weighted_pick(weights: Sequence[float], random: "DeterministicRandom") -> int:
        """Index picked proportionally, or -1 when there is nothing to pick."""
        total = sum(max(0.0, weight) for weight in weights)
        if not weights or total <= 0.0:
            return -1

        roll = random.range(0.0, total)
        for index, weight in enumerate(weights):
            roll -= max(0.0, weight)
            if roll <= 0.0:
                return index

        # Floating-point drift can leave a hair of the total on the table.
        return len(weights) - 1


def mix_seed(seed: int, stable_id: int) -> int:
    """Derive an independent stream from a world seed and a stable id."""
    value = (seed ^ (stable_id + GOLDEN_RATIO_32 + (seed << 6) + (seed >> 2))) & UINT32_MASK
    value = (value * 0x85EBCA6B) & UINT32_MASK
    value ^= value >> 13
    value = (value * 0xC2B2AE35) & UINT32_MASK
    value ^= value >> 16
    return value & UINT32_MASK


def for_stable_id(seed: int, stable_id: int) -> DeterministicRandom:
    return DeterministicRandom(mix_seed(seed, stable_id))


def stable_id_from_string(key: str) -> int:
    """FNV-1a over the string's code points, matching the C++ side exactly."""
    value = FNV_OFFSET_BASIS_32
    for character in key:
        value ^= ord(character)
        value = (value * FNV_PRIME_32) & UINT32_MASK
    return value


# --------------------------------------------------------------------------------------
# Stats (mirrors UEclipseStatComponent / UEclipseWeaponInstance)
# --------------------------------------------------------------------------------------


@dataclass(frozen=True)
class StatModifier:
    stat: str
    additive: float = 0.0
    multiplicative: float = 1.0


def aggregate_stat(base: float, modifiers: Iterable[StatModifier], stat: str) -> float:
    """(Base + Additive) * Multiplicative, summed per stat.

    Additive first, then multiplicative, is the contract: two +0.5 multipliers give 1.25x
    base damage from a 1.0 base plus nothing, not 2.0x.
    """
    additive = 0.0
    multiplicative = 1.0
    for modifier in modifiers:
        if modifier.stat != stat:
            continue
        additive += modifier.additive
        multiplicative *= modifier.multiplicative
    return (base + additive) * multiplicative


# --------------------------------------------------------------------------------------
# Combat (mirrors UEclipseDamageLibrary)
# --------------------------------------------------------------------------------------

ARMOR_MITIGATION_CONSTANT = 100.0
MAX_ARMOR_MITIGATION = 0.9
MINIMUM_RANGE_FALLOFF = 0.35
DEFAULT_RESISTANCE = 1.0
MIN_RESISTANCE = 0.0
MAX_RESISTANCE = 2.0


def armor_mitigation(armor: float, armor_penetration: float) -> float:
    """Diminishing-returns armour: mitigation = A / (A + K), capped."""
    effective_armor = max(0.0, armor * (1.0 - min(1.0, max(0.0, armor_penetration))))
    if effective_armor <= 0.0:
        return 0.0
    return min(MAX_ARMOR_MITIGATION, effective_armor / (effective_armor + ARMOR_MITIGATION_CONSTANT))


def resistance_multiplier(profile: Dict[str, float], damage_type: str) -> float:
    """Resistance for a damage type, defaulting to 1.0 and clamped to [0, 2]."""
    value = profile.get(damage_type, DEFAULT_RESISTANCE)
    return min(MAX_RESISTANCE, max(MIN_RESISTANCE, value))


def distance_falloff(distance: float, effective_range: float, maximum_range: float) -> float:
    """Linear falloff between effective and maximum range, floored so shots still hurt."""
    if effective_range <= 0.0 or distance <= effective_range:
        return 1.0
    if maximum_range <= effective_range or distance >= maximum_range:
        return MINIMUM_RANGE_FALLOFF

    alpha = (distance - effective_range) / (maximum_range - effective_range)
    return 1.0 + (MINIMUM_RANGE_FALLOFF - 1.0) * alpha


def radial_falloff(distance: float, radius: float) -> float:
    """Explosions: 1.0 at the centre, floored at the edge."""
    if radius <= 0.0:
        return MINIMUM_RANGE_FALLOFF
    return min(1.0, max(MINIMUM_RANGE_FALLOFF, 1.0 - (distance / radius)))


@dataclass
class DamageContext:
    base_damage: float
    damage_type: str = "Physical"
    distance_falloff: float = 1.0
    weak_point_multiplier: float = 1.0
    critical_multiplier: float = 1.0
    armor_penetration: float = 0.0
    ignore_armor: bool = False

    def is_valid(self) -> bool:
        return self.base_damage > 0.0 and self.distance_falloff > 0.0


@dataclass
class DamageResult:
    applied_damage: float = 0.0
    armor_mitigation: float = 0.0
    shield_absorbed: float = 0.0
    health_damage: float = 0.0
    was_lethal: bool = False


def compute_damage(
    context: DamageContext,
    target_armor: float,
    resistances: Optional[Dict[str, float]] = None,
    target_shield: float = 0.0,
    target_health: float = 0.0,
) -> DamageResult:
    """The full pipeline: Base * Falloff * WeakPoint * Critical * (1 - Mitigation) * Resist."""
    result = DamageResult()
    if not context.is_valid():
        return result

    result.armor_mitigation = 0.0 if context.ignore_armor else armor_mitigation(target_armor, context.armor_penetration)

    damage = (
        context.base_damage
        * min(1.0, max(0.0, context.distance_falloff))
        * max(0.0, context.weak_point_multiplier)
        * max(0.0, context.critical_multiplier)
        * (1.0 - result.armor_mitigation)
        * resistance_multiplier(resistances or {}, context.damage_type)
    )

    result.applied_damage = damage

    # Shields eat first, then health.
    if target_shield > 0.0:
        result.shield_absorbed = min(target_shield, damage)
        damage -= result.shield_absorbed

    result.health_damage = max(0.0, min(target_health, damage)) if target_health > 0.0 else max(0.0, damage)
    result.was_lethal = target_health > 0.0 and result.health_damage >= target_health
    return result


# --------------------------------------------------------------------------------------
# Loot (mirrors UEclipseLootTable)
# --------------------------------------------------------------------------------------


@dataclass(frozen=True)
class LootEntry:
    item_id: str
    rarity: str
    weight: float = 1.0


@dataclass
class LootRoll:
    rarity: str
    item_id: str


def pick_rarity(rarity_weights: Dict[str, float], random: DeterministicRandom) -> str:
    """Rarity is rolled in enum order, never in dictionary order."""
    weights = [max(0.0, rarity_weights.get(rarity, 0.0)) for rarity in RARITIES]
    picked = DeterministicRandom.weighted_pick(weights, random)
    if picked < 0:
        return RARITIES[0]
    return RARITIES[picked]


def pick_entry(entries: Sequence[LootEntry], rarity: str, random: DeterministicRandom) -> Optional[LootEntry]:
    candidates = [entry for entry in entries if entry.rarity == rarity and entry.weight > 0.0]
    if not candidates:
        return None

    picked = DeterministicRandom.weighted_pick([entry.weight for entry in candidates], random)
    return candidates[picked] if picked >= 0 else None


def roll_loot(
    entries: Sequence[LootEntry],
    rarity_weights: Dict[str, float],
    roll_count_min: int,
    roll_count_max: int,
    random: DeterministicRandom,
) -> List[LootRoll]:
    """Roll rarity first, then an entry of that rarity. A rarity with no entries is a
    wasted roll, which is what stops a table with a Legendary row and no Legendary item
    from handing out free loot."""
    rolls: List[LootRoll] = []
    minimum = max(0, min(roll_count_min, roll_count_max))
    maximum = max(0, max(roll_count_min, roll_count_max))
    count = minimum if minimum == maximum else random.range_int(minimum, maximum)

    for _ in range(count):
        rarity = pick_rarity(rarity_weights, random)
        entry = pick_entry(entries, rarity, random)
        if entry is not None:
            rolls.append(LootRoll(rarity=rarity, item_id=entry.item_id))

    return rolls


# --------------------------------------------------------------------------------------
# Progression (mirrors UEclipseProgressionComponent)
# --------------------------------------------------------------------------------------

MAX_LEVEL = 20
XP_BASE = 400
XP_GROWTH_PER_LEVEL = 250
SKILL_POINTS_PER_LEVEL = 1


def xp_required_for_level(level: int) -> int:
    """XP needed to go from ``level - 1`` to ``level``. Level 1 is free."""
    if level <= 1:
        return 0
    return XP_BASE + XP_GROWTH_PER_LEVEL * (level - 1)


def total_xp_for_level(level: int) -> int:
    return sum(xp_required_for_level(step) for step in range(2, max(1, level) + 1))


def level_for_total_xp(total_xp: int) -> int:
    """Level recomputed from total XP, which is how a loaded save is validated."""
    level = 1
    while level < MAX_LEVEL and total_xp >= total_xp_for_level(level + 1):
        level += 1
    return level


def available_skill_points(level: int, spent_points: int) -> int:
    earned = max(0, min(level, MAX_LEVEL) - 1) * SKILL_POINTS_PER_LEVEL
    return max(0, earned - spent_points)


# --------------------------------------------------------------------------------------
# Economy (mirrors UEclipseTradeSubsystem and UEclipseVendorDefinition)
# --------------------------------------------------------------------------------------

BAND_BUY_MULTIPLIERS: Dict[str, float] = {
    "Hostile": 1.25,
    "Neutral": 1.0,
    "Friendly": 0.92,
    "Allied": 0.85,
}

BAND_SELL_MULTIPLIERS: Dict[str, float] = {
    "Hostile": 0.75,
    "Neutral": 1.0,
    "Friendly": 1.08,
    "Allied": 1.15,
}

FALLBACK_BUY_MULTIPLIER = 1.25
FALLBACK_SELL_MULTIPLIER = 0.35
REPAIR_COST_PER_DURABILITY_POINT = 1.5
REPUTATION_MIN = -1000.0
REPUTATION_MAX = 1000.0
MAX_REPUTATION_DELTA = 250.0


def round_half_up(value: float) -> int:
    """C++ rounds with floor(x + 0.5); Python's round() is banker's rounding, so it is not
    an acceptable substitute here."""
    return int(math.floor(value + 0.5))


def quote_prices(base_value: float, buy_multiplier: float, sell_multiplier: float, band: str) -> Tuple[int, int]:
    """Buy and sell prices for one unit."""
    buy = max(1, round_half_up(base_value * buy_multiplier * BAND_BUY_MULTIPLIERS.get(band, 1.0)))
    sell = max(0, round_half_up(base_value * sell_multiplier * BAND_SELL_MULTIPLIERS.get(band, 1.0)))
    return buy, sell


def repair_cost(missing_durability: float) -> int:
    return max(0, round_half_up(max(0.0, missing_durability) * REPAIR_COST_PER_DURABILITY_POINT))


def clamp_reputation(value: float) -> float:
    return min(REPUTATION_MAX, max(REPUTATION_MIN, value))


def reputation_band(value: float, hostile_threshold: float, friendly_threshold: float, allied_threshold: float) -> str:
    if value <= hostile_threshold:
        return "Hostile"
    if value >= allied_threshold:
        return "Allied"
    if value >= friendly_threshold:
        return "Friendly"
    return "Neutral"


# --------------------------------------------------------------------------------------
# AI (mirrors Public/AI/EclipseAITypes.h)
# --------------------------------------------------------------------------------------

AI_HIGH_FREQUENCY_RADIUS = 4000.0
AI_LOW_FREQUENCY_RADIUS_MULTIPLIER = 4.0
PLAYER_TARGET_PRIORITY = 500.0
DEFAULT_TARGET_PRIORITY = 100.0
DISTANCE_PENALTY_PER_CENTIMETRE = 0.01
SQUAD_FOCUS_BONUS = 150.0
LEASH_RADIUS_CENTIMETRES = 6000.0
NOISE_MEMORY_SECONDS = 6.0
MAX_RECORDED_NOISE_EVENTS = 64
SNIPER_RANGE_THRESHOLD = 8000.0


def tier_for_distance(distance_to_nearest_player: float, high_frequency_radius: float = AI_HIGH_FREQUENCY_RADIUS) -> str:
    if high_frequency_radius <= 0.0:
        return "Dormant"
    if distance_to_nearest_player <= high_frequency_radius:
        return "HighFrequency"
    if distance_to_nearest_player <= high_frequency_radius * AI_LOW_FREQUENCY_RADIUS_MULTIPLIER:
        return "LowFrequency"
    return "Dormant"


def score_target(is_player: bool, distance_centimetres: float, is_squad_focus: bool) -> float:
    score = PLAYER_TARGET_PRIORITY if is_player else DEFAULT_TARGET_PRIORITY
    score -= distance_centimetres * DISTANCE_PENALTY_PER_CENTIMETRE
    if is_squad_focus:
        score += SQUAD_FOCUS_BONUS
    return score


def is_leashed(distance_from_spawn: float) -> bool:
    return distance_from_spawn > LEASH_RADIUS_CENTIMETRES


def noise_is_remembered(age_seconds: float) -> bool:
    return age_seconds <= NOISE_MEMORY_SECONDS


def squad_role_for_index(index: int, definition_role: str, attack_range: float) -> str:
    """Leader is index 0, the definition's role wins next, then a long reach makes a
    sniper, and otherwise the line alternates flanker and assault."""
    if index == 0:
        return "Leader"
    if definition_role == "Medic":
        return "Medic"
    if attack_range >= SNIPER_RANGE_THRESHOLD:
        return "Sniper"
    return "Flanker" if index % 2 == 1 else "Assault"


# --------------------------------------------------------------------------------------
# Weather (mirrors UEclipseWeatherTable::PickWeather)
# --------------------------------------------------------------------------------------


def pick_weather(weights_by_weather: Dict[str, float], random: DeterministicRandom) -> str:
    """Weather is rolled in enum order over the biome's weights; an empty or all-zero row
    falls back to Clear rather than to an arbitrary weather."""
    weights = [max(0.0, weights_by_weather.get(weather, 0.0)) for weather in WEATHER]
    picked = DeterministicRandom.weighted_pick(weights, random)
    return "Clear" if picked < 0 else WEATHER[picked]


# --------------------------------------------------------------------------------------
# Content loading (used by the tests and by Tools/ci/validate_content.py)
# --------------------------------------------------------------------------------------

REQUIRED_DATA_FILES: Tuple[str, ...] = (
    "items.json", "weapons.json", "loot.json", "crafting.json",
    "skills.json", "factions.json", "enemies.json", "missions.json",
)


def content_entry_count(document: dict) -> int:
    """Every array-valued key in a slice data file contributes its elements."""
    return sum(len(value) for value in document.values() if isinstance(value, list))


__all__ = [
    "MODEL_VERSION",
    "DAMAGE_TYPES", "RARITIES", "ITEM_CATEGORIES", "EQUIPMENT_SLOTS", "ATTACHMENT_SLOTS",
    "FIRE_MODES", "DELIVERY_MODES", "MOVEMENT_STATES", "STATS", "BIOMES", "WEATHER",
    "TIME_BANDS", "FACTIONS", "REPUTATION_BANDS", "AI_TIERS", "SQUAD_ROLES",
    "MISSION_TYPES", "OBJECTIVE_TYPES", "MISSION_STATES", "RAID_OUTCOMES", "CRAFT_REFUSALS",
    "DeterministicRandom", "mix_seed", "for_stable_id", "stable_id_from_string",
    "StatModifier", "aggregate_stat",
    "armor_mitigation", "resistance_multiplier", "distance_falloff", "radial_falloff",
    "DamageContext", "DamageResult", "compute_damage",
    "LootEntry", "LootRoll", "pick_rarity", "pick_entry", "roll_loot",
    "xp_required_for_level", "total_xp_for_level", "level_for_total_xp", "available_skill_points",
    "quote_prices", "repair_cost", "clamp_reputation", "reputation_band", "round_half_up",
    "tier_for_distance", "score_target", "is_leashed", "noise_is_remembered", "squad_role_for_index",
    "pick_weather", "content_entry_count", "REQUIRED_DATA_FILES",
]
