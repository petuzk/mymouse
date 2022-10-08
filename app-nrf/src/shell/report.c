#include "shell/report.h"

#include <math.h>
#include <shell/shell.h>
#include <stdlib.h>
#include <zephyr.h>

#include "hid_report_struct.h"

static const float sin_table[21] = {
    0.0,
    0.07845909572784,
    0.15643446504023,
    0.23344536385590,
    0.30901699437494,
    0.38268343236508,
    0.45399049973954,
    0.52249856471594,
    0.58778525229247,
    0.64944804833018,
    0.70710678118654,
    0.76040596560003,
    0.80901699437494,
    0.85264016435409,
    0.89100652418836,
    0.92387953251128,
    0.95105651629515,
    0.97236992039767,
    0.98768834059513,
    0.99691733373312,
    1.0,
};

static float sin_from_table(int step) {
    step = step % 80;
    if (0 <= step && step < 20) {
        return sin_table[step];
    }
    else if (20 <= step && step < 40) {
        return sin_table[40-step];
    }
    else if (40 <= step && step < 60) {
        return -(sin_table[step-40]);
    }
    else if (60 <= step && step < 80) {
        return -(sin_table[80-step]);
    }
    return 0.0f;
}

static float cos_from_table(int step) {
    return sin_from_table(step + 20);
}

static struct {
    int wheel_delta;
    struct {
        int x_delta;
        int y_delta;
        int radius;
    } movement;
} data = {};

static int cmd_scroll(const struct shell *shell, size_t argc, char **argv) {
    data.wheel_delta += strtol(argv[1], NULL, 0);
    return 0;
}

bool report_shell_fill_rotation(struct hid_report *report) {
    if (data.wheel_delta) {
        report->wheel_delta = data.wheel_delta;
        data.wheel_delta = 0;
        return true;
    }

    return false;
}

static int cmd_move(const struct shell *shell, size_t argc, char **argv) {
    data.movement.x_delta += strtol(argv[1], NULL, 0);
    data.movement.y_delta += strtol(argv[2], NULL, 0);
    return 0;
}

static int cmd_cmove(const struct shell *shell, size_t argc, char **argv) {
    data.movement.radius = strtol(argv[1], NULL, 0);
    return 0;
}

bool report_shell_fill_movement(struct hid_report *report) {
    if (data.movement.x_delta || data.movement.y_delta) {
        report->x_delta = data.movement.x_delta;
        report->y_delta = data.movement.y_delta;
        data.movement.x_delta = 0;
        data.movement.y_delta = 0;
        return true;
    }

    if (data.movement.radius) {
        report->x_delta = (int) (data.movement.radius * cos_from_table((k_uptime_get() >> 4) % 80));
        report->y_delta = (int) (data.movement.radius * sin_from_table((k_uptime_get() >> 4) % 80));
        return true;
    }

    return false;
}

SHELL_STATIC_SUBCMD_SET_CREATE(report_cmdset,
    SHELL_CMD_ARG(scroll, NULL, "Scroll by N rows", cmd_scroll, 2, 0),
    SHELL_CMD_ARG(move, NULL, "Move mouse by X Y", cmd_move, 3, 0),
    SHELL_CMD_ARG(cmove, NULL, "Continuously move in a circle of radius R (0 = stop)", cmd_cmove, 2, 0),
    // SHELL_CMD(ls, NULL, "List files in current directory", cmd_ls),
    // SHELL_CMD_ARG(mkdir, NULL, "Create directory", cmd_mkdir, 2, 0),
    // SHELL_CMD(pwd, NULL, "Print current working directory", cmd_pwd),
    // SHELL_CMD_ARG(read, NULL, "Read from file", cmd_read, 2, 255),
    // SHELL_CMD_ARG(rm, NULL, "Remove file", cmd_rm, 2, 0),
    // SHELL_CMD_ARG(statvfs, NULL, "Show file system state", cmd_statvfs, 2, 0),
    // SHELL_CMD_ARG(trunc, NULL, "Truncate file", cmd_trunc, 2, 255),
    // SHELL_CMD_ARG(write, NULL, "Write file", cmd_write, 3, 255),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(report, &report_cmdset, "Report modification", NULL);
