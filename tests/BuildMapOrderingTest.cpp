// BuildMapOrderingTest.cpp — verify that calling BuildProfessionToSkillMap()
// or BuildResourceToSkillMap() before ResolveCrossRefs() triggers the ordering
// guard (early return without building the map, error printed to stderr).
//
// Build via CMake target BuildMapOrderingTest (see CMakeLists.txt).
// Run:  ./build/BuildMapOrderingTest
//
// The ordering contract: ResolveCrossRefs() must be called before either
// Build*Map() method.  Both methods check `crossRefsResolved` and silently
// return if it is false (with an error message to stderr).  This test
// documents that contract explicitly.

#include "World/WorldSchema.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

static int passed = 0;

#define TEST(name) static void name()
#define RUN(name) do { name(); ++passed; std::printf("  PASS  %s\n", #name); } while(0)

// ---------------------------------------------------------------------------
// Helper: build a minimal WorldSchema with 1 resource, 1 skill, 1 profession
// ---------------------------------------------------------------------------

static WorldSchema makeSchema() {
    WorldSchema ws;

    ResourceDef r;
    r.name      = "Food";
    r.basePrice = 1.0f;
    ws.resources.push_back(r);

    SkillDef s;
    s.name        = "farming";
    s.forResource = 0;  // points at ResourceID 0 ("Food")
    ws.skills.push_back(s);

    ProfessionDef p;
    p.name             = "Farmer";
    p.producesResource = 0;   // produces "Food" (ResourceID 0)
    p.primarySkill     = 0;   // uses "farming" (SkillID 0)
    ws.professions.push_back(p);

    ws.BuildMaps();  // assigns IDs and builds name lookup maps
    return ws;
}

// ---------------------------------------------------------------------------
// BuildProfessionToSkillMap — guard blocks when crossRefsResolved is false
// ---------------------------------------------------------------------------

TEST(professionToSkillMap_blocked_before_crossRefs) {
    auto ws = makeSchema();

    // crossRefsResolved is false by default (ResolveCrossRefs not called)
    assert(!ws.crossRefsResolved);

    // Calling BuildProfessionToSkillMap should silently return
    ws.BuildProfessionToSkillMap();

    // The map must remain empty (the function returned early)
    assert(ws.professionToSkill.empty()
           && "professionToSkill should be empty when called before ResolveCrossRefs()");
}

// ---------------------------------------------------------------------------
// BuildResourceToSkillMap — guard blocks when crossRefsResolved is false
// ---------------------------------------------------------------------------

TEST(resourceToSkillMap_blocked_before_crossRefs) {
    auto ws = makeSchema();

    assert(!ws.crossRefsResolved);

    ws.BuildResourceToSkillMap();

    assert(ws.resourceToSkill.empty()
           && "resourceToSkill should be empty when called before ResolveCrossRefs()");
}

// ---------------------------------------------------------------------------
// Both maps build successfully when crossRefsResolved is true
// ---------------------------------------------------------------------------

TEST(professionToSkillMap_succeeds_after_crossRefs) {
    auto ws = makeSchema();

    // Simulate that ResolveCrossRefs() has run by setting the flag.
    // In real code, ResolveCrossRefs() also resolves string references into
    // integer IDs, but our makeSchema() already set them directly.
    ws.crossRefsResolved = true;

    ws.BuildProfessionToSkillMap();

    assert(!ws.professionToSkill.empty()
           && "professionToSkill should be populated after crossRefsResolved = true");
    assert(ws.professionToSkill.size() == ws.professions.size());
    // Farmer (profession 0) should map to farming (skill 0)
    assert(ws.professionToSkill[0] == 0);
}

TEST(resourceToSkillMap_succeeds_after_crossRefs) {
    auto ws = makeSchema();

    ws.crossRefsResolved = true;

    ws.BuildResourceToSkillMap();

    assert(!ws.resourceToSkill.empty()
           && "resourceToSkill should be populated after crossRefsResolved = true");
    assert(ws.resourceToSkill.size() == ws.resources.size());
    // Food (resource 0) should map to farming (skill 0)
    assert(ws.resourceToSkill[0] == 0);
}

// ---------------------------------------------------------------------------
// Stderr output — verify the guard prints a diagnostic message.
// We redirect stderr to a temporary file, call the guarded function, then
// check the file contents for the expected error string.
// ---------------------------------------------------------------------------

TEST(professionToSkillMap_prints_stderr_before_crossRefs) {
    auto ws = makeSchema();
    assert(!ws.crossRefsResolved);

    // Redirect stderr to a temp file
    FILE* origStderr = stderr;
    FILE* tmp = tmpfile();
    assert(tmp && "tmpfile() failed");

    // Swap stderr to the temp file
    int origFd = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    ws.BuildProfessionToSkillMap();

    // Restore stderr
    fflush(stderr);
    dup2(origFd, fileno(stderr));
    close(origFd);

    // Read back the captured output
    fseek(tmp, 0, SEEK_END);
    long sz = ftell(tmp);
    assert(sz > 0 && "Expected stderr output from the ordering guard");
    fseek(tmp, 0, SEEK_SET);

    std::string captured(sz, '\0');
    fread(&captured[0], 1, sz, tmp);
    fclose(tmp);

    assert(captured.find("BuildProfessionToSkillMap") != std::string::npos
           && "Stderr message should mention BuildProfessionToSkillMap");
    assert(captured.find("before ResolveCrossRefs()") != std::string::npos
           && "Stderr message should mention ResolveCrossRefs ordering");
}

TEST(resourceToSkillMap_prints_stderr_before_crossRefs) {
    auto ws = makeSchema();
    assert(!ws.crossRefsResolved);

    FILE* tmp = tmpfile();
    assert(tmp && "tmpfile() failed");

    int origFd = dup(fileno(stderr));
    dup2(fileno(tmp), fileno(stderr));

    ws.BuildResourceToSkillMap();

    fflush(stderr);
    dup2(origFd, fileno(stderr));
    close(origFd);

    fseek(tmp, 0, SEEK_END);
    long sz = ftell(tmp);
    assert(sz > 0 && "Expected stderr output from the ordering guard");
    fseek(tmp, 0, SEEK_SET);

    std::string captured(sz, '\0');
    fread(&captured[0], 1, sz, tmp);
    fclose(tmp);

    assert(captured.find("BuildResourceToSkillMap") != std::string::npos
           && "Stderr message should mention BuildResourceToSkillMap");
    assert(captured.find("before ResolveCrossRefs()") != std::string::npos
           && "Stderr message should mention ResolveCrossRefs ordering");
}

// ---------------------------------------------------------------------------

int main() {
    std::printf("Running BuildMap ordering-guard tests...\n\n");

    // Guard blocks before cross-refs
    RUN(professionToSkillMap_blocked_before_crossRefs);
    RUN(resourceToSkillMap_blocked_before_crossRefs);

    // Maps build correctly after cross-refs
    RUN(professionToSkillMap_succeeds_after_crossRefs);
    RUN(resourceToSkillMap_succeeds_after_crossRefs);

    // Guard prints diagnostic to stderr
    RUN(professionToSkillMap_prints_stderr_before_crossRefs);
    RUN(resourceToSkillMap_prints_stderr_before_crossRefs);

    std::printf("\nAll %d tests passed.\n", passed);
    return 0;
}
