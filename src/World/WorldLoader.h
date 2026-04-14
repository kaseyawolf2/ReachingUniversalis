#pragma once
// WorldLoader.h — TOML config parser that reads worlds/<name>/*.toml
// and populates a WorldSchema.
//
// Usage:
//   WorldSchema schema;
//   std::string err;
//   bool ok = WorldLoader::Load("worlds/medieval", schema, err);
//   if (!ok) fprintf(stderr, "Load failed: %s\n", err.c_str());

#include "WorldSchema.h"
#include <string>

class WorldLoader {
public:
    // Load all TOML files from `worldDir` into `schema`.
    // Returns true on success.  On failure, `errorMsg` describes
    // what went wrong (file, field, cross-reference, etc.).
    static bool Load(const std::string& worldDir,
                     WorldSchema& schema,
                     std::string& errorMsg);
};
