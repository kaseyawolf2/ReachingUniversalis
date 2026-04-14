#pragma once
// WorldLoader.h — TOML config parser that reads worlds/<name>/*.toml
// and populates a WorldSchema.
//
// Usage:
//   WorldSchema schema;
//   std::string err;
//   std::vector<LoadWarning> warnings;
//   bool ok = WorldLoader::Load("worlds/medieval", schema, err, &warnings);
//   if (!ok) fprintf(stderr, "Load failed: %s\n", err.c_str());

#include "WorldSchema.h"
#include <string>
#include <vector>

// ---- Structured load diagnostics ----

enum class LoadWarningLevel {
    Info,       // Informational (e.g. summary of loaded data)
    Warning,    // Non-fatal issue that may cause unexpected behaviour
};

struct LoadWarning {
    LoadWarningLevel level   = LoadWarningLevel::Warning;
    std::string      category;  // e.g. "seasons", "events", "goals"
    std::string      message;   // Human-readable description
};

class WorldLoader {
public:
    // Load all TOML files from `worldDir` into `schema`.
    // Returns true on success.  On failure, `errorMsg` describes
    // what went wrong (file, field, cross-reference, etc.).
    //
    // If `warnings` is non-null, all non-fatal diagnostics are collected
    // into the vector AND printed to stderr as before.  A summary block
    // is printed to stderr after loading completes.
    static bool Load(const std::string& worldDir,
                     WorldSchema& schema,
                     std::string& errorMsg,
                     std::vector<LoadWarning>* warnings = nullptr);
};
