#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#include "hid_report_struct.h"
#include "services/hid/collector.h"
#include "services/hid/source.h"
#include "services/hid/types.h"

// helper functions

static inline int num_registered_sources() {
    int num_sources;
    STRUCT_SECTION_COUNT(hid_source, &num_sources);
    return num_sources;
}

static const char* hid_source_id_status_str(int source_id) {
    if (unlikely(source_id < 0)) {
        return "invalid";
    }
    if (unlikely(source_id >= num_registered_sources())) {
        return "unregistered";
    }
    if (unlikely(source_id >= MAX_NUM_OF_HID_SOURCES)) {
        return "permanently disabled";
    }
    return hid_collector_is_source_id_enabled(source_id) ? "enabled" : "disabled";
}

// shell hid source

static struct hid_report pending_report = {};

static void shell_report_filler(struct hid_report* report) {
    report->x_delta += pending_report.x_delta;
    report->y_delta += pending_report.y_delta;
    pending_report.x_delta = 0;
    pending_report.y_delta = 0;
}

HID_SOURCE_REGISTER(hid_src_shell, shell_report_filler, 5);  // todo: priority

// actual commands

static int cmd_sources(const struct shell *shell, size_t argc, char **argv) {
    int source_id = 0;
    shell_print(shell, "List of sources (ordered by descending prioroty):");

    STRUCT_SECTION_FOREACH(hid_source, source) {
        shell_print(shell, "#%d %s:", source_id, source->name);
        shell_print(shell, "    filler %p, %s", source->report_filler, hid_source_id_status_str(source_id));
        source_id++;
    }

    return 0;
}

static int cmd_enable_disable(const struct shell *shell, size_t argc, char **argv) {
    const struct hid_source* source;
    if (argc == 1) {
        source = hid_src_shell;
    } else {
        int source_id = strtol(argv[1], NULL, 0);
        int num_sources = num_registered_sources();
        if (source_id < 0 || source_id >= num_sources) {
            shell_error(shell, "id must be in range 0-%d", num_sources - 1);
            return -EINVAL;
        }
        STRUCT_SECTION_GET(hid_source, source_id, &source);
    }

    bool enable = argv[0][0] == 'e';
    hid_collector_set_source_enabled(source, enable);
    shell_print(shell, "%s is now %s", source->name, hid_source_id_status_str(hid_source_id(source)));

    return 0;
}

static int cmd_report_move(const struct shell *shell, size_t argc, char **argv) {
    pending_report.x_delta += strtol(argv[1], NULL, 0);
    pending_report.y_delta += strtol(argv[2], NULL, 0);
    hid_collector_notify_data_available(hid_src_shell);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(hid_report_cmdset,
    SHELL_CMD_ARG(move, NULL, "Move cursor by X Y", cmd_report_move, 3, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(hid_cmdset,
    SHELL_CMD(sources, NULL, "List all HID sources", cmd_sources),
    SHELL_CMD_ARG(enable, NULL, "Enable HID source by id", cmd_enable_disable, 1, 1),
    SHELL_CMD_ARG(disable, NULL, "Disable HID source by id", cmd_enable_disable, 1, 1),
    SHELL_CMD(report, &hid_report_cmdset, "Report modification", NULL),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(hid, &hid_cmdset, "HID sources & collector config", NULL);
