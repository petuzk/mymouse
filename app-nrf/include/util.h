#pragma once

#include <stdint.h>

#include <zephyr/arch/common/ffs.h>
#include <zephyr/sys/util_macro.h>

/**
 * @brief Helper function for iterating over positions of set bits in a bitmask.
 *
 * The bitmask is modified after each call (set bits are cleared one by one).
 * The function should only be called when @p mask is non-zero.
 *
 * @param mask Pointer to non-zero bitmask
 * @return Position of the first set least significant bit
 */
static inline uint32_t get_next_bit_pos(uint32_t* mask) {
    uint32_t pos = find_lsb_set(*mask) - 1;
    WRITE_BIT(*mask, pos, 0);
    return pos;
}
