/*
 * drift_tracker.h
 *
 * PPO2 drift rate tracking via linear regression
 *
 * Tracks PPO2 drift rate (mbar/minute) over configurable time window
 * using least-squares linear regression on sparse samples.
 *
 * Created on: Feb 6, 2026
 *     Author: Claude Sonnet 4.5
 */

#ifndef DRIFT_TRACKER_H
#define DRIFT_TRACKER_H

#include <stdint.h>
#include "sensor.h"  // For drift_tracker_t and DRIFT_WINDOW_SIZE

/**
 * @brief Initialize drift tracker state
 * @param tracker Pointer to drift tracker structure
 */
void drift_tracker_init(volatile drift_tracker_t* tracker);

/**
 * @brief Sample current PPO value (called every 10 seconds by scheduler)
 * @param tracker Pointer to drift tracker structure
 * @param current_ppo Current PPO2 value in mbar
 */
void drift_tracker_sample(volatile drift_tracker_t* tracker, int32_t current_ppo);

/**
 * @brief Calculate drift rate via linear regression
 * @param tracker Pointer to drift tracker structure
 * @note Called automatically by drift_tracker_sample() when sample_count >= 3
 */
void drift_tracker_calculate_slope(volatile drift_tracker_t* tracker);

/**
 * @brief Get drift rate in mbar/minute
 * @param tracker Pointer to drift tracker structure
 * @return Drift rate in mbar/min (positive = rising, negative = falling)
 */
int32_t drift_get_rate_per_min(const volatile drift_tracker_t* tracker);

#endif // DRIFT_TRACKER_H
