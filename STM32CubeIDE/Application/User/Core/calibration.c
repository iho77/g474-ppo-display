/*
 * calibration.c
 *
 *  Created on: Nov 15, 2025
 *      Author: imele
 */

#include "calibration.h"
#include "sensor.h"


/*
void calibrate_sensor(calibration_point p1, calibration_point p2, float *a, float *b) {
	*a =  ((float)p2.ppo - (float)p1.ppo) / (float)(p2.raw - p1.raw);
	*b =  ((float)p2.raw*(float)p1.ppo-(float)p1.raw*(float)p2.ppo)/(float)(p2.raw - p1.raw);
	return;
} */

void calibrate_sensor(calibration_point p1, calibration_point p2, uint8_t SensorID) {

	if (SensorID >= (SENSOR_COUNT - 2U)) return; /* bounds check: only 3 O2 sensors (indices 0-2) */

	float a, b;

	// CRITICAL: Validate denominator to prevent division by zero
	int32_t raw_delta = (int32_t)p2.raw - (int32_t)p1.raw;
	if (raw_delta == 0) {
		// Identical ADC readings - calibration invalid
		sensor_data.os_s_cal_state[SensorID] = NON_CALIBRATED;
		return;
	}

	// SAFETY: Reject inverted calibration (negative slope)
	int32_t ppo_delta = (int32_t)p2.ppo - (int32_t)p1.ppo;
	if ((ppo_delta > 0 && raw_delta < 0) || (ppo_delta < 0 && raw_delta > 0)) {
		sensor_data.os_s_cal_state[SensorID] = NON_CALIBRATED;
		return;
	}

	// SAFETY: Reject dead sensor (zero slope)
	if (ppo_delta == 0) {
		sensor_data.os_s_cal_state[SensorID] = NON_CALIBRATED;
		return;
	}

	// SAFETY: Check minimum point separation (>50 counts)
	if (raw_delta > 0 && raw_delta < 50) {
		sensor_data.os_s_cal_state[SensorID] = NON_CALIBRATED;
		return;
	}
	if (raw_delta < 0 && raw_delta > -50) {
		sensor_data.os_s_cal_state[SensorID] = NON_CALIBRATED;
		return;
	}

	// Calculate coefficients (now safe)
	a =  ((float)p2.ppo - (float)p1.ppo) / (float)(p2.raw - p1.raw);
	b =  ((float)p2.raw*(float)p1.ppo-(float)p1.raw*(float)p2.ppo)/(float)(p2.raw - p1.raw);

	// Shared calibration points for all sensors
	sensor_data.s_cpt[0] = p1;
    sensor_data.s_cpt[1] = p2;
    // Per-sensor calibration coefficients
    sensor_data.s_cal[0][SensorID] = a;
   	sensor_data.s_cal[1][SensorID] = b;

	return;

}
