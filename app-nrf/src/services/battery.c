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

K_SEM_DEFINE(adc_meas_sem, 0, 1);

static void adc_meas_ready() {
    latest_meas_time_ms = k_uptime_get();
    k_sem_give(&adc_meas_sem);
}

static inline void battery_meas_send() {
    adc_trigger_measurement();
    if (k_sem_take(&adc_meas_sem, K_SECONDS(1)) == -EAGAIN) {
        LOG_WRN("timed out waiting for meas result");
        return;
    }

    int level = battery_charge_level();
    int err = bt_transport.upd_bat_lvl(level);
    if (err) {
        LOG_WRN("upd_bat_val returned %d", err);
        return;
    }

    LOG_INF("updated level to %d", level);
}

static int battery_meas_send_thread(void* p1, void* p2, void* p3) {
    adc_set_measurement_ready_cb(adc_meas_ready);

    while (true) {
        battery_meas_send();
        k_sleep(K_SECONDS(CONFIG_APP_BATTERY_MEAS_PERIOD_SEC));
    }

    return 0;
}

K_THREAD_DEFINE(bat_meas,
                CONFIG_APP_BATTERY_MEAS_THREAD_STACK_SIZE,
                battery_meas_send_thread,
                NULL, NULL, NULL,
                CONFIG_APP_BATTERY_MEAS_THREAD_PRIORITY,
                0,
                0);
