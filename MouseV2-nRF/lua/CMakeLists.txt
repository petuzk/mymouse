add_library(lua STATIC)

target_link_libraries(lua
    PRIVATE
        zephyr      # filesystem, hardware random number generator
)

target_sources(lua
    PUBLIC
        lapi.c
        lauxlib.c
        lbaselib.c
        lcode.c
        lcorolib.c
        lctype.c
        ldblib.c
        ldebug.c
        ldo.c
        ldump.c
        lfunc.c
        lgc.c
        linit.c
        # liolib.c  # uses files
        llex.c
        lmathlib.c
        lmem.c
        # loadlib.c  # provides support for dynamic libraries
        lobject.c
        lopcodes.c
        loslib.c
        lparser.c
        lstate.c
        lstring.c
        lstrlib.c
        ltable.c
        ltablib.c
        ltm.c
        # lua.c  # interpreter (has main)
        # luac.c  # compiler (has main)
        lundump.c
        lutf8lib.c
        lvm.c
        lzio.c
)

target_include_directories(app PUBLIC .)
