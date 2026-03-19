/*
 * drift_tracker.c
 *
 * PPO2 drift rate tracking implementation
 *
 * Uses linear regression on sparse samples (10-second intervals) to calculate
 * drift rate over configurable time window (default 3 minutes).
 *
 * Created on: Feb 6, 2026
 *     Author: Claude Sonnet 4.5
 */

#include "drift_tracker.h"
#include <stdint.h>

/**
 * @brief Precomputed linear regression constants for x = [0, 1, 2, ..., n-1]
 *
 * Indexed by (n - 3) since minimum sample count is 3
 * Array size = DRIFT_WINDOW_SIZE - 2 (for n = 3 to DRIFT_WINDOW_SIZE)
 *
 * sum_x[i] = n*(n-1)/2 for n = i+3
 * sum_x2[i] = n*(n-1)*(2n-1)/6 for n = i+3
 * denominator[i] = n*sum_x2 - sum_x*sum_x for n = i+3
 */

// For DRIFT_WINDOW_SIZE = 18: arrays of size 16 (n=3 to n=18)
static const int32_t linreg_sum_x[DRIFT_WINDOW_SIZE - 2] = {
    3,    // n=3:  sum_x = 3
    6,    // n=4:  sum_x = 6
    10,   // n=5:  sum_x = 10
    15,   // n=6:  sum_x = 15
    21,   // n=7:  sum_x = 21
    28,   // n=8:  sum_x = 28
    36,   // n=9:  sum_x = 36
    45,   // n=10: sum_x = 45
    55,   // n=11: sum_x = 55
    66,   // n=12: sum_x = 66
    78,   // n=13: sum_x = 78
    91,   // n=14: sum_x = 91
    105,  // n=15: sum_x = 105
    120,  // n=16: sum_x = 120
    136,  // n=17: sum_x = 136
    153   // n=18: sum_x = 153
};

// Note: sum_x2 precomputed but not used (included for completeness/future use)
__attribute__((unused))
static const int32_t linreg_sum_x2[DRIFT_WINDOW_SIZE - 2] = {
    5,     // n=3:  sum_x2 = 5
    14,    // n=4:  sum_x2 = 14
    30,    // n=5:  sum_x2 = 30
    55,    // n=6:  sum_x2 = 55
    91,    // n=7:  sum_x2 = 91
    140,   // n=8:  sum_x2 = 140
    204,   // n=9:  sum_x2 = 204
    285,   // n=10: sum_x2 = 285
    385,   // n=11: sum_x2 = 385
    506,   // n=12: sum_x2 = 506
    650,   // n=13: sum_x2 = 650
    819,   // n=14: sum_x2 = 819
    1015,  // n=15: sum_x2 = 1015
    1240,  // n=16: sum_x2 = 1240
    1496,  // n=17: sum_x2 = 1496
    1785   // n=18: sum_x2 = 1785
};

static const int32_t linreg_denominator[DRIFT_WINDOW_SIZE - 2] = {
    6,      // n=3:  denom = 6
    20,     // n=4:  denom = 20
    50,     // n=5:  denom = 50
    105,    // n=6:  denom = 105
    196,    // n=7:  denom = 196
    336,    // n=8:  denom = 336
    540,    // n=9:  denom = 540
    825,    // n=10: denom = 825
    1210,   // n=11: denom = 1210
    1716,   // n=12: denom = 1716
    2366,   // n=13: denom = 2366
    3185,   // n=14: denom = 3185
    4200,   // n=15: denom = 4200
    5440,   // n=16: denom = 5440
    6936,   // n=17: denom = 6936
    8721    // n=18: denom = 8721
};

/**
 * @brief Initialize drift tracker state
 */
void drift_tracker_init(volatile drift_tracker_t* tracker) {
    // Initialize circular buffer
    for (uint8_t i = 0; i < DRIFT_WINDOW_SIZE; i++) {
        tracker->ppo_history[i] = 0;
    }

    tracker->buffer_index = 0;
    tracker->sample_count = 0;
    tracker->drift_rate_mbar_per_min = 0;
}

/**
 * @brief Sample current PPO value (timing controlled externally by scheduler)
 */
void drift_tracker_sample(volatile drift_tracker_t* tracker, int32_t current_ppo) {
    // Store sample in circular buffer
    tracker->ppo_history[tracker->buffer_index] = current_ppo;
    tracker->buffer_index = (tracker->buffer_index + 1) % DRIFT_WINDOW_SIZE;

    // Update sample count (saturates at DRIFT_WINDOW_SIZE)
    if (tracker->sample_count < DRIFT_WINDOW_SIZE) {
        tracker->sample_count++;
    }

    // Calculate drift rate if we have enough samples (minimum 3 for meaningful regression)
    if (tracker->sample_count >= 3) {
        drift_tracker_calculate_slope(tracker);
    }
}

/**
 * @brief Calculate drift rate via linear regression with precomputed constants
 */
void drift_tracker_calculate_slope(volatile drift_tracker_t* tracker) {
    uint8_t n = tracker->sample_count;
    if (n < 3) {
        tracker->drift_rate_mbar_per_min = 0;
        return;
    }

    // Validate sample count is within precomputed array bounds
    if (n > DRIFT_WINDOW_SIZE) {
        // Sample count corrupted - reset drift tracking to safe state
        tracker->drift_rate_mbar_per_min = 0;
        tracker->sample_count = 0;  // Force re-initialization
        return;
    }

    // Lookup precomputed constants (index = n - 3)
    uint8_t idx = n - 3;  // Safe: n ∈ [3, 18] → idx ∈ [0, 15]
    int32_t sum_x = linreg_sum_x[idx];
    int32_t denominator = linreg_denominator[idx];

    // Calculate sum_y and sum_xy from circular buffer
    int32_t sum_y = 0;
    int32_t sum_xy = 0;

    uint8_t read_idx = (tracker->buffer_index + DRIFT_WINDOW_SIZE - n) % DRIFT_WINDOW_SIZE;

    for (uint8_t x = 0; x < n; x++) {
        int32_t y = tracker->ppo_history[read_idx];
        sum_y += y;
        sum_xy += x * y;

        read_idx = (read_idx + 1) % DRIFT_WINDOW_SIZE;
    }

    // Calculate slope: (n*sum_xy - sum_x*sum_y) / denominator
    int32_t numerator = n * sum_xy - sum_x * sum_y;
    int32_t slope_per_interval = numerator / denominator;  // mbar per 10-second interval

    // Convert to mbar/minute: 6 intervals per minute
    tracker->drift_rate_mbar_per_min = slope_per_interval * 6;
}

/**
 * @brief Get drift rate in mbar/minute
 */
int32_t drift_get_rate_per_min(const volatile drift_tracker_t* tracker) {
    return tracker->drift_rate_mbar_per_min;
}
