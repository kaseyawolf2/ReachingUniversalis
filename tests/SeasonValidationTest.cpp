// SeasonValidationTest.cpp — verify that LoadSeasons() warns on inverted thresholds.
//
// Build via CMake target SeasonValidationTest (see CMakeLists.txt).
// Run:  ./build/SeasonValidationTest
//
// This test loads tests/test_seasons_invalid.toml (which has every cold threshold
// ordering inverted) through WorldLoader::Load() and verifies that the expected
// warning messages appear in the collected warnings vector.
//
// The Load() call may return false (missing other TOML files in the test
// directory), but the season-threshold warnings are emitted before cross-
// reference resolution, so the warnings vector is populated regardless.

#include "World/WorldLoader.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int passed = 0;
static int failed = 0;

#define ASSERT_TRUE(cond, msg)                                          \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::fprintf(stderr, "  FAIL  %s\n        %s\n", msg, #cond); \
            ++failed;                                                   \
        } else {                                                        \
            std::printf("  PASS  %s\n", msg);                           \
            ++passed;                                                   \
        }                                                               \
    } while (0)

// Return true if any warning message contains the given substring.
static bool HasWarning(const std::vector<LoadWarning>& warnings,
                       const char* substring) {
    for (const auto& w : warnings) {
        if (w.message.find(substring) != std::string::npos)
            return true;
    }
    return false;
}

// Return true if any warning has the given category.
static bool HasCategory(const std::vector<LoadWarning>& warnings,
                        const char* category) {
    for (const auto& w : warnings) {
        if (w.category == category)
            return true;
    }
    return false;
}

int main() {
    std::printf("Running season threshold validation tests...\n\n");

    // --- Locate the test TOML directory ---
    // The test TOML lives in tests/, which we treat as a pseudo world directory.
    // WorldLoader::Load expects a directory; it will look for seasons.toml inside.
    // We copy our invalid TOML into a temp directory named seasons.toml.

    // Determine the project root: walk up from the executable or use cwd.
    // The CMake build runs from the project root, so "tests" should be relative.
    std::string testDir;

    // Try a few likely locations for the test data directory.
    for (const char* candidate : {"tests", "../tests", "../../tests"}) {
        if (fs::exists(std::string(candidate) + "/test_seasons_invalid.toml")) {
            testDir = candidate;
            break;
        }
    }
    if (testDir.empty()) {
        std::fprintf(stderr, "ERROR: cannot find tests/test_seasons_invalid.toml "
                             "from working directory %s\n",
                     fs::current_path().c_str());
        return 1;
    }

    // WorldLoader::Load expects <dir>/seasons.toml.  Our test file is named
    // test_seasons_invalid.toml, so we create a temporary directory with a
    // symlink (or copy) named seasons.toml pointing to our test file.
    std::string tmpDir = "build/test_season_tmpdir";
    fs::create_directories(tmpDir);

    // Copy the invalid TOML as seasons.toml in the temp directory.
    fs::copy_file(testDir + "/test_seasons_invalid.toml",
                  tmpDir + "/seasons.toml",
                  fs::copy_options::overwrite_existing);

    // --- Load with the invalid config ---
    WorldSchema schema;
    std::string errorMsg;
    std::vector<LoadWarning> warnings;

    std::fprintf(stderr, "\n--- Expected warnings follow (part of the test) ---\n");
    bool ok = WorldLoader::Load(tmpDir, schema, errorMsg, &warnings);
    std::fprintf(stderr, "--- End expected warnings ---\n\n");

    // Load may fail due to missing needs.toml etc. during cross-reference
    // resolution, but the season warnings should still have been collected.
    // We do NOT assert on ok; we only care about the warnings.
    (void)ok;

    // --- Verify all expected warnings were emitted ---

    ASSERT_TRUE(!warnings.empty(),
                "At least one warning emitted");

    ASSERT_TRUE(HasCategory(warnings, "seasons"),
                "At least one warning has category 'seasons'");

    // Pairwise ordering: mild_cold (0.800) should be less than cold_season (0.300)
    ASSERT_TRUE(HasWarning(warnings, "mild_cold") &&
                HasWarning(warnings, "should be less than cold_season"),
                "mild_cold >= cold_season pairwise warning");

    // Pairwise ordering: cold_season (0.300) should be less than moderate_cold (0.100)
    ASSERT_TRUE(HasWarning(warnings, "cold_season") &&
                HasWarning(warnings, "should be less than moderate_cold"),
                "cold_season >= moderate_cold pairwise warning");

    // Pairwise ordering: moderate_cold (0.100) should be less than harsh_cold (0.050)
    ASSERT_TRUE(HasWarning(warnings, "moderate_cold") &&
                HasWarning(warnings, "should be less than harsh_cold"),
                "moderate_cold >= harsh_cold pairwise warning");

    // Top-level ordering invariant: mild_cold < moderate_cold
    ASSERT_TRUE(HasWarning(warnings, "cold threshold ordering violated"),
                "Top-level cold threshold ordering violated warning");

    // Production threshold ordering: low_production >= harvest_season
    ASSERT_TRUE(HasWarning(warnings, "production threshold ordering violated"),
                "Production threshold ordering violated warning");

    // Count season-category warnings: we expect exactly 5
    int seasonWarnings = 0;
    for (const auto& w : warnings) {
        if (w.category == "seasons" && w.level == LoadWarningLevel::Warning)
            ++seasonWarnings;
    }
    ASSERT_TRUE(seasonWarnings == 5,
                "Exactly 5 season threshold warnings emitted");

    // --- Cleanup ---
    fs::remove_all(tmpDir);

    // --- Summary ---
    std::printf("\n%d passed, %d failed\n", passed, failed);
    if (failed > 0) {
        std::printf("\nSome tests FAILED.\n");
        return 1;
    }
    std::printf("\nAll tests passed.\n");
    return 0;
}
