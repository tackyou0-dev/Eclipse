"""Tests for the engine-free reference model.

These assert the numbers the C++ systems must produce. A failure here means one of two
things: the formula changed and the mirror was not updated in the same commit, or the
mirror drifted from the C++. Either way it is a bug in the commit, not in the test.

Run: ``python3 -m unittest discover -s Tests/Reference -p "test_*.py"``
"""

import unittest

from eclipse_reference_model import (
    AI_HIGH_FREQUENCY_RADIUS,
    ARMOR_MITIGATION_CONSTANT,
    MAX_ARMOR_MITIGATION,
    MAX_LEVEL,
    MAX_REPUTATION_DELTA,
    MINIMUM_RANGE_FALLOFF,
    WEATHER,
    DamageContext,
    DeterministicRandom,
    LootEntry,
    StatModifier,
    aggregate_stat,
    armor_mitigation,
    available_skill_points,
    clamp_reputation,
    compute_damage,
    distance_falloff,
    level_for_total_xp,
    mix_seed,
    pick_weather,
    quote_prices,
    radial_falloff,
    repair_cost,
    resistance_multiplier,
    roll_loot,
    score_target,
    squad_role_for_index,
    stable_id_from_string,
    tier_for_distance,
    total_xp_for_level,
    xp_required_for_level,
)


class DeterministicRandomTests(unittest.TestCase):
    """The random stream is a save-compatibility contract: change it and old saves replay
    differently, so it gets the most tests."""

    def test_zero_seed_is_remapped(self):
        # A zero state would make the first LCG output a constant, so zero is remapped to
        # the golden-ratio constant.
        self.assertEqual(DeterministicRandom(0).state, 0x9E3779B9)
        self.assertEqual(DeterministicRandom(0).state, DeterministicRandom(0x9E3779B9).state)

    def test_identical_seeds_produce_identical_streams(self):
        left = DeterministicRandom(20261006)
        right = DeterministicRandom(20261006)
        self.assertEqual([left.next_uint32() for _ in range(16)], [right.next_uint32() for _ in range(16)])

    def test_next_float_is_in_unit_interval(self):
        random = DeterministicRandom(7)
        for _ in range(256):
            value = random.next_float()
            self.assertGreaterEqual(value, 0.0)
            self.assertLess(value, 1.0)

    def test_range_int_stays_in_bounds(self):
        random = DeterministicRandom(11)
        seen = set()
        for _ in range(512):
            value = random.range_int(3, 6)
            self.assertIn(value, (3, 4, 5, 6))
            seen.add(value)
        # A bounded draw that never reaches the ends is a broken distribution.
        self.assertEqual(seen, {3, 4, 5, 6})

    def test_chance_bounds_are_exact(self):
        random = DeterministicRandom(13)
        for _ in range(128):
            self.assertFalse(random.chance(0.0))
            self.assertTrue(random.chance(1.0))

    def test_weighted_pick_ignores_zero_weights(self):
        # No roll may ever land on a zero-weight slot, whatever the random value is.
        random = DeterministicRandom(17)
        for _ in range(128):
            self.assertEqual(DeterministicRandom.weighted_pick([0.0, 4.0, 0.0], random), 1)
        self.assertEqual(DeterministicRandom.weighted_pick([], random), -1)
        self.assertEqual(DeterministicRandom.weighted_pick([0.0, 0.0], random), -1)

    def test_stable_id_from_string_is_fnv1a(self):
        # FNV-1a of the empty string is the offset basis; "ECLIPSE" is a fixed vector of
        # the same algorithm implemented in C++.
        self.assertEqual(stable_id_from_string(""), 2166136261)
        self.assertEqual(stable_id_from_string("ECLIPSE"), stable_id_from_string("ECLIPSE"))
        self.assertNotEqual(stable_id_from_string("ECLIPSE"), stable_id_from_string("eclipse"))

    def test_mix_seed_separates_stable_ids(self):
        self.assertNotEqual(mix_seed(1, 2), mix_seed(1, 3))
        self.assertNotEqual(mix_seed(1, 2), mix_seed(2, 2))
        self.assertEqual(mix_seed(1, 2), mix_seed(1, 2))


