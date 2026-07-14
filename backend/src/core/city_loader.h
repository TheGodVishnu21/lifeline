// core/city_loader.h
// Loads data/city_graph.json into a Graph. Throws std::runtime_error
// with a readable message if the file is missing or malformed.
#pragma once

#include <string>

#include "core/graph.h"

namespace lifeline {

// Fills `cityName` (if non-null) with the "city" field from the file.
Graph loadCityFromJson(const std::string& path, std::string* cityName);

}  // namespace lifeline
