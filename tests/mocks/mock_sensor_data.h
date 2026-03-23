/**
 * @file mock_sensor_data.h
 * @brief Shared sensor_data global and test helpers
 *
 * Declares the volatile sensor_data and active_setpoint globals that
 * sensor.c, calibration.c, warning.c, and pressure_sensor.c all reference.
 * Also declares reset_sensor_data() for test setup.
 */
#pragma once

#include "mock_stm32.h"
#include "sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reset sensor_data to a safe, predictable default state.
 *        Call at the start of every test that touches sensor_data.
 */
void reset_sensor_data(void);

/* Expose the htim5 stub required by warning.c */
extern TIM_HandleTypeDef htim5;

#ifdef __cplusplus
}
#endif
