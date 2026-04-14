#pragma once
// SeasonThresholds.h -- Named constants for season property checks.
//
// Multiple systems compare SeasonDef::heatDrainMod and SeasonDef::productionMod
// against hard-coded float values.  This header replaces those magic numbers
// with self-documenting constants that modders can tune in one place.
//
// heatDrainMod ranges from 0 (summer, no cold drain) to ~1.0 (deep winter).
// productionMod is a multiplier on facility output: 1.0 = baseline.

namespace SeasonThreshold {

// ---- Heat-drain thresholds (heatDrainMod) ----

// Harsh cold: winter-like season with significant cold pressure.
// Effects: schedule contraction (earlier sleep, later wake, shorter work day),
//          stronger work-song affinity bonds, fireside song variant,
//          migration penalty, icy-blue sky tint.
static constexpr float HARSH_COLD = 0.8f;

// Moderate cold: autumn-like season with noticeable chill.
// Effects: amber/orange sky tint.
static constexpr float MODERATE_COLD = 0.3f;

// Cold season: any season where wood becomes an essential heating fuel.
// Effects: wood shortage included in settlement health ring assessment.
static constexpr float COLD_SEASON = 0.2f;

// Mild cold: spring-like season with barely perceptible chill.
// Effects: slight green sky tint.
static constexpr float MILD_COLD = 0.05f;

// ---- Production-mod thresholds (productionMod) ----

// Harvest season: high-production season (summer bumper crops).
// Effects: more frequent work shanties, harvest shanty song variant.
static constexpr float HARVEST_SEASON = 1.1f;

// Low production: scarce-output season (deep winter).
// Effects: food price floor doubles.
static constexpr float LOW_PRODUCTION = 0.5f;

} // namespace SeasonThreshold
