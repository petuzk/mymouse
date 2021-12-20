# SPDX-License-Identifier: Apache-2.0

# For the sake of demonstration, we add the driver directory as a zephyr module
# by hand. If your driver is a project that's managed by west, you can remove this line.
list(APPEND ZEPHYR_EXTRA_MODULES
  ${CMAKE_CURRENT_SOURCE_DIR}/adns7530_driver
  )

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mousev2-nrf)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
  src/main.c
  src/sys.c
  src/state.c
  src/bluetooth.c
  src/hids.c
  )
