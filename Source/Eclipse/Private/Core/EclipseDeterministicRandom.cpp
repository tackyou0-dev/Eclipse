// PROJECT ECLIPSE - Deterministic random stream implementation.
//
// Purpose
//   The stream is header-only by design so the compiler can inline it in hot paths, but
//   a translation unit is kept so the type has a stable home for future non-inline work
//   (for example a serialisation helper) and so unit tests can link against it.

#include "Core/EclipseDeterministicRandom.h"

// Intentionally empty: FEclipseDeterministicRandom is fully inline. This file exists so
// that the type is covered by the module's unity build and so that future helpers have
// an obvious home.
