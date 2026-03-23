/*
 * sensor.c
 *
 *  Created on: Nov 14, 2025
 *      Author: imele
 *
 * Two-stage digital filter for O2 sensor ADC readings:
 * 1. Hampel filter: Rejects large outliers/spikes using MAD-based threshold
 * 2. EMA filter: Smooths ADC noise (±2-3 counts) while tracking real changes
 *
 * Filter operates on raw ADC counts (0-4095 range).
 * Conversion to voltage happens AFTER filtering in sensor_data_update().
 */
#include "stm32g4xx_ll_adc.h"
#include "stdint.h"
#include "stdbool.h"
#include "sensor.h"
#include "calibration.h"
#include "screen_manager.h"
#include "drift_tracker.h"

/* Two-stage EMA filter for O2 sensor ADC readings
 *
 * ADC DMA callback rates (continuous mode, 16x oversampling):
 *   ADC1: ~9.5 kHz (3 channels: VREFINT, BAT, SENSOR3)
 *   ADC2: ~14.3 kHz (2 channels: SENSOR1, SENSOR2)
 *
 * Filter response at ~10 kHz effective rate:
 *   S=9   -> α=1/512,  ~54ms time constant (fast response)
 *   Sb=11 -> α=1/2048, ~215ms baseline tracking
 *   K=70  -> ~7ms Hampel window for spike rejection
 *
 * Auto-calibration: 2500 steps ≈ 263ms settling time
 */
#define S   9                     // EMA alpha = 1/512
#define Sb  11                    // Baseline alpha = 1/2048
#define K   70                    // Hampel window: ~7ms at 10kHz
#define SPIKE_K 5                 // MAD multiplier for spike threshold
#define MAD_MIN_COUNTS 2          // Minimum MAD (prevents divide-by-zero)

// Stability-based auto-calibration constants
#define STABLE_THRESHOLD    3     // Max |y - b| difference in ADC counts
#define STABLE_REQUIRED     50    // Consecutive stable samples needed (~5ms)
#define MIN_WARMUP_STEPS    100  // Minimum samples before checking stability
#define MAX_STABILITY_WAIT  0 /* Intentionally 0 — no extra wait beyond MIN_WARMUP_STEPS */

uint16_t steps[SENSOR_COUNT - 2];
static uint16_t stable_count[SENSOR_COUNT - 2];  // Per-sensor stability counter

// In unit test mode, these are defined in mock_sensor_data.c
#ifndef UNIT_TEST
volatile sensor_data_t sensor_data;
uint8_t active_setpoint = 1;  // Default to SP1
#else
extern volatile sensor_data_t sensor_data;
extern uint8_t active_setpoint;
#endif

// Single-stage EMA filter state (filter_step_sensor)
static int32_t lastK[SENSOR_COUNT - 1][K];
static uint8_t k_idx[SENSOR_COUNT - 1];          // ring index per sensor
static int32_t acc_y[SENSOR_COUNT - 1];       // EMA accumulator Q(S) per sensor
static int32_t acc_b[SENSOR_COUNT - 1];         // baseline EMA Q(Sb) per sensor
static bool init_done[SENSOR_COUNT - 1];         // init flag per sensor
static bool startup = true;

static inline int32_t median_int32(const int32_t *a, int n) {
	_Static_assert(K <= 70, "K exceeds median_int32 buffer size (max 70)");

	int32_t t[72] = {0};  // Buffer for sorting (initialized below, works for K up to 70)
	for (int i = 0; i < n; i++)
		t[i] = a[i];  // Copy input array before in-place sort

	for (int i = 1; i < n; i++) {
		int32_t key = t[i];
		int j = i - 1;
		while (j >= 0 && t[j] > key) {
			t[j + 1] = t[j];
			j--;
		}
		t[j + 1] = key;
	}
	return t[n >> 1];
}

static inline int32_t mad_int32_q0(const int32_t *a, int n, int32_t med) {
	_Static_assert(K <= 70, "K exceeds mad_int32_q0 buffer size (max 70)");

	int32_t d[72];
	for (int i = 0; i < n; i++) {
		int32_t diff = a[i] - med;
		d[i] = (diff >= 0) ? diff : -diff;
	}

	for (int i = 1; i < n; i++) {
		int32_t key = d[i];
		int j = i - 1;
		while (j >= 0 && d[j] > key) {
			d[j + 1] = d[j];
			j--;
		}
		d[j + 1] = key;
	}
	return d[n >> 1];
}

