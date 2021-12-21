#ifndef MOUSEV2_BATTERY_H
#define MOUSEV2_BATTERY_H

#include <zephyr.h>
#include <drivers/adc.h>

#define VBATT            DT_PATH(vbatt)

// 0.6V / (1/5) = 3V range
#define BATTERY_ADC_REF  ADC_REF_INTERNAL  // 0.6 V
#define BATTERY_ADC_GAIN ADC_GAIN_1_5      // 1/5

#define BATTERY_DIV_OUT  DT_PROP(VBATT, output_ohms)
#define BATTERY_DIV_FULL DT_PROP(VBATT, full_ohms)

typedef int16_t millivolts_t;
typedef int16_t points_t;

struct battery_level_point {
	/** Remaining life in 0.01% units */
	points_t points;
	/** Battery voltage in mV */
	millivolts_t voltage;
};

int mv2_bat_init();
int mv2_bat_sample_mv(millivolts_t *mv);
points_t mv2_bat_convert_mv2pts(millivolts_t mv);

#endif // MOUSEV2_BATTERY_H
