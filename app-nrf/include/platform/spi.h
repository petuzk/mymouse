#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <hal/nrf_spim.h>
#include <zephyr/kernel.h>

struct spi_configuration {
    nrf_spim_mode_t op_mode;
    nrf_spim_bit_order_t bit_order;
    nrf_spim_frequency_t freq;

    /// If true, subsequent calls to spi_configure() with the same pointer will have no effect
    bool is_const;
};

struct spi_transfer_spec {
    void* tx_buf;
    uint32_t tx_len;
    void* rx_buf;
    uint32_t rx_len;
};

// adapted from zephyr/drivers/spi/spi_nrfx_spim.c
#define SPI_CONFIG_FREQUENCY(frequencyHz) \
    (frequencyHz) < 250000 ? NRF_SPIM_FREQ_125K : (\
    (frequencyHz) < 500000 ? NRF_SPIM_FREQ_250K : (\
    (frequencyHz) < 1000000 ? NRF_SPIM_FREQ_500K : (\
    (frequencyHz) < 2000000 ? NRF_SPIM_FREQ_1M : (\
    (frequencyHz) < 4000000 ? NRF_SPIM_FREQ_2M : (\
    (frequencyHz) < 8000000 ? NRF_SPIM_FREQ_4M : NRF_SPIM_FREQ_8M)))))

/**
 * @brief Obtain SPI lock (i.e. lock the mutex).
 * A thread may only call other SPI functions with lock obtained.
 *
 * @param timeout Timeout passed to k_mutex_lock
 * @return 0 on success, negative error code otherwise
 */
int spi_lock(k_timeout_t timeout);

/**
 * @brief Unlock SPI mutex.

 * @return 0 on success, negative error code otherwise
 */
int spi_unlock();

int spi_configure(const struct spi_configuration* config);
int spi_transceive(const struct spi_transfer_spec* spec, void (*callback)());
int spi_transceive_sync(struct spi_transfer_spec* spec);
