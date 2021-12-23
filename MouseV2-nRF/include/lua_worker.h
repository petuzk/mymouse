#ifndef MOUSEV2_LUA_WORKER_H
#define MOUSEV2_LUA_WORKER_H

#include <zephyr.h>

#include <umm_malloc.h>

#define MV2_LW_NUM_PROG_BUTTONS 6

/**
 * @brief Pin numbers of the programmable buttons.
 *
 * Correspond by index to @c prog_btn_counters.
 */
extern uint8_t prog_btn_pins[MV2_LW_NUM_PROG_BUTTONS];

/**
 * @brief In order to report button state change events, counters are used.
 *
 * The user is responsible to increment a counter when state changes and never decrement it.
 * The last bit of the counter should represent current button state.
 * Internally, counters are compared to a value stored in lua_State extra space.
 * Correspond by index to @c prog_btn_pins.
 */
extern uint32_t prog_btn_counters[MV2_LW_NUM_PROG_BUTTONS];

#endif // MOUSEV2_LUA_WORKER_H