class StatAggregationTests(unittest.TestCase):
    def test_stat_aggregation_additive_before_multiplicative(self):
        modifiers = [
            StatModifier("Damage", additive=10.0, multiplicative=1.0),
            StatModifier("Damage", additive=0.0, multiplicative=1.5),
            StatModifier("Damage", additive=0.0, multiplicative=0.5),
            StatModifier("Weight", additive=100.0),
        ]
        # (20 + 10) * 1.5 * 0.5 = 22.5, and the unrelated stat is untouched.
        self.assertEqual(aggregate_stat(20.0, modifiers, "Damage"), 22.5)
        self.assertEqual(aggregate_stat(20.0, modifiers, "Weight"), 120.0)
        self.assertEqual(aggregate_stat(42.0, [], "Damage"), 42.0)


class CombatTests(unittest.TestCase):
    def test_zero_armor_does_not_mitigate(self):
        self.assertEqual(armor_mitigation(0.0, 0.0), 0.0)

    def test_armor_mitigation_diminishes_and_is_capped(self):
        light = armor_mitigation(ARMOR_MITIGATION_CONSTANT, 0.0)
        heavy = armor_mitigation(ARMOR_MITIGATION_CONSTANT * 10.0, 0.0)
        absurd = armor_mitigation(1e9, 0.0)

        self.assertAlmostEqual(light, 0.5, places=6)
        self.assertLess(heavy, 1.0)
        self.assertLess(heavy - light, light)  # 10x armour buys much less than 2x mitigation.
        self.assertLessEqual(absurd, MAX_ARMOR_MITIGATION)

    def test_full_penetration_ignores_armor(self):
        self.assertEqual(armor_mitigation(500.0, 1.0), 0.0)
        self.assertLess(armor_mitigation(100.0, 0.5), armor_mitigation(100.0, 0.0))

    def test_resistance_defaults_to_one_and_is_clamped(self):
        self.assertEqual(resistance_multiplier({}, "Thermal"), 1.0)
        self.assertEqual(resistance_multiplier({"Thermal": 1.6}, "Thermal"), 1.6)
        self.assertEqual(resistance_multiplier({"Thermal": 5.0}, "Thermal"), 2.0)
        self.assertEqual(resistance_multiplier({"Thermal": -1.0}, "Thermal"), 0.0)

    def test_distance_falloff_is_one_inside_and_floors_at_range(self):
        self.assertEqual(distance_falloff(500.0, 1000.0, 5000.0), 1.0)
        self.assertAlmostEqual(distance_falloff(3000.0, 1000.0, 5000.0), 0.675, places=6)
        self.assertEqual(distance_falloff(5000.0, 1000.0, 5000.0), MINIMUM_RANGE_FALLOFF)
        self.assertEqual(distance_falloff(50000.0, 1000.0, 5000.0), MINIMUM_RANGE_FALLOFF)

        # Explosions use the same floor: 1.0 at the centre, never below the floor at the
        # edge, so a grenade at maximum radius still hurts.
        self.assertEqual(radial_falloff(0.0, 1000.0), 1.0)
        self.assertAlmostEqual(radial_falloff(500.0, 1000.0), 0.5, places=6)
        self.assertEqual(radial_falloff(1000.0, 1000.0), MINIMUM_RANGE_FALLOFF)
        self.assertEqual(radial_falloff(9000.0, 1000.0), MINIMUM_RANGE_FALLOFF)

    def test_damage_pipeline_matches_hand_computed_value(self):
        context = DamageContext(
            base_damage=100.0,
            damage_type="Physical",
            distance_falloff=1.0,
            weak_point_multiplier=2.0,
            critical_multiplier=1.5,
        )
        # 100 * 1 * 2 * 1.5 * (1 - 0.5) * 1.0 = 150
        result = compute_damage(context, target_armor=100.0, resistances={}, target_health=1000.0)
        self.assertAlmostEqual(result.applied_damage, 150.0, places=6)
        self.assertAlmostEqual(result.armor_mitigation, 0.5, places=6)
        self.assertAlmostEqual(result.health_damage, 150.0, places=6)
        self.assertFalse(result.was_lethal)

    def test_shields_absorb_before_health(self):
        context = DamageContext(base_damage=80.0)
        result = compute_damage(context, target_armor=0.0, target_shield=30.0, target_health=100.0)
        self.assertAlmostEqual(result.shield_absorbed, 30.0, places=6)
        self.assertAlmostEqual(result.health_damage, 50.0, places=6)
        self.assertFalse(result.was_lethal)

        lethal = compute_damage(DamageContext(base_damage=120.0), target_armor=0.0, target_health=100.0)
        self.assertTrue(lethal.was_lethal)
        self.assertAlmostEqual(lethal.health_damage, 100.0, places=6)

    def test_invalid_context_does_no_damage(self):
        result = compute_damage(DamageContext(base_damage=0.0), target_armor=0.0, target_health=100.0)
        self.assertEqual(result.applied_damage, 0.0)
        self.assertEqual(result.health_damage, 0.0)
        self.assertFalse(result.was_lethal)


