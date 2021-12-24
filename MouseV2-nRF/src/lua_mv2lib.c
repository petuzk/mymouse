#include "lua_mv2lib.h"

#include "lstate.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lua_worker.h"
#include "sys.h"

#define MV2LIB_TYPE_LED   (0x00 << 8)  // lower byte is a pin number
#define MV2LIB_TYPE_BTN   (0x01 << 8)
#define MV2LIB_TYPE_MASK  (0xFF << 8)

#define MV2LIB_VAL_MASK   (0xFF)

// based on luaB_yield from lcorolib.c
// sleep just yields the coroutine and the main thread then scedules resume
static int mv2_lw_lualib_sleep(lua_State *L) {
	return lua_yield(L, lua_gettop(L));
}

static int mv2_lw_lualib_set(lua_State *L) {
	if (lua_gettop(L) != 2) {
		return luaL_error(L, "2 args expected");
	}
	if (!lua_isinteger(L, 1)) {
		return luaL_typeerror(L, 1, "integer");
	}
	if (!lua_isboolean(L, 2)) {
		return luaL_typeerror(L, 2, "bool");
	}

	int what = lua_tointeger(L, 1);
	int to_what = lua_toboolean(L, 2);

	if ((what & MV2LIB_TYPE_MASK) == MV2LIB_TYPE_LED) {
		uint8_t led_pin = what & MV2LIB_VAL_MASK;
		if (led_pin != LED_GREEN_PIN && led_pin != LED_RED_PIN) {
			return luaL_argerror(L, 2, "bad LED");
		}

		const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);
		gpio_pin_set(gpio, led_pin, to_what);
	}
    else {
        return luaL_argerror(L, 2, "unknown value");
    }

	lua_settop(L, 0);
	return 0;
}

static const luaL_Reg base_funcs[] = {
	{"sleep", mv2_lw_lualib_sleep},
	{"set", mv2_lw_lualib_set},
	{NULL, NULL}
};

static void mv2lib_create_led_table(lua_State *L) {
    lua_createtable(L, 0, 2);
	lua_pushinteger(L, MV2LIB_TYPE_LED | LED_GREEN_PIN);
	lua_setfield(L, -2, "GREEN");
	lua_pushinteger(L, MV2LIB_TYPE_LED | LED_RED_PIN);
	lua_setfield(L, -2, "RED");
	lua_setfield(L, -2, "LED");  // assuming there is a global table
}

int mv2lib_open(lua_State *L) {
    lua_pushglobaltable(L);
	luaL_setfuncs(L, base_funcs, 0);
	mv2lib_create_led_table(L);
	return 0;
}
