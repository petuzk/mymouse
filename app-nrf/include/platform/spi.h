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

/**
 * @brief Configure SPI for the data transfer. Requires lock to be obtained with spi_lock.
 */
int spi_configure(const struct spi_configuration* config);

/**
 * @brief Initiate an SPI data transfer with simultaneous tramsission and reception.
 *
 * This is a low-level interface. This function configures the buffers and starts
 * SPIM transfer with DMA engine. The specified callback is called from ISR when
 * the transfer is done.
 */
int spi_transceive(const struct spi_transfer_spec* spec, void (*callback)(void*), void* arg);

/**
 * @brief Similar to spi_transceive, but waits for the transaction to end.
 */
int spi_transceive_sync(struct spi_transfer_spec* spec);

/**
 * @brief Perform an SPI data transfer with given SPI configuration and managed CS pin.
 *
 * This is a higher-level interface that manages SPI lock, SPI configuration and CS pin state.
 * When the transfer is completed, it calls the callback from ISR context and passes a semafore
 * that the caller thread awaits on with the specified timeout. The callback can then either
 * perform an additional transmission(s) with @ref spi_transceive, or give the semafore to signal
 * the completion and unblock the caller thread. Upon return, the function will pull CS high.
 */
int spi_transceive_managed(const struct spi_configuration* config, uint32_t cs_pin,
                           const struct spi_transfer_spec* spec, void (*callback)(void*),
                           k_timeout_t timeout);
