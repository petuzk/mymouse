# SPDX-License-Identifier: MIT

list(APPEND ZEPHYR_EXTRA_MODULES
    ${CMAKE_CURRENT_SOURCE_DIR}/adns7530_driver
)

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

add_subdirectory(lua)

project(mousev2-nrf)

target_link_libraries(app PRIVATE lua)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
    src/main.c
    src/sys.c
    src/state.c
    src/bluetooth.c
    src/hids.c
    src/battery.c
    src/fs.c
)
