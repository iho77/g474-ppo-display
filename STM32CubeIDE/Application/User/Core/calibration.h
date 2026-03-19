/*
 * calibration.h
 *
 *  Created on: Nov 14, 2025
 *      Author: imele
 */


#ifndef APPLICATION_USER_CORE_USER_INC_CALIBRATION_H_
#define APPLICATION_USER_CORE_USER_INC_CALIBRATION_H_


#include <stdint.h>
#include <stdbool.h>


typedef struct {
	uint16_t raw;
	uint16_t ppo;
} calibration_point;



void calibrate_sensor(calibration_point p1, calibration_point p2, uint8_t SensorID);


#endif /* CALIBRATION_H_ */
