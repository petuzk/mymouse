#include "services/battery.h"

#include <stdint.h>

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "platform/adc.h"
#include "transport/bt/transport.h"

LOG_MODULE_REGISTER(battery);

static int64_t latest_meas_time_ms = -1;

int64_t battery_latest_measure_time_ms() {
    return latest_meas_time_ms;
}

struct battery_level_point {
    /** Remaining life in 0.01% units */
    int points;
    /** Battery voltage in mV */
    int voltage;
};

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

int battery_charge_level() {
    const struct battery_level_point *pa, *pb = bat_capacity_curve;
    int millivolts = adc_measurement_get_mv();

    if (millivolts >= pb->voltage) {
        // voltage above highest point
        return pb->points / 100;
    }

    // find lower bound for linear interpolation
    while (millivolts < pb->voltage) {
        if (pb->points > 0) {
            pb++;
        } else {
            // voltage below lowest point
            return pb->points / 100;
        }
    }

    // upper bound is a previous point
    pa = pb - 1;

    // linear interpolation
    return (pb->points + ((pa->points - pb->points) * (millivolts - pb->voltage) / (pa->voltage - pb->voltage))) / 100;
}

static void adc_meas_timer_cb(struct k_timer *timer) {
    ARG_UNUSED(timer);
    adc_trigger_measurement();
}

K_TIMER_DEFINE(adc_meas_timer, adc_meas_timer_cb, NULL);

static void send_meas_result(struct k_work *work) {
    ARG_UNUSED(work);
    int err = bt_transport.upd_bat_lvl(battery_charge_level());
    if (err) {
        LOG_WRN("upd_bat_val returned %d", err);
    }
}

K_WORK_DEFINE(send_meas_result_work, send_meas_result);

static void adc_meas_ready() {
    latest_meas_time_ms = k_uptime_get();
    k_work_submit(&send_meas_result_work);
}

static int battery_init(const struct device* dev) {
    ARG_UNUSED(dev);

    adc_set_measurement_ready_cb(adc_meas_ready);
    // start first measurement immediately instead of waiting until it will be called by kernel
    adc_trigger_measurement();
    // schedule sequential measurements
    k_timer_start(&adc_meas_timer, K_SECONDS(CONFIG_APP_BATTERY_MEAS_PERIOD_SEC), K_SECONDS(CONFIG_APP_BATTERY_MEAS_PERIOD_SEC));

    return 0;
}

SYS_INIT(battery_init, APPLICATION, CONFIG_APP_BATTERY_INIT_PRIORITY);
