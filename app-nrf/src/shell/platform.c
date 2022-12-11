#include <stdlib.h>

#include <hal/nrf_gpio.h>
#include <zephyr/shell/shell.h>

#include "platform/adc.h"
#include "platform/clock_suppl.h"
#include "platform/pwm.h"
#include "platform/shutdown.h"

static int cmd_shutdown(const struct shell *shell, size_t argc, char **argv) {
    shutdown();

    return 0;
}

static int cmd_clock_suppl(const struct shell *shell, size_t argc, char **argv) {
    const char* arg = argv[1];
    if ((arg[0] != '0' && arg[0] != '1') || arg[1] != 0) {
        shell_error(shell, "argument must be one of {0, 1}");
        return -ENOEXEC;
    }

    if (arg[0] == '1') {
        shell_info(shell, "enabling clock supply");
        enable_clock_supply();
    } else {
        shell_info(shell, "disabling clock supply");
        disable_clock_supply();
    }

    return 0;
}

static int cmd_pwm_sched(const struct shell *shell, size_t argc, char **argv) {
    struct led_value new_value = {
        .r = strtol(argv[1], NULL, 0),
        .g = strtol(argv[2], NULL, 0),
    };
    pwm_schedule_sequence_from_last(new_value, strtol(argv[3], NULL, 0));

    return 0;
}

static int cmd_pwm_done(const struct shell *shell, size_t argc, char **argv) {
    shell_info(shell, "%s", pwm_sequence_done() ? "true" : "false");

    return 0;
}

static int cmd_adc(const struct shell *shell, size_t argc, char **argv) {
    adc_trigger_measurement();
    k_usleep(70);
    shell_info(shell, "%d mV", adc_measurement_get_mv());

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(platform_cmdset_pwm,
    SHELL_CMD_ARG(sched, NULL, "pwm red green time_ms, duty cycle 0-99", cmd_pwm_sched, 4, 0),
    SHELL_CMD(done, NULL, "check whether sequence is done playing", cmd_pwm_done),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(platform_cmdset,
    SHELL_CMD(shutdown, NULL, "Cleanup and enter poweroff mode", cmd_shutdown),
    SHELL_CMD_ARG(clock_suppl, NULL, "Enable/disable AVR clock supply", cmd_clock_suppl, 2, 0),
    SHELL_CMD(pwm, &platform_cmdset_pwm, "pwm commands", NULL),
    SHELL_CMD(adc, NULL, "Perform ADC measurement", cmd_adc),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(platform, &platform_cmdset, "Platform control", NULL);
