/**
 * @file sensor_diagnostics.c
 * @brief Sensor health diagnostics implementation
 *
 * Uses flash calibration history to compute statistical health metrics.
 */

#include "sensor_diagnostics.h"
#include "flash_storage.h"
#include <string.h>
#include <math.h>

// ============================================================================
// ACCUMULATOR STRUCTURES (stack-based during calculation)
// ============================================================================

#define MAX_HISTORY_SAMPLES 20  // Limit memory usage (~244 bytes)

typedef struct {
    float slopes[3][MAX_HISTORY_SAMPLES];  // 3 sensors × 20 samples × 4 bytes
    uint32_t sequences[MAX_HISTORY_SAMPLES];
    uint32_t count;
} history_accumulator_t;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Calculate mean of array
 */
static float calculate_mean(const float *data, size_t count) {
    if (count == 0) {
        return 0.0f;
    }

    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum += data[i];
    }
    return sum / (float)count;
}

/**
 * @brief Calculate standard deviation of array
 */
static float calculate_stddev(const float *data, size_t count, float mean) {
    if (count <= 1) {
        return 0.0f;
    }

    float sum_sq = 0.0f;
    for (size_t i = 0; i < count; i++) {
        float diff = data[i] - mean;
        sum_sq += diff * diff;
    }

    float variance = sum_sq / (float)(count - 1);
    return sqrtf(variance);
}

// ============================================================================
// FLASH ITERATOR CALLBACK
// ============================================================================

/**
 * @brief Callback to accumulate historical slopes/intercepts
 */
static void accumulate_history(
    uint32_t sequence,
    const float slopes[3],
    const float intercepts[3],
    void *user_data
) {
    (void)intercepts;  // Not used for diagnostics

    history_accumulator_t *acc = (history_accumulator_t*)user_data;

    if (acc->count >= MAX_HISTORY_SAMPLES) {
        return;  // Buffer full
    }

    // Store slopes for all 3 sensors
    for (int i = 0; i < 3; i++) {
        acc->slopes[i][acc->count] = slopes[i];
    }
    acc->sequences[acc->count] = sequence;
    acc->count++;
}

// ============================================================================
// METRIC CALCULATIONS
// ============================================================================

/**
 * @brief Metric 1: Sensitivity ratio (current slope / first slope)
 * @note Requires ≥2 calibrations
 */
static void calc_sensitivity_ratio(
    const history_accumulator_t *acc,
    sensor_diag_t results[3]
) {
    if (acc->count < 2) {
        return;  // Insufficient data
    }

    for (int sensor = 0; sensor < 3; sensor++) {
        float first_slope = acc->slopes[sensor][acc->count - 1];  // Oldest
        float latest_slope = acc->slopes[sensor][0];  // Newest

        if (first_slope > 1e-9f) {  // Avoid division by zero
            results[sensor].sensitivity_ratio = latest_slope / first_slope;
        } else {
            results[sensor].sensitivity_ratio = 0.0f;
        }
    }
}

/**
 * @brief Metric 2: Degradation acceleration (recent vs historical da/dn)
 * @note Requires ≥10 calibrations (5 recent + 5 historical)
 */
static void calc_degradation_accel(
    const history_accumulator_t *acc,
    sensor_diag_t results[3]
) {
    if (acc->count < 10) {
        for (int sensor = 0; sensor < 3; sensor++) {
            results[sensor].degradation_accel = 0.0f;
        }
        return;  // Insufficient data
    }

    for (int sensor = 0; sensor < 3; sensor++) {
        // Recent rate: (slopes[0] - slopes[4]) / 5
        float recent_delta = acc->slopes[sensor][0] - acc->slopes[sensor][4];
        float recent_rate = recent_delta / 5.0f;

        // Historical rate: (slopes[5] - slopes[N-1]) / (N-5)
        uint32_t hist_count = acc->count - 5;
        float hist_delta = acc->slopes[sensor][5] - acc->slopes[sensor][acc->count - 1];
        float hist_rate = hist_delta / (float)hist_count;

        // Acceleration: recent_rate / hist_rate
        if (fabsf(hist_rate) > 1e-9f) {
            results[sensor].degradation_accel = recent_rate / hist_rate;
        } else {
            results[sensor].degradation_accel = 0.0f;
        }
    }
}

/**
 * @brief Metric 3: Stability (standard deviation of slope changes)
 * @note Requires ≥5 calibrations
 */
static void calc_slope_stability(
    const history_accumulator_t *acc,
    sensor_diag_t results[3]
) {
    if (acc->count < 5) {
        for (int sensor = 0; sensor < 3; sensor++) {
            results[sensor].slope_stddev = 0.0f;
        }
        return;  // Insufficient data
    }

    for (int sensor = 0; sensor < 3; sensor++) {
        // Calculate deltas: delta[i] = slopes[i] - slopes[i+1]
        float deltas[MAX_HISTORY_SAMPLES - 1];
        uint32_t delta_count = acc->count - 1;

        for (uint32_t i = 0; i < delta_count; i++) {
            deltas[i] = acc->slopes[sensor][i] - acc->slopes[sensor][i + 1];
        }

        // Calculate mean and stddev of deltas
        float mean = calculate_mean(deltas, delta_count);
        results[sensor].slope_stddev = calculate_stddev(deltas, delta_count, mean);
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Calculate sensor health diagnostics from flash history
 */
bool sensor_diagnostics_calculate(sensor_diag_t results[3]) {
    // Initialize results
    (void)memset(results, 0, sizeof(sensor_diag_t) * 3);

    // Check if any calibration history exists
    uint32_t count = flash_calibration_get_count();
    if (count < 2) {
        return false;  // Need at least 2 calibrations
    }

    // Accumulate historical slopes
    history_accumulator_t acc = {0};
    uint32_t processed = flash_calibration_iterate(
        accumulate_history,
        &acc,
        MAX_HISTORY_SAMPLES  // Limit to 20 most recent
    );

    if (processed < 2) {
        return false;
    }

    // Calculate metrics
    calc_sensitivity_ratio(&acc, results);

    if (processed >= 10) {
        calc_degradation_accel(&acc, results);
    }

    if (processed >= 5) {
        calc_slope_stability(&acc, results);
    }

    // Mark results as valid and store sample count
    for (int sensor = 0; sensor < 3; sensor++) {
        results[sensor].valid = true;
        results[sensor].sample_count = (uint8_t)processed;
    }

    return true;
}
