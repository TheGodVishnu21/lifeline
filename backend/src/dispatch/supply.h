// dispatch/supply.h
// -----------------------------------------------------------------------
// Relief truck loading = 0/1 KNAPSACK (dynamic programming).
//
// A truck has a weight capacity. Each supply item has a weight and a
// "relief value" (how much it helps). Choose the subset that maximises
// total value without exceeding capacity — the textbook 0/1 knapsack.
//
// DP table: dp[i][w] = best value using the first i items within weight w.
//   dp[i][w] = max( dp[i-1][w],                        (skip item i)
//                   dp[i-1][w - wt[i]] + val[i] )      (take item i)
// Time O(n * W), space O(n * W). We keep the full table so we can
// BACKTRACK the chosen items (not just the value).
//
// Capacities/weights are integers (kg), which knapsack DP requires.
// -----------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

namespace lifeline {

struct SupplyItem {
    std::string name;
    int         weight = 0;   // kg
    int         value  = 0;   // relief value (unitless priority)
};

struct SupplyPlan {
    std::vector<SupplyItem> chosen;
    int    total_weight = 0;
    int    total_value  = 0;
    int    capacity     = 0;
    double runtime_us   = 0.0;
};

// Negative/oversized weights are ignored. capacity < 0 treated as 0.
SupplyPlan planSupplies(std::vector<SupplyItem> items, int capacity);

}  // namespace lifeline
