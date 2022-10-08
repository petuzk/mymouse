#pragma once

#include <stdint.h>

/**
 * @brief Return timestamp of latest measurement, in milliseconds
 * (as returned by k_uptime_get()). In case if there was no measurement
 * yet return -1.
 */
int64_t battery_latest_measure_time_ms();

/**
 * @brief Return latest charge level measurement
 *
 * @return int Charge level in percents (0-100 both inclusive)
 */
int battery_charge_level();

// aliases to make everything accessible from battery service public interface
#include "platform/adc.h"

#define battery_millivolts()          adc_measurement_get_mv()
#define battery_trigger_measurement() adc_trigger_measurement()
