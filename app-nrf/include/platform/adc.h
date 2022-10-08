#pragma once

typedef void (*measurement_ready_cb_t)();

void adc_trigger_measurement();
int adc_measurement_get_mv();
void adc_set_measurement_ready_cb(measurement_ready_cb_t callback);
