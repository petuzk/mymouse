#pragma once

#include <stdint.h>

#include <hal/nrf_spim.h>
#include <zephyr/kernel.h>

// stolen from zephyr/drivers/spi/spi_nrfx_spim.c
// static inline is verified to be computed & inlined in each compilation unit
static inline nrf_spim_frequency_t get_nrf_spim_frequency(uint32_t frequency)
{
    if (frequency < 250000) {
        return NRF_SPIM_FREQ_125K;
    } else if (frequency < 500000) {
        return NRF_SPIM_FREQ_250K;
    } else if (frequency < 1000000) {
        return NRF_SPIM_FREQ_500K;
    } else if (frequency < 2000000) {
        return NRF_SPIM_FREQ_1M;
    } else if (frequency < 4000000) {
        return NRF_SPIM_FREQ_2M;
    } else if (frequency < 8000000) {
        return NRF_SPIM_FREQ_4M;
    } else {
        return NRF_SPIM_FREQ_8M;
    }
}

static inline void spi_configure(nrf_spim_mode_t op_mode, nrf_spim_bit_order_t bit_order, nrf_spim_frequency_t freq) {
    nrf_spim_configure(NRF_SPIM0, op_mode, bit_order);
    nrf_spim_frequency_set(NRF_SPIM0, freq);
}

extern struct k_sem spi_sem;

static inline bool spi_transfer_in_progress() {
    return k_sem_count_get(&spi_sem) == 0;
}

int spi_transceive(void* tx_buf, uint32_t tx_len, void* rx_buf, uint32_t rx_len, void (*callback)());
