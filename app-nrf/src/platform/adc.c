#include "platform/adc.h"

#include <stddef.h>
#include <stdint.h>

#include <devicetree.h>
#include <hal/nrf_saadc.h>
#include <init.h>
#include <irq.h>
#include <zephyr.h>

#define ADC_CHANNEL_ID    0
#define ADC_INP_RANGE_MV  3000
#define ADC_RES_BITS      14

#define RESISTOR_DIV_OUT  DT_PROP(DT_PATH(vbatt), output_ohms)
#define RESISTOR_DIV_FULL DT_PROP(DT_PATH(vbatt), full_ohms)
#define ADC_DT_NODE       DT_NODELABEL(adc)
#define ADC_INPUT_CHANNEL (NRF_SAADC_INPUT_AIN0 + DT_IO_CHANNELS_INPUT(DT_PATH(vbatt)))

static const float resistor_div_meas_to_inp = (float) RESISTOR_DIV_FULL / RESISTOR_DIV_OUT;

// 0.6V ref / (1/5) gain = 3V input range
static nrf_saadc_channel_config_t adc_config = {
    .acq_time = NRF_SAADC_ACQTIME_40US,
    .burst = NRF_SAADC_BURST_DISABLED,
    .gain = NRF_SAADC_GAIN1_5,
    .mode = NRF_SAADC_MODE_SINGLE_ENDED,
    .reference = NRF_SAADC_REFERENCE_INTERNAL,
    .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
    .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
};

static nrf_saadc_value_t result_buffer = 0;
static measurement_ready_cb_t meas_rdy_cb = NULL;

static void adc_irq_handler() {
    // irq is called on END event only
    nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_END);
    nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_STOP);
    nrf_saadc_disable(NRF_SAADC);
    if (meas_rdy_cb) {
        meas_rdy_cb();
    }
}

static int adc_init(const struct device* dev) {
    ARG_UNUSED(dev);

    nrf_saadc_channel_init(NRF_SAADC, ADC_CHANNEL_ID, &adc_config);
    nrf_saadc_resolution_set(NRF_SAADC, CONCAT(CONCAT(NRF_SAADC_RESOLUTION_, ADC_RES_BITS), BIT));
    nrf_saadc_buffer_init(NRF_SAADC, &result_buffer, 1);
    nrf_saadc_channel_input_set(NRF_SAADC, ADC_CHANNEL_ID, ADC_INPUT_CHANNEL, NRF_SAADC_INPUT_DISABLED);

    nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_END);
    nrf_saadc_int_set(NRF_SAADC, NRF_SAADC_INT_END);
    IRQ_CONNECT(DT_IRQN(ADC_DT_NODE), DT_IRQ(ADC_DT_NODE, priority), adc_irq_handler, NULL, 0);
    irq_enable(DT_IRQN(ADC_DT_NODE));

    return 0;
}

void adc_trigger_measurement() {
    nrf_saadc_enable(NRF_SAADC);
    nrf_saadc_event_clear(NRF_SAADC, NRF_SAADC_EVENT_END);
    nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_START);
    nrf_saadc_task_trigger(NRF_SAADC, NRF_SAADC_TASK_SAMPLE);
}

int adc_measurement_get_mv() {
    return (result_buffer * ADC_INP_RANGE_MV >> ADC_RES_BITS) * resistor_div_meas_to_inp;
}

void adc_set_measurement_ready_cb(measurement_ready_cb_t callback) {
    meas_rdy_cb = callback;
}

SYS_INIT(adc_init, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY);
