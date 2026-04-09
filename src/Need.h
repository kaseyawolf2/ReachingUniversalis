#pragma once

enum class NeedType { Hunger, Thirst, Energy, COUNT };

struct Need {
    NeedType type;
    float value;               // 0.0 (empty/critical) to 1.0 (full/satisfied)
    float drainRate;           // units drained per second
    float criticalThreshold;   // below this value the need demands attention
    float refillRate;          // units refilled per second while satisfying
};
