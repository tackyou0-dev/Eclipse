// PROJECT ECLIPSE - Deterministic random stream.
//
// Purpose
//   The project's single source of randomness for anything gameplay-affecting: loot
//   rolls, procedural placement, spawn diversification, spread patterns. The algorithm
//   is mirrored exactly by Tests/Reference/eclipse_reference_model.py, so any change here
//   must be made there in the same commit.
//
// Guarantees
//   * Same seed and same call sequence produce the same results on every machine and
//     every platform. No use of engine rand, no use of wall-clock time.
//   * Seed 0 is remapped to a non-zero constant so a zeroed save cannot produce a
//     degenerate stream.

#pragma once

#include "CoreMinimal.h"

/**
 * A small, fast, deterministic 32-bit random stream.
 *
 * The generator is an LCG whose output is passed through a 32-bit hash finaliser
 * (the same splitmix-style mixing the reference model implements). It is not
 * cryptographically secure and must never be used for anything security related; use
 * FPlatformMisc entropy or the session backend for that.
 */
struct ECLIPSE_API FEclipseDeterministicRandom
{
public:
	FEclipseDeterministicRandom() = default;

	/** Construct from a world seed. Zero is remapped; see class comment. */
	explicit FEclipseDeterministicRandom(uint32 InSeed)
	{
		Reset(InSeed);
	}

	/** Restart the stream. Zero is remapped to the 32-bit golden-ratio constant. */
	void Reset(uint32 InSeed)
	{
		State = (InSeed == 0u) ? 0x9E3779B9u : InSeed;
	}

	/** Current raw state. Saved and restored so a stream survives a save/load. */
	uint32 GetState() const { return State; }

	/** Restore a previously captured state. Zero is remapped like Reset. */
	void SetState(uint32 InState) { Reset(InState); }

	/** Next raw 32-bit value. */
	uint32 NextUInt32()
	{
		State = State * 1664525u + 1013904223u;
		uint32 X = State;
		X ^= X >> 16;
		X *= 0x7FEB352Du;
		X ^= X >> 15;
		X *= 0x846CA68Bu;
		X ^= X >> 16;
		return X;
	}

	/** Uniform float in [0, 1). */
	float NextFloat()
	{
		return static_cast<float>(NextUInt32()) / 4294967296.0f;
	}

	/** Uniform float in [Min, Max). Asserts Min < Max in development builds. */
	float Range(float Min, float Max)
	{
		checkf(Max >= Min, TEXT("FEclipseDeterministicRandom::Range called with Max < Min"));
		return Min + (Max - Min) * NextFloat();
	}

	/** Uniform integer in [Min, MaxInclusive]. */
	int32 RangeInt(int32 Min, int32 MaxInclusive)
	{
		checkf(MaxInclusive >= Min, TEXT("FEclipseDeterministicRandom::RangeInt called with Max < Min"));
		const uint32 Span = static_cast<uint32>(MaxInclusive - Min) + 1u;
		return Min + static_cast<int32>(NextUInt32() % Span);
	}

	/** True with probability P (clamped to [0, 1]). */
	bool Chance(float P)
	{
		return NextFloat() < FMath::Clamp(P, 0.0f, 1.0f);
	}

	/**
	 * Pick an index in [0, Count) using a weighted distribution.
	 * Returns INDEX_NONE when Count is not positive or every weight is zero.
	 */
	static int32 WeightedPick(const TArray<float>& Weights, FEclipseDeterministicRandom& Random)
	{
		if (Weights.Num() <= 0)
		{
			return INDEX_NONE;
		}

		float Total = 0.0f;
		for (const float Weight : Weights)
		{
			Total += FMath::Max(0.0f, Weight);
		}

		if (Total <= 0.0f)
		{
			return INDEX_NONE;
		}

		float Roll = Random.Range(0.0f, Total);
		for (int32 Index = 0; Index < Weights.Num(); ++Index)
		{
			Roll -= FMath::Max(0.0f, Weights[Index]);
			if (Roll <= 0.0f)
			{
				return Index;
			}
		}

		return Weights.Num() - 1;
	}

	/**
	 * Derive a new independent stream from a seed and a stable id.
	 *
	 * Used for anything that must be reproducible per-object rather than per-sequence:
	 * container contents, PCG chunk dressing, spawn-table selection. Container contents
	 * use MixSeed(WorldSeed, StableId), which is the contract the reference model tests.
	 */
	static uint32 MixSeed(uint32 Seed, uint32 StableId)
	{
		uint32 Hash = Seed ^ (StableId + 0x9E3779B9u + (Seed << 6) + (Seed >> 2));
		Hash *= 0x85EBCA6Bu;
		Hash ^= Hash >> 13;
		Hash *= 0xC2B2AE35u;
		Hash ^= Hash >> 16;
		return Hash;
	}

	/** Convenience: build a stream for a stable id under a world seed. */
	static FEclipseDeterministicRandom ForStableId(uint32 Seed, uint32 StableId)
	{
		return FEclipseDeterministicRandom(MixSeed(Seed, StableId));
	}

	/**
	 * Stable id for a string key. Uses FNV-1a so the same identifier produces the same
	 * id in C++ and in the Python reference model.
	 */
	static uint32 StableIdFromString(const FString& Key)
	{
		uint32 Hash = 2166136261u;
		for (int32 Index = 0; Index < Key.Len(); ++Index)
		{
			Hash ^= static_cast<uint32>(Key[Index]);
			Hash *= 16777619u;
		}
		return Hash;
	}

private:
	/** Internal state. Never read directly by gameplay code. */
	uint32 State = 0x9E3779B9u;
};