int32_t filter_step_sensor(sensor_id_t id, int32_t raw) {
	// Safety: Only filter O2 sensors (SENSOR1-SENSOR3), reject BAT/REFERENCE
	// Filter arrays sized [SENSOR_COUNT-1] = [4], valid indices 0-3
	// But only SENSOR1(0), SENSOR2(1), SENSOR3(2) should use filter
	if (id < SENSOR1 || id > SENSOR3) {
		return raw;
	}

	int32_t *lastK_ch = lastK[id];
	uint8_t *k_idx_ch = &k_idx[id];
	int32_t *acc_y_ch = &acc_y[id];
	int32_t *acc_b_ch = &acc_b[id];
	bool *init_ch = &init_done[id];

	// --- 0) First-call init for this sensor ---
	if (!*init_ch) {
		for (int i = 0; i < K; i++)
			lastK_ch[i] = raw;

		*acc_y_ch = raw << S;   // EMA accumulator in Q(S)
		*acc_b_ch = raw << Sb;  // baseline accumulator in Q(Sb)
		*init_ch = true;
	}

	// --- 5a) Hampel-lite on existing window (DISABLED for testing) ---
	// At 1 kHz sampling, K=7 window is too small (7ms) - disabling Hampel for now

	/* ORIGINAL HAMPEL CODE (disabled for testing): */
	int32_t median = median_int32(lastK_ch, K);
	int32_t mad = mad_int32_q0(lastK_ch, K, median);
	if (mad < MAD_MIN_COUNTS)
		mad = MAD_MIN_COUNTS;

	int32_t x_clip = raw;
	if (mad > 0) {
		int32_t thr = mad * SPIKE_K;
		int32_t diff = raw - median;

		if (diff > thr)
			x_clip = median + thr;
		if (diff < -thr)
			x_clip = median - thr;
	}

	// Update ring buffer for this sensor
	lastK_ch[*k_idx_ch] = x_clip;
	*k_idx_ch = (uint8_t) ((*k_idx_ch + 1) % K);

	// --- 5b) Primary EMA (Q(S)) ---
	*acc_y_ch += (((x_clip << S) - *acc_y_ch) >> S);
	int32_t y = *acc_y_ch >> S;    // back to mV

	// --- 5c) Slow baseline (optional) ---
	*acc_b_ch += (((y << Sb) - *acc_b_ch) >> Sb);
	int32_t b = *acc_b_ch >> Sb;

	// TODO: per-sensor baseline if you want:
	// int32_t y_out = y - ((b - g_b0_uV[id]) >> 3);
	(void) b;   // silence unused for now

	return y;  // filtered mV
}

static inline uint32_t adc_to_uV_at_sensor(uint16_t counts, uint32_t vdda_uv,
		int16_t gain) {
	// Safety: Validate gain to prevent division by zero
	if (gain <= 0) {
		return 0;  // Invalid gain = sensor fault
	}

	// Vin_sensor [µV] = (counts × VDDA[µV]) / (4095 × gain)
	// Use 64-bit intermediate to prevent overflow and preserve precision
	return (uint32_t)(((uint64_t)counts * vdda_uv) / ((uint64_t)4095 * gain));
}

