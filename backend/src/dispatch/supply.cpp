#include "dispatch/supply.h"

#include <algorithm>
#include <chrono>

namespace lifeline {

SupplyPlan planSupplies(std::vector<SupplyItem> items, int capacity) {
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();

    SupplyPlan plan;
    plan.capacity = std::max(0, capacity);
    const int W = plan.capacity;

    // Keep only usable items (positive weight that fits, non-negative value).
    std::vector<SupplyItem> pool;
    for (const auto& it : items)
        if (it.weight > 0 && it.weight <= W && it.value >= 0)
            pool.push_back(it);
    const int n = static_cast<int>(pool.size());

    if (n == 0 || W == 0) {
        plan.runtime_us = std::chrono::duration<double, std::micro>(
                              Clock::now() - t0).count();
        return plan;
    }

    // dp[i][w]: best value using first i items with capacity w.
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(W + 1, 0));
    for (int i = 1; i <= n; ++i) {
        const int wt = pool[i - 1].weight;
        const int val = pool[i - 1].value;
        for (int w = 0; w <= W; ++w) {
            dp[i][w] = dp[i - 1][w];                     // skip item i
            if (wt <= w)                                 // or take it
                dp[i][w] = std::max(dp[i][w],
                                    dp[i - 1][w - wt] + val);
        }
    }
    plan.total_value = dp[n][W];

    // Backtrack: an item was taken iff including it changed the optimum.
    int w = W;
    for (int i = n; i >= 1; --i) {
        if (dp[i][w] != dp[i - 1][w]) {
            plan.chosen.push_back(pool[i - 1]);
            plan.total_weight += pool[i - 1].weight;
            w -= pool[i - 1].weight;
        }
    }
    std::reverse(plan.chosen.begin(), plan.chosen.end());

    plan.runtime_us = std::chrono::duration<double, std::micro>(
                          Clock::now() - t0).count();
    return plan;
}

}  // namespace lifeline
