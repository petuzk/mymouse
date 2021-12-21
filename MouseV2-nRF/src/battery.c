#include "battery.h"

// https://github.com/nrfconnect/sdk-zephyr/blob/main/samples/boards/nrf/battery/src/main.c
static const struct battery_level_point bat_capacity_curve[] = {
    /* "Curve" here eyeballed from captured data for the [Adafruit
	 * 3.7v 2000 mAh](https://www.adafruit.com/product/2011) LIPO
	 * under full load that started with a charge of 3.96 V and
	 * dropped about linearly to 3.58 V over 15 hours.  It then
	 * dropped rapidly to 3.10 V over one hour, at which point it
	 * stopped transmitting.
	 *
	 * Based on eyeball comparisons we'll say that 15/16 of life
	 * goes between 3.95 and 3.55 V, and 1/16 goes between 3.55 V
	 * and 3.1 V.
	 */

	{ 10000, 3950 },
	{ 625, 3550 },
	{ 0, 3100 },
};

static int32_t adc_buf;
static struct adc_sequence adc_seq = {
	.buffer = &adc_buf,
	.buffer_size = sizeof(adc_buf),
	.calibrate = true,
	.channels = BIT(0),
	.oversampling = 4,
	.resolution = 14,
};

static const struct adc_channel_cfg adc_cfg = {
	.input_positive = SAADC_CH_PSELP_PSELP_AnalogInput0 + DT_IO_CHANNELS_INPUT(VBATT),
	.gain = BATTERY_ADC_GAIN,
	.reference = BATTERY_ADC_REF,
	.acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
};

int mv2_bat_init() {
	const struct device *adc = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR(VBATT));
	return adc_channel_setup(adc, &adc_cfg);
}

int mv2_bat_sample_mv(millivolts_t *mv) {
	int rv;
	const struct device *adc = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR(VBATT));

	rv = adc_read(adc, &adc_seq);
	if (rv < 0) {
		return rv;
	}
	adc_seq.calibrate = false;

	// reverse adc gain
	rv = adc_raw_to_millivolts(adc_ref_internal(adc), adc_cfg.gain, adc_seq.resolution, adc_seq.buffer);
	if (rv) {
		return rv;
	}

	// reverse resistor divider gain
	*mv = (int64_t) adc_buf * BATTERY_DIV_FULL / BATTERY_DIV_OUT;
	return 0;
}

points_t mv2_bat_convert_mv2pts(millivolts_t mv) {
	const struct battery_level_point *pa, *pb = bat_capacity_curve;

	if (mv >= pb->voltage) {
		// voltage above highest point
		return pb->points;
	}

	// find lower bound for linear interpolation
	while (mv < pb->voltage) {
		if (pb->points > 0) {
			pb++;
		} else {
			// voltage below lowest point
			return pb->points;
		}
	}

	// upper bound is a previous point
	pa = pb - 1;

	// linear interpolation
	return pb->points + ((pa->points - pb->points) * (mv - pb->voltage) / (pa->voltage - pb->voltage));
}
