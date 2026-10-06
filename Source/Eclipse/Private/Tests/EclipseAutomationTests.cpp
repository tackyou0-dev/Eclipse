// PROJECT ECLIPSE - In-engine automation tests.
//
// Purpose
//   The engine-side twin of Tests/Reference. These tests exercise the C++ implementations
//   of the algorithms the Python reference model also mirrors (the deterministic random
//   stream, the damage pipeline, trade pricing), so a change to either side that breaks
//   the contract fails where it was made.
//
// Running
//   These need an editor or a commandlet: `UnrealEditor-Cmd.exe <project> -ExecCmds=
//   "Automation RunTests Eclipse; Quit" -unattended -nullrhi`. CI cannot run them (no
//   engine on the runners), which is exactly why the same maths is asserted in
//   Tests/Reference/eclipse_reference_model.py.

#include "Combat/EclipseDamageLibrary.h"
#include "Core/EclipseDeterministicRandom.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEclipseDeterministicRandomTest,
	"Eclipse.Core.DeterministicRandom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEclipseDeterministicRandomTest::RunTest(const FString& Parameters)
{
	// Seed zero is remapped, so the first draw is always the golden-ratio constant's
	// successor rather than the seed itself. This is the contract the reference model
	// asserts with the number 0x9E3779B9.
	FEclipseDeterministicRandom Random(0u);
	TestEqual(TEXT("Zero seed is remapped to the golden ratio constant."), Random.GetState(), 0x9E3779B9u);

	// The same seed gives the same stream, and two streams agree draw for draw.
	FEclipseDeterministicRandom Left(20261006u);
	FEclipseDeterministicRandom Right(20261006u);
	for (int32 Index = 0; Index < 32; ++Index)
	{
		TestEqual(FString::Printf(TEXT("Draw %d matches between identical seeds."), Index), Left.NextUInt32(), Right.NextUInt32());
	}

	// MixSeed must separate keys: two different stable ids may not collide into one stream.
	TestNotEqual(TEXT("Different stable ids mix to different seeds."),
		FEclipseDeterministicRandom::MixSeed(1u, 2u),
		FEclipseDeterministicRandom::MixSeed(1u, 3u));

	// StableIdFromString is FNV-1a: the empty string hashes to the FNV offset basis.
	TestEqual(TEXT("Stable id of the empty string is the FNV offset basis."),
		FEclipseDeterministicRandom::StableIdFromString(FString()), 2166136261u);

	// Chance is a threshold on NextFloat, so a zero probability never fires and a one
	// probability always does.
	FEclipseDeterministicRandom ChanceRandom(7u);
	for (int32 Index = 0; Index < 64; ++Index)
	{
		TestFalse(TEXT("Chance(0) never fires."), ChanceRandom.Chance(0.0f));
		TestTrue(TEXT("Chance(1) always fires."), ChanceRandom.Chance(1.0f));
	}

	// WeightedPick never returns an index outside the array, and never returns an empty
	// slot from a table that only has one non-zero weight.
	const TArray<float> Weights = { 0.0f, 0.0f, 5.0f };
	FEclipseDeterministicRandom PickRandom(99u);
	for (int32 Index = 0; Index < 64; ++Index)
	{
		TestEqual(TEXT("Weighted pick lands on the only non-zero entry."), FEclipseDeterministicRandom::WeightedPick(Weights, PickRandom), 2);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEclipseDamageFormulaTest,
	"Eclipse.Combat.DamageFormula",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEclipseDamageFormulaTest::RunTest(const FString& Parameters)
{
	// Armour mitigation is A / (A + K) with K = 100, and it is capped.
	TestEqual(TEXT("Zero armour does not mitigate."), UEclipseDamageLibrary::ComputeArmorMitigation(0.0f, 0.0f), 0.0f);
	TestEqual(TEXT("100 armour mitigates half."), UEclipseDamageLibrary::ComputeArmorMitigation(100.0f, 0.0f), 0.5f);
	TestTrue(TEXT("Mitigation is capped below 1."), UEclipseDamageLibrary::ComputeArmorMitigation(1000000.0f, 0.0f) <= UEclipseDamageLibrary::MaxArmorMitigation);

	// Penetration reduces effective armour before the curve, and full penetration removes
	// armour entirely.
	TestTrue(TEXT("Penetration lowers mitigation."),
		UEclipseDamageLibrary::ComputeArmorMitigation(100.0f, 0.5f) < UEclipseDamageLibrary::ComputeArmorMitigation(100.0f, 0.0f));
	TestEqual(TEXT("Full penetration ignores armour."), UEclipseDamageLibrary::ComputeArmorMitigation(500.0f, 1.0f), 0.0f);

	// The full pipeline: Base * Falloff * WeakPoint * Critical * (1 - Mitigation) * Resistance.
	FEclipseDamageContext Context;
	Context.BaseDamage = 100.0f;
	Context.DamageType = EEclipseDamageType::Physical;
	Context.DistanceFalloff = 1.0f;
	Context.WeakPointMultiplier = 2.0f;
	Context.CriticalMultiplier = 1.5f;
	Context.ArmorPenetration = 0.0f;

	FEclipseResistanceProfile Profile;
	float Mitigation = 0.0f;
	const float Damage = UEclipseDamageLibrary::ComputeFinalDamage(Context, 100.0f, Profile, Mitigation);

	// 100 * 1 * 2 * 1.5 * 0.5 * 1.0 = 150.
	TestEqual(TEXT("Full damage formula."), Damage, 150.0f);
	TestEqual(TEXT("Reported mitigation matches the armour curve."), Mitigation, 0.5f);

	// Distance falloff clamps at the floor rather than reaching zero, so a shot at maximum
	// range still does something.
	TestEqual(TEXT("Falloff inside effective range is 1."),
		UEclipseDamageLibrary::ComputeDistanceFalloff(500.0f, 1000.0f, 5000.0f), 1.0f);
	TestEqual(TEXT("Falloff at max range is the floor."),
		UEclipseDamageLibrary::ComputeDistanceFalloff(5000.0f, 1000.0f, 5000.0f), UEclipseDamageLibrary::MinimumRangeFalloff);
	TestEqual(TEXT("Falloff beyond max range is still the floor."),
		UEclipseDamageLibrary::ComputeDistanceFalloff(9000.0f, 1000.0f, 5000.0f), UEclipseDamageLibrary::MinimumRangeFalloff);

	// Always-on: a context with no damage is invalid and must not produce a result.
	FEclipseDamageContext EmptyContext;
	TestFalse(TEXT("A zero-damage context is invalid."), EmptyContext.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEclipseTimeOfDayTest,
	"Eclipse.World.TimeOfDayBands",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEclipseTimeOfDayTest::RunTest(const FString& Parameters)
{
	// Bands: Dawn 05-08, Day 08-18, Dusk 18-21, Night 21-05.
	TestTrue(TEXT("04:00 is night."), EclipseTimeOfDay::FromHour(4.0f) == EEclipseTimeOfDay::Night);
	TestTrue(TEXT("06:00 is dawn."), EclipseTimeOfDay::FromHour(6.0f) == EEclipseTimeOfDay::Dawn);
	TestTrue(TEXT("12:00 is day."), EclipseTimeOfDay::FromHour(12.0f) == EEclipseTimeOfDay::Day);
	TestTrue(TEXT("19:00 is dusk."), EclipseTimeOfDay::FromHour(19.0f) == EEclipseTimeOfDay::Dusk);
	TestTrue(TEXT("23:00 is night."), EclipseTimeOfDay::FromHour(23.0f) == EEclipseTimeOfDay::Night);

	// Hours wrap rather than clamping, so a clock that runs past midnight keeps counting.
	TestEqual(TEXT("25 hours wraps to 1."), EclipseTimeOfDay::WrapHour(25.0f), 1.0f);
	TestEqual(TEXT("Negative hours wrap forwards."), EclipseTimeOfDay::WrapHour(-1.0f), 23.0f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
