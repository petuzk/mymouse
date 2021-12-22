add_library(umm_malloc STATIC)

target_include_directories(umm_malloc PUBLIC src)

target_sources(umm_malloc
    PUBLIC
        src/umm_info.c
        src/umm_integrity.c
        src/umm_malloc.c
        src/umm_poison.c
)
