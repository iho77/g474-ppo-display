/*
 * sensor.h
 *
 *  Created on: Nov 14, 2025
 *      Author: imele
 */


#ifndef APPLICATION_USER_CORE_USER_INC_SENSOR_H_
#define APPLICATION_USER_CORE_USER_INC_SENSOR_H_


#include <stdint.h>
#include <stdbool.h>
#include "calibration.h"
#include "settings_storage.h"
#include "pressure_sensor.h"

#define BAT_MIN_MV 2800
#define BAT_MAX_MV 3300
#define BAT_BASE (BAT_MAX_MV - BAT_MIN_MV)  // Parentheses for macro safety

// Compile-time safety: Prevent division by zero if BAT_MAX == BAT_MIN
_Static_assert(BAT_BASE > 0, "BAT_MAX_MV must be greater than BAT_MIN_MV");

// Sensor validation thresholds (ADC counts after filtering)
// ADC: 12-bit, 3.3V ref, 0.806mV/count at ADC input
// Signal chain: Sensor → OPAMP(16×) → ADC
#define SENSOR_MIN_ADC_COUNTS 100   // 5.0mV at sensor (disconnected/fault)
#define SENSOR_MAX_ADC_COUNTS 3500  // 176mV at sensor (1.47× max spec, headroom for faults)
// Note: Max sensor output = 120.24mV at 1866mbar → 2386 ADC counts

typedef enum {
	AUTO_CALIBRATED = 0,
	CALIBRATED_1PT,
	CALIBRATED_2PT,
	NON_CALIBRATED
} sensor_cal_state;

typedef enum {
    SENSOR1 = 0,
    SENSOR2,
    SENSOR3,
    BAT,
	REFERENCE,
    SENSOR_COUNT
} sensor_id_t;


// Drift tracking configuration
#define DRIFT_WINDOW_SIZE 18  // 18 samples × 10s = 3 minutes

/**
 * @brief Drift tracking state for one sensor
 *
 * Uses circular buffer with linear regression to calculate drift rate.
 * Samples PPO value every 10 seconds from already-filtered sensor_data.o2_s_ppo[]
 */
typedef struct {
    // Circular buffer for PPO samples
    int32_t ppo_history[DRIFT_WINDOW_SIZE];  // PPO values (mbar)
    uint8_t buffer_index;                     // Current write position (0-17)
    uint8_t sample_count;                     // Samples collected (0-18)

    // Linear regression results
    int32_t drift_rate_mbar_per_min;         // Calculated slope in mbar/minute
} drift_tracker_t;

typedef struct {
	// Raw sensor readings (dual O2 sensors) - INTEGER STORAGE for performance
	int32_t o2_s_uv[SENSOR_COUNT-2];
	int32_t o2_s_ppo[SENSOR_COUNT-2];
	float s_cal[2][SENSOR_COUNT-2];
	sensor_cal_state os_s_cal_state[SENSOR_COUNT-2];
	calibration_point s_cpt[2];  // Shared calibration points for all sensors
	bool s_valid[SENSOR_COUNT-2];            // Sensor #1 data validity

	int32_t reference_uv;
	// Battery data - PRIMARY INTEGER STORAGE for performance
	int32_t battery_voltage_mv;    // Battery voltage in millivolts (no FPU)
	uint8_t battery_percentage;    // Battery charge percentage (0-100)
	bool battery_low;              // Low battery warning flag

	// System settings (persistent)
	settings_data_t settings;

	// Drift tracking per sensor
	drift_tracker_t drift[SENSOR_COUNT-2];  // Per-sensor drift tracking

	// Pressure sensor data (depth monitoring)
	pressure_sensor_data_t pressure;  // MS5837 pressure sensor data

} sensor_data_t;




extern volatile sensor_data_t sensor_data;
extern uint8_t active_setpoint;  // 1 or 2 (setpoint index)
sensor_data_t sensor_get_data(void);
void sensor_data_update(sensor_id_t SensorID, uint16_t val);
void sensor_calibration_update(sensor_id_t SensorID, uint16_t a, uint16_t b);
void sensor_init(void);

// Two-stage EMA filter (alternative to single-stage EMA)
int32_t filter_step_sensor_two_stage(sensor_id_t id, int32_t raw);




#endif /* APPLICATION_USER_CORE_USER_INC_SENSOR_H_ */

/*
 * sensor.h
 *
 *  Created on: Nov 14, 2025
 *      Author: imele
 */