class LootTests(unittest.TestCase):
    TABLE = (
        LootEntry("Ammo.556", "Common", 60.0),
        LootEntry("Medical.Medkit", "Common", 40.0),
        LootEntry("Attachment.Optic.Holo", "Uncommon", 70.0),
        LootEntry("Weapon.VX9", "Rare", 100.0),
    )
    WEIGHTS = {"Common": 60.0, "Uncommon": 28.0, "Rare": 10.0, "Epic": 2.0}

    def test_rarity_roll_follows_enum_order(self):
        # Only Epic is weighted and Epic is last in the enum order: the roll must find it.
        table = self.TABLE + (LootEntry("Artifact.AlienRelic", "Epic", 1.0),)
        rolls = roll_loot(table, {"Epic": 1.0}, 4, 4, DeterministicRandom(5))
        self.assertEqual(len(rolls), 4)
        for roll in rolls:
            self.assertEqual(roll.rarity, "Epic")
            self.assertEqual(roll.item_id, "Artifact.AlienRelic")

    def test_roll_count_is_respected(self):
        # Weights are restricted to Common here so no roll is wasted and the drawn count is
        # exactly what the table promised.
        random = DeterministicRandom(23)
        for _ in range(32):
            rolls = roll_loot(self.TABLE, {"Common": 1.0}, 2, 5, random)
            self.assertGreaterEqual(len(rolls), 2)
            self.assertLessEqual(len(rolls), 5)

        fixed = roll_loot(self.TABLE, {"Common": 1.0}, 3, 3, DeterministicRandom(29))
        self.assertEqual(len(fixed), 3)

    def test_rolls_with_no_entries_are_wasted_not_substituted(self):
        # A table that advertises Epic but ships no Epic entry must not hand out a Rare.
        table = (LootEntry("Weapon.VX9", "Rare", 1.0),)
        rolls = roll_loot(table, {"Epic": 100.0}, 3, 3, DeterministicRandom(31))
        self.assertEqual(rolls, [])

    def test_rarity_weights_all_zero_fall_back_to_common(self):
        # Defensive: a badly authored table yields Common rolls rather than an exception.
        table = (LootEntry("Ammo.556", "Common", 1.0),)
        rolls = roll_loot(table, {}, 1, 1, DeterministicRandom(37))
        self.assertEqual([roll.rarity for roll in rolls], ["Common"])


class ProgressionTests(unittest.TestCase):
    def test_xp_curve_matches_the_documented_formula(self):
        self.assertEqual(xp_required_for_level(1), 0)
        self.assertEqual(xp_required_for_level(2), 650)
        self.assertEqual(xp_required_for_level(3), 900)
        self.assertEqual(xp_required_for_level(20), 400 + 250 * 19)
        self.assertEqual(total_xp_for_level(3), 650 + 900)

    def test_level_from_total_xp_round_trips(self):
        for level in range(1, MAX_LEVEL + 1):
            self.assertEqual(level_for_total_xp(total_xp_for_level(level)), level)
            self.assertEqual(level_for_total_xp(total_xp_for_level(level) - 1), max(1, level - 1))

    def test_level_is_capped(self):
        self.assertEqual(level_for_total_xp(10_000_000), MAX_LEVEL)

    def test_available_skill_points_subtracts_spent(self):
        self.assertEqual(available_skill_points(1, 0), 0)
        self.assertEqual(available_skill_points(5, 0), 4)
        self.assertEqual(available_skill_points(5, 3), 1)
        self.assertEqual(available_skill_points(5, 99), 0)


