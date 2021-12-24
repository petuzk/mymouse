#include "lua_worker.h"

#include <string.h>

#include "lua.h"
#include "lstate.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lua_mv2lib.h"
#include "sys.h"

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

static k_ticks_t prog_btn_resume_at[MV2_LW_NUM_PROG_BUTTONS] = {};

static void mv2_lw_thread_f();

K_THREAD_DEFINE(mv2_lw_thread_id,
	CONFIG_PRJ_LUA_WORKER_THREAD_STACKSIZE,
	mv2_lw_thread_f,
	NULL, NULL, NULL,
	CONFIG_PRJ_LUA_WORKER_THREAD_PRIORITY,
	0,
	0);

// based on code from linit.c
static const luaL_Reg mv2_lw_libs[] = {
	{LUA_GNAME, luaopen_base},
	{LUA_TABLIBNAME, luaopen_table},
	{LUA_STRLIBNAME, luaopen_string},
	{MV2LIB_LIBNAME, mv2lib_open},
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
 * Based on luaB_cocreate from lcorolib.c
 *
 * @param L Lua main state
 * @return int
 */
static void mv2_lw_create_threads(lua_State *L) {
	// reset counter to 0 on main thread, this value will be copied to new threads
	mv2_lw_reset_counter_val(L);

	// first, push registry table on stack
	lua_pushvalue(L, LUA_REGISTRYINDEX);

	for (int i = 0; i < MV2_LW_NUM_PROG_BUTTONS; i++) {
		// push handler function on stack
		lua_getglobal(L, prog_btn_handler_names[i]);

		if (lua_isfunction(L, -1)) {
			lua_State *NL = lua_newthread(L);

			// L's stack contents now:
			//   -1 [newly created thread]
			//   -2 [  handler function  ]
			//   -3 [   registry table   ]

			// add thread to registry: registry[handler_func] = thread
			// table is at -3, handler function being a key and thread being a value will be popped from top
			lua_settable(L, -3);

			// store thread
			prog_btn_threads[i] = NL;
		} else {
			// no thread created, pop handler function
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
	for (int i = 0;; i = (++i == MV2_LW_NUM_PROG_BUTTONS) ? 0 : i) {
		int nargs = -1;
		lua_State *thread_L = prog_btn_threads[i];
		uint32_t curr_counter = prog_btn_counters[i];
		k_ticks_t resume_time = prog_btn_resume_at[i];

		// no thread means no user-defined handler
		if (!thread_L) {
			continue;
		}

		// if the thread is not running, fire event handler (if any)
		if (thread_L->status != LUA_YIELD && mv2_lw_get_counter_val(thread_L) < curr_counter) {
			// todo: replace lua_getglobal with possibly faster lookup
			lua_getglobal(thread_L, prog_btn_handler_names[i]);
			// handler's argument, i.e. current button state
			lua_pushboolean(thread_L, mv2_lw_inc_counter_val(thread_L) & 1);
			// require resume with 1 arg
			nargs = 1;
		}
		else if (thread_L->status == LUA_YIELD && resume_time && resume_time <= k_uptime_ticks()) {
			// require resume with no args
			nargs = 0;
		}

		if (nargs >= 0) {
			int status, nresults;
			status = lua_resume(thread_L, L, nargs, &nresults);

			if (status != LUA_YIELD) {
				// unschedule resume
				prog_btn_resume_at[i] = 0;
			} else {
				// thread yielded, schedule resume
				k_ticks_t resume_at = k_uptime_ticks();

				if (nresults && lua_isinteger(thread_L, 1)) {
					// add specified number of milliseconds to resume time
					resume_at += k_ms_to_ticks_floor64(lua_tointeger(thread_L, 1));
				}

				prog_btn_resume_at[i] = resume_at;
			}

			if (nresults) {
				lua_pop(thread_L, nresults);
			}
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
