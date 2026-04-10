#pragma once
#include "raylib.h"
#include <array>
#include <entt/entt.hpp>

// ---- Domain enums ----

enum class NeedType     { Hunger = 0, Thirst = 1, Energy = 2 };
enum class ResourceType { Food, Water, Shelter };

// ---- Need data ----

struct Need {
    NeedType type;
    float value;              // 0.0 (depleted) to 1.0 (full)
    float drainRate;          // units drained per real second
    float criticalThreshold;  // below this = urgent
    float refillRate;         // units refilled per real second while satisfying
};

struct Needs {
    std::array<Need, 3> list;
};

// ---- Spatial / movement ----

struct Position  { float x, y; };
struct Velocity  { float vx, vy; };
struct MoveSpeed { float value; };

// ---- Agent behaviour ----

enum class AgentBehavior { Idle, SeekingFood, SeekingWater, SeekingSleep, Satisfying };

struct AgentState {
    AgentBehavior behavior = AgentBehavior::Idle;
    entt::entity  target   = entt::null;
};

// ---- World components ----

struct ResourceNode {
    ResourceType type;
    float interactionRadius;
};

// ---- Rendering ----

struct Renderable {
    Color color;
    float size;
};

// ---- Time ----

// At 1x speed: 1 real second = 1 game minute → 1 game day = 24 real minutes.
// GAME_MINS_PER_REAL_SEC controls how fast game time runs relative to real time.
static constexpr float GAME_MINS_PER_REAL_SEC = 1.0f;

struct TimeManager {
    float gameSeconds  = 0.0f;   // total elapsed game-time seconds
    int   day          = 1;      // current day (1-indexed)
    float hourOfDay    = 6.0f;   // 0.0–24.0, starts at dawn
    int   tickSpeed    = 1;      // multiplier: 1, 2, or 4
    bool  paused       = false;

    // Scaled dt for all game systems (0 when paused).
    // At 1x: gameDt == realDt. At 2x: gameDt == realDt*2. Etc.
    float GameDt(float realDt) const {
        if (paused) return 0.0f;
        return realDt * static_cast<float>(tickSpeed);
    }
};

// ---- Tags ----

struct PlayerTag {};   // marks the entity the HUD observes
