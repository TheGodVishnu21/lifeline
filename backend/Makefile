# LifeLine backend build
# Linux / macOS / WSL:   make               -> ./lifeline (file-based history)
#                        make USE_SQLITE=1  -> ./lifeline with SQLite history
#                        make test          -> build + run all test suites
# Windows (MinGW-w64): see README.md (needs -lws2_32 for httplib)

CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Iinclude -Isrc
LDFLAGS  = -pthread

# Windows (MSYS2 / MinGW-w64): cpp-httplib needs winsock
ifeq ($(OS),Windows_NT)
LDFLAGS += -lws2_32
endif

# Opt-in SQLite: `make USE_SQLITE=1`. Off by default so the project builds
# with zero external libraries.
ifdef USE_SQLITE
CXXFLAGS += -DUSE_SQLITE
LDFLAGS  += -lsqlite3
endif

SRC = src/core/graph.cpp \
      src/core/city_loader.cpp \
      src/routing/dijkstra.cpp \
      src/routing/astar.cpp \
      src/routing/bellman_ford.cpp \
      src/routing/floyd_warshall.cpp \
      src/evacuation/max_flow.cpp \
      src/simulation/disaster.cpp \
      src/dispatch/dispatch.cpp \
      src/dispatch/supply.cpp \
      src/resilience/critical.cpp \
      src/resilience/mst.cpp \
      src/db/history.cpp \
      src/api/server.cpp

all: lifeline

lifeline: $(SRC) src/main.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# builds and runs ALL test suites
test: test_routing test_evacuation test_dispatch test_resilience test_analytics

test_routing: $(SRC) tests/test_routing.cpp
	$(CXX) $(CXXFLAGS) $^ -o run_routing $(LDFLAGS)
	./run_routing

test_evacuation: $(SRC) tests/test_evacuation.cpp
	$(CXX) $(CXXFLAGS) $^ -o run_evac $(LDFLAGS)
	./run_evac

test_dispatch: $(SRC) tests/test_dispatch.cpp
	$(CXX) $(CXXFLAGS) $^ -o run_dispatch $(LDFLAGS)
	./run_dispatch

test_resilience: $(SRC) tests/test_resilience.cpp
	$(CXX) $(CXXFLAGS) $^ -o run_resil $(LDFLAGS)
	./run_resil

test_analytics: $(SRC) tests/test_analytics.cpp
	$(CXX) $(CXXFLAGS) $^ -o run_analytics $(LDFLAGS)
	./run_analytics

clean:
	rm -f lifeline run_routing run_evac run_dispatch run_resil run_analytics *.exe

.PHONY: all test test_routing test_evacuation test_dispatch test_resilience test_analytics clean
