// SkillRateTest.cpp — unit tests for WorldSchema::SkillGrowthRate / SkillDecayRate
// Compile: g++ -std=c++17 -I src tests/SkillRateTest.cpp -o build/SkillRateTest
//
// Verifies that out-of-range SkillIDs return the documented fallback values
// and that valid IDs return the rates set in the SkillDef.

#include "World/WorldSchema.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static int passed = 0;

#define TEST(name) static void name()
#define RUN(name) do { name(); ++passed; std::printf("  PASS  %s\n", #name); } while(0)

// Tolerance for floating-point comparisons
static bool feq(float a, float b) { return std::fabs(a - b) < 1e-6f; }

// ---------------------------------------------------------------------------
// Helper: build a minimal WorldSchema with 2 skills
// ---------------------------------------------------------------------------

static WorldSchema makeSchema() {
    WorldSchema ws;

    SkillDef s0;
    s0.name       = "farming";
    s0.growthRate = 2.5f;
    s0.decayRate  = 0.01f;

    SkillDef s1;
    s1.name       = "woodcutting";
    s1.growthRate = 0.8f;
    s1.decayRate  = 0.005f;

    ws.skills.push_back(s0);
    ws.skills.push_back(s1);
    ws.BuildMaps();  // assigns IDs
    return ws;
}

// ---------------------------------------------------------------------------
// SkillGrowthRate — valid IDs
// ---------------------------------------------------------------------------

TEST(growthRate_valid_0) {
    auto ws = makeSchema();
    assert(feq(ws.SkillGrowthRate(0), 2.5f));
}

TEST(growthRate_valid_1) {
    auto ws = makeSchema();
    assert(feq(ws.SkillGrowthRate(1), 0.8f));
}

// ---------------------------------------------------------------------------
// SkillGrowthRate — out-of-range IDs (fallback = 1.0f)
// ---------------------------------------------------------------------------

TEST(growthRate_INVALID_ID) {
    auto ws = makeSchema();
    assert(feq(ws.SkillGrowthRate(INVALID_ID), 1.0f));
}

TEST(growthRate_neg1) {
    auto ws = makeSchema();
    assert(feq(ws.SkillGrowthRate(-1), 1.0f));
}

TEST(growthRate_999) {
    auto ws = makeSchema();
    assert(feq(ws.SkillGrowthRate(999), 1.0f));
}

// ---------------------------------------------------------------------------
// SkillDecayRate — valid IDs
// ---------------------------------------------------------------------------

TEST(decayRate_valid_0) {
    auto ws = makeSchema();
    assert(feq(ws.SkillDecayRate(0), 0.01f));
}

TEST(decayRate_valid_1) {
    auto ws = makeSchema();
    assert(feq(ws.SkillDecayRate(1), 0.005f));
}

// ---------------------------------------------------------------------------
// SkillDecayRate — out-of-range IDs (fallback = 0.0005f, SKILL_RUST legacy)
// ---------------------------------------------------------------------------

TEST(decayRate_INVALID_ID) {
    auto ws = makeSchema();
    assert(feq(ws.SkillDecayRate(INVALID_ID), 0.0005f));
}

TEST(decayRate_neg1) {
    auto ws = makeSchema();
    assert(feq(ws.SkillDecayRate(-1), 0.0005f));
}

TEST(decayRate_999) {
    auto ws = makeSchema();
    assert(feq(ws.SkillDecayRate(999), 0.0005f));
}

// ---------------------------------------------------------------------------

int main() {
    std::printf("Running SkillRate bounds-check tests...\n\n");

    // SkillGrowthRate — valid
    RUN(growthRate_valid_0);
    RUN(growthRate_valid_1);

    // SkillGrowthRate — out-of-range
    RUN(growthRate_INVALID_ID);
    RUN(growthRate_neg1);
    RUN(growthRate_999);

    // SkillDecayRate — valid
    RUN(decayRate_valid_0);
    RUN(decayRate_valid_1);

    // SkillDecayRate — out-of-range
    RUN(decayRate_INVALID_ID);
    RUN(decayRate_neg1);
    RUN(decayRate_999);

    std::printf("\nAll %d tests passed.\n", passed);
    return 0;
}
