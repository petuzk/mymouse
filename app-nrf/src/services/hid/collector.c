#include "services/hid/collector.h"

#include <assert.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "hid_report_struct.h"
#include "services/hid/types.h"
#include "services/hid/sink.h"
#include "util.h"

LOG_MODULE_REGISTER(hid_collector);

K_EVENT_DEFINE(hid_collector_event);
uint32_t hid_collector_enabled_sources = BIT_MASK(MAX_NUM_OF_HID_SOURCES);

static inline uint32_t get_existing_sources_bitmask() {
    int num_sources;
    STRUCT_SECTION_COUNT(hid_source, &num_sources);
    if (num_sources > MAX_NUM_OF_HID_SOURCES) {
        LOG_WRN(
            "number of sources exceeds %d, %d sources will be permanently disabled",
            MAX_NUM_OF_HID_SOURCES, num_sources - MAX_NUM_OF_HID_SOURCES);
        num_sources = MAX_NUM_OF_HID_SOURCES;
    }
    return BIT_MASK(num_sources);
}

static uint32_t wait_for_data_events(struct k_event* thread_event, uint32_t* enabled_sources) {
    uint32_t events = 0;
    while (!events) {
        events = k_event_wait(thread_event,
                              *enabled_sources | ENABLED_HID_SOURCES_CHANGED_EVENT_MASK,
                              false,
                              K_FOREVER);
        if (unlikely(events & ENABLED_HID_SOURCES_CHANGED_EVENT_MASK)) {
            // clear "sources changed" event and start over to process events from all enabled sources in one go
            k_event_set_masked(thread_event, 0, ENABLED_HID_SOURCES_CHANGED_EVENT_MASK);
            events = 0;
        }
    }
    return events;
}

static int hid_collector_thread_entry(struct k_event* thread_event, uint32_t* enabled_sources, void* unused) {
    ARG_UNUSED(unused);

    struct hid_report report = {};

    // perform "and" instead of assignment to preserve already changed states (if any)
    *enabled_sources &= get_existing_sources_bitmask();

    while (true) {
        uint32_t events = wait_for_data_events(thread_event, enabled_sources);
        k_event_set_masked(thread_event, 0, events);  // clear received events

        while (events) {
            uint32_t source_id = get_next_bit_pos(&events);
            struct hid_source* source;
            STRUCT_SECTION_GET(hid_source, source_id, &source);
            source->report_filler(&report);
        }

        hid_sink_impl(&report);
        clear_non_persistent_data_in_report(&report);
    }

    return 0;
}

K_THREAD_DEFINE(hid_collector_thread,
                CONFIG_APP_HID_COLLECTOR_THREAD_STACK_SIZE,
                hid_collector_thread_entry,
                &hid_collector_event, &hid_collector_enabled_sources, NULL,
                CONFIG_APP_HID_COLLECTOR_THREAD_PRIORITY,
                K_ESSENTIAL,
                0);
