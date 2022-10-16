/*
 * Copyright (c) 2021 Taras Radchenko
 *
 * SPDX-License-Identifier: MIT
 */

#include <logging/log.h>

LOG_MODULE_REGISTER(adns7530);

#include "drivers/adns7530.h"

#ifdef CONFIG_SOC_NRF52832_ALLOW_SPIM_DESPITE_PAN_58
// when SPIM is used, transactions where RX buffer size is 1 will be declined by sdk
// workaround: receive 2 bytes instead, sensor won't do anything during 2nd byte clk
#define ONE_BYTE_RX_BUF_SIZE 2
#else
#define ONE_BYTE_RX_BUF_SIZE 1
#endif

static inline struct spi_dt_spec* adns7530_get_spi(const struct device *dev) {
    return &((struct adns7530_config *) dev->config)->spi;
}

/**
 * @brief Read data from a single sensor register
 *
 * @param dev Sensor device
 * @param reg Register to read from (address)
 * @param dst Destination buffer to write to (if NULL, read data is discarded)
 * @param count Number of bytes to read
 *
 * @retval a value from spi_transceive_dt()
 */
static inline int adns7530_reg_read(const struct device *restrict dev, uint8_t reg, void *restrict dst, int count) {
    uint8_t addr = reg & 0x7f;
    const struct spi_buf tx_buf = {
        .buf = &addr,
        .len = 1
    };
    const struct spi_buf_set tx = {
        .buffers = &tx_buf,
        .count = 1
    };

    if (dst == NULL) {
        // for some reason when `spi_buf.buf` is NULL, `spi_buf.len` is ignored and always equals to 1.
        // therefore some valid memory region has to be allocated and proveded as a buf.
        uint8_t dst_dummy_buf[count];
        dst = dst_dummy_buf;
    }
    struct spi_buf rx_buf[2] = {
        // after some debugging with logic analyzer, it looks like the first byte is not clocked out
        // but gets written to the destination buffer. therefore the first spi_buf is required to skip it.
        {
            .buf = NULL,
            .len = 1
        },
        {
            .buf = dst,
            .len = count
        }
    };
    const struct spi_buf_set rx = {
        .buffers = rx_buf,
        .count = 2
    };

    return spi_transceive_dt(adns7530_get_spi(dev), &tx, &rx);
}

static inline int adns7530_reg_write(const struct device *restrict dev, uint8_t reg, uint8_t value) {
    uint8_t addr = 0x80 | (reg & 0x7f);
    uint8_t data[2] = {addr, value};
    const struct spi_buf tx_buf = {
        .buf = data,
        .len = 2
    };
    const struct spi_buf_set tx = {
        .buffers = &tx_buf,
        .count = 1
    };

    return spi_transceive_dt(adns7530_get_spi(dev), &tx, NULL);
}

int adns7530_init(const struct device *dev) {
    const struct adns7530_config *config = dev->config;
    const struct device *mot_gpio = device_get_binding(config->mot_label);
    if (mot_gpio == NULL) {
        return -ENXIO;
    }
    gpio_pin_configure(mot_gpio, config->mot_pin, GPIO_INPUT | config->mot_flags);

    adns7530_reg_write(dev, ADNS7530_REG_POWER_UP_RESET, ADNS7530_RESET_VALUE);
    k_msleep(10);
    adns7530_reg_write(dev, ADNS7530_REG_OBSERVATION, 0x00);
    k_msleep(10);
    uint8_t observation_val[ONE_BYTE_RX_BUF_SIZE] = {};
    adns7530_reg_read(dev, ADNS7530_REG_OBSERVATION, observation_val, ONE_BYTE_RX_BUF_SIZE);
    if ((*observation_val & 0xF) != 0xF) {
        LOG_ERR("invalid observation value %x", *observation_val);
        return -ENODATA;
    }
    adns7530_reg_read(dev, ADNS7530_REG_MOTION_BURST, NULL, 4);

    // Init sequence as specified in datasheet, meaning of all these registers is undocumented
    adns7530_reg_write(dev, 0x3C, 0x27);
    adns7530_reg_write(dev, 0x22, 0x0A);
    adns7530_reg_write(dev, 0x21, 0x01);
    adns7530_reg_write(dev, 0x3C, 0x32);
    adns7530_reg_write(dev, 0x23, 0x20);
    adns7530_reg_write(dev, 0x3C, 0x05);
    adns7530_reg_write(dev, 0x37, 0xB9);

    // Verify product ID
    uint8_t product_id[ONE_BYTE_RX_BUF_SIZE] = {};
    adns7530_reg_read(dev, ADNS7530_REG_PRODUCT_ID, product_id, ONE_BYTE_RX_BUF_SIZE);
    if (*product_id != ADNS7530_PRODUCT_ID) {
        LOG_ERR("invalid product id %x", *product_id);
        return -ENODATA;
    }

    // Settings
    adns7530_reg_write(dev, ADNS7530_REG_LASER_CTRL0, 0x00);
    adns7530_reg_write(dev, ADNS7530_REG_LASER_CTRL1, 0xC0);
    adns7530_reg_write(dev, ADNS7530_REG_LSRPWR_CFG0, 0xE0);
    adns7530_reg_write(dev, ADNS7530_REG_LSRPWR_CFG1, 0x1F);

    // Resolution and sleep mode timings
    adns7530_reg_write(dev, ADNS7530_REG_CFG, ADNS7530_RESOLUTION_1200 | ADNS7530_REST_ENABLE | ADNS7530_RUN_RATE_4MS);
    adns7530_reg_write(dev, ADNS7530_REG_REST2_DOWNSHIFT, 0x0A);
    adns7530_reg_write(dev, ADNS7530_REG_REST3_RATE, 0x63);

    return 0;
}

