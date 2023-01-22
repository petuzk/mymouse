#include <stdbool.h>
#include <stdint.h>

#include <hal/nrf_gpio.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include "platform/gpio.h"
#include "services/hid/collector.h"
#include "services/hid/source.h"

#define MOT_PIN PINOFPROP(optical_sensor, mot_gpios)

static void hid_src_opt_sensor_report_filler(struct hid_report* report) {
    const struct device* sensor = DEVICE_DT_GET(DT_NODELABEL(optical_sensor));
    struct sensor_value value;

    sensor_sample_fetch(sensor);
    sensor_channel_get(sensor, SENSOR_CHAN_POS_DX, &value);
    report->x_delta = value.val1;
    sensor_channel_get(sensor, SENSOR_CHAN_POS_DY, &value);
    report->y_delta = -value.val1;
}

HID_SOURCE_REGISTER(hid_src_opt_sensor, hid_src_opt_sensor_report_filler, APP_HID_SOURCE_OPT_SENSOR_PRIORITY);

static void hid_src_opt_sensor_gpio_cb(uint32_t pin, bool new_value) {
    // motion detect pin is active low
    if (!new_value) {
        hid_collector_notify_data_available(hid_src_opt_sensor);
    }
    gpio_set_edge_cb(pin, new_value, hid_src_opt_sensor_gpio_cb);
}

static int hid_src_opt_sensor_init(const struct device* dev) {
    ARG_UNUSED(dev);

    nrf_gpio_cfg_input(PINOFPROP(optical_sensor, mot_gpios), NRF_GPIO_PIN_NOPULL);
    // notify collector if the pin is already low, configure callback
    hid_src_opt_sensor_gpio_cb(MOT_PIN, nrf_gpio_pin_read(MOT_PIN));

    return 0;
}

SYS_INIT(hid_src_opt_sensor_init, APPLICATION, CONFIG_APP_HID_SOURCE_INIT_PRIORITY);
