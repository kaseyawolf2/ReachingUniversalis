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

// ---- Tags ----

struct PlayerTag {};   // marks the entity the HUD observes