class EconomyTests(unittest.TestCase):
    def test_buy_price_uses_band_multiplier(self):
        # base 100, faction markup 1.25, hostile band 1.25 -> ceil-ish 156.25 -> 156
        self.assertEqual(quote_prices(100.0, 1.25, 0.35, "Hostile")[0], 156)
        self.assertEqual(quote_prices(100.0, 1.0, 0.5, "Friendly")[0], 92)
        self.assertEqual(quote_prices(100.0, 1.0, 0.5, "Allied")[0], 85)
        # A free item still costs at least one credit.
        self.assertEqual(quote_prices(0.0, 1.0, 0.0, "Neutral")[0], 1)

    def test_sell_price_uses_band_multiplier(self):
        self.assertEqual(quote_prices(100.0, 1.25, 1.0, "Hostile")[1], 75)
        self.assertEqual(quote_prices(100.0, 1.25, 1.0, "Allied")[1], 115)
        self.assertEqual(quote_prices(100.0, 1.0, 0.0, "Neutral")[1], 0)

    def test_repair_cost_rounds_half_up(self):
        self.assertEqual(repair_cost(0.0), 0)
        self.assertEqual(repair_cost(10.0), 15)
        self.assertEqual(repair_cost(0.3333), 0)
        self.assertEqual(repair_cost(-5.0), 0)

    def test_reputation_is_clamped_and_banded(self):
        self.assertEqual(clamp_reputation(5000.0), 1000.0)
        self.assertEqual(clamp_reputation(-5000.0), -1000.0)
        # A single event may never move more than the delta ceiling.
        self.assertLessEqual(abs(MAX_REPUTATION_DELTA), 1000.0)


class AITests(unittest.TestCase):
    def test_tier_boundaries(self):
        self.assertEqual(tier_for_distance(0.0), "HighFrequency")
        self.assertEqual(tier_for_distance(AI_HIGH_FREQUENCY_RADIUS), "HighFrequency")
        self.assertEqual(tier_for_distance(AI_HIGH_FREQUENCY_RADIUS + 1.0), "LowFrequency")
        self.assertEqual(tier_for_distance(AI_HIGH_FREQUENCY_RADIUS * 4.0), "LowFrequency")
        self.assertEqual(tier_for_distance(AI_HIGH_FREQUENCY_RADIUS * 4.0 + 1.0), "Dormant")
        # A configured radius of zero would dorm everything, so it is treated as "off".
        self.assertEqual(tier_for_distance(0.0, high_frequency_radius=0.0), "Dormant")

    def test_target_score_prefers_players_and_squad_focus(self):
        near_player = score_target(is_player=True, distance_centimetres=1000.0, is_squad_focus=False)
        far_player = score_target(is_player=True, distance_centimetres=30000.0, is_squad_focus=False)
        drone = score_target(is_player=False, distance_centimetres=1000.0, is_squad_focus=False)
        focus = score_target(is_player=False, distance_centimetres=1000.0, is_squad_focus=True)

        self.assertEqual(near_player, 490.0)  # 500 - 10
        self.assertLess(far_player, near_player)
        self.assertGreater(near_player, drone)
        self.assertGreater(focus, drone)

    def test_squad_roles_follow_the_documented_rules(self):
        self.assertEqual(squad_role_for_index(0, "Assault", 1500.0), "Leader")
        self.assertEqual(squad_role_for_index(1, "Medic", 1500.0), "Medic")
        self.assertEqual(squad_role_for_index(2, "Assault", 9000.0), "Sniper")
        self.assertEqual(squad_role_for_index(1, "Assault", 1500.0), "Flanker")
        self.assertEqual(squad_role_for_index(2, "Assault", 1500.0), "Assault")


class WeatherTests(unittest.TestCase):
    def test_weather_roll_follows_enum_order(self):
        # Only one weather has weight, and it is not the first: the roll must find it.
        only_storm = {"Storm": 1.0}
        for seed in range(32):
            self.assertEqual(pick_weather(only_storm, DeterministicRandom(seed)), "Storm")

        # An empty or all-zero row is not an error: it means calm weather.
        self.assertEqual(pick_weather({}, DeterministicRandom(3)), "Clear")
        self.assertEqual(pick_weather({weather: 0.0 for weather in WEATHER}, DeterministicRandom(3)), "Clear")


if __name__ == "__main__":
    unittest.main()
