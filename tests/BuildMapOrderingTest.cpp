// BuildMapOrderingTest.cpp — verify that calling BuildProfessionToSkillMap(),
// BuildResourceToSkillMap(), or InitDerivedData() before ResolveCrossRefs()
// aborts the process with a diagnostic message on stderr.
//
// Build via CMake target BuildMapOrderingTest (see CMakeLists.txt).
// Run:  ./build/BuildMapOrderingTest
//
// The ordering contract: ResolveCrossRefs() must be called before
// InitDerivedData() (or the individual Build*Map() methods).  Violation
// is a fatal programmer error — the guard calls std::abort().

#include "World/WorldSchema.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

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
// Helper: fork a child, run `fn`, and verify the child was killed by a signal
// (SIGABRT).  Optionally capture the child's stderr into `stderrOut`.
// Returns true if the child was signalled (i.e. aborted as expected).
// ---------------------------------------------------------------------------

static bool expectAbort(void (*fn)(), std::string* stderrOut = nullptr) {
    int pipeFds[2] = {-1, -1};
    if (stderrOut) {
        if (pipe(pipeFds) != 0) return false;
    }

    pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0) {
        // Child: redirect stderr to pipe if requested, then run fn (should abort)
        if (stderrOut) {
            close(pipeFds[0]);  // close read end
            dup2(pipeFds[1], fileno(stderr));
            close(pipeFds[1]);
        }
        fn();
        // If fn() returns, exit normally — the parent will detect this as failure
        _exit(0);
    }

    // Parent
    if (stderrOut) {
        close(pipeFds[1]);  // close write end
        char buf[1024];
        ssize_t n;
        while ((n = read(pipeFds[0], buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            stderrOut->append(buf);
        }
        close(pipeFds[0]);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    // The child should have been killed by SIGABRT (signal 6)
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

// ---------------------------------------------------------------------------
// BuildProfessionToSkillMap — guard aborts when crossRefsResolved is false
// ---------------------------------------------------------------------------

static void callBuildProfessionToSkillMap() {
    auto ws = makeSchema();
    ws.BuildProfessionToSkillMap();
}

TEST(professionToSkillMap_aborts_before_crossRefs) {
    std::string captured;
    bool aborted = expectAbort(callBuildProfessionToSkillMap, &captured);
    assert(aborted && "BuildProfessionToSkillMap should abort when crossRefsResolved is false");
    assert(captured.find("BuildProfessionToSkillMap") != std::string::npos
           && "Stderr message should mention BuildProfessionToSkillMap");
    assert(captured.find("before ResolveCrossRefs()") != std::string::npos
           && "Stderr message should mention ResolveCrossRefs ordering");
}

// ---------------------------------------------------------------------------
// BuildResourceToSkillMap — guard aborts when crossRefsResolved is false
// ---------------------------------------------------------------------------

static void callBuildResourceToSkillMap() {
    auto ws = makeSchema();
    ws.BuildResourceToSkillMap();
}

TEST(resourceToSkillMap_aborts_before_crossRefs) {
    std::string captured;
    bool aborted = expectAbort(callBuildResourceToSkillMap, &captured);
    assert(aborted && "BuildResourceToSkillMap should abort when crossRefsResolved is false");
    assert(captured.find("BuildResourceToSkillMap") != std::string::npos
           && "Stderr message should mention BuildResourceToSkillMap");
    assert(captured.find("before ResolveCrossRefs()") != std::string::npos
           && "Stderr message should mention ResolveCrossRefs ordering");
}

// ---------------------------------------------------------------------------
// InitDerivedData — guard aborts when crossRefsResolved is false
// ---------------------------------------------------------------------------

static void callInitDerivedData() {
    auto ws = makeSchema();
    ws.InitDerivedData();
}

TEST(initDerivedData_aborts_before_crossRefs) {
    std::string captured;
    bool aborted = expectAbort(callInitDerivedData, &captured);
    assert(aborted && "InitDerivedData should abort when crossRefsResolved is false");
    assert(captured.find("InitDerivedData") != std::string::npos
           && "Stderr message should mention InitDerivedData");
    assert(captured.find("before ResolveCrossRefs()") != std::string::npos
           && "Stderr message should mention ResolveCrossRefs ordering");
}

// ---------------------------------------------------------------------------
// Both maps build successfully when crossRefsResolved is true
// ---------------------------------------------------------------------------

TEST(professionToSkillMap_succeeds_after_crossRefs) {
    auto ws = makeSchema();

    // Known test gap: we set the flag directly instead of calling the real
    // ResolveCrossRefs(), which is a static function inside WorldLoader.cpp
    // that re-parses TOML files from disk.  Calling it here would require
    // the full TOML world directory (needs.toml, skills.toml, professions.toml,
    // etc.).  Our makeSchema() already populates integer IDs directly, so the
    // maps build correctly, but this does not exercise the TOML-to-ID
    // resolution path.
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

    // Known test gap: see comment in professionToSkillMap_succeeds_after_crossRefs.
    ws.crossRefsResolved = true;

    ws.BuildResourceToSkillMap();

    assert(!ws.resourceToSkill.empty()
           && "resourceToSkill should be populated after crossRefsResolved = true");
    assert(ws.resourceToSkill.size() == ws.resources.size());
    // Food (resource 0) should map to farming (skill 0)
    assert(ws.resourceToSkill[0] == 0);
}

// ---------------------------------------------------------------------------
// InitDerivedData builds both maps when crossRefsResolved is true
// ---------------------------------------------------------------------------

TEST(initDerivedData_builds_both_maps) {
    auto ws = makeSchema();
    ws.crossRefsResolved = true;

    ws.InitDerivedData();

    // resourceToSkill
    assert(!ws.resourceToSkill.empty()
           && "resourceToSkill should be populated by InitDerivedData");
    assert(ws.resourceToSkill.size() == ws.resources.size());
    assert(ws.resourceToSkill[0] == 0);

    // professionToSkill
    assert(!ws.professionToSkill.empty()
           && "professionToSkill should be populated by InitDerivedData");
    assert(ws.professionToSkill.size() == ws.professions.size());
    assert(ws.professionToSkill[0] == 0);
}

// ---------------------------------------------------------------------------

int main() {
    std::printf("Running BuildMap ordering-guard tests...\n\n");

    // Guard aborts before cross-refs (with diagnostic on stderr)
    RUN(professionToSkillMap_aborts_before_crossRefs);
    RUN(resourceToSkillMap_aborts_before_crossRefs);
    RUN(initDerivedData_aborts_before_crossRefs);

    // Maps build correctly after cross-refs
    RUN(professionToSkillMap_succeeds_after_crossRefs);
    RUN(resourceToSkillMap_succeeds_after_crossRefs);

    // InitDerivedData builds both maps in one call
    RUN(initDerivedData_builds_both_maps);

    std::printf("\nAll %d tests passed.\n", passed);
    return 0;
}
