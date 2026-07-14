// api/server.h
// REST layer. Endpoints are documented in docs/api_spec.md — that file
// is the contract between backend and frontend. Change it first, code
// second.
#pragma once

#include <string>

#include "core/graph.h"
#include "db/history.h"

namespace lifeline {

// Blocks forever serving HTTP. `frontendDir` (if non-empty and existing)
// is mounted at "/" so the demo UI is reachable at http://localhost:port
void runServer(Graph& g, const std::string& cityName, int port,
               const std::string& frontendDir, History& history);

}  // namespace lifeline
