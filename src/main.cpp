#include "raylib.h"
#include "GameState.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <map>
#include <string>
#include <vector>

static constexpr int DESIGN_W = 1280;
static constexpr int DESIGN_H = 720;

// --------------------------------------------------------------------
// Benchmark mode: run sim at max speed, sample stats, dump report.
// Usage: ./ReachingUniversalis --benchmark <seconds> [output.txt]
// --------------------------------------------------------------------
static void RunBenchmark(int durationSec, const char* outFile) {
    InitWindow(DESIGN_W, DESIGN_H, "ReachingUniversalis [benchmark]");
    SetTargetFPS(0);

    GameState state;

    // Set max tick speed directly for benchmark
    state.SetTickSpeed(128);
    state.Update(1.f / 60.f);
    BeginDrawing(); ClearBackground(BLACK); EndDrawing();

    auto startTime = std::chrono::steady_clock::now();
    int  sampleCount = 0;

    // Accumulators
    double sumStepsPerSec = 0;
    int    maxPop = 0, minPop = 999999;
    double sumPop = 0;
    double sumGold = 0, sumAvgWealth = 0;
    float  peakWealth = 0.f;
    std::string peakWealthName;
    int    lastDay = 0, lastDeaths = 0;

    // Per-system profiler accumulators
    struct ProfAccum { double totalUs = 0; int count = 0; std::vector<float> samples; };
    std::map<std::string, ProfAccum> profAccum;

    // Settlement tracking
    struct SettlStats { double sumPop = 0; double sumMorale = 0; int samples = 0; };
    std::map<std::string, SettlStats> settlStats;

    // Per-agent wealth samples (snapshot at end)
    struct AgentSample { std::string name; float gold; std::string goal; bool atHome; };
    std::vector<AgentSample> finalAgents;

    fprintf(stderr, "[benchmark] Running for %d seconds at max tick speed...\n", durationSec);

    while (true) {
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration<float>(now - startTime).count();
        if (elapsed >= (float)durationSec) break;

        float dt = GetFrameTime();
        if (dt <= 0.f) dt = 1.f / 60.f;
        state.Update(dt);
        BeginDrawing(); ClearBackground(BLACK); EndDrawing();

        // Sample every ~1s of wall time
        if (elapsed >= sampleCount * 1.0f) {
            ++sampleCount;
            const auto& snap = state.Snapshot();
            std::lock_guard<std::mutex> lock(snap.mutex);

            sumStepsPerSec += snap.simStepsPerSec;
            sumPop += snap.population;
            if (snap.population > maxPop) maxPop = snap.population;
            if (snap.population < minPop) minPop = snap.population;
            sumGold += snap.econTotalGold;
            sumAvgWealth += snap.econAvgNpcWealth;
            if (snap.econRichestWealth > peakWealth) {
                peakWealth = snap.econRichestWealth;
                peakWealthName = snap.econRichestName;
            }
            lastDay = snap.day;
            lastDeaths = snap.totalDeaths;

            // Profiler
            for (const auto& p : snap.profiling) {
                auto& acc = profAccum[p.name];
                acc.totalUs += p.avgUs;
                acc.samples.push_back(p.avgUs);
                ++acc.count;
            }

            // Settlements
            for (const auto& s : snap.settlements) {
                auto& ss = settlStats[s.name];
                ss.sumPop += s.pop;
                ss.sumMorale += s.morale;
                ++ss.samples;
            }

            fprintf(stderr, "  [%.0fs] day=%d pop=%d deaths=%d steps/s=%d gold=%.0f\n",
                    elapsed, snap.day, snap.population, snap.totalDeaths,
                    snap.simStepsPerSec, snap.econTotalGold);
        }
    }

    // Final agent snapshot
    {
        const auto& snap = state.Snapshot();
        std::lock_guard<std::mutex> lock(snap.mutex);
        for (const auto& a : snap.agents) {
            if (a.role == RenderSnapshot::AgentRole::Player) continue;
            finalAgents.push_back({ a.npcName, a.balance, a.goalDescription, a.atHome });
        }
    }

    float totalElapsed;
    {
        auto now = std::chrono::steady_clock::now();
        totalElapsed = std::chrono::duration<float>(now - startTime).count();
    }

    // Write report
    FILE* f = fopen(outFile, "w");
    if (!f) { fprintf(stderr, "Cannot open %s for writing\n", outFile); CloseWindow(); return; }

    fprintf(f, "=== ReachingUniversalis Benchmark Report ===\n");
    fprintf(f, "Duration: %.1f seconds wall time\n", totalElapsed);
    fprintf(f, "Samples:  %d (1 per second)\n\n", sampleCount);

    fprintf(f, "--- Simulation ---\n");
    fprintf(f, "Final game day:     %d\n", lastDay);
    fprintf(f, "Avg steps/sec:      %.0f\n", sampleCount > 0 ? sumStepsPerSec / sampleCount : 0);
    fprintf(f, "Total deaths:       %d\n\n", lastDeaths);

    fprintf(f, "--- Population ---\n");
    fprintf(f, "Average:            %.1f\n", sampleCount > 0 ? sumPop / sampleCount : 0);
    fprintf(f, "Min:                %d\n", minPop == 999999 ? 0 : minPop);
    fprintf(f, "Max:                %d\n\n", maxPop);

    fprintf(f, "--- Economy ---\n");
    fprintf(f, "Avg total gold:     %.0f\n", sampleCount > 0 ? sumGold / sampleCount : 0);
    fprintf(f, "Avg NPC wealth:     %.1f\n", sampleCount > 0 ? sumAvgWealth / sampleCount : 0);
    fprintf(f, "Peak individual:    %.0fg (%s)\n\n", peakWealth, peakWealthName.c_str());

    fprintf(f, "--- Per-System Profiler (avg / median / p99 us per step) ---\n");
    // Compute avg, median, p99 per system
    struct ProfStats { std::string name; double avg; float median; float p99; };
    std::vector<ProfStats> profSorted;
    double totalUs = 0;
    for (auto& [name, acc] : profAccum) {
        double avg = acc.count > 0 ? acc.totalUs / acc.count : 0;
        float median = 0.f, p99 = 0.f;
        if (!acc.samples.empty()) {
            std::sort(acc.samples.begin(), acc.samples.end());
            size_t n = acc.samples.size();
            median = acc.samples[n / 2];
            size_t p99idx = std::min(n - 1, (size_t)(n * 0.99f));
            p99 = acc.samples[p99idx];
        }
        profSorted.push_back({ name, avg, median, p99 });
        totalUs += avg;
    }
    std::sort(profSorted.begin(), profSorted.end(),
        [](const auto& a, const auto& b) { return a.avg > b.avg; });
    for (const auto& ps : profSorted)
        fprintf(f, "  %-20s avg=%7.1f  med=%7.1f  p99=%7.1f us  (%4.1f%%)\n",
                ps.name.c_str(), ps.avg, ps.median, ps.p99,
                totalUs > 0 ? ps.avg / totalUs * 100 : 0);
    fprintf(f, "  %-20s avg=%7.1f us  (100%%)\n\n", "TOTAL", totalUs);

    fprintf(f, "--- Settlements (averages) ---\n");
    for (const auto& [name, ss] : settlStats) {
        fprintf(f, "  %-16s pop=%.1f  morale=%.0f%%\n",
                name.c_str(),
                ss.samples > 0 ? ss.sumPop / ss.samples : 0,
                ss.samples > 0 ? ss.sumMorale / ss.samples * 100 : 0);
    }
    fprintf(f, "\n");

    // Agent wealth distribution
    if (!finalAgents.empty()) {
        std::sort(finalAgents.begin(), finalAgents.end(),
            [](const auto& a, const auto& b) { return a.gold > b.gold; });

        fprintf(f, "--- NPC Wealth Distribution (final snapshot, top 20) ---\n");
        int shown = 0;
        for (const auto& a : finalAgents) {
            if (shown >= 20) break;
            fprintf(f, "  %-20s %7.1fg  %s%s\n",
                    a.name.c_str(), a.gold,
                    a.atHome ? "home" : "away",
                    a.goal.empty() ? "" : ("  goal: " + a.goal).c_str());
            ++shown;
        }

        // Wealth statistics
        float totalWealth = 0, medianWealth = 0;
        for (const auto& a : finalAgents) totalWealth += a.gold;
        if (!finalAgents.empty()) medianWealth = finalAgents[finalAgents.size() / 2].gold;
        fprintf(f, "\n  Total NPCs:    %d\n", (int)finalAgents.size());
        fprintf(f, "  Mean wealth:   %.1fg\n", finalAgents.empty() ? 0 : totalWealth / finalAgents.size());
        fprintf(f, "  Median wealth: %.1fg\n", medianWealth);
        fprintf(f, "  Gini approx:   ");
        // Simple Gini coefficient
        if (finalAgents.size() > 1) {
            double sumDiffs = 0;
            for (size_t i = 0; i < finalAgents.size(); ++i)
                for (size_t j = 0; j < finalAgents.size(); ++j)
                    sumDiffs += std::abs(finalAgents[i].gold - finalAgents[j].gold);
            double gini = sumDiffs / (2.0 * finalAgents.size() * totalWealth);
            fprintf(f, "%.3f (0=equal, 1=concentrated)\n", gini);
        } else {
            fprintf(f, "N/A\n");
        }
    }

    fclose(f);
    fprintf(stderr, "[benchmark] Report written to %s\n", outFile);

    CloseWindow();
}

