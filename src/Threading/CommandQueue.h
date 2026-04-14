#pragma once
// CommandQueue.h — Thread-safe command queue replacing InputSnapshot atomic flags.
//
// The main thread pushes Commands (one-shot actions with variant payloads).
// The sim thread drains the queue each tick via DrainAll().
//
// Continuous state (movement, camera) is handled separately via atomic
// fields on ContinuousInput, which the sim thread samples each frame.
//
// This is world-agnostic: commands carry string action names and generic
// payloads rather than game-specific enum values.

#include <mutex>
#include <queue>
#include <string>
#include <variant>
#include <vector>
#include <atomic>
#include <cstdint>

// ---- Command payloads ----

struct EmptyPayload {};

struct FloatPayload {
    float value = 0.f;
};

struct IntPayload {
    int value = 0;
};

struct StringPayload {
    std::string value;
};

struct PositionPayload {
    float x = 0.f;
    float y = 0.f;
};

struct RoadBuildPayload {
    float fromX = 0.f, fromY = 0.f;
    float toX   = 0.f, toY   = 0.f;
};

// A variant that covers all payload types.  Extend as needed.
using CommandPayload = std::variant<
    EmptyPayload,
    FloatPayload,
    IntPayload,
    StringPayload,
    PositionPayload,
    RoadBuildPayload
>;

// ---- Command ----
// action: a string identifying the command (e.g. "pause_toggle", "player_trade",
//         "speed_up", "set_tick_speed", "road_build").
// payload: optional data associated with the command.

struct Command {
    std::string    action;
    CommandPayload payload;

    // Convenience constructors
    Command() = default;
    explicit Command(std::string act)
        : action(std::move(act)), payload(EmptyPayload{}) {}
    Command(std::string act, CommandPayload pl)
        : action(std::move(act)), payload(std::move(pl)) {}
};

// ---- Continuous input state ----
// Sampled every frame by the sim thread — not queued.
// Main thread writes freely; sim thread reads freely.
// Atomic fields ensure no torn reads.

struct ContinuousInput {
    std::atomic<float> playerMoveX{0.f};
    std::atomic<float> playerMoveY{0.f};
};

// ---- The queue ----

class CommandQueue {
public:
    // Push a single command (called from the main thread).
    void Push(Command cmd) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::move(cmd));
    }

    // Push a one-shot command by action name only.
    void Push(const std::string& action) {
        Push(Command(action));
    }

    // Push a command with a payload.
    void Push(const std::string& action, CommandPayload payload) {
        Push(Command(action, std::move(payload)));
    }

    // Drain all pending commands into `out` (called from the sim thread).
    // Returns the number of commands drained.
    int DrainAll(std::vector<Command>& out) {
        std::lock_guard<std::mutex> lock(m_mutex);
        int n = 0;
        while (!m_queue.empty()) {
            out.push_back(std::move(m_queue.front()));
            m_queue.pop();
            ++n;
        }
        return n;
    }

    // Check if the queue is empty (approximate — may race with Push).
    bool Empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

private:
    mutable std::mutex    m_mutex;
    std::queue<Command>   m_queue;
};
