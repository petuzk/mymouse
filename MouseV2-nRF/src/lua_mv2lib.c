#include "lua_mv2lib.h"

#include "lstate.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lua_worker.h"
#include "hids.h"
#include "sys.h"

#define MV2LIB_SET_TYPE_LED    (0x00 << 8)  // lower byte is a pin number
#define MV2LIB_SET_TYPE_BTN    (0x01 << 8)  // lower byte is a button id as defined by hids
#define MV2LIB_SET_TYPE_MASK   (0xFF << 8)

#define MV2LIB_SET_DEST_MASK   (0xFF)

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

	const int set_type_dest = lua_tointeger(L, 1);
	const int set_value = lua_toboolean(L, 2);
	const int set_type = set_type_dest & MV2LIB_SET_TYPE_MASK;
	const int set_dest = set_type_dest & MV2LIB_SET_DEST_MASK;

	if (set_type == MV2LIB_SET_TYPE_LED) {
		if (set_dest != LED_GREEN_PIN && set_dest != LED_RED_PIN) {
			return luaL_argerror(L, 2, "bad LED");
		}

		const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);
		gpio_pin_set(gpio, set_dest, set_value);
	}
	else if (set_type == MV2LIB_SET_TYPE_BTN) {
		if (set_dest != BUTTON_ID_LEFT && set_dest != BUTTON_ID_RIGHT && set_dest != BUTTON_ID_MIDDLE) {
			return luaL_argerror(L, 2, "bad button");
		}

		mv2_hids_set_button(set_dest, set_value);
	}
    else {
        return luaL_argerror(L, 1, "unknown destination");
    }

	lua_settop(L, 0);
	return 0;
}

static const luaL_Reg base_funcs[] = {
	{"sleep", mv2_lw_lualib_sleep},
	{"set", mv2_lw_lualib_set},
	{NULL, NULL}
};

struct mv2lib_set_id_table_elem {
	const char *name;  // either field or table name
	int value;  // if -1, the name represents table name (should be last element)
};

static struct mv2lib_set_id_table_elem mv2lib_set_led_table[] = {
	{"GREEN",  MV2LIB_SET_TYPE_LED | LED_GREEN_PIN},
	{"RED",    MV2LIB_SET_TYPE_LED | LED_RED_PIN},
	{"LED", -1}  // table name
};

static struct mv2lib_set_id_table_elem mv2lib_set_btn_table[] = {
	{"LEFT",   MV2LIB_SET_TYPE_BTN | BUTTON_ID_LEFT},
	{"RIGHT",  MV2LIB_SET_TYPE_BTN | BUTTON_ID_RIGHT},
	{"MIDDLE", MV2LIB_SET_TYPE_BTN | BUTTON_ID_MIDDLE},
	{"BTN", -1}  // table name
};

static void mv2lib_create_set_id_table(lua_State *L, struct mv2lib_set_id_table_elem *table) {
    lua_createtable(L, 0, 2);
	for (; table->value != -1; table++) {
		lua_pushinteger(L, table->value);
		lua_setfield(L, -2, table->name);
	}
	// assuming there is a global table
	lua_setfield(L, -2, table->name);
}

int mv2lib_open(lua_State *L) {
    lua_pushglobaltable(L);
	luaL_setfuncs(L, base_funcs, 0);
	mv2lib_create_set_id_table(L, mv2lib_set_led_table);
	mv2lib_create_set_id_table(L, mv2lib_set_btn_table);
	return 0;
}
