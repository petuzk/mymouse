#ifndef MOUSEV2_LUA_PATCHES_H
#define MOUSEV2_LUA_PATCHES_H

#include <random/rand32.h>

struct lua_State;

static inline unsigned int lua_rand32_get(struct lua_State *L) {
    return sys_rand32_get();
}

// Lua implementation often uses time() to achieve randomness
// use hardware rng instead of time
// see ltablib.c
#define l_randomizePivot  sys_rand32_get
// lstate.c
#define luai_makeseed     lua_rand32_get

#endif // MOUSEV2_LUA_PATCHES_H
