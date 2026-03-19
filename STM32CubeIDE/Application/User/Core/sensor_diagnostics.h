/**
 * @file sensor_diagnostics.h
 * @brief Sensor health diagnostics using historical calibration data
 *
 * Analyzes flash-stored calibration history to detect:
 * - Sensitivity loss (ratio of current/first slope)
 * - Accelerating degradation (recent vs historical slope change rate)
 * - Erratic behavior (standard deviation of slope changes)
 */

#ifndef SENSOR_DIAGNOSTICS_H
#define SENSOR_DIAGNOSTICS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Diagnostic results per sensor
 */
typedef struct {
    float sensitivity_ratio;      // a(n) / a(1): 1.0 = no change, <1.0 = sensitivity loss
    float degradation_accel;      // Recent vs historical da/dn: >1.2 = accelerating degradation
    float slope_stddev;           // StdDev of Δa: <0.01 = stable, >0.02 = erratic
    bool valid;                   // Sufficient calibration history?
    uint8_t sample_count;         // Number of calibrations used
} sensor_diag_t;

/**
 * @brief Calculate sensor health diagnostics from flash history
 * @param results Array of 3 sensor_diag_t structs to populate
 * @return true if at least one sensor has valid diagnostics, false if no history
 * @note Stack usage: ~400 bytes peak during calculation
 */
bool sensor_diagnostics_calculate(sensor_diag_t results[3]);

/**
 * @brief Get minimum calibration count required for diagnostics
 * @return Minimum number of calibrations (2)
 */
static inline uint32_t sensor_diagnostics_min_history(void) {
    return 2;
}

#endif // SENSOR_DIAGNOSTICS_H