int adns7530_sample_fetch(const struct device *dev, enum sensor_channel chan) {
    struct adns7530_data* data = dev->data;
    struct {
        uint8_t motion;
        uint8_t delta_x_l;
        uint8_t delta_y_l;
        uint8_t delta_xy_h;
        uint8_t surf_qual;
    } motion_burst = {};

    adns7530_reg_read(dev, ADNS7530_REG_MOTION_BURST, &motion_burst, sizeof(motion_burst));

    if (motion_burst.motion & ADNS7530_LASER_FAULT_MASK || !(motion_burst.motion & ADNS7530_LASER_CFG_VALID_MASK)) {
        LOG_ERR("laser fault or laser invalid cfg");
        return -ENODATA;
    }

    // ADNS7530_MOTION_FLAG probably means "data ready" rather than "motion detected"
    if (!(motion_burst.motion & ADNS7530_MOTION_FLAG) || motion_burst.surf_qual < CONFIG_ADNS7530_SURF_QUAL_THRESHOLD) {
        data->delta_x = data->delta_y = 0;
        return 0;
    }

    data->delta_x = motion_burst.delta_x_l | ((motion_burst.delta_xy_h & 0xF0) << 4);
    data->delta_y = motion_burst.delta_y_l | ((motion_burst.delta_xy_h & 0x0F) << 8);

    // data is 12-bit signed int
    if (data->delta_x >> 11) data->delta_x = (data->delta_x & 0x7FF) - 0x800;
    if (data->delta_y >> 11) data->delta_y = (data->delta_y & 0x7FF) - 0x800;

    return 0;
}

int adns7530_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val) {
    struct adns7530_data* data = dev->data;

    switch (chan) {
        case SENSOR_CHAN_POS_DX:
            val->val1 = data->delta_x;
            break;
        case SENSOR_CHAN_POS_DY:
            val->val1 = data->delta_y;
            break;
        default:
            return -ENOTSUP;
    }

    return 0;
}

static struct adns7530_data adns7530_data = { };

static struct adns7530_config adns7530_config = {
    .spi = SPI_DT_SPEC_INST_GET(0, ADNS7530_SPI_OPERATION, 0),
    // motion pin config
    .mot_label = DT_INST_GPIO_LABEL(0, mot_gpios),
    .mot_pin = DT_INST_GPIO_PIN(0, mot_gpios),
    .mot_flags = DT_INST_GPIO_FLAGS(0, mot_gpios),
};

static const struct sensor_driver_api adns7530_api_funcs = {
    .sample_fetch = adns7530_sample_fetch,
    .channel_get = adns7530_channel_get,
};

DEVICE_DT_INST_DEFINE(
    0,
    adns7530_init,
    NULL,
    &adns7530_data,
    &adns7530_config,
    POST_KERNEL,
    CONFIG_SENSOR_INIT_PRIORITY,
    &adns7530_api_funcs);
