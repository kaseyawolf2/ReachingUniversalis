#pragma once
#include <atomic>
#include <cstdint>

// Written by the main thread each frame from raw Raylib input.
// Read and consumed by the simulation thread.
//
// One-shot flags: main thread sets true; sim thread sets false after processing.
// Continuous values: main thread overwrites every frame.
//
// All fields are std::atomic so no lock is needed for individual reads/writes.
// Ordering guarantees: relaxed loads/stores are fine here because we only care
// about eventual visibility, not strict ordering between fields.

struct InputSnapshot {
    // ---- One-shot events ----
    std::atomic<bool> pauseToggle    {false};
    std::atomic<bool> speedUp        {false};
    std::atomic<bool> speedDown      {false};
    std::atomic<bool> roadToggle     {false};
    std::atomic<bool> camFollowToggle{false};
    std::atomic<bool> playerTrade    {false};  // T: buy/sell at nearest settlement
    std::atomic<bool> playerSleep    {false};  // Z: toggle sleep (restores energy)
    std::atomic<bool> playerSettle   {false};  // H: adopt nearest settlement as home
    std::atomic<bool> playerWork     {false};  // E: work at nearest production facility
    std::atomic<bool> playerBuy      {false};  // Q: buy 1 unit of cheapest resource at nearest settlement
    std::atomic<bool> playerBuild    {false};  // C: invest 200g to build a new production facility
    std::atomic<bool> playerBuyCart      {false};  // V: pay 300g at a settlement to expand carry capacity by 10
    std::atomic<bool> playerFoundSettlement{false};  // P: pay 1500g to found a new settlement at current location
    std::atomic<bool> roadRepair     {false};  // R: pay 50g to clear nearest blocked road
    std::atomic<bool> roadBuild      {false};  // N (two-press): pay 400g to build a new road
    std::atomic<float> roadBuildFromX{0.f};    // world-space coords of road start (player pos on 1st N press)
    std::atomic<float> roadBuildFromY{0.f};
    std::atomic<float> roadBuildToX  {0.f};    // world-space coords of road end (player pos on 2nd N press)
    std::atomic<float> roadBuildToY  {0.f};

    // ---- Direct tick speed override (-1 = no override, 0 = uncapped) ----
    std::atomic<int>  setTickSpeed    {-1};

    // ---- Continuous player movement (normalised, -1..1) ----
    std::atomic<float> playerMoveX{0.f};
    std::atomic<float> playerMoveY{0.f};

    // ---- Settlement selection (entt::entity is uint32_t) ----
    // main thread sets clickedSettlement to an entity value and
    // settlementClicked to true; sim thread clears the flag after processing.
    std::atomic<bool>     settlementClicked{false};
    std::atomic<uint32_t> clickedSettlement{0xffffffffu}; // entt::null
};
