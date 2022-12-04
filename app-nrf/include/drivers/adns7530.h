#pragma once

#include <stdint.h>

#include <hal/nrf_spim.h>

#define DT_DRV_COMPAT pixart_adns7530

#define ADNS7530_SPI_MODE   NRF_SPIM_MODE_3
#define ADNS7530_SPI_BITORD NRF_SPIM_BIT_ORDER_MSB_FIRST

#define ADNS7530_REG_PRODUCT_ID        0x00
#define ADNS7530_REG_REVISION_ID       0x01
#define ADNS7530_REG_MOTION            0x02
#define ADNS7530_REG_DELTA_X_L         0x03
#define ADNS7530_REG_DELTA_Y_L         0x04
#define ADNS7530_REG_DELTA_XY_H        0x05
#define ADNS7530_REG_SURF_QUAL         0x06
#define ADNS7530_REG_SHUTTER_H         0x07
#define ADNS7530_REG_SHUTTER_L         0x08
#define ADNS7530_REG_MAX_PIXEL         0x09
#define ADNS7530_REG_PIXEL_SUM         0x0a
#define ADNS7530_REG_MIN_PIXEL         0x0b
#define ADNS7530_REG_CRC0              0x0c
#define ADNS7530_REG_CRC1              0x0d
#define ADNS7530_REG_CRC2              0x0e
#define ADNS7530_REG_CRC3              0x0f
#define ADNS7530_REG_SELF_TEST         0x10
#define ADNS7530_REG_CFG               0x12
#define ADNS7530_REG_RUN_DOWNSHIFT     0x13
#define ADNS7530_REG_REST1_RATE        0x14
#define ADNS7530_REG_REST1_DOWNSHIFT   0x15
#define ADNS7530_REG_REST2_RATE        0x16
#define ADNS7530_REG_REST2_DOWNSHIFT   0x17
#define ADNS7530_REG_REST3_RATE        0x18
#define ADNS7530_REG_LASER_CTRL0       0x1a
#define ADNS7530_REG_LASER_CTRL1       0x1f
#define ADNS7530_REG_LSRPWR_CFG0       0x1c
#define ADNS7530_REG_LSRPWR_CFG1       0x1d
#define ADNS7530_REG_OBSERVATION       0x2e
#define ADNS7530_REG_PIXEL_GRAB        0x35
#define ADNS7530_REG_H_RESOLUTION      0x36
#define ADNS7530_REG_POWER_UP_RESET    0x3a
#define ADNS7530_REG_SHUTDOWN          0x3b
#define ADNS7530_REG_SHUTTER_THR       0x3d
#define ADNS7530_REG_INV_PRODUCT_ID    0x3f
#define ADNS7530_REG_INV_REVISION_ID   0x3e
#define ADNS7530_REG_MOTION_BURST      0x42

#define ADNS7530_PRODUCT_ID            0x31
#define ADNS7530_RESET_VALUE           0x5a  // writing this value to ADNS7530_REG_POWER_UP_RESET resets the chip

// ADNS7530_REG_MOTION
#define ADNS7530_LASER_FAULT_MASK      BIT(2)
#define ADNS7530_LASER_CFG_VALID_MASK  BIT(3)
#define ADNS7530_MOTION_FLAG           BIT(7)

// ADNS7530_REG_CFG
#define ADNS7530_RESOLUTION_400        (0b00 << 5)
#define ADNS7530_RESOLUTION_800        (0b01 << 5)
#define ADNS7530_RESOLUTION_1200       (0b10 << 5)
#define ADNS7530_RESOLUTION_1600       (0b11 << 5)
#define ADNS7530_REST_ENABLE           (0 << 3)
#define ADNS7530_REST_DISABLE          (1 << 3)
#define ADNS7530_RUN_RATE_2MS          0b000
#define ADNS7530_RUN_RATE_3MS          0b001
#define ADNS7530_RUN_RATE_4MS          0b010
#define ADNS7530_RUN_RATE_5MS          0b011
#define ADNS7530_RUN_RATE_6MS          0b100
#define ADNS7530_RUN_RATE_7MS          0b101
#define ADNS7530_RUN_RATE_8MS          0b110

struct adns7530_data {
    int16_t delta_x;
    int16_t delta_y;
};
