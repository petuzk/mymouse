#pragma once

#include <stdint.h>

#include <zephyr/kernel.h>

#include "services/hid/types.h"
#include "services/hid/source.h"
#include "util/bitmanip.h"

/* The collector thread uses a k_event object to wait on. The event is 32-bit long,
 * i.e. allows to wait for 32 conditions. The first 31 bits are used to ready the thread
 * when there is data available. The last, most significant bit is used to notify the thread
 * that the enabled sources bitmask has changed.
 */

#define MAX_NUM_OF_HID_SOURCES                  31
#define ENABLED_HID_SOURCES_CHANGED_EVENT_MASK  BIT(MAX_NUM_OF_HID_SOURCES)

static inline void hid_collector_notify_data_available(const struct hid_source* from_source) {
    extern struct k_event hid_collector_event;
    int source_id = hid_source_id(from_source);
    if (unlikely(source_id < 0 || source_id >= MAX_NUM_OF_HID_SOURCES)) {
        return;
    }
    k_event_post(&hid_collector_event, BIT(source_id));
}

static inline bool hid_collector_is_source_id_enabled(int source_id) {
    extern uint32_t hid_collector_enabled_sources;
    if (unlikely(source_id < 0 || source_id >= MAX_NUM_OF_HID_SOURCES)) {
        return false;
    }
    return BIT_AS_BOOL(hid_collector_enabled_sources, source_id);
}

static inline bool hid_collector_is_source_enabled(const struct hid_source* source) {
    return hid_collector_is_source_id_enabled(hid_source_id(source));
}

static inline void hid_collector_set_source_id_enabled(int source_id, bool enabled) {
    extern struct k_event hid_collector_event;
    extern uint32_t hid_collector_enabled_sources;
    if (unlikely(source_id < 0 || source_id >= MAX_NUM_OF_HID_SOURCES)) {
        return;
    }
    WRITE_BIT(hid_collector_enabled_sources, source_id, enabled);
    k_event_post(&hid_collector_event, ENABLED_HID_SOURCES_CHANGED_EVENT_MASK);
}

static inline void hid_collector_set_source_enabled(const struct hid_source* source, bool enabled) {
    hid_collector_set_source_id_enabled(hid_source_id(source), enabled);
}
