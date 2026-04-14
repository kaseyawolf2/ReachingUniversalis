#pragma once
// SeasonDef.h — Lightweight header for season definitions.
//
// Extracted from WorldSchema.h so that Components.h (included by every
// translation unit) does not pull in the full schema.

#include <string>

using SeasonID = int;

struct SeasonDef {
    SeasonID    id             = -1;
    std::string name;                         // "Spring", "Summer", ...
    std::string displayName;
    int         durationDays   = 30;          // game-days this season lasts
    float       productionMod  = 1.0f;        // multiplier on all production
    float       energyDrainMod = 1.0f;        // multiplier on energy need drain
    float       heatDrainMod   = 0.0f;        // multiplier on heat need drain (0 = no cold)
    float       baseTemperature = 20.0f;      // noon temperature in °C
    float       tempSwing       = 8.0f;       // ± degrees from diurnal cycle
};

// Approximate air temperature in degrees Celsius.
// Combines season baseline with time-of-day variation (cooler at night).
#include <cmath>
inline float AmbientTemperature(const SeasonDef& sdef, float hourOfDay) {
    float base = sdef.baseTemperature;
    // Diurnal swing: ± tempSwing, coldest at 4am, hottest at 2pm
    float swing = -sdef.tempSwing * std::cos((hourOfDay - 14.f) * 3.14159f / 12.f);
    return base + swing;
}
