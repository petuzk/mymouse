#include <stdlib.h>

#include <zephyr/shell/shell.h>

#include "services/battery.h"

static int cmd_battery(const struct shell *shell, size_t argc, char **argv) {
    if (argc == 1) {
        int64_t meas_time = battery_latest_measure_time_ms();
        if (meas_time == -1) {
            shell_warn(shell, "no data available yet, use 'service battery measure' to update");
        } else {
            int meas_mv = battery_millivolts();
            int chg_level = battery_charge_level();
            int64_t age_seconds = k_uptime_delta(&meas_time) / 1000;
            shell_info(shell, "Battery voltage: %d mV (%d%%)", meas_mv, chg_level);
            shell_info(shell, "Last update: %lld s ago (use 'service battery measure' to update)", age_seconds);
        }
    }
    else if (argc == 2 && !strcmp(argv[1], "measure")) {
        // todo: this does not really work anymore, the thread is sleeping for a fixed time
        battery_trigger_measurement();
    }
    else {
        shell_error(shell, "invalid arguments");
    }
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(services_cmdset,
    SHELL_CMD(battery, NULL, "Battery information", cmd_battery),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(service, &services_cmdset, "Services information and control", NULL);
