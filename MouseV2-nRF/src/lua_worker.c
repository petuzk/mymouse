#include "lua_worker.h"

#include "lua.h"
#include "lstate.h"
#include "lauxlib.h"
#include "lualib.h"

#include "sys.h"  // used in mv2_lw_testlib_helloworld

static uint8_t mv2_lw_heap_buf[CONFIG_PRJ_LUA_WORKER_HEAP_SIZE];

// per-button entities (pins, counters, handler names, Lua threads)
// the button is identified by an index 0 to 5

uint8_t prog_btn_pins[MV2_LW_NUM_PROG_BUTTONS] = {
	BUTTON_SPEC_PIN,
	BUTTON_CENTER_PIN,
	BUTTON_UP_PIN,
	BUTTON_DOWN_PIN,
	BUTTON_FWD_PIN,
	BUTTON_BWD_PIN,
};

uint32_t prog_btn_counters[MV2_LW_NUM_PROG_BUTTONS] = {};

static const char *prog_btn_handler_names[MV2_LW_NUM_PROG_BUTTONS] = {
	"spec_h",
	"center_h",
	"up_h",
	"down_h",
	"fwd_h",
	"bwd_h",
};

static lua_State *prog_btn_threads[MV2_LW_NUM_PROG_BUTTONS] = {};

static void mv2_lw_thread_f();

K_THREAD_DEFINE(mv2_lw_thread_id,
	CONFIG_PRJ_LUA_WORKER_THREAD_STACKSIZE,
	mv2_lw_thread_f,
	NULL, NULL, NULL,
	CONFIG_PRJ_LUA_WORKER_THREAD_PRIORITY,
	0,
	0);

static int mv2_lw_testlib_helloworld(lua_State *L) {
	const struct device *gpio = DEVICE_DT_GET_ONE(nordic_nrf_gpio);

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

static inline void mv2_lw_reset_counter_val(lua_State *L) {
	*((uint32_t *) lua_getextraspace(L)) = 0;
}

static inline uint32_t mv2_lw_get_counter_val(lua_State *L) {
	return *((uint32_t *) lua_getextraspace(L));
}

static inline uint32_t mv2_lw_inc_counter_val(lua_State *L) {
	return ++(*((uint32_t *) lua_getextraspace(L)));
}

/**
 * @brief Creates Lua threads for each button.
 *
 * If a handler is defined by user, a thread is created with a handler function
 * on top of the stack. The thread is saved in corresponding @c prog_btn_threads slot.
 * If no handler is defined by user, a thread is not created and NULL is stored instead.
 * Created threads are saved in Lua registry to not be collected by GC.
 *
 * @param L Lua main state
 * @return int
 */
static void mv2_lw_create_threads(lua_State *L) {
	// reset counter to 0 on main thread, this value will be copied to new threads
	mv2_lw_reset_counter_val(L);
	// initialize threads (based on luaB_cocreate from lcorolib.c)
	// first, push registry table on stack
	lua_pushvalue(L, LUA_REGISTRYINDEX);

	for (int i = 0; i < MV2_LW_NUM_PROG_BUTTONS; i++) {
		// push handler function on stack
		lua_getglobal(L, prog_btn_handler_names[i]);

		if (lua_isfunction(L, -1)) {
			// duplicate the handler function
			lua_pushvalue(L, -1);
			lua_State *NL = lua_newthread(L);

			// stack contents now (there might be objects underneath):
			//   -1 [newly created thread]
			//   -2 [  handler function  ]
			//   -3 [  handler function  ]
			//   -4 [   registry table   ]

			// add thread to registry: registry[handler_func] = thread
			// table is at -4, handler function being a key and thread being a value will be popped from top
			lua_settable(L, -4);

			// store thread and move handler function (was -3) to the new thread
			prog_btn_threads[i] = NL;
			lua_xmove(L, NL, 1);
		} else {
			// pop handler function
			prog_btn_threads[i] = NULL;
			lua_pop(L, 1);
		}
	}

	// pop the registry table
	lua_pop(L, 1);
}

static int mv2_lw_luamain(lua_State *L) {
	// initialize Lua (based on code from lua.c)
	luaL_checkversion(L);
	mv2_lw_openlibs(L);
	lua_gc(L, LUA_GCGEN, 0, 0);

	// execute user code to obtain handler definitions
	if (luaL_loadfile(L, MV2FS_MOUNTPOINT "/" CONFIG_PRJ_LUA_SCRIPT_NAME) != LUA_OK) {
		return 0;
	}

	if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
		return 0;
	}

	// create Lua threads for running handlers (coroutines)
	mv2_lw_create_threads(L);

	// main loop
	for (int i = 0;;) {
		lua_State *thread_L = prog_btn_threads[i];
		uint32_t curr_counter = prog_btn_counters[i];

		// fire event handlers
		if (thread_L && thread_L->status != LUA_YIELD && mv2_lw_get_counter_val(thread_L) < curr_counter) {
			int status, nresults;

			// default thread's stack contents is a handler function
			// lua_resume pops the function, so we duplicate it first
			lua_pushvalue(thread_L, -1);
			// handler's argument, i.e. current button state
			lua_pushboolean(thread_L, mv2_lw_inc_counter_val(thread_L) & 1);
			// actual call
			status = lua_resume(thread_L, L, 1, &nresults);

			// clean results for now, can be used later
			if (status != LUA_YIELD) {
				// when status != LUA_YIELD, lua_resume treats everything on stack as return values,
				// but actually we've pushed a copy of the function earlier which should not be popped.
				nresults -= 1;
			}
			lua_pop(thread_L, nresults);
		}

		if (++i == MV2_LW_NUM_PROG_BUTTONS) {
			i = 0;
		}
	}

	// signal no errors
	lua_pushboolean(L, 1);
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

	// todo: is this really needed?
	// wouldn't it be better to save some ram and not instantiate another thread?
	lua_pushcfunction(L, &mv2_lw_luamain);  // to call 'mv2_lw_luamain' in protected mode
	status = lua_pcall(L, 0, 1, 0);         // do the call, require 1 return value (or nil)
	result = lua_toboolean(L, -1);          // if no value was returned, nil will be pushed and evaluated to false
	lua_close(L);
	// return (result && status == LUA_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
}
