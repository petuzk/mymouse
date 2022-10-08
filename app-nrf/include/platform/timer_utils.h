#pragma once

#include <hal/nrf_timer.h>

static inline void timer_reconfigure(NRF_TIMER_Type* timer, nrf_timer_frequency_t frequency) {
    nrf_timer_task_trigger(timer, NRF_TIMER_TASK_STOP);
    nrf_timer_task_trigger(timer, NRF_TIMER_TASK_CLEAR);

    nrf_timer_mode_set(timer, NRF_TIMER_MODE_TIMER);
    nrf_timer_bit_width_set(timer, NRF_TIMER_BIT_WIDTH_32);
    nrf_timer_frequency_set(timer, frequency);
}

#define timer_set_cc(timer, channel_num, cc_value) do {                     \
    nrf_timer_cc_set(timer, NRF_TIMER_CC_CHANNEL ## channel_num, cc_value); \
    nrf_timer_event_clear(timer, NRF_TIMER_EVENT_COMPARE ## channel_num);   \
} while (0)

#define timer_start(timer) \
    nrf_timer_task_trigger(timer, NRF_TIMER_TASK_START)

#define timer_stop(timer) \
    nrf_timer_task_trigger(timer, NRF_TIMER_TASK_STOP)

#define timer_expired(timer, channel_num) \
    nrf_timer_event_check(timer, NRF_TIMER_EVENT_COMPARE ## channel_num)
