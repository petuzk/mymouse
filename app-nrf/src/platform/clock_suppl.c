#include "platform/clock_suppl.h"

#include <hal/nrf_clock.h>
#include <hal/nrf_gpio.h>
#include <hal/nrf_gpiote.h>
#include <hal/nrf_ppi.h>
#include <hal/nrf_timer.h>
#include <logging/log.h>

#include "platform/hw_resources.h"

LOG_MODULE_DECLARE(platform);

#define CLK_SUP_GPIOTE_PIN_NUM 4

void enable_clock_supply() {
start:
    if (nrf_clock_hf_src_get(NRF_CLOCK) != NRF_CLOCK_HFCLK_HIGH_ACCURACY) {
        LOG_INF("low accuracy clock, requesting HFXO");
        nrf_clock_task_trigger(NRF_CLOCK, NRF_CLOCK_TASK_HFCLKSTART);
        while (!nrf_clock_event_check(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKSTARTED));
        nrf_clock_event_clear(NRF_CLOCK, NRF_CLOCK_EVENT_HFCLKSTARTED);
        goto start;
    } else {
        LOG_INF("HFXO is enabled");
    }

    nrf_timer_task_trigger(CLK_SUP_TIMER, NRF_TIMER_TASK_STOP);
    nrf_timer_task_trigger(CLK_SUP_TIMER, NRF_TIMER_TASK_CLEAR);
    nrf_timer_event_clear(CLK_SUP_TIMER, NRF_TIMER_EVENT_COMPARE0);

    nrf_timer_mode_set(CLK_SUP_TIMER, NRF_TIMER_MODE_TIMER);
    nrf_timer_bit_width_set(CLK_SUP_TIMER, NRF_TIMER_BIT_WIDTH_16);
    nrf_timer_frequency_set(CLK_SUP_TIMER, NRF_TIMER_FREQ_16MHz);
    nrf_timer_cc_set(CLK_SUP_TIMER, NRF_TIMER_CC_CHANNEL0, 1);
    nrf_timer_shorts_set(CLK_SUP_TIMER, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK);

    nrf_gpiote_task_configure(NRF_GPIOTE, CLK_SUP_GPIOTE_CH_NUM, CLK_SUP_GPIOTE_PIN_NUM, NRF_GPIOTE_POLARITY_TOGGLE, NRF_GPIOTE_INITIAL_VALUE_LOW);
    nrf_gpiote_task_enable(NRF_GPIOTE, CLK_SUP_GPIOTE_CH_NUM);

    nrf_ppi_channel_endpoint_setup(
        NRF_PPI, CLK_SUP_PPI_CH,
        nrf_timer_event_address_get(CLK_SUP_TIMER, NRF_TIMER_EVENT_COMPARE0),
        nrf_gpiote_task_address_get(NRF_GPIOTE, CONCAT(NRF_GPIOTE_TASK_OUT_, CLK_SUP_GPIOTE_CH_NUM))
    );
    nrf_ppi_channel_enable(NRF_PPI, CLK_SUP_PPI_CH);

    nrf_timer_task_trigger(CLK_SUP_TIMER, NRF_TIMER_TASK_START);
}

void disable_clock_supply() {
    nrf_timer_task_trigger(CLK_SUP_TIMER, NRF_TIMER_TASK_STOP);
    nrf_timer_task_trigger(CLK_SUP_TIMER, NRF_TIMER_TASK_CLEAR);

    nrf_ppi_channel_disable(NRF_PPI, CLK_SUP_PPI_CH);
    nrf_gpiote_task_trigger(NRF_GPIOTE, CONCAT(NRF_GPIOTE_TASK_CLR_, CLK_SUP_GPIOTE_CH_NUM));
}
