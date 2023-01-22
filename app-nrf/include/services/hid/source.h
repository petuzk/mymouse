#pragma once

#include <zephyr/kernel.h>

#include "services/hid/types.h"

/**
 * @brief Create and register a HID source.
 *
 * @param _name Defines the name of this source handle variable,
 *              and is passed as string to @c hid_source::name
 * @param _report_filler Passed to @c hid_source::report_filler
 * @param _priority Priority of a HID source (lower number is higher priority),
 *                  defines the order of report filling. It is recommended to
 *                  assign sources with long-running fillers a higher priority
 *                  so that other sources can collect up-to-date data.
 */
#define HID_SOURCE_REGISTER(_name, _report_filler, _priority) \
    /* this name is constructed so that the linker-generated list will be sorted by priority */ \
    const STRUCT_SECTION_ITERABLE(hid_source, _hid_source_##_priority##_##_name) = { \
        .name = #_name, \
        .report_filler = _report_filler, \
    }; \
    /* this is "an alias" for this name to be accessible from user code */ \
    const struct hid_source* _name = &_hid_source_##_priority##_##_name

/**
 * @brief Returns source ID (index in the linker-generated list) of a given @c source
 */
static inline int hid_source_id(const struct hid_source* source) {
    static const struct hid_source* first;
    STRUCT_SECTION_GET(hid_source, 0, &first);
    return source - first;
}