int main(int argc, char* argv[]) {
    // Check for --benchmark flag
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--benchmark") == 0) {
            int secs = (i + 1 < argc) ? atoi(argv[i + 1]) : 30;
            const char* outFile = (i + 2 < argc) ? argv[i + 2] : "benchmark_report.txt";
            RunBenchmark(secs, outFile);
            return 0;
        }
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(DESIGN_W, DESIGN_H, "ReachingUniversalis");
    SetTargetFPS(0);

    RenderTexture2D target = LoadRenderTexture(DESIGN_W, DESIGN_H);
    SetTextureFilter(target.texture, TEXTURE_FILTER_POINT);

    GameState state;

    while (!WindowShouldClose()) {
        // F11 toggles borderless fullscreen
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

        float dt = GetFrameTime();

        // Compute scale and offset for letterboxed rendering
        float scaleX = (float)GetScreenWidth()  / DESIGN_W;
        float scaleY = (float)GetScreenHeight() / DESIGN_H;
        float scale  = std::min(scaleX, scaleY);
        float drawW  = DESIGN_W * scale;
        float drawH  = DESIGN_H * scale;
        float offsetX = (GetScreenWidth()  - drawW) * 0.5f;
        float offsetY = (GetScreenHeight() - drawH) * 0.5f;

        // Remap mouse input to design-space coordinates
        SetMouseOffset((int)(-offsetX), (int)(-offsetY));
        SetMouseScale(1.0f / scale, 1.0f / scale);

        state.Update(dt);

        // Render everything at design resolution into RenderTexture
        BeginTextureMode(target);
            ClearBackground(state.SkyColor());
            state.Draw();
        EndTextureMode();

        // Draw scaled RenderTexture to actual window
        BeginDrawing();
            ClearBackground(BLACK);
            DrawTexturePro(
                target.texture,
                { 0, 0, (float)DESIGN_W, -(float)DESIGN_H },
                { offsetX, offsetY, drawW, drawH },
                { 0, 0 }, 0.f, WHITE
            );
        EndDrawing();
    }

    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}