void sensor_data_update(sensor_id_t SensorID, uint16_t val) {
	// Safety: Validate SensorID to prevent out-of-bounds array access
	// Valid range: SENSOR1(0), SENSOR2(1), SENSOR3(2), BAT(3), REFERENCE(4)
	if (SensorID >= SENSOR_COUNT) {
		return;  // Invalid ID, abort safely
	}

	uint32_t voltage_uv;

	// VREFINT uses raw ADC, no filtering
	if (SensorID == REFERENCE) {
		// VREFINT: macro returns uint32_t in mV, convert to µV
		// Cast to uint32_t to prevent overflow if macro returns uint16_t
		uint32_t vref_mv = (uint32_t)__LL_ADC_CALC_VREFANALOG_VOLTAGE(val, LL_ADC_RESOLUTION_12B);
		sensor_data.reference_uv = (int32_t)(vref_mv * 100U);
		return;
	}

	if (SensorID == BAT) {
		// Battery voltage: ADC → µV → mV
		// Copy volatile reference to local to prevent torn read
		uint32_t vref_uv = sensor_data.reference_uv;
		//voltage_uv = adc_to_uV_at_sensor(val, vref_uv, 1);
		voltage_uv =__LL_ADC_CALC_DATA_TO_VOLTAGE((int32_t)vref_uv/100, val, LL_ADC_RESOLUTION_12B);
			sensor_data.battery_voltage_mv = voltage_uv * 3U; //(int32_t) ((voltage_uv / 100)*3)/2; //using voltage divider 200kO:100kO
			if (sensor_data.battery_voltage_mv <= BAT_MIN_MV) {
				sensor_data.battery_percentage = 0;
			} else if (sensor_data.battery_voltage_mv >= BAT_MAX_MV) {
				sensor_data.battery_percentage = 100;
			} else {
				// Linear interpolation: % = (V - Vmin) / (Vmax - Vmin) * 100
				// BAT_BASE = BAT_MAX_MV - BAT_MIN_MV = 500mV
				sensor_data.battery_percentage =
						(uint8_t) (((sensor_data.battery_voltage_mv - BAT_MIN_MV) * 100)
								/ BAT_BASE);
			}
			sensor_data.battery_low = (sensor_data.battery_percentage < 10U);
			return;
	}

	// Only filter O2 sensors (not REFERENCE or BAT)
	// Count steps during warmup phase
	if (SensorID <= SENSOR3 && steps[SensorID] < MIN_WARMUP_STEPS + MAX_STABILITY_WAIT) {
		steps[SensorID]++;
	}

	int32_t filtered_sensor_adc = filter_step_sensor(SensorID, val);
	// O2 sensor voltage: filtered ADC counts → µV (no further conversion needed)
	voltage_uv = adc_to_uV_at_sensor(filtered_sensor_adc,
			sensor_data.reference_uv, 16);

	if (filtered_sensor_adc <= SENSOR_MIN_ADC_COUNTS ||
	    filtered_sensor_adc >= SENSOR_MAX_ADC_COUNTS) {
		sensor_data.s_valid[SensorID] = false;
		sensor_data.o2_s_uv[SensorID] = 0;
		sensor_data.o2_s_ppo[SensorID] = 0;
	} else {
		sensor_data.s_valid[SensorID] = true;
	}

	// Stability-based filter settling (after minimum warmup)
	// No longer auto-calibrates - just waits for filters to stabilize
	if (steps[SensorID] >= MIN_WARMUP_STEPS
			&& sensor_data.s_valid[SensorID]) {

		// Get fast and slow EMA values for stability check
		int32_t y = acc_y[SensorID] >> S;
		int32_t b = acc_b[SensorID] >> Sb;
		int32_t diff = (y > b) ? (y - b) : (b - y);

		if (diff <= STABLE_THRESHOLD) {
			stable_count[SensorID]++;
		} else {
			stable_count[SensorID] = 0;  // Reset on instability
		}

		// Stability achieved - trigger startup complete (no auto-cal)
		if (stable_count[SensorID] >= STABLE_REQUIRED) {
			// Do NOT auto-calibrate - just mark as stable
			// Startup screen will load calibration from flash
			if (startup) {
				screen_manager_startup_complete();
				startup = false;
			}
		}
		// Timeout - mark sensor as invalid, proceed anyway
		else if (stable_count[SensorID] < STABLE_REQUIRED) {
			steps[SensorID]++;
			if (steps[SensorID] >= MIN_WARMUP_STEPS + MAX_STABILITY_WAIT) {
				sensor_data.s_valid[SensorID] = false;
			}
		}
	}

	// Unconditional startup timeout - fires even when sensor is disconnected/invalid
	if (startup && SensorID <= SENSOR3
			&& steps[SensorID] >= MIN_WARMUP_STEPS + MAX_STABILITY_WAIT) {
		screen_manager_startup_complete();
		startup = false;
	}

	if (SensorID <= SENSOR3 && sensor_data.s_valid[SensorID]) {
		// Read calibration coefficients atomically (floats are not atomic on Cortex-M4)
		float slope = sensor_data.s_cal[0][SensorID];
		float intercept = sensor_data.s_cal[1][SensorID];

		int32_t ppo_calculated = (int32_t)(voltage_uv * slope + intercept);

		// Write results atomically to prevent torn reads by display/warning subsystems
		// Critical section <1µs: 2× float read + 2× int32_t write
		__disable_irq();
		sensor_data.o2_s_ppo[SensorID] = ppo_calculated;
		sensor_data.o2_s_uv[SensorID] = voltage_uv;
		__enable_irq();
	}

}

void sensor_init(void) {
	for (int i = 0; i < SENSOR_COUNT - 2; i++) {
		sensor_data.o2_s_uv[i] = 0;
		sensor_data.o2_s_ppo[i] = 0;
		sensor_data.s_cal[0][i] = 0;
		sensor_data.s_cal[1][i] = 0;
		sensor_data.s_valid[i] = true;
		sensor_data.os_s_cal_state[i] = NON_CALIBRATED;
		steps[i] = 0;
		stable_count[i] = 0;
		sensor_data.battery_voltage_mv = BAT_MAX_MV;
		sensor_data.battery_percentage = 100;

		// Initialize drift trackers for each sensor
		drift_tracker_init(&sensor_data.drift[i]);
	}

	// Note: Settings are loaded from flash in main.c after sensor_init
	// Do NOT initialize settings here to avoid overwriting flash data
}

