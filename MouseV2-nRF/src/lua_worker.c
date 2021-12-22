#include "lua_worker.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "sys.h"  // used in mv2_lw_testlib_helloworld

static uint8_t mv2_lw_heap_buf[CONFIG_PRJ_LUA_WORKER_HEAP_SIZE];

static void mv2_lw_thread_f();

K_THREAD_DEFINE(mv2_lw_thread_id,
	CONFIG_PRJ_LUA_WORKER_THREAD_STACKSIZE,
	mv2_lw_thread_f,
	NULL, NULL, NULL,
	CONFIG_PRJ_LUA_WORKER_THREAD_PRIORITY,
	0,
	0);

// K_FIFO_DEFINE(name);

static int mv2_lw_testlib_helloworld(lua_State *L) {
	const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);

	k_msleep(1500);

	for (int i = 0; i < 3; i++) {
		gpio_pin_set(gpio, LED_RED_PIN, true);
		k_msleep(i == 2 ? 500 : 100);
		gpio_pin_set(gpio, LED_RED_PIN, false);
		k_msleep(100);
	}

	return 0;
}

static const luaL_Reg base_funcs[] = {
	{"hello_world", mv2_lw_testlib_helloworld},
	{NULL, NULL}
};

static int mv2_lw_open_test(lua_State *L) {
	lua_pushglobaltable(L);
	luaL_setfuncs(L, base_funcs, 0);
	return 0;
}

// based on code from linit.c
static const luaL_Reg mv2_lw_libs[] = {
	{LUA_GNAME, luaopen_base},
	{LUA_TABLIBNAME, luaopen_table},
	{LUA_STRLIBNAME, luaopen_string},
	{"test", mv2_lw_open_test},
	{NULL, NULL}
};

// based on code from linit.c
void mv2_lw_openlibs(lua_State *L) {
	const luaL_Reg *lib;
	for (lib = mv2_lw_libs; lib->func; lib++) {
		luaL_requiref(L, lib->name, lib->func, 1);
		lua_pop(L, 1);
	}
}

// based on code from lua.c
static int mv2_lw_luamain(lua_State *L) {
	luaL_checkversion(L);
	mv2_lw_openlibs(L);
	lua_gc(L, LUA_GCGEN, 0, 0);

	if (luaL_loadfile(L, MV2FS_MOUNTPOINT "/" CONFIG_PRJ_LUA_SCRIPT_NAME) != LUA_OK) {
		return 0;
	}

	if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
		return 0;
	}

	lua_pushboolean(L, 1);  // signal no errors
	return 1;
}

// based on code from lua.c
static void mv2_lw_thread_f() {
	int status, result;
	umm_init_heap(mv2_lw_heap_buf, CONFIG_PRJ_LUA_WORKER_HEAP_SIZE);

	lua_State *L = luaL_newstate();
	if (!L) {
		return;
	}

	lua_pushcfunction(L, &mv2_lw_luamain);  // to call 'mv2_lw_luamain' in protected mode
	status = lua_pcall(L, 0, 1, 0);         // do the call, require 1 return value (or nil)
	result = lua_toboolean(L, -1);          // if no value was returned, nil will be pushed and evaluated to false
	lua_close(L);
	// return (result && status == LUA_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
